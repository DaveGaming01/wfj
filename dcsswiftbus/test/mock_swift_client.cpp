/*
 * Mock swift pilot client: connects to dcsswiftbus as a DBus peer and performs
 * the same call sequence swift's FlightGear plugin performs on connect
 * (see pilotclient src/plugins/simulator/flightgear/simulatorflightgear.cpp).
 * Exit code 0 = the daemon looks like a healthy FGSwiftBus to swift.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <dbus/dbus.h>

#include <cmath>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char *SERVICE_IFACE = "org.swift_project.fgswiftbus.service";
constexpr const char *SERVICE_PATH = "/fgswiftbus/service";
constexpr const char *TRAFFIC_IFACE = "org.swift_project.fgswiftbus.traffic";
constexpr const char *TRAFFIC_PATH = "/fgswiftbus/traffic";

int g_failures = 0;

void check(bool ok, const std::string &what)
{
    std::cout << (ok ? "PASS" : "FAIL") << ": " << what << std::endl;
    if (!ok) { ++g_failures; }
}

DBusMessage *call(DBusConnection *conn, const char *path, const char *iface, const char *method,
                  int firstArgType = DBUS_TYPE_INVALID, ...)
{
    DBusMessage *msg = dbus_message_new_method_call(nullptr, path, iface, method);
    if (firstArgType != DBUS_TYPE_INVALID) {
        va_list args;
        va_start(args, firstArgType);
        dbus_message_append_args_valist(msg, firstArgType, args);
        va_end(args);
    }
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 2000, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) {
        std::cout << "  DBus error calling " << method << ": " << err.message << std::endl;
        dbus_error_free(&err);
        return nullptr;
    }
    return reply;
}

bool getInt(DBusConnection *conn, const char *method, int &out)
{
    DBusMessage *reply = call(conn, SERVICE_PATH, SERVICE_IFACE, method);
    if (!reply) { return false; }
    DBusError err;
    dbus_error_init(&err);
    const bool ok = dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &out, DBUS_TYPE_INVALID);
    dbus_message_unref(reply);
    return ok;
}

bool getString(DBusConnection *conn, const char *method, std::string &out)
{
    DBusMessage *reply = call(conn, SERVICE_PATH, SERVICE_IFACE, method);
    if (!reply) { return false; }
    DBusError err;
    dbus_error_init(&err);
    const char *str = nullptr;
    const bool ok = dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);
    if (ok) { out = str; }
    dbus_message_unref(reply);
    return ok;
}

} // namespace

int main(int argc, char **argv)
{
    std::string address = "tcp:host=127.0.0.1,port=45003";
    if (argc > 1) { address = argv[1]; }

    DBusError err;
    dbus_error_init(&err);

    // swift (QDBus) does the equivalent of connectToPeer()
    DBusConnection *conn = dbus_connection_open_private(address.c_str(), &err);
    if (!conn || dbus_error_is_set(&err)) {
        std::cerr << "FAIL: could not connect to " << address << ": " << (err.message ? err.message : "?") << std::endl;
        return 1;
    }
    check(true, "peer connection + auth handshake on " + address);

    // --- swift listener sequence: version check, traffic init, acquire planes ---
    int version = -1;
    const bool versionOk = getInt(conn, "getVersionNumber", version);
    check(versionOk && version == 3, "getVersionNumber == 3 (got " + std::to_string(version) + ")");

    DBusMessage *reply = call(conn, TRAFFIC_PATH, TRAFFIC_IFACE, "initialize");
    dbus_bool_t initialized = FALSE;
    if (reply) {
        dbus_message_get_args(reply, &err, DBUS_TYPE_BOOLEAN, &initialized, DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
    }
    check(initialized == TRUE, "traffic.initialize == true");

    reply = call(conn, TRAFFIC_PATH, TRAFFIC_IFACE, "acquireMultiplayerPlanes");
    dbus_bool_t acquired = FALSE;
    const char *owner = nullptr;
    if (reply) {
        dbus_message_get_args(reply, &err, DBUS_TYPE_BOOLEAN, &acquired, DBUS_TYPE_STRING, &owner, DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
    }
    check(acquired == TRUE, "traffic.acquireMultiplayerPlanes == (true, \"\")");

    // --- swift connectTo() sequence: own model info ---
    std::string modelString, name, description;
    const bool modelOk = getString(conn, "getAircraftModelString", modelString);
    check(modelOk && modelString.rfind("DCS ", 0) == 0, "getAircraftModelString == \"" + modelString + "\"");
    const bool nameOk = getString(conn, "getAircraftName", name);
    check(nameOk && !name.empty(), "getAircraftName == \"" + name + "\"");
    getString(conn, "getAircraftDescription", description);

    // --- fast timer equivalent: own aircraft situation ---
    reply = call(conn, SERVICE_PATH, SERVICE_IFACE, "getOwnAircraftSituationData");
    double lat = 0, lon = 0, altFt = 0, gsKts = 0, pitch = 0, roll = 0, hdg = 0, pressAlt = 0;
    bool situationOk = false;
    if (reply) {
        situationOk = dbus_message_get_args(reply, &err,
                                            DBUS_TYPE_DOUBLE, &lat, DBUS_TYPE_DOUBLE, &lon,
                                            DBUS_TYPE_DOUBLE, &altFt, DBUS_TYPE_DOUBLE, &gsKts,
                                            DBUS_TYPE_DOUBLE, &pitch, DBUS_TYPE_DOUBLE, &roll,
                                            DBUS_TYPE_DOUBLE, &hdg, DBUS_TYPE_DOUBLE, &pressAlt,
                                            DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
    }
    check(situationOk, "getOwnAircraftSituationData signature (8 doubles)");
    std::cout << "  situation: lat=" << lat << " lon=" << lon << " alt=" << altFt << "ft gs=" << gsKts
              << "kts pitch=" << pitch << " roll=" << roll << " hdg=" << hdg << " pressAlt=" << pressAlt << "ft"
              << std::endl;
    // mock_dcs.py orbits Batumi at 2000 m with QNH 745 mmHg (993.2 hPa)
    check(std::fabs(lat - 41.61) < 0.5 && std::fabs(lon - 41.60) < 0.5, "position matches DCS feed (Batumi)");
    check(std::fabs(altFt - 2000.0 * 3.2808398950131) < 50.0, "altitude matches DCS feed (2000 m -> ft)");
    // pressure altitude = 2000 m + (1013.25 - 745*1.3332239) * 8.23 m = ~2165 m = ~7103 ft
    const double expectedPressAltFt = (2000.0 + (1013.25 - 745.0 * 1.3332239) * 8.23) * 3.2808398950131;
    check(std::fabs(pressAlt - expectedPressAltFt) < 20.0,
          "pressure altitude corrected for QNH (expected ~" + std::to_string(static_cast<int>(expectedPressAltFt)) +
              " ft, got " + std::to_string(static_cast<int>(pressAlt)) + " ft)");

    reply = call(conn, SERVICE_PATH, SERVICE_IFACE, "getOwnAircraftVelocityData");
    double ve = 0, vu = 0, vn = 0, pr = 0, rr = 0, yr = 0;
    bool velocityOk = false;
    if (reply) {
        velocityOk = dbus_message_get_args(reply, &err,
                                           DBUS_TYPE_DOUBLE, &ve, DBUS_TYPE_DOUBLE, &vu, DBUS_TYPE_DOUBLE, &vn,
                                           DBUS_TYPE_DOUBLE, &pr, DBUS_TYPE_DOUBLE, &rr, DBUS_TYPE_DOUBLE, &yr,
                                           DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
    }
    check(velocityOk, "getOwnAircraftVelocityData signature (6 doubles)");

    reply = call(conn, SERVICE_PATH, SERVICE_IFACE, "isPaused");
    dbus_bool_t paused = TRUE;
    if (reply) {
        dbus_message_get_args(reply, &err, DBUS_TYPE_BOOLEAN, &paused, DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
    }
    check(paused == FALSE, "isPaused == false while DCS feed is live");

    // --- radios: with the SRS feed live, the DCS cockpit owns COM1/COM2 and squawk ---
    int com1 = 0, com2 = 0;
    const bool com1Ok = getInt(conn, "getCom1ActiveKhz", com1);
    check(com1Ok && com1 == 305000, "getCom1ActiveKhz == 305000 from SRS cockpit feed (got " + std::to_string(com1) + ")");
    const bool com2Ok = getInt(conn, "getCom2ActiveKhz", com2);
    check(com2Ok && com2 == 127500, "getCom2ActiveKhz == 127500 from SRS cockpit feed (got " + std::to_string(com2) + ")");

    // swift trying to set COM1 must not shadow the cockpit while SRS data is live
    dbus_int32_t freq = 122800;
    reply = call(conn, SERVICE_PATH, SERVICE_IFACE, "setCom1ActiveKhz", DBUS_TYPE_INT32, &freq, DBUS_TYPE_INVALID);
    if (reply) { dbus_message_unref(reply); }
    const bool com1AgainOk = getInt(conn, "getCom1ActiveKhz", com1);
    check(com1AgainOk && com1 == 305000, "cockpit radio overrides swift's setCom1ActiveKhz while SRS feed is live");

    int code = 0, mode = 0;
    const bool codeOk = getInt(conn, "getTransponderCode", code);
    check(codeOk && code == 4520, "getTransponderCode == 4520 from SRS IFF (got " + std::to_string(code) + ")");
    const bool modeOk = getInt(conn, "getTransponderMode", mode);
    check(modeOk && mode == 4, "getTransponderMode == 4 (mode C) from SRS IFF status");

    // standby frequency is still swift-owned (SRS does not report it)
    dbus_int32_t standby = 118250;
    reply = call(conn, SERVICE_PATH, SERVICE_IFACE, "setCom1StandbyKhz", DBUS_TYPE_INT32, &standby, DBUS_TYPE_INVALID);
    if (reply) { dbus_message_unref(reply); }
    int com1Standby = 0;
    check(getInt(conn, "getCom1StandbyKhz", com1Standby) && com1Standby == 118250,
          "setCom1StandbyKhz/getCom1StandbyKhz round-trip (swift-owned)");

    // --- traffic: add a remote plane like swift does when a VATSIM aircraft comes in range ---
    const char *cs = "DAL123";
    const char *model = "B738";
    const char *icao = "B738";
    const char *airline = "DAL";
    const char *livery = "";
    reply = call(conn, TRAFFIC_PATH, TRAFFIC_IFACE, "addPlane",
                 DBUS_TYPE_STRING, &cs, DBUS_TYPE_STRING, &model, DBUS_TYPE_STRING, &icao,
                 DBUS_TYPE_STRING, &airline, DBUS_TYPE_STRING, &livery, DBUS_TYPE_INVALID);
    if (reply) { dbus_message_unref(reply); }
    check(true, "traffic.addPlane accepted");

    // wait for the remoteAircraftAdded signal + a simFrame signal
    bool sawPlaneAdded = false, sawSimFrame = false;
    for (int i = 0; i < 100 && !(sawPlaneAdded && sawSimFrame); ++i) {
        dbus_connection_read_write(conn, 50);
        while (DBusMessage *msg = dbus_connection_pop_message(conn)) {
            if (dbus_message_is_signal(msg, TRAFFIC_IFACE, "remoteAircraftAdded")) { sawPlaneAdded = true; }
            if (dbus_message_is_signal(msg, TRAFFIC_IFACE, "simFrame")) { sawSimFrame = true; }
            dbus_message_unref(msg);
        }
    }
    check(sawPlaneAdded, "remoteAircraftAdded signal received");
    check(sawSimFrame, "simFrame signal received");

    dbus_connection_close(conn);
    dbus_connection_unref(conn);

    std::cout << (g_failures == 0 ? "\nALL CHECKS PASSED - swift should accept this daemon as FGSwiftBus"
                                  : "\nFAILURES: " + std::to_string(g_failures))
              << std::endl;
    return g_failures == 0 ? 0 : 1;
}
