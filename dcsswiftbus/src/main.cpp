/*
 * dcsswiftbus - standalone FGSwiftBus-compatible daemon bridging DCS World to the swift pilot client
 *
 * swift connects to this daemon believing it is FlightGear's FGSwiftBus plugin
 * (P2P DBus server, default tcp:host=127.0.0.1,port=45003). Own-aircraft data
 * is fed from DCS via Export.lua over UDP (default port 47788).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dbus/dbusconnection.h"
#include "dbus/dbusdispatcher.h"
#include "dbus/dbusserver.h"
#include "service.h"
#include "state.h"
#include "traffic.h"
#include "udplistener.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_quit { false };
void signalHandler(int) { g_quit = true; }
} // namespace

int main(int argc, char **argv)
{
    using namespace dcsswiftbus;

    std::string dbusHost = "127.0.0.1";
    std::string dbusPort = "45003";
    std::uint16_t udpPort = 47788;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto nextArg = [&]() -> const char * { return (i + 1 < argc) ? argv[++i] : nullptr; };
        if (arg == "--dbus-host") {
            if (const char *v = nextArg()) { dbusHost = v; }
        } else if (arg == "--dbus-port") {
            if (const char *v = nextArg()) { dbusPort = v; }
        } else if (arg == "--udp-port") {
            if (const char *v = nextArg()) { udpPort = static_cast<std::uint16_t>(std::atoi(v)); }
        } else {
            std::cout << "usage: dcsswiftbus [--dbus-host 127.0.0.1] [--dbus-port 45003] [--udp-port 47788]\n"
                      << "\n"
                      << "In swift, set the FlightGear plugin's DBus server address to\n"
                      << "tcp:host=<dbus-host>,port=<dbus-port> and enable the FlightGear simulator plugin.\n";
            return (arg == "--help" || arg == "-h") ? 0 : 1;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    CState state;

    CUdpListener udpListener(state, udpPort);
    if (!udpListener.start()) { return 1; }
    std::cout << "dcsswiftbus: listening for DCS Export.lua data on udp://127.0.0.1:" << udpPort << std::endl;

    CDBusDispatcher dispatcher;
    CService service(state);
    CTraffic traffic;
    std::shared_ptr<CDBusConnection> connection;

    CDBusServer server;
    const std::string listenAddress = "tcp:host=" + dbusHost + ",port=" + dbusPort;
    if (!server.listen(listenAddress)) {
        std::cerr << "dcsswiftbus: failed to listen on " << listenAddress << std::endl;
        return 1;
    }
    server.setDispatcher(&dispatcher);
    server.setNewConnectionFunc([&](const std::shared_ptr<CDBusConnection> &conn) {
        std::cout << "dcsswiftbus: swift connected" << std::endl;
        connection = conn;
        connection->setDispatcher(&dispatcher);
        service.setDBusConnection(connection);
        service.registerDBusObjectPath(CService::InterfaceName(), CService::ObjectPath());
        traffic.setDBusConnection(connection);
        traffic.registerDBusObjectPath(CTraffic::InterfaceName(), CTraffic::ObjectPath());
    });
    std::cout << "dcsswiftbus: FGSwiftBus-compatible DBus server on " << listenAddress << std::endl;

    // Main loop, mirrors FGSwiftBus' fastLoop(): dispatch DBus, run queued calls, emit simFrame
    auto lastSimFrame = std::chrono::steady_clock::now();
    auto lastStatus = std::chrono::steady_clock::now();
    while (!g_quit) {
        dispatcher.runOnce();
        service.process();
        traffic.process();

        const auto now = std::chrono::steady_clock::now();
        if (now - lastSimFrame >= std::chrono::milliseconds(33)) {
            traffic.emitSimFrame();
            lastSimFrame = now;
        }
        if (now - lastStatus >= std::chrono::seconds(10)) {
            const OwnAircraft a = state.aircraft();
            std::cout << "dcsswiftbus: " << (state.isStale() ? "NO DCS DATA" : "DCS feed OK")
                      << " | packets=" << udpListener.packetCount()
                      << " | " << a.aircraftName
                      << " lat=" << a.latitudeDeg << " lon=" << a.longitudeDeg
                      << " altMSL=" << a.altitudeMslM << "m" << std::endl;
            lastStatus = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "dcsswiftbus: shutting down" << std::endl;
    udpListener.stop();
    return 0;
}
