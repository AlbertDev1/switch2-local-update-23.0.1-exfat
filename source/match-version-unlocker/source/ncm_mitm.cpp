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
#include <stratosphere.hpp>
#include <stratosphere/sf/sf_mitm_dispatch.h>
#include <cstdarg>

#include "ncm_mitm.hpp"
#include "system_update_cnmt.hpp"
#include "nx2_content_meta_table.hpp"

namespace ams::mvu
{
    namespace
    {
        bool s_fs_initialized = false;
        bool s_sd_mounted = false;
        os::SdkMutex s_sd_mutex;

        bool EnsureFsInitialized()
        {
            if (!s_fs_initialized)
            {
                fs::SetAllocator(Allocate, Deallocate);
                fs::SetEnabledAutoAbort(false);
                fs::InitializeForSystem();
                s_fs_initialized = true;
            }
            return true;
        }

        bool EnsureSdMounted()
        {
            std::scoped_lock lk(s_sd_mutex);
            if (s_sd_mounted)
            {
                return true;
            }
            EnsureFsInitialized();
            if (R_SUCCEEDED(fs::MountSdCard("sdmc")))
            {
                s_sd_mounted = true;
                return true;
            }
            return false;
        }

        constexpr size_t LogBufferSize = 64 * 1024;
        char g_log_buffer[LogBufferSize];
        size_t g_log_offset = 0;
        size_t g_log_flushed = 0;
        os::SdkMutex g_log_mutex;

        void Log(const char *fmt, ...)
        {
            std::scoped_lock lk(g_log_mutex);
            char tmp[256];
            const u64 ms = os::GetSystemTick().ToTimeSpan().GetMilliSeconds();
            int prefix_len = util::SNPrintf(tmp, sizeof(tmp), "[%08llu ms] ", static_cast<unsigned long long>(ms));
            if (prefix_len > 0 && static_cast<size_t>(prefix_len) < sizeof(tmp))
            {
                std::va_list args;
                va_start(args, fmt);
                int msg_len = util::VSNPrintf(tmp + prefix_len, sizeof(tmp) - prefix_len, fmt, args);
                va_end(args);

                if (msg_len > 0)
                {
                    const size_t total = prefix_len + msg_len;
                    if (g_log_offset + total < LogBufferSize)
                    {
                        std::memcpy(g_log_buffer + g_log_offset, tmp, total);
                        g_log_offset += total;
                    }
                }
            }
        }

        constexpr s32 ThreadPriority = 6;
        constexpr size_t ThreadStackSize = 0x4000;

        struct ServerOptions
        {
            static constexpr size_t PointerBufferSize = 0x800;
            static constexpr size_t MaxDomains = 0;
            static constexpr size_t MaxDomainObjects = 0;
            static constexpr bool CanDeferInvokeRequest = false;
            static constexpr bool CanManageMitmServers = true;
        };

        constexpr size_t NcmMaxSessions = 48;
        constexpr size_t NssuMaxSessions = 48;
        constexpr size_t NimShpMaxSessions = 48;
        constexpr size_t NcmThreadCount = 2;
        constexpr size_t NssuThreadCount = 2;
        constexpr size_t NimShpThreadCount = 2;

        class NcmServerManager final : public sf::hipc::ServerManager<1, ServerOptions, NcmMaxSessions>
        {
        private:
            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
        };

        class NssuServerManager final : public sf::hipc::ServerManager<1, ServerOptions, NssuMaxSessions>
        {
        private:
            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
        };

        class NimShpServerManager final : public sf::hipc::ServerManager<1, ServerOptions, NimShpMaxSessions>
        {
        private:
            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
        };

        NcmServerManager g_ncm_server_manager;
        NssuServerManager g_nssu_server_manager;
        NimShpServerManager g_nimshp_server_manager;

        Result NcmServerManager::OnNeedsToAccept(int port_index, Server *server)
        {
            AMS_UNUSED(port_index);
            std::shared_ptr<::Service> fsrv;
            sm::MitmProcessInfo client_info;
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
            return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<IContentManagerMitmInterface, ContentManagerMitmService>(decltype(fsrv)(fsrv), client_info), fsrv);
        }

