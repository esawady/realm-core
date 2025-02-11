#include <realm/object-store/c_api/types.hpp>
#include "realm.hpp"

realm_callback_token_realm::~realm_callback_token_realm()
{
    realm::c_api::CBindingContext::get(*m_realm).realm_changed_callbacks().remove(m_token);
}

realm_callback_token_schema::~realm_callback_token_schema()
{
    realm::c_api::CBindingContext::get(*m_realm).schema_changed_callbacks().remove(m_token);
}

realm_refresh_callback_token::~realm_refresh_callback_token()
{
    realm::c_api::CBindingContext::get(*m_realm).realm_pending_refresh_callbacks().remove(m_token);
}

namespace realm::c_api {


RLM_API bool realm_get_version_id(const realm_t* realm, bool* out_found, realm_version_id_t* out_version)
{
    return wrap_err([&]() {
        util::Optional<VersionID> version = (*realm)->current_transaction_version();
        if (version) {
            if (out_version) {
                *out_version = to_capi(*version);
            }
            if (out_found) {
                *out_found = true;
            }
        }
        else {
            if (out_version) {
                *out_version = to_capi(VersionID(0, 0));
            }
            if (out_found) {
                *out_found = false;
            }
        }
        return true;
    });
}

RLM_API bool realm_get_num_versions(const realm_t* realm, uint64_t* out_versions_count)
{
    return wrap_err([&]() {
        if (out_versions_count) {
            *out_versions_count = (*realm)->get_number_of_versions();
        }
        return true;
    });
}

RLM_API const char* realm_get_library_version()
{
    return REALM_VERSION_STRING;
}

RLM_API void realm_get_library_version_numbers(int* out_major, int* out_minor, int* out_patch, const char** out_extra)
{
    *out_major = REALM_VERSION_MAJOR;
    *out_minor = REALM_VERSION_MINOR;
    *out_patch = REALM_VERSION_PATCH;
    *out_extra = REALM_VERSION_EXTRA;
}

RLM_API realm_t* realm_open(const realm_config_t* config)
{
    return wrap_err([&]() {
        return new shared_realm{Realm::get_shared_realm(*config)};
    });
}

RLM_API bool realm_convert_with_config(const realm_t* realm, const realm_config_t* config, bool merge_with_existing)
{
    return wrap_err([&]() {
        (*realm)->convert(*config, merge_with_existing);
        return true;
    });
}

RLM_API bool realm_convert_with_path(const realm_t* realm, const char* path, realm_binary_t encryption_key,
                                     bool merge_with_existing)
{
    return wrap_err([&]() {
        Realm::Config config;
        config.path = path;
        if (encryption_key.data) {
            config.encryption_key.assign(encryption_key.data, encryption_key.data + encryption_key.size);
        }
        (*realm)->convert(config, merge_with_existing);
        return true;
    });
}

RLM_API bool realm_delete_files(const char* realm_file_path, bool* did_delete_realm)
{
    return wrap_err([&]() {
        Realm::delete_files(realm_file_path, did_delete_realm);
        return true;
    });
}

RLM_API realm_t* _realm_from_native_ptr(const void* pshared_ptr, size_t n)
{
    REALM_ASSERT_RELEASE(n == sizeof(SharedRealm));
    auto ptr = static_cast<const SharedRealm*>(pshared_ptr);
    return new shared_realm{*ptr};
}

RLM_API void _realm_get_native_ptr(const realm_t* realm, void* pshared_ptr, size_t n)
{
    REALM_ASSERT_RELEASE(n == sizeof(SharedRealm));
    auto& shared_ptr = *static_cast<SharedRealm*>(pshared_ptr);
    shared_ptr = *realm;
}

RLM_API bool realm_is_closed(realm_t* realm)
{
    return (*realm)->is_closed();
}

RLM_API bool realm_is_writable(const realm_t* realm)
{
    return (*realm)->is_in_transaction() || (*realm)->is_in_async_transaction();
}

RLM_API bool realm_close(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->close();
        return true;
    });
}

RLM_API bool realm_begin_read(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->read_group();
        return true;
    });
}

RLM_API bool realm_begin_write(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->begin_transaction();
        return true;
    });
}

RLM_API bool realm_commit(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->commit_transaction();
        return true;
    });
}

RLM_API bool realm_rollback(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->cancel_transaction();
        return true;
    });
}

RLM_API unsigned int realm_async_begin_write(realm_t* realm, realm_async_begin_write_func_t callback,
                                             realm_userdata_t userdata, realm_free_userdata_func_t userdata_free,
                                             bool notify_only)
{
    auto cb = [callback, userdata = UserdataPtr{userdata, userdata_free}]() {
        callback(userdata.get());
    };
    return wrap_err([&]() {
        return (*realm)->async_begin_transaction(std::move(cb), notify_only);
    });
}

