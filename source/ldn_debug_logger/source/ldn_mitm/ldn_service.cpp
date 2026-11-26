/*
 * Copyright (c) 2022 TSRBerry
 *  -> added ldn forward service
 *  -> changed log messages
 * Copyright (c) 2018 Atmosphère-NX
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

#include <switch.h>

#include "logging.hpp"
#include "ldn_mitm/ldn_service.hpp"
#include "ldn_mitm/ldn_userlocalcommunication.hpp"

namespace ams::mitm::ldn
{
    Result LdnUMitmService::CreateUserLocalCommunicationService(sf::Out<sf::SharedPointer<IUserLocalCommunicationInterface>> out)
    {
        DEBUG_LOG("CreateUserLocalCommunicationService: enabled out_service ptr: %p", &out);
        LdnUserLocalCommunicationInterface doc;
        Result rc = ldnGetUserLocalCommunicationInterfaceFwd(m_forward_service.get(), std::addressof(doc));
        DEBUG_LOG("ldnGetUserLocalCommunicationInterfaceFwd rc: %#x, srv.session: 0x%x, srv.obj_id: %u",
                  rc, doc.s.session, doc.s.object_id);
        if (R_FAILED(rc)) {
            return rc;
        }
        const sf::cmif::DomainObjectId target_object_id{serviceGetObjectId(std::addressof(doc.s))};
        DEBUG_LOG("target_object_id: %u", target_object_id.value);

        out.SetValue(sf::CreateSharedObjectEmplaced<IUserLocalCommunicationInterface, UserLocalCommunicationService>(std::make_unique<LdnUserLocalCommunicationInterface>(doc)), target_object_id);
        R_SUCCEED();
    }

    Result LdnUMitmService::CreateClientProcessMonitor(sf::Out<sf::SharedPointer<IClientProcessMonitorInterface>> out)
    {
        DEBUG_LOG("CreateClientProcessMonitor");
        Service srv{};
        Result rc = serviceMitmDispatch(m_forward_service.get(), 1,
                                        .out_num_objects = 1,
                                        .out_objects = &srv, );
        DEBUG_LOG("CreateClientProcessMonitor rc: %#x, srv.session: 0x%x, srv.obj_id: %u", rc, srv.session, srv.object_id);
        if (R_FAILED(rc)) {
            return rc;
        }
        const sf::cmif::DomainObjectId target_object_id{serviceGetObjectId(&srv)};
        out.SetValue(sf::CreateSharedObjectEmplaced<IClientProcessMonitorInterface, ClientProcessMonitorService>(srv), target_object_id);
        R_SUCCEED();
    }
}
