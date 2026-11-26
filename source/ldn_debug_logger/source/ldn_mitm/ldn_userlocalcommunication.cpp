#include "ldn_mitm/ldn_userlocalcommunication.hpp"

namespace ams::mitm::ldn
{
    Result UserLocalCommunicationService::GetState(sf::Out<u32> state)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetState ptr: %p", state.GetPointer());
        if (m_config->ldn_u.commands.disable_getstate)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetState(m_srv.get(), state.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetState rc: %#x", rc);
        DEBUG_LOG("UserLocalCommunicationService::GetState value: %d", state.GetValue());
        return rc;
    }

    Result UserLocalCommunicationService::GetNetworkInfo(sf::Out<NetworkInfo> buffer)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetNetworkInfo ptr: %p", buffer.GetPointer());
        if (m_config->ldn_u.commands.disable_getnetworkinfo)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetNetworkInfo(m_srv.get(), buffer.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetNetworkInfo rc: %#x", rc);
        if (R_SUCCEEDED(rc) && buffer.GetPointer() != nullptr)
        {
            buffer.GetPointer()->networkInfo.nodes[0].platform = 1;
            DEBUG_LOG("Spoofed nodes[0].platform = 1 (Ounce/Switch 2)");

            if (buffer.GetPointer()->networkInfo.advertise_data_size >= 16)
            {
                auto &adv = buffer.GetPointer()->networkInfo.advertise_data;
                adv[1] = 0x07;
                adv[7] = 0x07;
                adv[8]  = 0x5C;
                adv[9]  = 0x01;
                adv[10] = 0x75;
                adv[11] = 0x3C;
                DEBUG_LOG("[LDN_MITM] Spoofed NetworkInfo.advertise_data: [1]=0x07, [7]=0x07, [8..11]=0x5C01753C");
            }
        }
        DEBUG_DATA_DUMP(buffer.GetPointer(), sizeof(NetworkInfo), "UserLocalCommunicationService::GetNetworkInfo NetworkInfo");
        return rc;
    }

    Result UserLocalCommunicationService::GetIpv4Address(sf::Out<LdnIpv4Address> address, sf::Out<LdnSubnetMask> netmask)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetIpv4Address addr ptr: %p netmask ptr: %p", address.GetPointer(), netmask.GetPointer());
        if (m_config->ldn_u.commands.disable_getipv4address)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetIpv4Address(m_srv.get(), address.GetPointer(), netmask.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetIpv4Address rc: %#x", rc);
        DEBUG_DATA_DUMP(address.GetPointer(), sizeof(LdnIpv4Address), "UserLocalCommunicationService::GetIpv4Address addr");
        DEBUG_DATA_DUMP(netmask.GetPointer(), sizeof(LdnSubnetMask), "UserLocalCommunicationService::GetIpv4Address netmask");
        return rc;
    }

    Result UserLocalCommunicationService::GetDisconnectReason(sf::Out<s16> reason)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetDisconnectReason ptr: %p", reason.GetPointer());
        if (m_config->ldn_u.commands.disable_getdisconnectreason)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetDisconnectReason(m_srv.get(), reason.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetDisconnectReason rc: %#x", rc);
        DEBUG_LOG("UserLocalCommunicationService::GetDisconnectReason value: %x", reason.GetValue());
        return rc;
    }

    Result UserLocalCommunicationService::GetSecurityParameter(sf::Out<LdnSecurityParameter> out)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetSecurityParameter ptr: %p", out.GetPointer());
        if (m_config->ldn_u.commands.disable_getsecurityparameter)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetSecurityParameter(m_srv.get(), out.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetSecurityParameter rc: %#x", rc);
        DEBUG_DATA_DUMP(out.GetPointer(), sizeof(LdnSecurityParameter), "UserLocalCommunicationService::GetSecurityParameter SecurityParameter");
        return rc;
    }

