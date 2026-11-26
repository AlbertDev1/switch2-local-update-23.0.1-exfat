/*
 * match-version-unlocker
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <stratosphere.hpp>
#include <switch.h>

/*
 * ncm mitm for the ns process only.
 *
 * Since 20.0.0, ns copies the ProperProgramExists content meta attribute (bit 3), which it reads
 * with IContentMetaDatabase::GetAttributes, into the ApplicationDeliveryInfo used by "Match Version
 * with Local Users". A Switch 2 receiving from a Switch 1 only accepts the update when
 * ContentMetaPlatform == 0 && ProperProgramExists == 0 (switchbrew, ns cmd 2019), so Switch 1
 * consoles on 20.0.0+ are refused (error 2211-0520) for games with Switch 2 specific content.
 * We clear that bit for applications and patches as seen by ns; ns then builds and signs the
 * delivery info itself.
 *
 * Atmosphere's ncm server has MaxDomains = 0, so every ncm session is a plain session. IContentManager
 * 5 (OpenContentMetaDatabase) is therefore declared with a move handle, which is the non-domain wire
 * format of an out object. We create the session pair ourselves and register our wrapper as a mitm
 * session whose forward service is the real database, so every command we don't implement is
 * forwarded unchanged. Signatures match libstratosphere's own ncm interfaces.
 */

#define AMS_MVU_CONTENT_MANAGER_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 4, Result, OpenContentStorage,      (ams::sf::OutMoveHandle out, ams::ncm::StorageId storage_id), (out, storage_id)) \
    AMS_SF_METHOD_INFO(C, H, 5, Result, OpenContentMetaDatabase, (ams::sf::OutMoveHandle out, ams::ncm::StorageId storage_id), (out, storage_id))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IContentManagerMitmInterface, AMS_MVU_CONTENT_MANAGER_INTERFACE_INFO, 0x4D560001)

#define AMS_MVU_CONTENT_META_DATABASE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H,  1, Result, Get,                      (ams::sf::Out<u64> out_size, const ams::ncm::ContentMetaKey &key, const ams::sf::OutBuffer &out_value), (out_size, key, out_value)) \
    AMS_SF_METHOD_INFO(C, H,  3, Result, GetContentIdByType,      (ams::sf::Out<ams::ncm::ContentId> out_content_id, const ams::ncm::ContentMetaKey &key, ams::ncm::ContentType type), (out_content_id, key, type)) \
    AMS_SF_METHOD_INFO(C, H,  4, Result, ListContentInfo,         (ams::sf::Out<s32> out_entries_written, const ams::sf::OutArray<ams::ncm::ContentInfo> &out_info, const ams::ncm::ContentMetaKey &key, s32 offset), (out_entries_written, out_info, key, offset)) \
    AMS_SF_METHOD_INFO(C, H,  5, Result, List,                     (ams::sf::Out<s32> out_entries_total, ams::sf::Out<s32> out_entries_written, const ams::sf::OutArray<ams::ncm::ContentMetaKey> &out_info, ams::ncm::ContentMetaType meta_type, ams::ncm::ApplicationId application_id, u64 min, u64 max, ams::ncm::ContentInstallType install_type), (out_entries_total, out_entries_written, out_info, meta_type, application_id, min, max, install_type)) \
    AMS_SF_METHOD_INFO(C, H,  6, Result, GetLatestContentMetaKey,  (ams::sf::Out<ams::ncm::ContentMetaKey> out_key, u64 id),                                 (out_key, id)) \
    AMS_SF_METHOD_INFO(C, H,  7, Result, ListApplication,         (ams::sf::Out<s32> out_entries_total, ams::sf::Out<s32> out_entries_written, const ams::sf::OutArray<ams::ncm::ApplicationContentMetaKey> &out_keys, ams::ncm::ContentMetaType meta_type), (out_entries_total, out_entries_written, out_keys, meta_type)) \
    AMS_SF_METHOD_INFO(C, H,  8, Result, Has,                      (ams::sf::Out<bool> out, const ams::ncm::ContentMetaKey &key),                            (out, key)) \
    AMS_SF_METHOD_INFO(C, H, 10, Result, GetSize,                  (ams::sf::Out<u64> out_size, const ams::ncm::ContentMetaKey &key),                        (out_size, key)) \
    AMS_SF_METHOD_INFO(C, H, 11, Result, GetRequiredSystemVersion, (ams::sf::Out<u32> out_version, const ams::ncm::ContentMetaKey &key),               (out_version, key)) \
    AMS_SF_METHOD_INFO(C, H, 16, Result, HasContent,              (ams::sf::Out<bool> out, const ams::ncm::ContentMetaKey &key, const ams::ncm::ContentId &content_id), (out, key, content_id)) \
    AMS_SF_METHOD_INFO(C, H, 18, Result, GetAttributes,            (ams::sf::Out<u8> out_attributes, const ams::ncm::ContentMetaKey &key),              (out_attributes, key)) \
    AMS_SF_METHOD_INFO(C, H, 24, Result, GetContentInfoByType,    (ams::sf::Out<ams::ncm::ContentInfo> out_content_info, const ams::ncm::ContentMetaKey &key, ams::ncm::ContentType type), (out_content_info, key, type)) \
    AMS_SF_METHOD_INFO(C, H, 26, Result, GetPlatform,              (ams::sf::Out<ams::ncm::ContentMetaPlatform> out_platform, const ams::ncm::ContentMetaKey &key), (out_platform, key)) \
    AMS_SF_METHOD_INFO(C, H, 27, Result, HasAttributes,            (ams::sf::Out<u8> out_attributes, u8 attr_mask),                                     (out_attributes, attr_mask))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IContentMetaDatabaseMitmInterface, AMS_MVU_CONTENT_META_DATABASE_INTERFACE_INFO, 0x4D560002)

