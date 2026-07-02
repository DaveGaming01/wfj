/*
 * dcsswiftbus - FGSwiftBus-compatible traffic object (stub: accepts traffic, renders nothing)
 *
 * Derived from FlightGear's FGSwiftBus traffic module:
 * SPDX-FileCopyrightText: (C) 2019-2022 swift Project Community / Contributors (https://swift-project.org/)
 * SPDX-FileCopyrightText: (C) 2019-2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dbus/dbusobject.h"

#include <map>
#include <string>

namespace dcsswiftbus {

//! Remote aircraft last known position (kept only to answer getRemoteAircraftData)
struct RemotePlane {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeFt = 0.0;
};

//! Traffic object exposed on DBus, mimicking FGSwiftBus' org.swift_project.fgswiftbus.traffic.
//! DCS cannot spawn arbitrary aircraft from outside, so remote traffic is tracked but not rendered.
class CTraffic : public CDBusObject
{
public:
    CTraffic() = default;

    //! DBus interface name
    static const std::string &InterfaceName();

    //! DBus object path
    static const std::string &ObjectPath();

    void emitSimFrame();
    int process();

protected:
    DBusHandlerResult dbusMessageHandler(const CDBusMessage &message) override;
    void dbusDisconnectedHandler() override;

private:
    void emitPlaneAdded(const std::string &callsign);

    std::map<std::string, RemotePlane> m_planes;
    bool m_emitSimFrame = true;
};

} // namespace dcsswiftbus
