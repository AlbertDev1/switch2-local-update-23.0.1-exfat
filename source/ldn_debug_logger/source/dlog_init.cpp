#include <stratosphere.hpp>

#include "dlog_init.hpp"
#include "logging.hpp"
#include "ldn_mitm/ldn_module.hpp"

namespace ams::mitm
{
    void LaunchModules()
    {
        DEBUG_LOG("Launching ldn:u mitm...");
        Result rc = ams::mitm::ldn::Launch();
        DEBUG_LOG("ldn:u Launch rc: 0x%08X", rc);
    }

    void WaitModules()
    {
        ams::mitm::ldn::WaitFinished();
    }
}
