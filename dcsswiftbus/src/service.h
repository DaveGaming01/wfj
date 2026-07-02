/*
 * dcsswiftbus - FGSwiftBus-compatible service object backed by DCS data
 *
 * Derived from FlightGear's FGSwiftBus service module:
 * SPDX-FileCopyrightText: (C) 2019-2022 swift Project Community / Contributors (https://swift-project.org/)
 * SPDX-FileCopyrightText: (C) 2019-2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dbus/dbusobject.h"
#include "state.h"

#include <string>
#include <vector>

namespace dcsswiftbus {

//! Service object exposed on DBus, mimicking FGSwiftBus' org.swift_project.fgswiftbus.service
class CService : public CDBusObject
{
public:
    explicit CService(CState &state);

    //! DBus interface name
    static const std::string &InterfaceName();

    //! DBus object path
    static const std::string &ObjectPath();

    //! FGSwiftBus API version reported to swift (3 = current, 1 and 2 are rejected by swift)
    static int getVersionNumber();

    int process();

protected:
    DBusHandlerResult dbusMessageHandler(const CDBusMessage &message) override;

private:
    CState &m_state;
};

} // namespace dcsswiftbus