#define AMS_MVU_CONTENT_STORAGE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H,  7, Result, Has,                      (ams::sf::Out<bool> out, ams::ncm::ContentId content_id),                                       (out, content_id)) \
    AMS_SF_METHOD_INFO(C, H,  8, Result, GetPath,                  (ams::sf::Out<ams::ncm::Path> out, ams::ncm::ContentId content_id),                            (out, content_id)) \
    AMS_SF_METHOD_INFO(C, H, 14, Result, GetSizeFromContentId,     (ams::sf::Out<s64> out_size, ams::ncm::ContentId content_id),                                  (out_size, content_id)) \
    AMS_SF_METHOD_INFO(C, H, 18, Result, ReadContentIdFile,        (const ams::sf::OutBuffer &buf, ams::ncm::ContentId content_id, s64 offset),                    (buf, content_id, offset)) \
    AMS_SF_METHOD_INFO(C, H, 20, Result, GetRightsIdFromContentId, (ams::sf::Out<ams::ncm::RightsId> out_rights_id, ams::ncm::ContentId content_id, ams::fs::ContentAttributes attr), (out_rights_id, content_id, attr))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IContentStorageMitmInterface, AMS_MVU_CONTENT_STORAGE_INTERFACE_INFO, 0x4D560003)

namespace ams::mvu
{
    struct SystemUpdateProgress
    {
        s64 current_size;
        s64 total_size;
    };

    struct NimTaskId
    {
        u64 lo;
        u64 hi;
    };
    static_assert(sizeof(NimTaskId) == 0x10);
}

#define AMS_MVU_NSSU_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 17, Result, RequestSendSystemUpdate,     (ams::sf::OutCopyHandle out_event, ams::sf::OutMoveHandle out_async, u16 port, u32 addr, const ams::sf::InBuffer &info), (out_event, out_async, port, addr, info)) \
    AMS_SF_METHOD_INFO(C, H, 18, Result, GetSendSystemUpdateProgress, (ams::sf::Out<ams::mvu::SystemUpdateProgress> out_progress), (out_progress))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, INssuMitmInterface, AMS_MVU_NSSU_INTERFACE_INFO, 0x4D560004)

#define AMS_MVU_ASYNC_RESULT_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, Get,    (), ()) \
    AMS_SF_METHOD_INFO(C, H, 1, Result, Cancel, (), ())

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IAsyncResultMitmInterface, AMS_MVU_ASYNC_RESULT_INTERFACE_INFO, 0x4D560005)