    Result UserLocalCommunicationService::GetNetworkConfig(sf::Out<LdnNetworkConfig> out)
    {
        DEBUG_LOG("UserLocalCommunicationService::GetNetworkConfig ptr: %p", out.GetPointer());
        if (m_config->ldn_u.commands.disable_getnetworkconfig)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationGetNetworkConfig(m_srv.get(), out.GetPointer());
        DEBUG_LOG("UserLocalCommunicationService::GetNetworkConfig rc: %#x", rc);
        DEBUG_DATA_DUMP(out.GetPointer(), sizeof(LdnNetworkConfig), "UserLocalCommunicationService::GetNetworkConfig NetworkConfig");
        return rc;
    }

    Result UserLocalCommunicationService::AttachStateChangeEvent(sf::Out<sf::CopyHandle> out_handle)
    {
        os::NativeHandle handle;
        DEBUG_LOG("UserLocalCommunicationService::AttachStateChangeEvent");
        if (m_config->ldn_u.commands.disable_attachstatechangeevent)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationAttachStateChangeEvent(m_srv.get(), &handle);
        DEBUG_LOG("UserLocalCommunicationService::AttachStateChangeEvent rc: %x", rc);
        out_handle.SetValue(handle, false);
        return rc;
    }

    Result UserLocalCommunicationService::Scan(sf::OutAutoSelectArray<NetworkInfo> buffer, sf::Out<s16> outCount, LdnScanFilter filter, s16 channel)
    {
        DEBUG_LOG("UserLocalCommunicationService::Scan outCount ptr: %p buffer ptr: %p size: %x", outCount.GetPointer(), buffer.GetPointer(), buffer.GetSize());
        DEBUG_LOG("UserLocalCommunicationService::Scan channel value: %x filter ptr: %p", channel, &filter);
        DEBUG_DATA_DUMP(&filter, sizeof(LdnScanFilter), "UserLocalCommunicationService::Scan filter");
        if (m_config->ldn_u.commands.disable_scan)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationScan(m_srv.get(), outCount.GetPointer(), buffer.GetPointer(), buffer.GetSize(), channel, filter);
        DEBUG_LOG("UserLocalCommunicationService::Scan rc: %#x", rc);
        if (R_SUCCEEDED(rc) && buffer.GetPointer() != nullptr)
        {
            for (s16 i = 0; i < outCount.GetValue(); i++)
            {
                buffer[i].networkInfo.nodes[0].platform = 1;
            }
        }
        DEBUG_DATA_DUMP(buffer.GetPointer(), buffer.GetSize(), "UserLocalCommunicationService::Scan NetworkInfo");
        return rc;
    }

    Result UserLocalCommunicationService::ScanPrivate(sf::OutAutoSelectArray<NetworkInfo> buffer, sf::Out<s16> outCount, LdnScanFilter filter, s16 channel)
    {
        DEBUG_LOG("UserLocalCommunicationService::ScanPrivate outCount ptr: %p buffer ptr: %p size: %x", outCount.GetPointer(), buffer.GetPointer(), buffer.GetSize());
        DEBUG_LOG("UserLocalCommunicationService::ScanPrivate channel value: %x filter ptr: %x", channel, &filter);
        DEBUG_DATA_DUMP(&filter, sizeof(LdnScanFilter), "UserLocalCommunicationService::ScanPrivate filter");
        if (m_config->ldn_u.commands.disable_scanprivate)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationScanPrivate(m_srv.get(), outCount.GetPointer(), buffer.GetPointer(), buffer.GetSize(), channel, filter);
        DEBUG_LOG("UserLocalCommunicationService::ScanPrivate rc: %#x", rc);
        if (R_SUCCEEDED(rc) && buffer.GetPointer() != nullptr)
        {
            for (s16 i = 0; i < outCount.GetValue(); i++)
            {
                buffer[i].networkInfo.nodes[0].platform = 1;
            }
        }
        DEBUG_DATA_DUMP(buffer.GetPointer(), buffer.GetSize(), "UserLocalCommunicationService::ScanPrivate NetworkInfo");
        return rc;
    }

