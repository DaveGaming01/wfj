/*
 * dcsswiftbus - standalone FGSwiftBus-compatible daemon bridging DCS World to the swift pilot client
 *
 * swift connects to this daemon believing it is FlightGear's FGSwiftBus plugin
 * (P2P DBus server, default tcp:host=127.0.0.1,port=45003). Own-aircraft data
 * is fed from DCS via Export.lua over UDP; cockpit radios come from the
 * DCS-SRS export broadcasts when enabled.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "dbus/dbusconnection.h"
#include "dbus/dbusdispatcher.h"
#include "dbus/dbusserver.h"
#include "service.h"
#include "srslistener.h"
#include "state.h"
#include "traffic.h"
#include "udplistener.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_quit { false };
void signalHandler(int) { g_quit = true; }
constexpr double M_TO_FT = 3.2808398950131;

//! Keep the console window readable when launched by double-click on Windows
int fail()
{
#ifdef _WIN32
    std::cout << "\nPress Enter to close..." << std::endl;
    std::cin.get();
#endif
    return 1;
}
} // namespace

int main(int argc, char **argv)
{
    using namespace dcsswiftbus;

    Config config;
    config.loadFile("dcsswiftbus.cfg"); // optional, next to the exe / in the working dir
    if (!config.applyArgs(argc, argv)) { return fail(); }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const std::string listenAddress = "tcp:host=" + config.dbusHost + ",port=" + config.dbusPort;

    std::cout << "======================================================================\n"
              << " dcsswiftbus - fly DCS World on VATSIM through the swift pilot client\n"
              << "======================================================================\n"
              << " swift connection:   " << listenAddress << "\n"
              << "                     (swift: enable the FlightGear plugin, DBus P2P,\n"
              << "                      host " << config.dbusHost << ", port " << config.dbusPort << ")\n"
              << " DCS position feed:  udp " << config.udpPort << " (Export.lua)\n";
    if (config.cockpitRadios) {
        std::cout << " Radios & squawk:    DCS cockpit via DCS-SRS broadcasts (udp " << config.srsPort << ")\n"
                  << "                     swift GUI takes over when no cockpit data\n";
    } else {
        std::cout << " Radios & squawk:    swift GUI (cockpit sync disabled)\n";
    }
    std::cout << " Settings file:      " << (config.loadedFrom.empty() ? "none (using defaults)" : config.loadedFrom)
              << "\n"
              << "======================================================================" << std::endl;

    CState state;

    CUdpListener udpListener(state, static_cast<std::uint16_t>(config.udpPort));
    if (!udpListener.start()) { return fail(); }

    std::unique_ptr<CSrsListener> srsListener;
    if (config.cockpitRadios && config.srsPort > 0) {
        srsListener = std::make_unique<CSrsListener>(state, static_cast<std::uint16_t>(config.srsPort));
        if (!srsListener->start()) {
            std::cout << "! Could not listen on the SRS port - is the SRS client running?\n"
                      << "! Cockpit radio sync is off for this session; use the swift GUI to tune." << std::endl;
            srsListener.reset();
        }
    }

    CDBusDispatcher dispatcher;
    CService service(state);
    CTraffic traffic;
    std::shared_ptr<CDBusConnection> connection;

    CDBusServer server;
    if (!server.listen(listenAddress)) {
        std::cerr << "dcsswiftbus: failed to listen on " << listenAddress
                  << " (port in use? another dcsswiftbus or FlightGear running?)" << std::endl;
        return fail();
    }
    server.setDispatcher(&dispatcher);
    server.setNewConnectionFunc([&](const std::shared_ptr<CDBusConnection> &conn) {
        std::cout << "* swift connected" << std::endl;
        connection = conn;
        connection->setDispatcher(&dispatcher);
        service.setDBusConnection(connection);
        service.registerDBusObjectPath(CService::InterfaceName(), CService::ObjectPath());
        traffic.setDBusConnection(connection);
        traffic.registerDBusObjectPath(CTraffic::InterfaceName(), CTraffic::ObjectPath());
    });
    std::cout << "Waiting for DCS (fly a mission) and swift (connect the FlightGear plugin)..." << std::endl;

    // Main loop, mirrors FGSwiftBus' fastLoop(): dispatch DBus, run queued calls, emit simFrame
    auto lastSimFrame = std::chrono::steady_clock::now();
    auto lastStatus = std::chrono::steady_clock::now();
    bool hadDcs = false;
    bool hadSrs = false;
    while (!g_quit) {
        dispatcher.runOnce();
        service.process();
        traffic.process();

        const auto now = std::chrono::steady_clock::now();
        if (now - lastSimFrame >= std::chrono::milliseconds(33)) {
            traffic.emitSimFrame();
            lastSimFrame = now;
        }

        // announce feed transitions immediately
        const bool hasDcs = !state.isStale();
        if (hasDcs != hadDcs) {
            std::cout << (hasDcs ? "* DCS position feed started (" + state.aircraft().aircraftName + ")"
                                 : "* DCS position feed lost (mission paused/ended?)")
                      << std::endl;
            hadDcs = hasDcs;
        }
        if (srsListener) {
            const bool hasSrs = state.srsFresh();
            if (hasSrs != hadSrs) {
                std::cout << (hasSrs ? "* cockpit radio feed started (DCS-SRS)"
                                     : "* cockpit radio feed lost - swift GUI owns the radios")
                          << std::endl;
                hadSrs = hasSrs;
            }
        }

        if (now - lastStatus >= std::chrono::seconds(10)) {
            const OwnAircraft a = state.aircraft();
            std::cout << "dcsswiftbus: " << (state.isStale() ? "NO DCS DATA" : "DCS feed OK")
                      << " | packets=" << udpListener.packetCount()
                      << " | " << a.aircraftName
                      << " lat=" << a.latitudeDeg << " lon=" << a.longitudeDeg
                      << " altMSL=" << static_cast<int>(a.altitudeMslM * M_TO_FT) << "ft"
                      << " QNH=" << a.qnhMmHg << "mmHg";
            if (srsListener) {
                if (state.srsFresh()) {
                    const SrsRadios srs = state.srsRadios();
                    std::cout << " | SRS radios OK com1=" << srs.com1ActiveKhz << "kHz com2=" << srs.com2ActiveKhz << "kHz";
                    if (srs.transponderCode >= 0) { std::cout << " squawk=" << srs.transponderCode; }
                    else { std::cout << " squawk=swift-GUI"; }
                } else {
                    std::cout << " | no SRS radio data (swift GUI owns radios)";
                }
            }
            std::cout << std::endl;
            lastStatus = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "dcsswiftbus: shutting down" << std::endl;
    udpListener.stop();
    return 0;
}
