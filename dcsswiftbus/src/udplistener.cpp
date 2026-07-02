/*
 * dcsswiftbus - UDP listener for the DCS Export.lua feed
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "udplistener.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
using socklen_type = int;
#else
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <unistd.h>
using socklen_type = socklen_t;
#endif

namespace dcsswiftbus {

CUdpListener::CUdpListener(CState &state, std::uint16_t port)
    : m_state(state), m_port(port)
{}

CUdpListener::~CUdpListener()
{
    stop();
}

bool CUdpListener::start()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { return false; }
#endif

    m_socket = static_cast<intptr_t>(::socket(AF_INET, SOCK_DGRAM, 0));
    if (m_socket < 0) {
        std::cerr << "dcsswiftbus: failed to create UDP socket" << std::endl;
        return false;
    }

    // 1s receive timeout so the thread can notice stop()
#ifdef _WIN32
    DWORD timeoutMs = 1000;
    setsockopt(static_cast<SOCKET>(m_socket), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
#else
    timeval tv {};
    tv.tv_sec = 1;
    setsockopt(static_cast<int>(m_socket), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_port);
    if (::bind(static_cast<int>(m_socket), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::cerr << "dcsswiftbus: failed to bind UDP port " << m_port << std::endl;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&CUdpListener::run, this);
    return true;
}

void CUdpListener::stop()
{
    if (!m_running) { return; }
    m_running = false;
    if (m_thread.joinable()) { m_thread.join(); }
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(m_socket));
    WSACleanup();
#else
    ::close(static_cast<int>(m_socket));
#endif
    m_socket = -1;
}

void CUdpListener::run()
{
    char buffer[2048];
    while (m_running) {
        const auto received = ::recvfrom(static_cast<int>(m_socket), buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
        if (received <= 0) { continue; } // timeout or error, loop to re-check m_running
        buffer[received] = '\0';
        parseDatagram(buffer, static_cast<std::size_t>(received));
        ++m_packetCount;
    }
}

void CUdpListener::parseDatagram(const char *data, std::size_t len)
{
    OwnAircraft aircraft = m_state.aircraft(); // start from last known values

    const std::string payload(data, len);
    std::size_t pos = 0;
    while (pos < payload.size()) {
        std::size_t end = payload.find(';', pos);
        if (end == std::string::npos) { end = payload.size(); }
        const std::string token = payload.substr(pos, end - pos);
        pos = end + 1;

        const std::size_t eq = token.find('=');
        if (eq == std::string::npos) { continue; }
        const std::string key = token.substr(0, eq);
        const std::string value = token.substr(eq + 1);
        if (key.empty() || value.empty()) { continue; }

        if (key == "name") {
            aircraft.aircraftName = value;
            continue;
        }

        const double d = std::strtod(value.c_str(), nullptr);
        if (key == "lat") { aircraft.latitudeDeg = d; }
        else if (key == "lon") { aircraft.longitudeDeg = d; }
        else if (key == "alt") { aircraft.altitudeMslM = d; }
        else if (key == "agl") { aircraft.heightAglM = d; }
        else if (key == "gs") { aircraft.groundSpeedMs = d; }
        else if (key == "pitch") { aircraft.pitchDeg = d; }
        else if (key == "roll") { aircraft.rollDeg = d; }
        else if (key == "hdg") { aircraft.trueHeadingDeg = d; }
        else if (key == "ve") { aircraft.velocityEastMs = d; }
        else if (key == "vu") { aircraft.velocityUpMs = d; }
        else if (key == "vn") { aircraft.velocityNorthMs = d; }
        else if (key == "pr") { aircraft.pitchRateRadS = d; }
        else if (key == "rr") { aircraft.rollRateRadS = d; }
        else if (key == "yr") { aircraft.yawRateRadS = d; }
        else if (key == "gear") { aircraft.gearDeployRatio = d; }
        else if (key == "flaps") { aircraft.flapsDeployRatio = d; }
        else if (key == "brk") { aircraft.speedBrakeRatio = d; }
    }

    m_state.updateFromDcs(aircraft);
}

} // namespace dcsswiftbus