        Result NssuServerManager::OnNeedsToAccept(int port_index, Server *server)
        {
            AMS_UNUSED(port_index);
            std::shared_ptr<::Service> fsrv;
            sm::MitmProcessInfo client_info;
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
            Log("ns:su: accepted MITM session for pid=0x%016llx\n", client_info.program_id.value);
            return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<INssuMitmInterface, NssuMitmService>(decltype(fsrv)(fsrv), client_info), fsrv);
        }

        Result NimShpServerManager::OnNeedsToAccept(int port_index, Server *server)
        {
            AMS_UNUSED(port_index);
            std::shared_ptr<::Service> fsrv;
            sm::MitmProcessInfo client_info;
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
            Log("nim: accepted MITM session for pid=0x%016llx\n", client_info.program_id.value);
            return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<INimShpMitmInterface, NimShpMitmService>(decltype(fsrv)(fsrv), client_info), fsrv);
        }

        alignas(os::MemoryPageSize) u8 g_ncm_thread_stacks[NcmThreadCount][ThreadStackSize];
        os::ThreadType g_ncm_threads[NcmThreadCount];

        alignas(os::MemoryPageSize) u8 g_nssu_thread_stacks[NssuThreadCount][ThreadStackSize];
        os::ThreadType g_nssu_threads[NssuThreadCount];

        alignas(os::MemoryPageSize) u8 g_nimshp_thread_stacks[NimShpThreadCount][ThreadStackSize];
        os::ThreadType g_nimshp_threads[NimShpThreadCount];

        void LoopNcmServerThread(void *)
        {
            g_ncm_server_manager.LoopProcess();
        }

        void LoopNssuServerThread(void *)
        {
            g_nssu_server_manager.LoopProcess();
        }

        void LoopNimShpServerThread(void *)
        {
            g_nimshp_server_manager.LoopProcess();
        }

        constexpr sm::ServiceName NcmServiceName    = sm::ServiceName::Encode("ncm");
        constexpr sm::ServiceName NssuServiceName   = sm::ServiceName::Encode("ns:su");
        constexpr sm::ServiceName NimShpServiceName = sm::ServiceName::Encode("nim");

        void InitAndLoopNssuServerThread(void *)
        {
            const Result rc = g_nssu_server_manager.RegisterMitmServer<NssuMitmService>(0, NssuServiceName);
            if (R_FAILED(rc))
            {
                Log("ns:su: RegisterMitmServer failed (0x%x), releasing future mitm\n", rc.GetValue());
                static_cast<void>(sm::mitm::ClearFutureMitm(NssuServiceName));
                return;
            }
            Log("ns:su: MITM server registered\n");
            for (size_t i = 1; i < NssuThreadCount; i++)
            {
                R_ABORT_UNLESS(os::CreateThread(g_nssu_threads + i, LoopNssuServerThread, nullptr, g_nssu_thread_stacks[i], ThreadStackSize, ThreadPriority));
                os::SetThreadNamePointer(g_nssu_threads + i, "mvu::NssuMitm");
                os::StartThread(g_nssu_threads + i);
            }
            g_nssu_server_manager.LoopProcess();
        }

        void InitAndLoopNimShpServerThread(void *)
        {
            const Result rc = g_nimshp_server_manager.RegisterMitmServer<NimShpMitmService>(0, NimShpServiceName);
            if (R_FAILED(rc))
            {
                Log("nim: RegisterMitmServer failed (0x%x), releasing future mitm\n", rc.GetValue());
                static_cast<void>(sm::mitm::ClearFutureMitm(NimShpServiceName));
                return;
            }
            Log("nim: MITM server registered\n");
            for (size_t i = 1; i < NimShpThreadCount; i++)
            {
                R_ABORT_UNLESS(os::CreateThread(g_nimshp_threads + i, LoopNimShpServerThread, nullptr, g_nimshp_thread_stacks[i], ThreadStackSize, ThreadPriority));
                os::SetThreadNamePointer(g_nimshp_threads + i, "mvu::NimShpMitm");
                os::StartThread(g_nimshp_threads + i);
            }
            g_nimshp_server_manager.LoopProcess();
        }

        const char *ClientTag(u64 pid)
        {
            if (pid == ncm::SystemProgramId::Ns.value)
                return "ns";
            if (pid == ncm::SystemProgramId::Nim.value)
                return "nim";
            return "other";
        }

        bool IsSupportedFirmware()
        {
            return hos::GetVersion() >= hos::Version_19_0_0;
        }

        bool IsApplicationOrPatch(const ncm::ContentMetaKey &key)
        {
            return key.type == ncm::ContentMetaType::Application || key.type == ncm::ContentMetaType::Patch;
        }

        bool IsSystemUpdateKey(const ncm::ContentMetaKey &key, ncm::StorageId storage_id)
        {
            return key.id == 0x0100000000000816 || key.id == 0x0400000000000816 || key.id == 0x0100000000000800 ||
                   key.type == ncm::ContentMetaType::SystemUpdate ||
                   (key.id == 0 && storage_id == ncm::StorageId::BuiltInSystem);
        }

        bool IsSystemUpdateId(u64 id, ncm::StorageId storage_id)
        {
            return id == 0x0100000000000816 || id == 0x0400000000000816 || id == 0x0100000000000800 ||
                   (id == 0 && storage_id == ncm::StorageId::BuiltInSystem);
        }

        /* Content id of 7a596467ac9a087c2db8a4373b2c653b.cnmt.nca (NX2 23.0.1 SystemUpdate
           meta NCA — its extracted .cnmt matches SystemUpdateCnmtData). The size MUST stay
           the real one (9216): the content id is the SHA-256 of the file (hash-verified
           downstream) and the file is NOT padded. */
        constexpr u8 kSystemUpdateMetaNcaId[16] = {
            0x7a, 0x59, 0x64, 0x67, 0xac, 0x9a, 0x08, 0x7c,
            0x2d, 0xb8, 0xa4, 0x37, 0x3b, 0x2c, 0x65, 0x3b,
        };
        constexpr s64 kSystemUpdateMetaNcaSize = 9216;

        ncm::ContentInfo MakeContentInfoFromBytes(const u8 id_bytes[16], u64 size, ncm::ContentType type, u8 id_offset = 0)
        {
            ncm::ContentId content_id = {};
            std::memcpy(std::addressof(content_id), id_bytes, 16);
            return ncm::ContentInfo::Make(content_id, size, ncm::ContentInfo::DefaultContentAttributes, type, id_offset);
        }

        os::NativeHandle WrapContentMetaDatabase(const ::Service &real, ncm::StorageId storage_id, u64 client_pid)
        {
            auto released = std::make_shared<bool>(false);
            std::shared_ptr<::Service> fsrv(new ::Service(real), [released](::Service *s) {
                if (!*released)
                {
                    serviceClose(s);
                }
                delete s;
            });

            os::NativeHandle server_handle, client_handle;
            if (R_SUCCEEDED(sf::hipc::CreateSession(std::addressof(server_handle), std::addressof(client_handle))))
            {
                sf::SharedPointer<IContentMetaDatabaseMitmInterface> obj = sf::CreateSharedObjectEmplaced<IContentMetaDatabaseMitmInterface, ContentMetaDatabaseService>(fsrv, storage_id, client_pid);
                if (R_SUCCEEDED(g_ncm_server_manager.RegisterMitmSession(server_handle, sf::cmif::ServiceObjectHolder(std::move(obj)), std::move(fsrv))))
                {
                    return client_handle;
                }
                os::CloseNativeHandle(server_handle);
                os::CloseNativeHandle(client_handle);
            }

            *released = true;
            return real.session;
        }

        os::NativeHandle WrapContentStorage(const ::Service &real, ncm::StorageId storage_id, u64 client_pid)
        {
            auto released = std::make_shared<bool>(false);
            std::shared_ptr<::Service> fsrv(new ::Service(real), [released](::Service *s) {
                if (!*released)
                {
                    serviceClose(s);
                }
                delete s;
            });

            os::NativeHandle server_handle, client_handle;
            if (R_SUCCEEDED(sf::hipc::CreateSession(std::addressof(server_handle), std::addressof(client_handle))))
            {
                sf::SharedPointer<IContentStorageMitmInterface> obj = sf::CreateSharedObjectEmplaced<IContentStorageMitmInterface, ContentStorageService>(fsrv, storage_id, client_pid);
                if (R_SUCCEEDED(g_ncm_server_manager.RegisterMitmSession(server_handle, sf::cmif::ServiceObjectHolder(std::move(obj)), std::move(fsrv))))
                {
                    return client_handle;
                }
                os::CloseNativeHandle(server_handle);
                os::CloseNativeHandle(client_handle);
            }

            *released = true;
            return real.session;
        }

        os::NativeHandle WrapAsyncResult(const ::Service &real)
        {
            auto released = std::make_shared<bool>(false);
            std::shared_ptr<::Service> fsrv(new ::Service(real), [released](::Service *s) {
                if (!*released)
                {
                    serviceClose(s);
                }
                delete s;
            });

            os::NativeHandle server_handle, client_handle;
            if (R_SUCCEEDED(sf::hipc::CreateSession(std::addressof(server_handle), std::addressof(client_handle))))
            {
                sf::SharedPointer<IAsyncResultMitmInterface> obj = sf::CreateSharedObjectEmplaced<IAsyncResultMitmInterface, AsyncResultMitmService>(fsrv);
                if (R_SUCCEEDED(g_nssu_server_manager.RegisterMitmSession(server_handle, sf::cmif::ServiceObjectHolder(std::move(obj)), std::move(fsrv))))
                {
                    return client_handle;
                }
                os::CloseNativeHandle(server_handle);
                os::CloseNativeHandle(client_handle);
            }

            *released = true;
            return real.session;
        }

        os::NativeHandle WrapNimAsyncResult(const ::Service &real)
        {
            auto released = std::make_shared<bool>(false);
            std::shared_ptr<::Service> fsrv(new ::Service(real), [released](::Service *s) {
                if (!*released)
                {
                    serviceClose(s);
                }
                delete s;
            });

            os::NativeHandle server_handle, client_handle;
            if (R_SUCCEEDED(sf::hipc::CreateSession(std::addressof(server_handle), std::addressof(client_handle))))
            {
                sf::SharedPointer<IAsyncResultMitmInterface> obj = sf::CreateSharedObjectEmplaced<IAsyncResultMitmInterface, NimAsyncResultMitmService>(fsrv);
                if (R_SUCCEEDED(g_nimshp_server_manager.RegisterMitmSession(server_handle, sf::cmif::ServiceObjectHolder(std::move(obj)), std::move(fsrv))))
                {
                    return client_handle;
                }
                os::CloseNativeHandle(server_handle);
                os::CloseNativeHandle(client_handle);
            }

            *released = true;
            return real.session;
        }
    }

    bool LaunchNcmMitm()
    {
        if (!IsSupportedFirmware())
        {
            static_cast<void>(sm::mitm::ClearFutureMitm(NcmServiceName));
            static_cast<void>(sm::mitm::ClearFutureMitm(NssuServiceName));
            static_cast<void>(sm::mitm::ClearFutureMitm(NimShpServiceName));
            return false;
        }

        if (R_FAILED(g_ncm_server_manager.RegisterMitmServer<ContentManagerMitmService>(0, NcmServiceName)))
        {
            static_cast<void>(sm::mitm::ClearFutureMitm(NcmServiceName));
            static_cast<void>(sm::mitm::ClearFutureMitm(NssuServiceName));
            static_cast<void>(sm::mitm::ClearFutureMitm(NimShpServiceName));
            return false;
        }

        /* Start ncm workers immediately before installing ns:su / nim:shp MITMs, because
           nim (and ns/qlaunch) query ncm before registering nim:shp / ns:su. */
        for (size_t i = 0; i < NcmThreadCount; i++)
        {
            R_ABORT_UNLESS(os::CreateThread(g_ncm_threads + i, LoopNcmServerThread, nullptr, g_ncm_thread_stacks[i], ThreadStackSize, ThreadPriority));
            os::SetThreadNamePointer(g_ncm_threads + i, "mvu::NcmMitm");
            os::StartThread(g_ncm_threads + i);
        }

        R_ABORT_UNLESS(os::CreateThread(g_nssu_threads + 0, InitAndLoopNssuServerThread, nullptr, g_nssu_thread_stacks[0], ThreadStackSize, ThreadPriority));
        os::SetThreadNamePointer(g_nssu_threads + 0, "mvu::NssuMitm");
        os::StartThread(g_nssu_threads + 0);

        R_ABORT_UNLESS(os::CreateThread(g_nimshp_threads + 0, InitAndLoopNimShpServerThread, nullptr, g_nimshp_thread_stacks[0], ThreadStackSize, ThreadPriority));
        os::SetThreadNamePointer(g_nimshp_threads + 0, "mvu::NimShpMitm");
        os::StartThread(g_nimshp_threads + 0);

        return true;
    }

    void FlushLogToSd()
    {
        if (g_log_offset == g_log_flushed)
        {
            return;
        }

        if (!EnsureSdMounted())
        {
            return;
        }

        constexpr const char LogFilePath[] = "sdmc:/mvu.log";
        bool has_file = false;
        if (R_FAILED(fs::HasFile(std::addressof(has_file), LogFilePath)) || !has_file)
        {
            if (R_FAILED(fs::CreateFile(LogFilePath, 0)))
            {
                return;
            }
        }

        fs::FileHandle file;
        if (R_SUCCEEDED(fs::OpenFile(std::addressof(file), LogFilePath, fs::OpenMode_Write | fs::OpenMode_AllowAppend)))
        {
            s64 offset = 0;
            if (R_SUCCEEDED(fs::GetFileSize(std::addressof(offset), file)))
            {
                size_t to_write = 0;
                size_t start = 0;
                {
                    std::scoped_lock lk(g_log_mutex);
                    if (g_log_offset > g_log_flushed)
                    {
                        start = g_log_flushed;
                        to_write = g_log_offset - g_log_flushed;
                        g_log_flushed = g_log_offset;
                    }
                }
                if (to_write > 0)
                {
                    auto _ = fs::WriteFile(file, offset, g_log_buffer + start, to_write, fs::WriteOption::Flush);
                    AMS_UNUSED(_);
                }
                {
                    std::scoped_lock lk(g_log_mutex);
                    if (g_log_offset == g_log_flushed)
                    {
                        g_log_offset = 0;
                        g_log_flushed = 0;
                    }
                    else if (g_log_flushed > 0)
                    {
                        const size_t remaining = g_log_offset - g_log_flushed;
                        std::memmove(g_log_buffer, g_log_buffer + g_log_flushed, remaining);
                        g_log_offset = remaining;
                        g_log_flushed = 0;
                    }
                }
            }
            fs::CloseFile(file);
        }
    }

    void DebugLog(const char *fmt, ...)
    {
        std::scoped_lock lk(g_log_mutex);
        char tmp[256];
        const u64 ms = os::GetSystemTick().ToTimeSpan().GetMilliSeconds();
        int prefix_len = util::SNPrintf(tmp, sizeof(tmp), "[%08llu ms] ", static_cast<unsigned long long>(ms));
        if (prefix_len > 0 && static_cast<size_t>(prefix_len) < sizeof(tmp))
        {
            std::va_list args;
            va_start(args, fmt);
            int msg_len = util::VSNPrintf(tmp + prefix_len, sizeof(tmp) - prefix_len, fmt, args);
            va_end(args);

            if (msg_len > 0)
            {
                const size_t total = prefix_len + msg_len;
                if (g_log_offset + total < LogBufferSize)
                {
                    std::memcpy(g_log_buffer + g_log_offset, tmp, total);
                    g_log_offset += total;
                }
            }
        }
    }

    /*
     * ns's RequestSendSystemUpdate / RequestReceiveSystemUpdate abort with 0x67c10 (2016-0830)
     * when bit 3 of the u32 at offset 0 of the "flagset" file in the ns-confidential system
     * save 0x8000000000002020 is set (one-shot delivery marker). Missing file = flags 0 = OK.
     * We clear bit 3 at boot, before ns mounts the save itself. Failures are logged and
     * ignored: the send_sysupdate IPS patch NOPs the check anyway.
     */
    void CheckAndFixSystemSaveData()
    {
        EnsureFsInitialized();

        constexpr u64 FlagsetSaveId = 0x8000000000002020ull;
        constexpr const char *Mount = "mvufix";

        Result rc = fs::MountSystemSaveData(Mount, fs::SaveDataSpaceId::System, FlagsetSaveId);
        if (R_FAILED(rc))
        {
            rc = fs::MountSystemSaveData(Mount, FlagsetSaveId);
        }
        if (R_FAILED(rc))
        {
            Log("flagset: mount 0x8000000000002020 failed 0x%x (busy? ips couvre)\n", rc.GetValue());
            return;
        }

        fs::FileHandle file;
        rc = fs::OpenFile(std::addressof(file), "mvufix:/flagset", fs::OpenMode_Read);
        if (R_FAILED(rc))
        {
            Log("flagset: fichier absent (0x%x) -> rien a effacer\n", rc.GetValue());
            fs::Unmount(Mount);
            return;
        }

        u32 flags = 0;
        rc = fs::ReadFile(file, 0, std::addressof(flags), sizeof(flags));
        fs::CloseFile(file);
        if (R_FAILED(rc))
        {
            Log("flagset: lecture echouee 0x%x\n", rc.GetValue());
            fs::Unmount(Mount);
            return;
        }

        const u32 fixed = flags & ~0x8u; /* bit 3 = verrou RequestSend/ReceiveSystemUpdate */
        if (fixed == flags)
        {
            Log("flagset: bit3 deja clair (0x%08x)\n", flags);
            fs::Unmount(Mount);
            return;
        }

        if (R_FAILED(fs::OpenFile(std::addressof(file), "mvufix:/flagset", fs::OpenMode_Write | fs::OpenMode_AllowAppend)))
        {
            Log("flagset: reouverture en ecriture echouee\n");
            fs::Unmount(Mount);
            return;
        }
        Result wr = fs::WriteFile(file, 0, std::addressof(fixed), sizeof(fixed), fs::WriteOption::Flush);
        fs::CloseFile(file);
        Result cm = fs::CommitSaveData(Mount);
        Log("flagset: 0x%08x -> 0x%08x (write 0x%x, commit 0x%x)\n", flags, fixed, wr.GetValue(), cm.GetValue());
        fs::Unmount(Mount);
    }

    Result ContentManagerMitmService::OpenContentStorage(sf::OutMoveHandle out, ncm::StorageId storage_id)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        const u8 storage = static_cast<u8>(storage_id);
        ::Service real;
        const Result rc = serviceMitmDispatchIn(m_forward_service.get(), 4, storage, .out_num_objects = 1, .out_objects = &real);
        R_TRY(rc);

        Log("[%s] IContentManager::OpenContentStorage: storage_id=%u (%s)\n",
            ClientTag(m_client_info.program_id.value),
            storage,
            storage_id == ncm::StorageId::BuiltInSystem ? "BuiltInSystem [3]" :
            storage_id == ncm::StorageId::BuiltInUser   ? "BuiltInUser [4]" :
            storage_id == ncm::StorageId::SdCard        ? "SdCard [5]" : "Other");

        out.SetValue(WrapContentStorage(real, storage_id, m_client_info.program_id.value), false);
        R_SUCCEED();
    }

    Result ContentManagerMitmService::OpenContentMetaDatabase(sf::OutMoveHandle out, ncm::StorageId storage_id)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        const u8 storage = static_cast<u8>(storage_id);
        ::Service real;
        const Result rc = serviceMitmDispatchIn(m_forward_service.get(), 5, storage, .out_num_objects = 1, .out_objects = &real);
        R_TRY(rc);

        Log("[%s] IContentManager::OpenContentMetaDatabase: storage_id=%u (%s)\n",
            ClientTag(m_client_info.program_id.value),
            storage,
            storage_id == ncm::StorageId::BuiltInSystem ? "BuiltInSystem [3]" :
            storage_id == ncm::StorageId::BuiltInUser   ? "BuiltInUser [4]" :
            storage_id == ncm::StorageId::SdCard        ? "SdCard [5]" : "Other");

        out.SetValue(WrapContentMetaDatabase(real, storage_id, m_client_info.program_id.value), false);
        R_SUCCEED();
    }

    // --- ContentMetaDatabaseService implementation ---

    Result ContentMetaDatabaseService::Get(sf::Out<u64> out_size, const ncm::ContentMetaKey &key, const sf::OutBuffer &out_value)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            const size_t copy_size = std::min(out_value.GetSize(), SystemUpdateCnmtSize);
            std::memcpy(out_value.GetPointer(), SystemUpdateCnmtData, copy_size);
            if (copy_size >= sizeof(u64))
            {
                const u64 effective_id = (key.id == 0) ? 0x0400000000000816 : key.id;
                std::memcpy(out_value.GetPointer(), &effective_id, sizeof(u64));
            }
            out_size.SetValue(SystemUpdateCnmtSize);
            Log("[%s] MetaDb[%u]::Get: [SPOOFED SystemUpdate] key={id=0x%016llx, ver=0x%x, type=%u} -> size=%zu\n",
                ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), SystemUpdateCnmtSize);
            R_SUCCEED();
        }

        u64 size = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 1, key, size,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
            .buffers = { { out_value.GetPointer(), out_value.GetSize() } });
        out_size.SetValue(size);
        Log("[%s] MetaDb[%u]::Get: key={id=0x%016llx, ver=0x%x, type=%u} -> rc=0x%x, size=%llu\n",
            ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), rc.GetValue(), size);
        return rc;
    }

    Result ContentMetaDatabaseService::GetContentIdByType(sf::Out<ncm::ContentId> out_content_id, const ncm::ContentMetaKey &key, ncm::ContentType type)
    {
        if (IsSystemUpdateKey(key, m_storage_id) && static_cast<u8>(type) == 0)
        {
            ncm::ContentId content_id = {};
            std::memcpy(std::addressof(content_id), kSystemUpdateMetaNcaId, sizeof(kSystemUpdateMetaNcaId));
            out_content_id.SetValue(content_id);
            Log("[%s] MetaDb[%u]::GetContentIdByType: [SPOOFED SysUpdate] type=0 -> 7a596467ac9a087c2db8a4373b2c653b\n",
                ClientTag(m_client_pid), static_cast<u8>(m_storage_id));
            R_SUCCEED();
        }

        if (m_storage_id == ncm::StorageId::BuiltInSystem)
        {
            if (const Nx2ContentMetaEntry *entry = FindNx2ContentMeta(key))
            {
                const u8 *id_ptr = nullptr;
                if (type == ncm::ContentType::Meta)
                {
                    id_ptr = entry->meta_id;
                }
                else if (entry->has_content && static_cast<u8>(type) == entry->content_type)
                {
                    id_ptr = entry->content_id;
                }
                if (id_ptr != nullptr)
                {
                    ncm::ContentId content_id = {};
                    std::memcpy(std::addressof(content_id), id_ptr, 16);
                    out_content_id.SetValue(content_id);
                    const auto id_str = ncm::GetContentIdString(content_id);
                    Log("[%s] MetaDb[%u]::GetContentIdByType: [NX2] key={id=0x%016llx, ver=0x%x, type=%u}, content_type=%u -> id=%s.nca\n",
                        ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), static_cast<u32>(type), id_str.data);
                    R_SUCCEED();
                }
            }
        }

        ncm::ContentId content_id = {};
        const struct {
            ncm::ContentMetaKey key;
            u8 type;
        } in = { key, static_cast<u8>(type) };

        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 3, in, content_id);
        out_content_id.SetValue(content_id);
        const auto id_str = ncm::GetContentIdString(content_id);
        Log("[%s] MetaDb[%u]::GetContentIdByType: key={id=0x%016llx, ver=0x%x, type=%u}, content_type=%u -> rc=0x%x, id=%s.nca\n",
            ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), static_cast<u32>(type), rc.GetValue(), id_str.data);
        return rc;
    }

    Result ContentMetaDatabaseService::ListContentInfo(sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ContentInfo> &out_info, const ncm::ContentMetaKey &key, s32 offset)
    {
        if (m_storage_id == ncm::StorageId::BuiltInSystem)
        {
            const Nx2ContentMetaEntry *entry = FindNx2ContentMeta(key);
            if (entry == nullptr && IsSystemUpdateKey(key, m_storage_id))
            {
                const ncm::ContentMetaKey sys_key = {
                    .id = 0x0400000000000816,
                    .version = 0x5C01753C,
                    .type = ncm::ContentMetaType::SystemUpdate,
                    .install_type = ncm::ContentInstallType::Full,
                };
                entry = FindNx2ContentMeta(sys_key);
            }

            if (entry != nullptr)
            {
                ncm::ContentInfo infos[2] = {};
                infos[0] = MakeContentInfoFromBytes(entry->meta_id, entry->meta_size, ncm::ContentType::Meta, 0);
                s32 total = 1;
                if (entry->has_content)
                {
                    infos[1] = MakeContentInfoFromBytes(entry->content_id, entry->content_size, static_cast<ncm::ContentType>(entry->content_type), entry->content_id_offset);
                    total = 2;
                }

                s32 written = 0;
                if (offset >= 0 && offset < total && out_info.GetSize() > 0)
                {
                    written = std::min<s32>(total - offset, static_cast<s32>(out_info.GetSize()));
                    for (s32 i = 0; i < written; i++)
                    {
                        out_info[i] = infos[offset + i];
                    }
                }
                out_entries_written.SetValue(written);
                Log("[%s] MetaDb[%u]::ListContentInfo: [NX2] key={id=0x%016llx, ver=0x%x, type=%u} offset=%d -> written=%d\n",
                    ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), offset, written);
                R_SUCCEED();
            }
        }

        const struct {
            s32 offset;
            ncm::ContentMetaKey key;
        } in = { offset, key };

        s32 written = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 4, in, written,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
            .buffers = { { out_info.GetPointer(), out_info.GetSize() * sizeof(ncm::ContentInfo) } });
        out_entries_written.SetValue(written);
        Log("MetaDb[%u]::ListContentInfo: key={id=0x%016llx, ver=0x%x}, offset=%d -> rc=0x%x, written=%d\n",
            static_cast<u8>(m_storage_id), key.id, key.version, offset, rc.GetValue(), written);
        return rc;
    }

    Result ContentMetaDatabaseService::List(sf::Out<s32> out_entries_total, sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ContentMetaKey> &out_info, ncm::ContentMetaType meta_type, ncm::ApplicationId application_id, u64 min, u64 max, ncm::ContentInstallType install_type)
    {
        if (meta_type == ncm::ContentMetaType::SystemUpdate)
        {
            if (out_info.GetSize() > 0)
            {
                out_info[0] = ncm::ContentMetaKey{
                    .id = 0x0400000000000816,
                    .version = 0x5C01753C,
                    .type = ncm::ContentMetaType::SystemUpdate,
                    .install_type = ncm::ContentInstallType::Full,
                };
                out_entries_written.SetValue(1);
            }
            else
            {
                out_entries_written.SetValue(0);
            }
            out_entries_total.SetValue(1);
            Log("[%s] MetaDb[%u]::List: [SPOOFED SystemUpdate] -> 0x0400000000000816 v0x5C01753C\n",
                ClientTag(m_client_pid), static_cast<u8>(m_storage_id));
            R_SUCCEED();
        }

        const struct {
            u8 meta_type;
            u8 install_type;
            u8 padding[6];
            u64 id;
            u64 id_min;
            u64 id_max;
        } in = {
            static_cast<u8>(meta_type),
            static_cast<u8>(install_type),
            {},
            application_id.value,
            min,
            max
        };

        struct {
            s32 total;
            s32 written;
        } out = {};

        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 5, in, out,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
            .buffers = { { out_info.GetPointer(), out_info.GetSize() * sizeof(ncm::ContentMetaKey) } });

        out_entries_total.SetValue(out.total);
        out_entries_written.SetValue(out.written);
        Log("MetaDb[%u]::List: type=%u, app=0x%016llx -> rc=0x%x, written=%d, total=%d\n",
            static_cast<u8>(m_storage_id), static_cast<u32>(meta_type), application_id.value, rc.GetValue(), out.written, out.total);
        return rc;
    }

    Result ContentMetaDatabaseService::GetLatestContentMetaKey(sf::Out<ncm::ContentMetaKey> out_key, u64 id)
    {
        if (IsSystemUpdateId(id, m_storage_id))
        {
            const u64 spoof_id = (id == 0) ? 0x0400000000000816 : id;
            out_key.SetValue(ncm::ContentMetaKey{
                .id = spoof_id,
                .version = 0x5C01753C,
                .type = ncm::ContentMetaType::SystemUpdate,
                .install_type = ncm::ContentInstallType::Full,
            });
            Log("MetaDb[%u]::GetLatestContentMetaKey: [SPOOFED] req_id=0x%016llx (using 0x%016llx) -> v0x5C01753C\n",
                static_cast<u8>(m_storage_id), id, spoof_id);
            R_SUCCEED();
        }

        ncm::ContentMetaKey key = {};
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 6, id, key);
        out_key.SetValue(key);
        Log("MetaDb[%u]::GetLatestContentMetaKey: req_id=0x%016llx -> rc=0x%x, key={id=0x%016llx, ver=0x%x, type=%u}\n",
            static_cast<u8>(m_storage_id), id, rc.GetValue(), key.id, key.version, static_cast<u32>(key.type));
        return rc;
    }

    Result ContentMetaDatabaseService::ListApplication(sf::Out<s32> out_entries_total, sf::Out<s32> out_entries_written, const sf::OutArray<ncm::ApplicationContentMetaKey> &out_keys, ncm::ContentMetaType meta_type)
    {
        const u8 type = static_cast<u8>(meta_type);
        struct {
            s32 total;
            s32 written;
        } out = {};

        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 7, type, out,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
            .buffers = { { out_keys.GetPointer(), out_keys.GetSize() * sizeof(ncm::ApplicationContentMetaKey) } });
        out_entries_total.SetValue(out.total);
        out_entries_written.SetValue(out.written);
        Log("MetaDb[%u]::ListApplication: type=%u -> rc=0x%x, written=%d, total=%d\n",
            static_cast<u8>(m_storage_id), static_cast<u32>(meta_type), rc.GetValue(), out.written, out.total);
        return rc;
    }

    Result ContentMetaDatabaseService::Has(sf::Out<bool> out, const ncm::ContentMetaKey &key)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            out.SetValue(true);
            Log("MetaDb[%u]::Has: [SPOOFED] key={id=0x%016llx, ver=0x%x, type=%u} -> true\n",
                static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type));
            R_SUCCEED();
        }

        if (m_storage_id == ncm::StorageId::BuiltInSystem && FindNx2ContentMeta(key) != nullptr)
        {
            out.SetValue(true);
            Log("[%s] MetaDb[%u]::Has: [NX2] key={id=0x%016llx, ver=0x%x, type=%u} -> true\n",
                ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type));
            R_SUCCEED();
        }

        u8 has = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 8, key, has);
        out.SetValue((has & 1) != 0);
        Log("MetaDb[%u]::Has: key={id=0x%016llx, ver=0x%x, type=%u} -> rc=0x%x, has=%d\n",
            static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(key.type), rc.GetValue(), has);
        return rc;
    }

    Result ContentMetaDatabaseService::GetSize(sf::Out<u64> out_size, const ncm::ContentMetaKey &key)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            out_size.SetValue(SystemUpdateCnmtSize);
            Log("MetaDb[%u]::GetSize: [SPOOFED] key={id=0x%016llx} -> %zu\n", static_cast<u8>(m_storage_id), key.id, SystemUpdateCnmtSize);
            R_SUCCEED();
        }

        u64 size = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 10, key, size);
        out_size.SetValue(size);
        Log("MetaDb[%u]::GetSize: key={id=0x%016llx} -> rc=0x%x, size=%llu\n", static_cast<u8>(m_storage_id), key.id, rc.GetValue(), size);
        return rc;
    }

    Result ContentMetaDatabaseService::GetRequiredSystemVersion(sf::Out<u32> out_version, const ncm::ContentMetaKey &key)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            out_version.SetValue(0x5C01753C);
            Log("MetaDb[%u]::GetRequiredSystemVersion: [SPOOFED SysUpdate] -> 0x5C01753C\n", static_cast<u8>(m_storage_id));
            R_SUCCEED();
        }

        u32 version = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 11, key, version);
        if (R_SUCCEEDED(rc) && IsApplicationOrPatch(key))
        {
            version = 0x5C01753C;
            Log("MetaDb[%u]::GetRequiredSystemVersion: [SPOOFED Game 0x%016llx] -> 0x5C01753C\n", static_cast<u8>(m_storage_id), key.id);
        }
        out_version.SetValue(version);
        Log("MetaDb[%u]::GetRequiredSystemVersion: key={id=0x%016llx} -> rc=0x%x, ver=0x%x\n",
            static_cast<u8>(m_storage_id), key.id, rc.GetValue(), version);
        return rc;
    }

    Result ContentMetaDatabaseService::HasContent(sf::Out<bool> out, const ncm::ContentMetaKey &key, const ncm::ContentId &content_id)
    {
        const struct {
            ncm::ContentMetaKey key;
            ncm::ContentId content_id;
        } in = { key, content_id };

        u8 has = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 16, in, has);
        out.SetValue((has & 1) != 0);
        const auto id_str = ncm::GetContentIdString(content_id);
        Log("MetaDb[%u]::HasContent: key={id=0x%016llx}, %s.nca -> rc=0x%x, has=%d\n",
            static_cast<u8>(m_storage_id), key.id, id_str.data, rc.GetValue(), has);
        return rc;
    }

    Result ContentMetaDatabaseService::GetAttributes(sf::Out<u8> out_attributes, const ncm::ContentMetaKey &key)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            out_attributes.SetValue(0);
            Log("MetaDb[%u]::GetAttributes: [SPOOFED SysUpdate 0x%016llx] -> 0\n", static_cast<u8>(m_storage_id), key.id);
            R_SUCCEED();
        }

        u8 attributes = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 18, key, attributes);
        if (R_SUCCEEDED(rc))
        {
            const u8 orig = attributes;
            if (IsApplicationOrPatch(key))
            {
                attributes = static_cast<u8>(attributes & ~ContentMetaAttribute_ProperProgramExists);
            }
            // Preserve native BootImagePackage attributes.
            // Do not suppress IncludesExFatDriver.
            Log("MetaDb[%u]::GetAttributes: key={id=0x%016llx, type=%u} -> orig=0x%02x, val=0x%02x\n",
                static_cast<u8>(m_storage_id), key.id, static_cast<u32>(key.type), orig, attributes);
        }
        out_attributes.SetValue(attributes);
        return rc;
    }

    Result ContentMetaDatabaseService::GetContentInfoByType(sf::Out<ncm::ContentInfo> out_content_info, const ncm::ContentMetaKey &key, ncm::ContentType type)
    {
        /* SystemUpdate spoof: ns asks for the Meta (type 0) content of 0x0400000000000816 to
           stream it first. Without this the real BuiltInSystem database returns 0xe05
           (ncm desc 517) which surfaces as 2211-0517 on the receiving console. */
        if (IsSystemUpdateKey(key, m_storage_id) && static_cast<u8>(type) == 0)
        {
            ncm::ContentInfo info = {};
            std::memcpy(std::addressof(info.content_id), kSystemUpdateMetaNcaId, sizeof(kSystemUpdateMetaNcaId));
            info.size_low  = static_cast<u32>(kSystemUpdateMetaNcaSize & 0xffffffffu);
            info.size_high = static_cast<u8>((kSystemUpdateMetaNcaSize >> 32) & 0xffu);
            info.content_attributes = ncm::ContentInfo::DefaultContentAttributes;
            info.content_type = static_cast<ncm::ContentType>(0);
            info.id_offset = 0;

            out_content_info.SetValue(info);
            Log("[%s] MetaDb[%u]::GetContentInfoByType: [SPOOFED SysUpdate] type=0 -> 7a596467ac9a087c2db8a4373b2c653b (%lld)\n",
                ClientTag(m_client_pid), static_cast<u8>(m_storage_id), static_cast<long long>(kSystemUpdateMetaNcaSize));
            R_SUCCEED();
        }

        if (m_storage_id == ncm::StorageId::BuiltInSystem)
        {
            if (const Nx2ContentMetaEntry *entry = FindNx2ContentMeta(key))
            {
                if (type == ncm::ContentType::Meta)
                {
                    const ncm::ContentInfo info = MakeContentInfoFromBytes(entry->meta_id, entry->meta_size, ncm::ContentType::Meta, 0);
                    out_content_info.SetValue(info);
                    const auto id_str = ncm::GetContentIdString(info.content_id);
                    Log("[%s] MetaDb[%u]::GetContentInfoByType: [NX2] key={id=0x%016llx, ver=0x%x}, type=0 -> %s.nca (%llu)\n",
                        ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, id_str.data, static_cast<unsigned long long>(entry->meta_size));
                    R_SUCCEED();
                }
                if (entry->has_content && static_cast<u8>(type) == entry->content_type)
                {
                    const ncm::ContentInfo info = MakeContentInfoFromBytes(entry->content_id, entry->content_size, static_cast<ncm::ContentType>(entry->content_type), entry->content_id_offset);
                    out_content_info.SetValue(info);
                    const auto id_str = ncm::GetContentIdString(info.content_id);
                    Log("[%s] MetaDb[%u]::GetContentInfoByType: [NX2] key={id=0x%016llx, ver=0x%x}, type=%u -> %s.nca (%llu)\n",
                        ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(type), id_str.data, static_cast<unsigned long long>(entry->content_size));
                    R_SUCCEED();
                }
            }
        }

        const struct {
            ncm::ContentMetaKey key;
            u8 type;
        } in = { key, static_cast<u8>(type) };

        ncm::ContentInfo info = {};
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 24, in, info);
        out_content_info.SetValue(info);
        const auto id_str = ncm::GetContentIdString(info.content_id);
        Log("[%s] MetaDb[%u]::GetContentInfoByType: key={id=0x%016llx, ver=0x%x}, type=%u -> rc=0x%x, id=%s.nca\n",
            ClientTag(m_client_pid), static_cast<u8>(m_storage_id), key.id, key.version, static_cast<u32>(type), rc.GetValue(), id_str.data);
        return rc;
    }

    Result ContentMetaDatabaseService::GetPlatform(sf::Out<ncm::ContentMetaPlatform> out_platform, const ncm::ContentMetaKey &key)
    {
        if (IsSystemUpdateKey(key, m_storage_id))
        {
            out_platform.SetValue(static_cast<ncm::ContentMetaPlatform>(1));
            Log("MetaDb[%u]::GetPlatform: [SPOOFED SysUpdate 0x%016llx] -> Ounce (1)\n", static_cast<u8>(m_storage_id), key.id);
            R_SUCCEED();
        }

        ncm::ContentMetaPlatform platform = ncm::ContentMetaPlatform::Nx;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 26, key, platform);
        out_platform.SetValue(platform);
        Log("MetaDb[%u]::GetPlatform: key={id=0x%016llx, type=%u} -> rc=0x%x, platform=%u\n",
            static_cast<u8>(m_storage_id), key.id, static_cast<u32>(key.type), rc.GetValue(), static_cast<u32>(platform));
        return rc;
    }

    Result ContentMetaDatabaseService::HasAttributes(sf::Out<u8> out_attributes, u8 attr_mask)
    {
        u8 attributes = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 27, attr_mask, attributes);
        if (R_SUCCEEDED(rc))
        {
            const u8 orig = attributes;
            attributes = static_cast<u8>(attributes & ~ContentMetaAttribute_ProperProgramExists);
            attributes = static_cast<u8>(attributes & ~0x01);
            Log("MetaDb[%u]::HasAttributes: mask=0x%02x -> orig=0x%02x, val=0x%02x\n", static_cast<u8>(m_storage_id), attr_mask, orig, attributes);
        }
        out_attributes.SetValue(attributes);
        return rc;
    }

    // --- ContentStorageService helpers and implementation ---

    namespace
    {
        bool FormatSdPath(char *dst, size_t dst_size, const ncm::ContentId &content_id)
        {
            const auto id_str = ncm::GetContentIdString(content_id);
            const int written = util::SNPrintf(dst, dst_size, "sdmc:/NX2_23_0_1/%s.nca", id_str.data);
            return written > 0 && static_cast<size_t>(written) < dst_size;
        }

        bool FormatCommonPath(char *dst, size_t dst_size, const ncm::ContentId &content_id)
        {
            const auto id_str = ncm::GetContentIdString(content_id);
            const int written = util::SNPrintf(dst, dst_size, "@Sdcard:/NX2_23_0_1/%s.nca", id_str.data);
            return written > 0 && static_cast<size_t>(written) < dst_size;
        }

        bool DoesContentExistOnSd(const ncm::ContentId &content_id)
        {
            if (!EnsureSdMounted())
            {
                return false;
            }
            char path[128];
            if (!FormatSdPath(path, sizeof(path), content_id))
            {
                return false;
            }
            fs::FileHandle file;
            if (R_SUCCEEDED(fs::OpenFile(std::addressof(file), path, fs::OpenMode_Read)))
            {
                fs::CloseFile(file);
                return true;
            }
            return false;
        }
    }

    Result ContentStorageService::Has(sf::Out<bool> out, ncm::ContentId content_id)
    {
        const auto id_str = ncm::GetContentIdString(content_id);
        if ((m_storage_id == ncm::StorageId::BuiltInSystem || m_storage_id == ncm::StorageId::BuiltInUser) && DoesContentExistOnSd(content_id))
        {
            out.SetValue(true);
            Log("[%s] Storage[%u]::Has: %s.nca -> TRUE [SD: sdmc:/NX2_23_0_1/]\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data);
            R_SUCCEED();
        }

        NcmContentStorage cs = { *m_srv.get() };
        bool has = false;
        const Result rc = ncmContentStorageHas(&cs, &has, reinterpret_cast<const NcmContentId *>(&content_id));
        out.SetValue(has);
        Log("[%s] Storage[%u]::Has: %s.nca -> rc=0x%x, has=%d\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, rc.GetValue(), has);
        return rc;
    }

    Result ContentStorageService::GetPath(sf::Out<ncm::Path> out, ncm::ContentId content_id)
    {
        const auto id_str = ncm::GetContentIdString(content_id);
        if ((m_storage_id == ncm::StorageId::BuiltInSystem || m_storage_id == ncm::StorageId::BuiltInUser) && DoesContentExistOnSd(content_id))
        {
            char common_path[sizeof(out.GetPointer()->str)];
            if (FormatCommonPath(common_path, sizeof(common_path), content_id))
            {
                out.SetValue(ncm::Path::Encode(common_path));
                Log("[%s] Storage[%u]::GetPath: %s.nca -> %s [SD]\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, common_path);
                R_SUCCEED();
            }
        }

        NcmContentStorage cs = { *m_srv.get() };
        const Result rc = ncmContentStorageGetPath(&cs, out.GetPointer()->str, sizeof(out.GetPointer()->str), reinterpret_cast<const NcmContentId *>(&content_id));
        Log("[%s] Storage[%u]::GetPath: %s.nca -> rc=0x%x, path=%s\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, rc.GetValue(), out.GetPointer()->str);
        return rc;
    }

    Result ContentStorageService::GetSizeFromContentId(sf::Out<s64> out_size, ncm::ContentId content_id)
    {
        const auto id_str = ncm::GetContentIdString(content_id);
        if ((m_storage_id == ncm::StorageId::BuiltInSystem || m_storage_id == ncm::StorageId::BuiltInUser) && EnsureSdMounted())
        {
            char path[128];
            if (FormatSdPath(path, sizeof(path), content_id))
            {
                fs::FileHandle file;
                if (R_SUCCEEDED(fs::OpenFile(std::addressof(file), path, fs::OpenMode_Read)))
                {
                    s64 size = 0;
                    const Result rc = fs::GetFileSize(std::addressof(size), file);
                    fs::CloseFile(file);
                    if (R_SUCCEEDED(rc))
                    {
                        out_size.SetValue(size);
                        Log("[%s] Storage[%u]::GetSize: %s.nca -> size=0x%llx (%lld bytes) [SD: sdmc:/NX2_23_0_1/]\n",
                            ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, size, size);
                        R_SUCCEED();
                    }
                }
            }
        }

        NcmContentStorage cs = { *m_srv.get() };
        s64 size = 0;
        const Result rc = ncmContentStorageGetSizeFromContentId(&cs, &size, reinterpret_cast<const NcmContentId *>(&content_id));
        out_size.SetValue(size);
        Log("[%s] Storage[%u]::GetSize: %s.nca -> rc=0x%x, size=0x%llx\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, rc.GetValue(), size);
        return rc;
    }

    Result ContentStorageService::ReadContentIdFile(const sf::OutBuffer &buf, ncm::ContentId content_id, s64 offset)
    {
        const auto id_str = ncm::GetContentIdString(content_id);
        if ((m_storage_id == ncm::StorageId::BuiltInSystem || m_storage_id == ncm::StorageId::BuiltInUser) && EnsureSdMounted())
        {
            char path[128];
            if (FormatSdPath(path, sizeof(path), content_id))
            {
                fs::FileHandle file;
                if (R_SUCCEEDED(fs::OpenFile(std::addressof(file), path, fs::OpenMode_Read)))
                {
                    const Result rc = fs::ReadFile(file, offset, buf.GetPointer(), buf.GetSize());
                    fs::CloseFile(file);
                    if (R_SUCCEEDED(rc))
                    {
                        if (offset == 0)
                        {
                            Log("[%s] Storage[%u]::ReadContentIdFile: %s.nca [SD] offset=0x0, bytes=0x%zx -> SUCCESS\n",
                                ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, buf.GetSize());
                        }
                        R_SUCCEED();
                    }
                    Log("[%s] Storage[%u]::ReadContentIdFile: %s.nca [SD] offset=0x%llx, bytes=0x%zx -> FAILED (rc=0x%x)\n",
                        ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, offset, buf.GetSize(), rc.GetValue());
                }
            }
        }

        NcmContentStorage cs = { *m_srv.get() };
        const Result rc = ncmContentStorageReadContentIdFile(&cs, buf.GetPointer(), buf.GetSize(), reinterpret_cast<const NcmContentId *>(&content_id), offset);
        Log("[%s] Storage[%u]::ReadContentIdFile: %s.nca offset=0x%llx, bytes=0x%zx -> rc=0x%x\n",
            ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, offset, buf.GetSize(), rc.GetValue());
        return rc;
    }

    Result ContentStorageService::GetRightsIdFromContentId(sf::Out<ncm::RightsId> out_rights_id, ncm::ContentId content_id, fs::ContentAttributes attr)
    {
        const auto id_str = ncm::GetContentIdString(content_id);
        if ((m_storage_id == ncm::StorageId::BuiltInSystem || m_storage_id == ncm::StorageId::BuiltInUser) && DoesContentExistOnSd(content_id))
        {
            ncm::RightsId rights_id = {};
            out_rights_id.SetValue(rights_id);
            Log("[%s] Storage[%u]::GetRightsId: %s.nca -> ZeroRightsId [SD]\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data);
            R_SUCCEED();
        }

        NcmContentStorage cs = { *m_srv.get() };
        NcmRightsId rights_id = {};
        const Result rc = ncmContentStorageGetRightsIdFromContentId(&cs, &rights_id, reinterpret_cast<const NcmContentId *>(&content_id), static_cast<FsContentAttributes>(attr));
        if (R_SUCCEEDED(rc))
        {
            static_assert(sizeof(*out_rights_id.GetPointer()) <= sizeof(rights_id));
            std::memcpy(out_rights_id.GetPointer(), &rights_id, sizeof(*out_rights_id.GetPointer()));
        }
        Log("[%s] Storage[%u]::GetRightsId: %s.nca -> rc=0x%x\n", ClientTag(m_client_pid), static_cast<u8>(m_storage_id), id_str.data, rc.GetValue());
        return rc;
    }

    // --- ns:su pass-through observability MITM ---

    Result AsyncResultMitmService::Get()
    {
        Log("ns:su::AsyncResult::Get ENTER\n");
        const Result rc = serviceMitmDispatch(m_srv.get(), 0);
        Log("ns:su::AsyncResult::Get -> rc=0x%x\n", rc.GetValue());
        return rc;
    }

    Result AsyncResultMitmService::Cancel()
    {
        const Result rc = serviceMitmDispatch(m_srv.get(), 1);
        Log("ns:su::AsyncResult::Cancel -> rc=0x%x\n", rc.GetValue());
        return rc;
    }

    Result NssuMitmService::RequestSendSystemUpdate(sf::OutCopyHandle out_event, sf::OutMoveHandle out_async, u16 port, u32 addr, const sf::InBuffer &info)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            Log("ns:su::RequestSendSystemUpdate: domain session, forwarding\n");
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        Log("ns:su::RequestSendSystemUpdate ENTER: addr=%u.%u.%u.%u (0x%08x) port=%u sz=%zu\n",
            (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff, addr, port, info.GetSize());

        const struct {
            u16 port;
            u32 addr;
        } in = { port, addr };

        ::Service real_async = {};
        Handle event_handle = INVALID_HANDLE;
        const Result rc = serviceMitmDispatchIn(m_forward_service.get(), 17, in,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
            .buffers = { { info.GetPointer(), info.GetSize() } },
            .out_num_objects = 1,
            .out_objects = &real_async,
            .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
            .out_handles = &event_handle);

        if (info.GetPointer() != nullptr && info.GetSize() >= 0x30)
        {
            const u8 *p = static_cast<const u8 *>(info.GetPointer());
            u32 proto0 = 0, proto1 = 0, ver = 0;
            u64 old_id = 0, tid = 0;
            std::memcpy(&proto0, p + 0x00, sizeof(proto0));
            std::memcpy(&proto1, p + 0x04, sizeof(proto1));
            const u8 exfat   = p[0x08];
            std::memcpy(&ver,    p + 0x0C, sizeof(ver));
            std::memcpy(&old_id, p + 0x10, sizeof(old_id));
            const u8 var     = p[0x18];
            const u8 grp     = p[0x19];
            const u8 reg     = p[0x1A];
            const u8 plat    = p[0x1B];
            const u8 present = p[0x1C];
            std::memcpy(&tid,    p + 0x20, sizeof(tid));
            Log("ns:su::RequestSendSystemUpdate: addr=%u.%u.%u.%u (0x%08x) port=%u sz=%zu rc=0x%x\n",
                (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff, addr, port, info.GetSize(), rc.GetValue());
            Log("ns:su::DeliveryInfo: p0=%u p1=%u exfat=%u ver=0x%x old=0x%016llx var=%u grp=%u reg=%u plat=%u pres=%u tid=0x%016llx\n",
                proto0, proto1, exfat, ver, old_id, var, grp, reg, plat, present, tid);
            char hex[97];
            for (size_t i = 0; i < 48; i++)
            {
                util::SNPrintf(hex + i * 2, 3, "%02x", p[i]);
            }
            Log("ns:su::DeliveryInfo hex[0..47]: %s\n", hex);
        }
        else
        {
            Log("ns:su::RequestSendSystemUpdate: addr=0x%08x port=%u sz=%zu rc=0x%x\n",
                addr, port, info.GetSize(), rc.GetValue());
        }

        if (R_SUCCEEDED(rc))
        {
            out_event.SetValue(event_handle, true);
            out_async.SetValue(WrapAsyncResult(real_async), false);
        }

        return rc;
    }

    Result NssuMitmService::GetSendSystemUpdateProgress(sf::Out<SystemUpdateProgress> out_progress)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            Log("ns:su::GetSendSystemUpdateProgress: domain session, forwarding\n");
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        SystemUpdateProgress progress = {};
        const Result rc = serviceMitmDispatchOut(m_forward_service.get(), 18, progress);
        out_progress.SetValue(progress);

        static os::SdkMutex s_prog_mutex;
        static u32 s_prog_count = 0;
        static u32 s_last_rc = 0xffffffffu;
        static s64 s_last_cur = -1;
        static s64 s_last_tot = -1;
        bool should_log = false;
        u32 seq = 0;
        {
            std::scoped_lock lk(s_prog_mutex);
            seq = s_prog_count++;
            if (seq == 0 || rc.GetValue() != s_last_rc || progress.current_size != s_last_cur || progress.total_size != s_last_tot || (seq % 100) == 0)
            {
                should_log = true;
                s_last_rc = rc.GetValue();
                s_last_cur = progress.current_size;
                s_last_tot = progress.total_size;
            }
        }
        if (should_log)
        {
            Log("ns:su::GetSendSystemUpdateProgress (#%u) -> rc=0x%x, cur=%lld, tot=%lld\n",
                seq, rc.GetValue(), static_cast<long long>(progress.current_size), static_cast<long long>(progress.total_size));
        }
        return rc;
    }

    // --- nim pass-through observability MITM ---

    Result NimAsyncResultMitmService::Get()
    {
        Log("nim::AsyncResult::Get ENTER\n");
        const Result rc = serviceMitmDispatch(m_srv.get(), 0);
        Log("nim::AsyncResult::Get -> rc=0x%x\n", rc.GetValue());
        return rc;
    }

    Result NimAsyncResultMitmService::Cancel()
    {
        Log("nim::AsyncResult::Cancel ENTER\n");
        const Result rc = serviceMitmDispatch(m_srv.get(), 1);
        Log("nim::AsyncResult::Cancel -> rc=0x%x\n", rc.GetValue());
        return rc;
    }

    Result NimShpMitmService::CreateSendSystemUpdateTask(sf::Out<NimTaskId> out_task_id, u16 port, u32 addr, u32 variation_id, ncm::ContentMetaKey key)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            Log("nim::CreateSendSystemUpdateTask: domain session, forwarding\n");
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        u32 effective_var = variation_id;
        if (key.id == 0x0400000000000816 && variation_id != 11)
        {
            effective_var = 11;
            Log("nim::CreateSendSystemUpdateTask ENTER: addr=%u.%u.%u.%u (0x%08x) port=%u var=%u -> 11 key={id=0x%016llx, ver=0x%x, type=%u, inst=%u}\n",
                (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff,
                addr, port, variation_id, key.id, key.version, static_cast<u32>(key.type), static_cast<u32>(key.install_type));
        }
        else
        {
            Log("nim::CreateSendSystemUpdateTask ENTER: addr=%u.%u.%u.%u (0x%08x) port=%u var=%u key={id=0x%016llx, ver=0x%x, type=%u, inst=%u}\n",
                (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff,
                addr, port, variation_id, key.id, key.version, static_cast<u32>(key.type), static_cast<u32>(key.install_type));
        }

        const struct {
            u16 port;
            u16 pad0;
            u32 addr;
            u32 variation_id;
            u32 pad1;
            ncm::ContentMetaKey key;
        } in = { port, 0, addr, effective_var, 0, key };
        static_assert(sizeof(in) == 0x20);

        NimTaskId task_id = {};
        const Result rc = serviceMitmDispatchInOut(m_forward_service.get(), 76, in, task_id);
        out_task_id.SetValue(task_id);
        Log("nim::CreateSendSystemUpdateTask -> rc=0x%x, task_id=0x%016llx%016llx\n",
            rc.GetValue(), static_cast<unsigned long long>(task_id.hi), static_cast<unsigned long long>(task_id.lo));
        return rc;
    }

    Result NimShpMitmService::RequestSendSystemUpdate(sf::OutCopyHandle out_event, sf::OutMoveHandle out_async, NimTaskId task_id)
    {
        if (serviceIsDomain(m_forward_service.get()))
        {
            Log("nim::RequestSendSystemUpdate: domain session, forwarding\n");
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        Log("nim::RequestSendSystemUpdate ENTER: task_id=0x%016llx%016llx\n",
            static_cast<unsigned long long>(task_id.hi), static_cast<unsigned long long>(task_id.lo));

        ::Service real_async = {};
        Handle event_handle = INVALID_HANDLE;
        const Result rc = serviceMitmDispatchIn(m_forward_service.get(), 77, task_id,
            .out_num_objects = 1,
            .out_objects = &real_async,
            .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
            .out_handles = &event_handle);

        Log("nim::RequestSendSystemUpdate -> rc=0x%x, event=0x%x\n", rc.GetValue(), event_handle);
        if (R_SUCCEEDED(rc))
        {
            out_event.SetValue(event_handle, true);
            out_async.SetValue(WrapNimAsyncResult(real_async), false);
        }

        return rc;
    }
}