#define AMS_MVU_NIMSHP_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 76, Result, CreateSendSystemUpdateTask, (ams::sf::Out<ams::mvu::NimTaskId> out_task_id, u16 port, u32 addr, u32 variation_id, ams::ncm::ContentMetaKey key), (out_task_id, port, addr, variation_id, key)) \
    AMS_SF_METHOD_INFO(C, H, 77, Result, RequestSendSystemUpdate,    (ams::sf::OutCopyHandle out_event, ams::sf::OutMoveHandle out_async, ams::mvu::NimTaskId task_id), (out_event, out_async, task_id))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, INimShpMitmInterface, AMS_MVU_NIMSHP_INTERFACE_INFO, 0x4D560006)

namespace ams::mvu
{
    void *Allocate(size_t size);
    void Deallocate(void *p, size_t size);

    /* ContentMetaAttribute bit 3, [20.0.0+]. Not named in libstratosphere's enum. */
    constexpr u8 ContentMetaAttribute_ProperProgramExists = (1 << 3);

    class ContentMetaDatabaseService
    {
    private:
        std::shared_ptr<::Service> m_srv;
        ncm::StorageId m_storage_id;
        u64 m_client_pid;

    public:
        explicit ContentMetaDatabaseService(std::shared_ptr<::Service> srv, ncm::StorageId storage_id, u64 client_pid)
            : m_srv(std::move(srv)), m_storage_id(storage_id), m_client_pid(client_pid) {}

    public:
        Result Get(sf::Out<u64> out_size, const ncm::ContentMetaKey &key, const sf::OutBuffer &out_value);
        Result GetContentIdByType(sf::Out<ncm::ContentId> out_content_id, const ncm::ContentMetaKey &key, ncm::ContentType type);
        Result ListContentInfo(sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ContentInfo> &out_info, const ncm::ContentMetaKey &key, s32 offset);
        Result List(sf::Out<s32> out_entries_total, sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ContentMetaKey> &out_info, ncm::ContentMetaType meta_type, ncm::ApplicationId application_id, u64 min, u64 max, ncm::ContentInstallType install_type);
        Result GetLatestContentMetaKey(sf::Out<ncm::ContentMetaKey> out_key, u64 id);
        Result ListApplication(sf::Out<s32> out_entries_total, sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ApplicationContentMetaKey> &out_keys, ncm::ContentMetaType meta_type);
        Result Has(sf::Out<bool> out, const ncm::ContentMetaKey &key);
        Result GetSize(sf::Out<u64> out_size, const ncm::ContentMetaKey &key);
        Result GetRequiredSystemVersion(sf::Out<u32> out_version, const ncm::ContentMetaKey &key);
        Result HasContent(sf::Out<bool> out, const ncm::ContentMetaKey &key, const ncm::ContentId &content_id);
        Result GetAttributes(sf::Out<u8> out_attributes, const ncm::ContentMetaKey &key);
        Result GetContentInfoByType(sf::Out<ncm::ContentInfo> out_content_info, const ncm::ContentMetaKey &key, ncm::ContentType type);
        Result GetPlatform(sf::Out<ncm::ContentMetaPlatform> out_platform, const ncm::ContentMetaKey &key);
        Result HasAttributes(sf::Out<u8> out_attributes, u8 attr_mask);
    };
    static_assert(IsIContentMetaDatabaseMitmInterface<ContentMetaDatabaseService>);

    class ContentStorageService
    {
    private:
        std::shared_ptr<::Service> m_srv;
        ncm::StorageId m_storage_id;
        u64 m_client_pid;

    public:
        explicit ContentStorageService(std::shared_ptr<::Service> srv, ncm::StorageId storage_id, u64 client_pid)
            : m_srv(std::move(srv)), m_storage_id(storage_id), m_client_pid(client_pid) {}

