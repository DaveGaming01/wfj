/*
 * dcsswiftbus - FGSwiftBus-compatible service object backed by DCS data
 *
 * Derived from FlightGear's FGSwiftBus service module:
 * SPDX-FileCopyrightText: (C) 2019-2022 swift Project Community / Contributors (https://swift-project.org/)
 * SPDX-FileCopyrightText: (C) 2019-2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "service.h"

#include <iostream>

#define FGSWIFTBUS_API_VERSION 3

namespace {
constexpr double M_TO_FT = 3.2808398950131;
constexpr double MS_TO_KTS = 1.9438444924406;
constexpr double MMHG_TO_HPA = 1.3332239;
constexpr double M_PER_HPA = 8.23; // near sea level, standard atmosphere

//! Pressure altitude in meters: true altitude corrected from mission QNH to 1013.25 hPa
double pressureAltitudeM(const dcsswiftbus::OwnAircraft &a)
{
    const double qnhHpa = a.qnhMmHg * MMHG_TO_HPA;
    return a.altitudeMslM + (1013.25 - qnhHpa) * M_PER_HPA;
}
} // namespace

namespace dcsswiftbus {

static const std::string k_fgswiftbus_service_interfacename = "org.swift_project.fgswiftbus.service";
static const std::string k_fgswiftbus_service_objectpath = "/fgswiftbus/service";

CService::CService(CState &state) : m_state(state) {}

const std::string &CService::InterfaceName()
{
    return k_fgswiftbus_service_interfacename;
}

const std::string &CService::ObjectPath()
{
    return k_fgswiftbus_service_objectpath;
}

int CService::getVersionNumber()
{
    return FGSWIFTBUS_API_VERSION;
}

static const char *introspection_service = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE;

DBusHandlerResult CService::dbusMessageHandler(const CDBusMessage &message_)
{
    CDBusMessage message(message_);
    const std::string sender = message.getSender();
    const dbus_uint32_t serial = message.getSerial();
    const bool wantsReply = message.wantsReply();

    if (message.getInterfaceName() == DBUS_INTERFACE_INTROSPECTABLE) {
        if (message.getMethodName() == "Introspect") {
            sendDBusReply(sender, serial, introspection_service);
        }
    } else if (message.getInterfaceName() == k_fgswiftbus_service_interfacename) {
        const std::string method = message.getMethodName();

        if (method == "addTextMessage") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            std::string text;
            message.beginArgumentRead();
            message.getArgument(text);
            queueDBusCall([=]() { std::cout << "[swift] " << text << std::endl; });
        } else if (method == "getOwnAircraftSituationData") {
            queueDBusCall([=]() {
                const OwnAircraft a = m_state.aircraft();
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(a.latitudeDeg);
                reply.appendArgument(a.longitudeDeg);
                reply.appendArgument(a.altitudeMslM * M_TO_FT);
                reply.appendArgument(a.groundSpeedMs * MS_TO_KTS);
                reply.appendArgument(a.pitchDeg);
                reply.appendArgument(a.rollDeg);
                reply.appendArgument(a.trueHeadingDeg);
                reply.appendArgument(pressureAltitudeM(a) * M_TO_FT);
                sendDBusMessage(reply);
            });
        } else if (method == "getOwnAircraftVelocityData") {
            queueDBusCall([=]() {
                const OwnAircraft a = m_state.aircraft();
                CDBusMessage reply = CDBusMessage::createReply(sender, serial);
                reply.beginArgumentWrite();
                reply.appendArgument(a.velocityEastMs);
                reply.appendArgument(a.velocityUpMs);
                reply.appendArgument(a.velocityNorthMs);
                reply.appendArgument(a.pitchRateRadS);
                reply.appendArgument(a.rollRateRadS);
                reply.appendArgument(a.yawRateRadS);
                sendDBusMessage(reply);
            });
        } else if (method == "getVersionNumber") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, getVersionNumber()); });
        } else if (method == "getAircraftModelPath") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, std::string()); });
        } else if (method == "getAircraftModelFilename") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().aircraftName); });
        } else if (method == "getAircraftModelString") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, "DCS " + m_state.aircraft().aircraftName); });
        } else if (method == "getAircraftName") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().aircraftName); });
        } else if (method == "getAircraftLivery") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, std::string()); });
        } else if (method == "getAircraftIcaoCode") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, std::string()); });
        } else if (method == "getAircraftDescription") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, "DCS World: " + m_state.aircraft().aircraftName); });
        } else if (method == "isPaused") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.isStale()); });
        } else if (method == "getLatitudeDeg") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().latitudeDeg); });
        } else if (method == "getLongitudeDeg") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().longitudeDeg); });
        } else if (method == "getAltitudeMslFt") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().altitudeMslM * M_TO_FT); });
        } else if (method == "getHeightAglFt") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().heightAglM * M_TO_FT); });
        } else if (method == "getGroundSpeedKts") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().groundSpeedMs * MS_TO_KTS); });
        } else if (method == "getPitchDeg") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().pitchDeg); });
        } else if (method == "getRollDeg") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().rollDeg); });
        } else if (method == "getAllWheelsOnGround") {
            queueDBusCall([=]() {
                const OwnAircraft a = m_state.aircraft();
                sendDBusReply(sender, serial, a.heightAglM < 3.0 && a.gearDeployRatio > 0.9);
            });
        } else if (method == "getCom1ActiveKhz") {
            queueDBusCall([=]() {
                // While the SRS export feed is live the DCS cockpit owns the radios
                const SrsRadios srs = m_state.srsRadios();
                const bool useSrs = m_state.srsFresh() && srs.com1ActiveKhz > 0;
                sendDBusReply(sender, serial, useSrs ? srs.com1ActiveKhz : m_state.avionics().com1ActiveKhz);
            });
        } else if (method == "getCom1StandbyKhz") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.avionics().com1StandbyKhz); });
        } else if (method == "getCom2ActiveKhz") {
            queueDBusCall([=]() {
                const SrsRadios srs = m_state.srsRadios();
                const bool useSrs = m_state.srsFresh() && srs.com2ActiveKhz > 0;
                sendDBusReply(sender, serial, useSrs ? srs.com2ActiveKhz : m_state.avionics().com2ActiveKhz);
            });
        } else if (method == "getCom2StandbyKhz") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.avionics().com2StandbyKhz); });
        } else if (method == "getTransponderCode") {
            queueDBusCall([=]() {
                const SrsRadios srs = m_state.srsRadios();
                const bool useSrs = m_state.srsFresh() && srs.transponderCode >= 0;
                sendDBusReply(sender, serial, useSrs ? srs.transponderCode : m_state.avionics().transponderCode);
            });
        } else if (method == "getTransponderMode") {
            queueDBusCall([=]() {
                const SrsRadios srs = m_state.srsRadios();
                // cockpit owns the transponder only while it reports a plausible squawk
                if (m_state.srsFresh() && srs.transponderCode >= 0 && srs.transponderStatus >= 0) {
                    // SRS iff.status: 0/-1 off, >=1 normal/ident -> FG convention: 0-2 standby, >2 mode C
                    sendDBusReply(sender, serial, srs.transponderStatus >= 1 ? 4 : 1);
                } else {
                    sendDBusReply(sender, serial, m_state.avionics().transponderMode);
                }
            });
        } else if (method == "getTransponderIdent") {
            queueDBusCall([=]() {
                const SrsRadios srs = m_state.srsRadios();
                if (m_state.srsFresh() && srs.transponderCode >= 0 && srs.transponderStatus >= 0) {
                    sendDBusReply(sender, serial, srs.transponderStatus == 2);
                } else {
                    sendDBusReply(sender, serial, m_state.avionics().transponderIdent);
                }
            });
        } else if (method == "getBeaconLightsOn") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, true); });
        } else if (method == "getLandingLightsOn") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().gearDeployRatio > 0.9); });
        } else if (method == "getNavLightsOn") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, true); });
        } else if (method == "getStrobeLightsOn") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, true); });
        } else if (method == "getTaxiLightsOn") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, false); });
        } else if (method == "getPressAlt") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, pressureAltitudeM(m_state.aircraft()) * M_TO_FT); });
        } else if (method == "getGroundElevation") {
            queueDBusCall([=]() {
                const OwnAircraft a = m_state.aircraft();
                sendDBusReply(sender, serial, a.altitudeMslM - a.heightAglM); // meters, like FG's /position/ground-elev-m
            });
        } else if (method == "setCom1ActiveKhz") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int frequency = 0;
            message.beginArgumentRead();
            message.getArgument(frequency);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.com1ActiveKhz = frequency; }); });
        } else if (method == "setCom1StandbyKhz") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int frequency = 0;
            message.beginArgumentRead();
            message.getArgument(frequency);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.com1StandbyKhz = frequency; }); });
        } else if (method == "setCom2ActiveKhz") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int frequency = 0;
            message.beginArgumentRead();
            message.getArgument(frequency);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.com2ActiveKhz = frequency; }); });
        } else if (method == "setCom2StandbyKhz") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int frequency = 0;
            message.beginArgumentRead();
            message.getArgument(frequency);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.com2StandbyKhz = frequency; }); });
        } else if (method == "setTransponderCode") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int code = 0;
            message.beginArgumentRead();
            message.getArgument(code);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.transponderCode = code; }); });
        } else if (method == "setTransponderMode") {
            maybeSendEmptyDBusReply(wantsReply, sender, serial);
            int mode = 0;
            message.beginArgumentRead();
            message.getArgument(mode);
            queueDBusCall([=]() { m_state.modifyAvionics([&](Avionics &av) { av.transponderMode = mode; }); });
        } else if (method == "getFlapsDeployRatio") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().flapsDeployRatio); });
        } else if (method == "getGearDeployRatio") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().gearDeployRatio); });
        } else if (method == "getEngineN1Percentage") {
            queueDBusCall([=]() {
                const std::vector<double> n1 { 85.0, 85.0 };
                sendDBusReply(sender, serial, n1);
            });
        } else if (method == "getSpeedBrakeRatio") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.aircraft().speedBrakeRatio); });
        } else if (method == "getCom1Volume") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.avionics().com1Volume); });
        } else if (method == "getCom2Volume") {
            queueDBusCall([=]() { sendDBusReply(sender, serial, m_state.avionics().com2Volume); });
        } else {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int CService::process()
{
    invokeQueuedDBusCalls();
    return 1;
}

} // namespace dcsswiftbus
