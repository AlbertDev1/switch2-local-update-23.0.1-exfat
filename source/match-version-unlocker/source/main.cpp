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

#include "ncm_mitm.hpp"

namespace ams
{
    namespace mvu
    {
        namespace
        {
            alignas(0x40) constinit u8 g_heap_memory[1_MB];
            constinit lmem::HeapHandle g_heap_handle;
            constinit bool g_heap_initialized;
            constinit os::SdkMutex g_heap_init_mutex;

            lmem::HeapHandle GetHeapHandle()
            {
                if (AMS_UNLIKELY(!g_heap_initialized))
                {
                    std::scoped_lock lk(g_heap_init_mutex);

                    if (AMS_LIKELY(!g_heap_initialized))
                    {
                        g_heap_handle = lmem::CreateExpHeap(g_heap_memory, sizeof(g_heap_memory), lmem::CreateOption_ThreadSafe);
                        g_heap_initialized = true;
                    }
                }

                return g_heap_handle;
            }
        }

        void *Allocate(size_t size)
        {
            return lmem::AllocateFromExpHeap(GetHeapHandle(), size);
        }

        void Deallocate(void *p, size_t size)
        {
            AMS_UNUSED(size);
            if (p)
            {
                lmem::FreeToExpHeap(GetHeapHandle(), p);
            }
        }
    }

    void *Malloc(size_t size)
    {
        return mvu::Allocate(size);
    }

    void Free(void *ptr)
    {
        mvu::Deallocate(ptr, 0);
    }

    void *MallocForRapidJson(size_t size)
    {
        return mvu::Allocate(size);
    }

    void *ReallocForRapidJson(void *ptr, size_t size)
    {
        if (!ptr)
            return mvu::Allocate(size);
        void *new_p = mvu::Allocate(size);
        if (new_p)
        {
            const size_t old_size = lmem::GetExpHeapMemoryBlockSize(ptr);
            std::memcpy(new_p, ptr, std::min(old_size, size));
            mvu::Deallocate(ptr, 0);
        }
        return new_p;
    }

    void FreeForRapidJson(void *ptr)
    {
        mvu::Deallocate(ptr, 0);
    }

    namespace init
    {
        void InitializeSystemModule()
        {
            /* Initialize our connection to sm. */
            R_ABORT_UNLESS(sm::Initialize());
        }

        void FinalizeSystemModule()
        {
            /* ... */
        }

        void Startup()
        {
            /* ... */
        }
    }

    void NORETURN Exit(int rc)
    {
        AMS_UNUSED(rc);
        AMS_ABORT("Exit called by immortal process");
    }

    namespace mvu
    {
        /*
         * ns's RequestSendSystemUpdate / RequestReceiveSystemUpdate abort with 0x67c10
         * (2016-0830) when bit 3 of the u32 stored at offset 0 of the "flagset" file in the
         * ns-confidential system save 0x8000000000002020 is set (one-shot delivery marker).
         * A missing file reads as flags == 0 (allowed). We clear bit 3 at boot, before ns can
         * mount the save itself. Implemented in ncm_mitm.cpp (shares the fs init flag with
         * EnsureSdMounted: calling fs::InitializeForSystem twice aborts). Failures are logged
         * and ignored: the send_sysupdate IPS patch NOPs the check anyway.
         */
    }

    void Main()
    {
        /* Clear the ns delivery lock (flagset bit 3) before ns reaches it. Best-effort:
           the send_sysupdate IPS patch is the primary fix and covers a busy/missing save. */
        mvu::CheckAndFixSystemSaveData();

        /* Install the ncm mitm first thing: ns waits for it at boot.
           On unsupported firmware this releases the declarations instead. */
        static_cast<void>(mvu::LaunchNcmMitm());

        /* Periodically flush the log buffer to SD in this background thread. */
        while (true)
        {
            os::SleepThread(TimeSpan::FromMilliSeconds(500));
            mvu::FlushLogToSd();
        }
    }

    namespace log
    {
        void DebugLog(const char *fmt, ...)
        {
            AMS_UNUSED(fmt);
        }

        void DebugDataDump(const void *data, size_t size, const char *fmt, ...)
        {
            AMS_UNUSED(data);
            AMS_UNUSED(size);
            AMS_UNUSED(fmt);
        }
    }
}

void *operator new(size_t size)
{
    return ams::mvu::Allocate(size);
}

void *operator new(size_t size, const std::nothrow_t &)
{
    return ams::mvu::Allocate(size);
}

void operator delete(void *p)
{
    return ams::mvu::Deallocate(p, 0);
}

void operator delete(void *p, size_t size)
{
    return ams::mvu::Deallocate(p, size);
}

void *operator new[](size_t size)
{
    return ams::mvu::Allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t &)
{
    return ams::mvu::Allocate(size);
}

void operator delete[](void *p)
{
    return ams::mvu::Deallocate(p, 0);
}

void operator delete[](void *p, size_t size)
{
    return ams::mvu::Deallocate(p, size);
}

extern "C"
{
    void *malloc(size_t size)
    {
        return ams::mvu::Allocate(size);
    }

    void free(void *ptr)
    {
        ams::mvu::Deallocate(ptr, 0);
    }

    void *calloc(size_t num, size_t size)
    {
        const size_t total = num * size;
        void *p = ams::mvu::Allocate(total);
        if (p)
        {
            std::memset(p, 0, total);
        }
        return p;
    }

    void *realloc(void *ptr, size_t size)
    {
        if (!ptr)
            return ams::mvu::Allocate(size);
        void *new_p = ams::mvu::Allocate(size);
        if (new_p)
        {
            const size_t old_size = ams::lmem::GetExpHeapMemoryBlockSize(ptr);
            std::memcpy(new_p, ptr, std::min(old_size, size));
            ams::mvu::Deallocate(ptr, 0);
        }
        return new_p;
    }
}