    public:
        Result Has(sf::Out<bool> out, ncm::ContentId content_id);
        Result GetPath(sf::Out<ncm::Path> out, ncm::ContentId content_id);
        Result GetSizeFromContentId(sf::Out<s64> out_size, ncm::ContentId content_id);
        Result ReadContentIdFile(const sf::OutBuffer &buf, ncm::ContentId content_id, s64 offset);
        Result GetRightsIdFromContentId(sf::Out<ncm::RightsId> out_rights_id, ncm::ContentId content_id, fs::ContentAttributes attr);
    };
    static_assert(IsIContentStorageMitmInterface<ContentStorageService>);

    class AsyncResultMitmService
    {
    private:
        std::shared_ptr<::Service> m_srv;

    public:
        explicit AsyncResultMitmService(std::shared_ptr<::Service> srv) : m_srv(std::move(srv)) {}

    public:
        Result Get();
        Result Cancel();
    };
    static_assert(IsIAsyncResultMitmInterface<AsyncResultMitmService>);

    class NimAsyncResultMitmService
    {
    private:
        std::shared_ptr<::Service> m_srv;

    public:
        explicit NimAsyncResultMitmService(std::shared_ptr<::Service> srv) : m_srv(std::move(srv)) {}

    public:
        Result Get();
        Result Cancel();
    };
    static_assert(IsIAsyncResultMitmInterface<NimAsyncResultMitmService>);

    class NimShpMitmService : public sf::MitmServiceImplBase
    {
    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        static bool ShouldMitm(const sm::MitmProcessInfo &client_info)
        {
            return client_info.program_id == ncm::SystemProgramId::Ns;
        }

    public:
        Result CreateSendSystemUpdateTask(sf::Out<NimTaskId> out_task_id, u16 port, u32 addr, u32 variation_id, ncm::ContentMetaKey key);
        Result RequestSendSystemUpdate(sf::OutCopyHandle out_event, sf::OutMoveHandle out_async, NimTaskId task_id);
    };
    static_assert(IsINimShpMitmInterface<NimShpMitmService>);

    class NssuMitmService : public sf::MitmServiceImplBase
    {
    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        static bool ShouldMitm(const sm::MitmProcessInfo &client_info)
        {
            return client_info.program_id == ncm::SystemAppletId::Qlaunch;
        }

    public:
        Result RequestSendSystemUpdate(sf::OutCopyHandle out_event, sf::OutMoveHandle out_async, u16 port, u32 addr, const sf::InBuffer &info);
        Result GetSendSystemUpdateProgress(sf::Out<SystemUpdateProgress> out_progress);
    };
    static_assert(IsINssuMitmInterface<NssuMitmService>);

    class ContentManagerMitmService : public sf::MitmServiceImplBase
    {
    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        static bool ShouldMitm(const sm::MitmProcessInfo &client_info)
        {
            return client_info.program_id == ncm::SystemProgramId::Ns ||
                   client_info.program_id == ncm::SystemProgramId::Nim;
        }

    public:
        Result OpenContentStorage(sf::OutMoveHandle out, ncm::StorageId storage_id);
        Result OpenContentMetaDatabase(sf::OutMoveHandle out, ncm::StorageId storage_id);
    };
    static_assert(IsIContentManagerMitmInterface<ContentManagerMitmService>);

    /*
     * Installs the ncm mitm when the firmware is supported. Otherwise releases the future mitm
     * declarations from mitm.lst.
     * Returns whether the mitm is active.
     */
    bool LaunchNcmMitm();

    /*
     * Periodically flushes the in-memory log buffer to sdmc:/mvu.log from a background thread.
     */
    void FlushLogToSd();

    /*
     * Timestamped logger writing to the shared in-memory ring buffer flushed to sdmc:/mvu.log.
     */
    void DebugLog(const char *fmt, ...);

    /*
     * Best-effort: mounts the ns-confidential system save 0x8000000000002020 and clears the
     * delivery lock (flags u32 at offset 0, bit 3) of the "flagset" file so that ns's
     * RequestSendSystemUpdate / RequestReceiveSystemUpdate pass their lock check.
     * The send_sysupdate IPS patch is the primary fix; this is belt & suspenders.
     */
    void CheckAndFixSystemSaveData();
}
