#pragma once
#include <stratosphere.hpp>
#include <switch.h>

#include "../ldn_types.hpp"

#define AMS_LDN_USERLOCALCOMMUNICATION_INTERFACE_INFO(C, H)                                                                                                                                                                \
       AMS_SF_METHOD_INFO(C, H, 0, Result, GetState, (ams::sf::Out<u32> state), (state))                                                                                                                                   \
       AMS_SF_METHOD_INFO(C, H, 1, Result, GetNetworkInfo, (ams::sf::Out<ams::mitm::ldn::NetworkInfo> buffer), (buffer))                                                                                                   \
       AMS_SF_METHOD_INFO(C, H, 2, Result, GetIpv4Address, (ams::sf::Out<LdnIpv4Address> address, ams::sf::Out<LdnSubnetMask> mask), (address, mask))                                                                      \
       AMS_SF_METHOD_INFO(C, H, 3, Result, GetDisconnectReason, (ams::sf::Out<s16> reason), (reason))                                                                                                                      \
       AMS_SF_METHOD_INFO(C, H, 4, Result, GetSecurityParameter, (ams::sf::Out<LdnSecurityParameter> out), (out))                                                                                                          \
       AMS_SF_METHOD_INFO(C, H, 5, Result, GetNetworkConfig, (ams::sf::Out<LdnNetworkConfig> out), (out))                                                                                                                  \
       AMS_SF_METHOD_INFO(C, H, 100, Result, AttachStateChangeEvent, (ams::sf::Out<ams::sf::CopyHandle> handle), (handle))                                                                                                 \
       AMS_SF_METHOD_INFO(C, H, 102, Result, Scan, (ams::sf::OutAutoSelectArray<ams::mitm::ldn::NetworkInfo> buffer, ams::sf::Out<s16> count, LdnScanFilter filter, u16 channel), (buffer, count, filter, channel))        \
       AMS_SF_METHOD_INFO(C, H, 103, Result, ScanPrivate, (ams::sf::OutAutoSelectArray<ams::mitm::ldn::NetworkInfo> buffer, ams::sf::Out<s16> count, LdnScanFilter filter, u16 channel), (buffer, count, filter, channel)) \
       AMS_SF_METHOD_INFO(C, H, 104, Result, SetWirelessControllerRestriction, (ams::mitm::ldn::WirelessControllerRestriction in), (in))                                                                                   \
       AMS_SF_METHOD_INFO(C, H, 106, Result, SetProtocol, (u32 protocol), (protocol))                                                                                                                                       \
       AMS_SF_METHOD_INFO(C, H, 200, Result, OpenAccessPoint, (), ())                                                                                                                                                      \
       AMS_SF_METHOD_INFO(C, H, 201, Result, CloseAccessPoint, (), ())                                                                                                                                                     \
       AMS_SF_METHOD_INFO(C, H, 202, Result, CreateNetwork, (ams::mitm::ldn::CreateNetworkConfig data), (data))                                                                                                            \
       AMS_SF_METHOD_INFO(C, H, 203, Result, CreateNetworkPrivate, (ams::mitm::ldn::CreateNetworkPrivateConfig data, ams::sf::InPointerArray<LdnAddressEntry> entries), (data, entries))                                   \
       AMS_SF_METHOD_INFO(C, H, 204, Result, DestroyNetwork, (), ())                                                                                                                                                       \
       AMS_SF_METHOD_INFO(C, H, 205, Result, Reject, (LdnIpv4Address addr), (addr))                                                                                                                                        \
       AMS_SF_METHOD_INFO(C, H, 206, Result, SetAdvertiseData, (ams::sf::InAutoSelectBuffer data), (data))                                                                                                                 \
       AMS_SF_METHOD_INFO(C, H, 207, Result, SetStationAcceptPolicy, (LdnAcceptPolicy policy), (policy))                                                                                                                   \
       AMS_SF_METHOD_INFO(C, H, 208, Result, AddAcceptFilterEntry, (LdnMacAddress mac), (mac))                                                                                                                             \
       AMS_SF_METHOD_INFO(C, H, 209, Result, ClearAcceptFilter, (), ())                                                                                                                                                    \
       AMS_SF_METHOD_INFO(C, H, 300, Result, OpenStation, (), ())                                                                                                                                                          \
       AMS_SF_METHOD_INFO(C, H, 301, Result, CloseStation, (), ())                                                                                                                                                         \
       AMS_SF_METHOD_INFO(C, H, 302, Result, Connect, (ams::mitm::ldn::ConnectNetworkData dat, const LdnNetworkInfo &data), (dat, data))                                                                                   \
       AMS_SF_METHOD_INFO(C, H, 303, Result, ConnectPrivate, (ams::mitm::ldn::ConnectNetworkPrivateData dat), (dat))                                                                                                       \
       AMS_SF_METHOD_INFO(C, H, 304, Result, Disconnect, (), ())                                                                                                                                                           \
       AMS_SF_METHOD_INFO(C, H, 500, Result, EnableActionFrame, (const LdnActionFrameSettings &settings), (settings))                                                                                                       \
       AMS_SF_METHOD_INFO(C, H, 501, Result, DisableActionFrame, (), ())                                                                                                                                                     \
       AMS_SF_METHOD_INFO(C, H, 502, Result, SendActionFrame, (const ams::mitm::ldn::SendActionFrameConfig &config, ams::sf::InAutoSelectBuffer data), (config, data))                                                     \
       AMS_SF_METHOD_INFO(C, H, 503, Result, RecvActionFrame, (ams::sf::Out<ams::mitm::ldn::RecvActionFrameOutput> out, ams::sf::OutAutoSelectBuffer data, u32 flags), (out, data, flags))                               \
       AMS_SF_METHOD_INFO(C, H, 505, Result, SetHomeChannel, (u32 channel), (channel))                                                                                                                                     \
       AMS_SF_METHOD_INFO(C, H, 600, Result, SetTxPower, (u32 power), (power))                                                                                                                                             \
       AMS_SF_METHOD_INFO(C, H, 601, Result, ResetTxPower, (), ())

AMS_SF_DEFINE_MITM_INTERFACE(ams::mitm::ldn, IUserLocalCommunicationInterface, AMS_LDN_USERLOCALCOMMUNICATION_INTERFACE_INFO, 0x85280DC3)
