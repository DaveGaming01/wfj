/*
 * dcsswiftbus - FGSwiftBus-compatible traffic object (stub: accepts traffic, renders nothing)
 *
 * Derived from FlightGear's FGSwiftBus traffic module:
 * SPDX-FileCopyrightText: (C) 2019-2022 swift Project Community / Contributors (https://swift-project.org/)
 * SPDX-FileCopyrightText: (C) 2019-2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "traffic.h"

#include <iostream>
#include <limits>

namespace dcsswiftbus {

static const std::string k_fgswiftbus_traffic_interfacename = "org.swift_project.fgswiftbus.traffic";
static const std::string k_fgswiftbus_traffic_objectpath = "/fgswiftbus/traffic";

const std::string &CTraffic::InterfaceName()
{
    return k_fgswiftbus_traffic_interfacename;
}

const std::string &CTraffic::ObjectPath()
{
    return k_fgswiftbus_traffic_objectpath;
}

void CTraffic::emitSimFrame()
{
    if (m_emitSimFrame) { sendDBusSignal("simFrame"); }
    m_emitSimFrame = !m_emitSimFrame;
}

void CTraffic::emitPlaneAdded(const std::string &callsign)
{
    CDBusMessage signalPlaneAdded = CDBusMessage::createSignal(k_fgswiftbus_traffic_objectpath, k_fgswiftbus_traffic_interfacename, "remoteAircraftAdded");
    signalPlaneAdded.beginArgumentWrite();
    signalPlaneAdded.appendArgument(callsign);
    sendDBusMessage(signalPlaneAdded);
}

void CTraffic::dbusDisconnectedHandler()
{
    m_planes.clear();
}

static const char *introspection_traffic = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE;

DBusHandlerResult CTraffic::dbusMessageHandler(const CDBusMessage &message_)
{
    CDBusMessage message(message_);
    const std::string sender = message.getSender();
    const dbus_uint32_t serial = message.getSerial();
    const bool wantsReply = message.wantsReply();

    if (message.getInterfaceName() == DBUS_INTERFACE_INTROSPECTABLE) {
        if (message.getMethodName() == "Introspect") {
            sendDBusReply(sender, serial, introspection_traffic);
        }
    } else if (message.getInterfaceName() == k_fgswiftbus_traffic_interfacename) {
        const std::string method = message.getMethodName();

        if (method == "acquireMultiplayerPlanes") {
            queueDBusCall([=]() {
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(true);          // acquired
                reply.appendArgument(std::string()); // owner
                sendDBusMessage(reply);
            });
        } else if (method == "initialize") {
            sendDBusReply(sender, serial, true);
        } else if (method == "cleanup") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            queueDBusCall([=]() { m_planes.clear(); });
        } else if (method == "addPlane") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::string callsign;
            std::string modelName;
            std::string aircraftIcao;
            std::string airlineIcao;
            std::string livery;
            message.beginArgumentRead();
            message.getArgument(callsign);
            message.getArgument(modelName);
            message.getArgument(aircraftIcao);
            message.getArgument(airlineIcao);
            message.getArgument(livery);
            queueDBusCall([=]() {
                m_planes[callsign] = RemotePlane {};
                std::cout << "[traffic] added " << callsign << " (" << aircraftIcao << ", not rendered)" << std::endl;
                emitPlaneAdded(callsign);
            });
        } else if (method == "removePlane") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::string callsign;
            message.beginArgumentRead();
            message.getArgument(callsign);
            queueDBusCall([=]() { m_planes.erase(callsign); });
        } else if (method == "removeAllPlanes") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            queueDBusCall([=]() { m_planes.clear(); });
        } else if (method == "setPlanesPositions") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::vector<std::string> callsigns;
            std::vector<double> latitudes;
            std::vector<double> longitudes;
            std::vector<double> altitudes;
            std::vector<double> pitches;
            std::vector<double> rolls;
            std::vector<double> headings;
            std::vector<double> groundspeeds;
            std::vector<bool> onGrounds;
            message.beginArgumentRead();
            message.getArgument(callsigns);
            message.getArgument(latitudes);
            message.getArgument(longitudes);
            message.getArgument(altitudes);
            message.getArgument(pitches);
            message.getArgument(rolls);
            message.getArgument(headings);
            message.getArgument(groundspeeds);
            message.getArgument(onGrounds);
            queueDBusCall([=]() {
                for (std::size_t i = 0; i < callsigns.size() && i < latitudes.size(); ++i) {
                    auto it = m_planes.find(callsigns.at(i));
                    if (it == m_planes.end()) { continue; }
                    it->second.latitudeDeg = latitudes.at(i);
                    it->second.longitudeDeg = longitudes.at(i);
                    it->second.altitudeFt = altitudes.at(i);
                }
            });
        } else if (method == "getRemoteAircraftData") {
            std::vector<std::string> requestedcallsigns;
            message.beginArgumentRead();
            message.getArgument(requestedcallsigns);
            queueDBusCall([=]() {
                std::vector<std::string> callsigns;
                std::vector<double> latitudesDeg;
                std::vector<double> longitudesDeg;
                std::vector<double> elevationsM;
                std::vector<double> verticalOffsets;
                for (const auto &cs : requestedcallsigns) {
                    auto it = m_planes.find(cs);
                    if (it == m_planes.end()) { continue; }
                    callsigns.push_back(cs);
                    latitudesDeg.push_back(it->second.latitudeDeg);
                    longitudesDeg.push_back(it->second.longitudeDeg);
                    elevationsM.push_back(0.0);
                    verticalOffsets.push_back(0.0);
                }
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(callsigns);
                reply.appendArgument(latitudesDeg);
                reply.appendArgument(longitudesDeg);
                reply.appendArgument(elevationsM);
                reply.appendArgument(verticalOffsets);
                sendDBusMessage(reply);
            });
        } else if (method == "getElevationAtPosition") {
            std::string callsign;
            double latitudeDeg = 0.0;
            double longitudeDeg = 0.0;
            double altitudeMeters = 0.0;
            message.beginArgumentRead();
            message.getArgument(callsign);
            message.getArgument(latitudeDeg);
            message.getArgument(longitudeDeg);
            message.getArgument(altitudeMeters);
            queueDBusCall([=]() {
                // No terrain probe available outside DCS; NaN = unknown, like FG with unloaded scenery
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(callsign);
                reply.appendArgument(std::numeric_limits<double>::quiet_NaN());
                sendDBusMessage(reply);
            });
        } else if (method == "setPlanesTransponders") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
        } else if (method == "setPlanesSurfaces") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
        } else {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int CTraffic::process()
{
    invokeQueuedDBusCalls();
    return 1;
}

} // namespace dcsswiftbus
