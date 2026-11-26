#pragma once
#include <switch.h>
#include "ldn_types.hpp"

typedef struct
{
    Service s;
} LdnUserLocalCommunicationInterface;

/* Command forwarders. */
Result ldnGetUserLocalCommunicationInterfaceFwd(Service *s, LdnUserLocalCommunicationInterface *out);

Result ldnUserCommunicationGetState(LdnUserLocalCommunicationInterface *doc, u32 *state);
Result ldnUserCommunicationGetNetworkInfo(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::NetworkInfo *out);
Result ldnUserCommunicationGetIpv4Address(LdnUserLocalCommunicationInterface *doc, LdnIpv4Address *addr, LdnSubnetMask *netmask);
Result ldnUserCommunicationGetDisconnectReason(LdnUserLocalCommunicationInterface *doc, s16 *reason);
Result ldnUserCommunicationGetSecurityParameter(LdnUserLocalCommunicationInterface *doc, LdnSecurityParameter *out);
Result ldnUserCommunicationGetNetworkConfig(LdnUserLocalCommunicationInterface *doc, LdnNetworkConfig *out);
Result ldnUserCommunicationAttachStateChangeEvent(LdnUserLocalCommunicationInterface *doc, Handle *handle);
Result ldnUserCommunicationGetNetworkInfoLatestUpdate(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::NetworkInfo *out, LdnNodeLatestUpdate *out_buffer, size_t out_buffer_size);
Result ldnUserCommunicationScan(LdnUserLocalCommunicationInterface *doc, s16 *total_out, ams::mitm::ldn::NetworkInfo *out_buffer, size_t out_buffer_size, s16 channel, LdnScanFilter filter);
Result ldnUserCommunicationScanPrivate(LdnUserLocalCommunicationInterface *doc, s16 *total_out, ams::mitm::ldn::NetworkInfo *out_buffer, size_t out_buffer_size, s16 channel, LdnScanFilter filter);
Result ldnUserCommunicationSetWirelessControllerRestriction(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::WirelessControllerRestriction in);
Result ldnUserCommunicationSetBluetoothAudioDeviceConnectableMode(LdnUserLocalCommunicationInterface *doc, u32 in);
Result ldnUserCommunicationSetProtocol(LdnUserLocalCommunicationInterface *doc, u32 protocol);
Result ldnUserCommunicationOpenAccessPoint(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationCloseAccessPoint(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationCreateNetwork(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::CreateNetworkConfig data);
Result ldnUserCommunicationCreateNetworkPrivate(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::CreateNetworkPrivateConfig data, const LdnAddressEntry *entries, size_t entires_size);
Result ldnUserCommunicationDestroyNetwork(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationReject(LdnUserLocalCommunicationInterface *doc, LdnIpv4Address *addr);
Result ldnUserCommunicationSetAdvertiseData(LdnUserLocalCommunicationInterface *doc, const void *advertiseData, size_t advertiseData_size);
Result ldnUserCommunicationSetStationAcceptPolicy(LdnUserLocalCommunicationInterface *doc, LdnAcceptPolicy *policy);
Result ldnUserCommunicationAddAcceptFilterEntry(LdnUserLocalCommunicationInterface *doc, LdnMacAddress *mac);
Result ldnUserCommunicationClearAcceptFilter(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationOpenStation(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationCloseStation(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationConnect(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::ConnectNetworkData *dat, const LdnNetworkInfo *data);
Result ldnUserCommunicationConnectPrivate(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::ConnectNetworkPrivateData *dat);
Result ldnUserCommunicationDisconnect(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationInitialize(LdnUserLocalCommunicationInterface *doc, u64 reserved);
Result ldnUserCommunicationFinalize(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationInitialize2(LdnUserLocalCommunicationInterface *doc, s32 version, u64 reserved);
Result ldnUserCommunicationSetOperationMode(LdnUserLocalCommunicationInterface *doc, u32 mode);
Result ldnUserCommunicationEnableActionFrame(LdnUserLocalCommunicationInterface *doc, const LdnActionFrameSettings *settings);
Result ldnUserCommunicationDisableActionFrame(LdnUserLocalCommunicationInterface *doc);
Result ldnUserCommunicationSendActionFrame(LdnUserLocalCommunicationInterface *doc, const ams::mitm::ldn::SendActionFrameConfig *config, const void *data, size_t size);
Result ldnUserCommunicationRecvActionFrame(LdnUserLocalCommunicationInterface *doc, ams::mitm::ldn::RecvActionFrameOutput *out, void *data, size_t size, u32 flags);
Result ldnUserCommunicationSetHomeChannel(LdnUserLocalCommunicationInterface *doc, u32 channel);
Result ldnUserCommunicationSetTxPower(LdnUserLocalCommunicationInterface *doc, u32 power);
Result ldnUserCommunicationResetTxPower(LdnUserLocalCommunicationInterface *doc);

void ldnUserLocalCommunicationInterfaceClose(LdnUserLocalCommunicationInterface *doc);