RLM_API unsigned int realm_async_commit(realm_t* realm, realm_async_commit_func_t callback, realm_userdata_t userdata,
                                        realm_free_userdata_func_t userdata_free, bool allow_grouping)
{
    auto cb = [callback, userdata = UserdataPtr{userdata, userdata_free}](std::exception_ptr err) {
        if (err) {
            try {
                std::rethrow_exception(err);
            }
            catch (const std::exception& e) {
                callback(userdata.get(), true, e.what());
            }
        }
        else {
            callback(userdata.get(), false, nullptr);
        }
    };
    return wrap_err([&]() {
        return (*realm)->async_commit_transaction(std::move(cb), allow_grouping);
    });
}

RLM_API bool realm_async_cancel(realm_t* realm, unsigned int token, bool* cancelled)
{
    return wrap_err([&]() {
        auto res = (*realm)->async_cancel_transaction(token);
        if (cancelled)
            *cancelled = res;
        return true;
    });
}

RLM_API realm_callback_token_t* realm_add_realm_changed_callback(realm_t* realm,
                                                                 realm_on_realm_change_func_t callback,
                                                                 realm_userdata_t userdata,
                                                                 realm_free_userdata_func_t free_userdata)
{
    util::UniqueFunction<void()> func = [callback, userdata = UserdataPtr{userdata, free_userdata}]() {
        callback(userdata.get());
    };
    return new realm_callback_token_realm(
        realm, CBindingContext::get(*realm).realm_changed_callbacks().add(std::move(func)));
}

RLM_API realm_refresh_callback_token_t* realm_add_realm_refresh_callback(realm_t* realm,
                                                                         realm_on_realm_refresh_func_t callback,
                                                                         realm_userdata_t userdata,
                                                                         realm_free_userdata_func_t userdata_free)
{
    util::UniqueFunction<void()> func = [callback, userdata = UserdataPtr{userdata, userdata_free}]() {
        callback(userdata.get());
    };

    if ((*realm)->is_frozen())
        return nullptr;

    const util::Optional<DB::version_type>& latest_snapshot_version = (*realm)->latest_snapshot_version();

    if (!latest_snapshot_version)
        return nullptr;

    auto& refresh_callbacks = CBindingContext::get(*realm).realm_pending_refresh_callbacks();
    return new realm_refresh_callback_token(realm, refresh_callbacks.add(*latest_snapshot_version, std::move(func)));
}

RLM_API bool realm_refresh(realm_t* realm)
{
    return wrap_err([&]() {
        (*realm)->refresh();
        return true;
    });
}

RLM_API realm_t* realm_freeze(const realm_t* live_realm)
{
    return wrap_err([&]() {
        auto& p = **live_realm;
        return new realm_t{p.freeze()};
    });
}

RLM_API bool realm_compact(realm_t* realm, bool* did_compact)
{
    return wrap_err([&]() {
        auto& p = **realm;
        *did_compact = p.compact();
        return true;
    });
}

RLM_API realm_t* realm_from_thread_safe_reference(realm_thread_safe_reference_t* tsr, realm_scheduler_t* scheduler)
{
    return wrap_err([&]() {
        auto rtsr = dynamic_cast<shared_realm::thread_safe_reference*>(tsr);
        if (!rtsr) {
            throw std::logic_error{"Thread safe reference type mismatch"};
        }

        // FIXME: This moves out of the ThreadSafeReference, so it isn't
        // reusable.
        std::shared_ptr<util::Scheduler> sch;
        if (scheduler) {
            sch = *scheduler;
        }
        auto realm = Realm::get_shared_realm(static_cast<ThreadSafeReference&&>(*rtsr), sch);
        return new shared_realm{std::move(realm)};
    });
}

CBindingContext& CBindingContext::get(SharedRealm realm)
{
    if (!realm->m_binding_context) {
        realm->m_binding_context.reset(new CBindingContext(realm));
    }

    CBindingContext* ctx = dynamic_cast<CBindingContext*>(realm->m_binding_context.get());
    REALM_ASSERT(ctx != nullptr);
    return *ctx;
}

void CBindingContext::did_change(std::vector<ObserverState> const&, std::vector<void*> const&, bool)
{
    if (auto ptr = realm.lock()) {
        auto version_id = ptr->read_transaction_version();
        m_realm_pending_refresh_callbacks.invoke(version_id.version);
    }
    m_realm_changed_callbacks.invoke();
}

} // namespace realm::c_api