    // HOS: 5.0.0+
    Result UserLocalCommunicationService::SetWirelessControllerRestriction(WirelessControllerRestriction in)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetWirelessControllerRestriction value: %d", in);
        if (m_config->ldn_u.commands.disable_setwirelesscontrollerrestriction)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationSetWirelessControllerRestriction(m_srv.get(), in);
        DEBUG_LOG("UserLocalCommunicationService::SetWirelessControllerRestriction rc: %#x", rc);
        return rc;
    }

    // HOS: 13.1.0+
    Result UserLocalCommunicationService::SetBluetoothAudioDeviceConnectableMode(u32 in)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetBluetoothAudioDeviceConnectableMode value: %d", in);
        if (m_config->ldn_u.commands.disable_setbluetoothaudiodeviceconnectablemode)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationSetBluetoothAudioDeviceConnectableMode(m_srv.get(), in);
        DEBUG_LOG("UserLocalCommunicationService::SetBluetoothAudioDeviceConnectableMode rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::OpenAccessPoint()
    {
        DEBUG_LOG("UserLocalCommunicationService::OpenAccessPoint");
        if (m_config->ldn_u.commands.disable_openaccesspoint)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationOpenAccessPoint(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::OpenAccessPoint rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::CloseAccessPoint()
    {
        DEBUG_LOG("UserLocalCommunicationService::CloseAccessPoint");
        if (m_config->ldn_u.commands.disable_closeaccesspoint)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationCloseAccessPoint(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::CloseAccessPoint rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::CreateNetwork(CreateNetworkConfig data)
    {
        DEBUG_LOG("UserLocalCommunicationService::CreateNetwork ptr: %p size: %d", &data, sizeof(CreateNetworkConfig));
        DEBUG_DATA_DUMP(&data, sizeof(CreateNetworkConfig), "UserLocalCommunicationService::CreateNetwork");
        if (m_config->ldn_u.commands.disable_createnetwork)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationCreateNetwork(m_srv.get(), data);
        DEBUG_LOG("UserLocalCommunicationService::CreateNetwork rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::CreateNetworkPrivate(CreateNetworkPrivateConfig data, const sf::InPointerArray<LdnAddressEntry> &entries)
    {
        DEBUG_LOG("UserLocalCommunicationService::CreateNetworkPrivate config ptr: %p size: %d", &data, sizeof(CreateNetworkPrivateConfig));
        DEBUG_DATA_DUMP(&data, sizeof(CreateNetworkConfig), "UserLocalCommunicationService::CreateNetworkPrivate config");
        DEBUG_LOG("UserLocalCommunicationService::CreateNetworkPrivate entries ptr: %p size: %d", entries.GetPointer(), entries.GetSize());
        DEBUG_DATA_DUMP(entries.GetPointer(), entries.GetSize(), "UserLocalCommunicationService::CreateNetworkPrivate entries");
        if (m_config->ldn_u.commands.disable_createnetworkprivate)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationCreateNetworkPrivate(m_srv.get(), data, entries.GetPointer(), entries.GetSize());
        DEBUG_LOG("UserLocalCommunicationService::CreateNetworkPrivate rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::DestroyNetwork()
    {
        DEBUG_LOG("UserLocalCommunicationService::DestroyNetwork");
        if (m_config->ldn_u.commands.disable_destroynetwork)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationDestroyNetwork(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::DestroyNetwork rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::Reject(LdnIpv4Address addr)
    {
        DEBUG_LOG("UserLocalCommunicationService::Reject addr: %x", addr);
        if (m_config->ldn_u.commands.disable_reject)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationReject(m_srv.get(), &addr);
        DEBUG_LOG("UserLocalCommunicationService::Reject rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::SetAdvertiseData(sf::InAutoSelectBuffer data)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetAdvertiseData ptr: %p size: %d", data.GetPointer(), data.GetSize());
        DEBUG_DATA_DUMP(data.GetPointer(), data.GetSize(), "UserLocalCommunicationService::SetAdvertiseData data");
        if (m_config->ldn_u.commands.disable_setadvertisedata)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }

        u8 mod_data[0x180];
        const void *buf_ptr = data.GetPointer();
        size_t buf_size = data.GetSize();
        if (buf_ptr != nullptr && buf_size >= 16)
        {
            size_t copy_size = std::min(buf_size, sizeof(mod_data));
            std::memcpy(mod_data, buf_ptr, copy_size);

            // Spoof version seulement : 0x5C01753C (big-endian: 5C 01 75 3C)
            // dans les bytes [8..11] du Tag 2.
            mod_data[8]  = 0x5C;
            mod_data[9]  = 0x01;
            mod_data[10] = 0x75;
            mod_data[11] = 0x3C;

            // Byte[1] = etat de la session LDN :
            //   0x01 = Idle/Init
            //   0x02 = Lobby ouvert (Group Open / en attente)
            //   0x07 = Mise a jour disponible (Ready to update)
            // On ne force 0x07 QUE si qlaunch a deja signale un etat > 2,
            // c'est-a-dire que la console est prete a transferer la mise a jour.
            // En mode lobby (etat <= 2), on preserve la valeur originale pour
            // que la Switch 2 puisse detecter la Switch 1 dans le scan.
            if (mod_data[1] > 2)
            {
                mod_data[1] = 0x07;
                mod_data[7] = 0x07;
                DEBUG_LOG("[LDN_MITM] Spoofed AdvertiseData: byte[1]=0x07 (etat>2), byte[7]=0x07, version=0x5C01753C");
            }
            else
            {
                // Preserve byte[1] en mode lobby, mais spoofe quand meme byte[7] et la version
                // Note: byte[7] est un mirror de byte[1] dans le Tag 2 selon les logs
                DEBUG_LOG("[LDN_MITM] AdvertiseData lobby (byte[1]=0x%02x preserve), version=0x5C01753C injectee", mod_data[1]);
            }

            buf_ptr = mod_data;
            DEBUG_DATA_DUMP(mod_data, copy_size, "Spoofed AdvertiseData");
        }

        Result rc = ldnUserCommunicationSetAdvertiseData(m_srv.get(), buf_ptr, buf_size);
        DEBUG_LOG("UserLocalCommunicationService::SetAdvertiseData rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::SetStationAcceptPolicy(LdnAcceptPolicy policy)
    {
        DEBUG_DATA_DUMP(&policy, sizeof(LdnAcceptPolicy), "UserLocalCommunicationService::SetStationAcceptPolicy policy");
        if (m_config->ldn_u.commands.disable_setstationacceptpolicy)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationSetStationAcceptPolicy(m_srv.get(), &policy);
        DEBUG_LOG("UserLocalCommunicationService::SetStationAcceptPolicy rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::AddAcceptFilterEntry(LdnMacAddress mac)
    {
        DEBUG_DATA_DUMP(&mac, sizeof(LdnMacAddress), "UserLocalCommunicationService::AddAcceptFilterEntry mac");
        if (m_config->ldn_u.commands.disable_addacceptfilterentry)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationAddAcceptFilterEntry(m_srv.get(), &mac);
        DEBUG_LOG("UserLocalCommunicationService::AddAcceptFilterEntry rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::ClearAcceptFilter()
    {
        DEBUG_LOG("UserLocalCommunicationService::ClearAcceptFilter");
        if (m_config->ldn_u.commands.disable_clearacceptfilter)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationClearAcceptFilter(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::ClearAcceptFilter rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::OpenStation()
    {
        DEBUG_LOG("UserLocalCommunicationService::OpenStation");
        if (m_config->ldn_u.commands.disable_openstation)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationOpenStation(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::OpenStation rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::CloseStation()
    {
        DEBUG_LOG("UserLocalCommunicationService::CloseStation");
        if (m_config->ldn_u.commands.disable_closestation)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationCloseStation(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::CloseStation rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::Connect(ConnectNetworkData param, const LdnNetworkInfo &data)
    {
        DEBUG_LOG("UserLocalCommunicationService::Connect param ptr: %p networkInfo ptr: %p", &param, data);
        DEBUG_DATA_DUMP(&param, sizeof(ConnectNetworkData), "UserLocalCommunicationService::Connect param");
        DEBUG_DATA_DUMP(&data, sizeof(LdnNetworkInfo), "UserLocalCommunicationService::Connect networkInfo");
        if (m_config->ldn_u.commands.disable_connect)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationConnect(m_srv.get(), &param, &data);
        DEBUG_LOG("UserLocalCommunicationService::Connect rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::ConnectPrivate(ConnectNetworkPrivateData param)
    {
        DEBUG_LOG("UserLocalCommunicationService::ConnectPrivate param ptr: %p", &param);
        DEBUG_DATA_DUMP(&param, sizeof(ConnectNetworkData), "UserLocalCommunicationService::ConnectPrivate param");
        if (m_config->ldn_u.commands.disable_connectprivate)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationConnectPrivate(m_srv.get(), &param);
        DEBUG_LOG("UserLocalCommunicationService::ConnectPrivate rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::Disconnect()
    {
        DEBUG_LOG("UserLocalCommunicationService::Disconnect");
        if (m_config->ldn_u.commands.disable_disconnect)
        {
            return sm::mitm::ResultShouldForwardToSession();
        }
        Result rc = ldnUserCommunicationDisconnect(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::Disconnect rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::SetProtocol(u32 protocol)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetProtocol protocol: %u", protocol);
        Result rc = ldnUserCommunicationSetProtocol(m_srv.get(), protocol);
        DEBUG_LOG("UserLocalCommunicationService::SetProtocol rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::EnableActionFrame(const LdnActionFrameSettings &settings)
    {
        DEBUG_LOG("UserLocalCommunicationService::EnableActionFrame local_comm_id: 0x%016llx", settings.local_communication_id);
        DEBUG_DATA_DUMP(&settings, sizeof(settings), "UserLocalCommunicationService::EnableActionFrame settings");
        Result rc = ldnUserCommunicationEnableActionFrame(m_srv.get(), &settings);
        DEBUG_LOG("UserLocalCommunicationService::EnableActionFrame rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::DisableActionFrame()
    {
        DEBUG_LOG("UserLocalCommunicationService::DisableActionFrame");
        Result rc = ldnUserCommunicationDisableActionFrame(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::DisableActionFrame rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::SendActionFrame(const SendActionFrameConfig &config, sf::InAutoSelectBuffer data)
    {
        DEBUG_LOG("UserLocalCommunicationService::SendActionFrame ch: %d band: %d size: %zu flags: 0x%x",
                  config.channel, config.band, data.GetSize(), config.flags);
        DEBUG_DATA_DUMP(data.GetPointer(), data.GetSize(), "UserLocalCommunicationService::SendActionFrame payload");
        Result rc = ldnUserCommunicationSendActionFrame(m_srv.get(), &config, data.GetPointer(), data.GetSize());
        DEBUG_LOG("UserLocalCommunicationService::SendActionFrame rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::RecvActionFrame(sf::Out<RecvActionFrameOutput> out, sf::OutAutoSelectBuffer data, u32 flags)
    {
        RecvActionFrameOutput out_struct{};
        Result rc = ldnUserCommunicationRecvActionFrame(m_srv.get(), &out_struct, data.GetPointer(), data.GetSize(), flags);
        if (R_SUCCEEDED(rc))
        {
            out.SetValue(out_struct);
            DEBUG_LOG("UserLocalCommunicationService::RecvActionFrame ch: %d size: %u link_level: %d",
                      out_struct.channel, out_struct.size, out_struct.link_level);
            DEBUG_DATA_DUMP(data.GetPointer(), out_struct.size, "UserLocalCommunicationService::RecvActionFrame payload");
        }
        return rc;
    }

    Result UserLocalCommunicationService::SetHomeChannel(u32 channel)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetHomeChannel ch: %u", channel);
        Result rc = ldnUserCommunicationSetHomeChannel(m_srv.get(), channel);
        DEBUG_LOG("UserLocalCommunicationService::SetHomeChannel rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::SetTxPower(u32 power)
    {
        DEBUG_LOG("UserLocalCommunicationService::SetTxPower power: %u", power);
        Result rc = ldnUserCommunicationSetTxPower(m_srv.get(), power);
        DEBUG_LOG("UserLocalCommunicationService::SetTxPower rc: %#x", rc);
        return rc;
    }

    Result UserLocalCommunicationService::ResetTxPower()
    {
        DEBUG_LOG("UserLocalCommunicationService::ResetTxPower");
        Result rc = ldnUserCommunicationResetTxPower(m_srv.get());
        DEBUG_LOG("UserLocalCommunicationService::ResetTxPower rc: %#x", rc);
        return rc;
    }
}
