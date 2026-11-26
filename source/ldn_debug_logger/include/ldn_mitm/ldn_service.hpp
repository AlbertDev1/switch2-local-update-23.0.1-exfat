#pragma once
#include <cstring>
#include <atomic>

#include <stratosphere.hpp>
#include <switch.h>

#include "logging.hpp"
#include "dlog_config.hpp"
#include "interfaces/iuservice.hpp"
#include "ldn_userlocalcommunication.hpp"
#include "ldn_shim.hpp"

namespace ams::mitm::ldn
{
    class ClientProcessMonitorService
    {
    private:
        Service m_srv;

    public:
        ClientProcessMonitorService(Service srv) : m_srv(srv)
        {
            DEBUG_LOG("ClientProcessMonitorService");
        }

        ~ClientProcessMonitorService()
        {
            DEBUG_LOG("~ClientProcessMonitorService");
            serviceClose(&m_srv);
        }

        Result RegisterClient(const sf::ClientProcessId &client_process_id)
        {
            DEBUG_LOG("ClientProcessMonitorService::RegisterClient pid: %" PRIu64, static_cast<u64>(client_process_id.GetValue()));
            u64 reserved = 0;
            Result rc = serviceMitmDispatchIn(&m_srv, 0, reserved,
                                              .in_send_pid = true,
                                              .override_pid = static_cast<u64>(client_process_id.GetValue()), );
            DEBUG_LOG("ClientProcessMonitorService::RegisterClient rc: %#x", rc);
            return rc;
        }
    };
    static_assert(ams::mitm::ldn::IsIClientProcessMonitorInterface<ClientProcessMonitorService>);

    class LdnUMitmService : public sf::MitmServiceImplBase
    {
    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        static bool ShouldMitm(const sm::MitmProcessInfo &client_info)
        {
            DEBUG_LOG("should_mitm pid: %" PRIu64 " tid: 0%" PRIx64, client_info.process_id, client_info.program_id);
            return ams::mitm::GetConfigInstance()->ldn_u.enable_mitm;
        }

    public:
        /* Overridden commands. */
        Result CreateUserLocalCommunicationService(sf::Out<sf::SharedPointer<IUserLocalCommunicationInterface>> out);
        Result CreateClientProcessMonitor(sf::Out<sf::SharedPointer<IClientProcessMonitorInterface>> out);
    };
    static_assert(ams::mitm::ldn::IsILdnUMitmInterface<LdnUMitmService>);
}
