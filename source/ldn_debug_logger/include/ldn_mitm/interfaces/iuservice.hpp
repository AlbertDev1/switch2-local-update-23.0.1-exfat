#pragma once
#include <cstring>
#include <atomic>

#include <stratosphere.hpp>

#include "../ldn_userlocalcommunication.hpp"

#define AMS_LDN_CLIENT_PROCESS_MONITOR_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, RegisterClient, (const ams::sf::ClientProcessId &client_process_id), (client_process_id))

AMS_SF_DEFINE_INTERFACE(ams::mitm::ldn, IClientProcessMonitorInterface, AMS_LDN_CLIENT_PROCESS_MONITOR_INTERFACE_INFO, 0x12345678)

#define AMS_LDN_U_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, CreateUserLocalCommunicationService, (ams::sf::Out<ams::sf::SharedPointer<ams::mitm::ldn::IUserLocalCommunicationInterface>> out), (out)) \
    AMS_SF_METHOD_INFO(C, H, 1, Result, CreateClientProcessMonitor, (ams::sf::Out<ams::sf::SharedPointer<ams::mitm::ldn::IClientProcessMonitorInterface>> out), (out))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mitm::ldn, ILdnUMitmInterface, AMS_LDN_U_INTERFACE_INFO, 0x1D8F875E)
