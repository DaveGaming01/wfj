/*
 * dcsswiftbus - listener for DCS-SRS export broadcasts (cockpit radios for ~60 modules)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "srslistener.h"

#include "minijson.h"

#include <cmath>
#include <iostream>

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <unistd.h>
#endif

namespace dcsswiftbus {

CSrsListener::CSrsListener(CState &state, std::uint16_t port)
    : m_state(state), m_port(port)
{}

CSrsListener::~CSrsListener()
{
    stop();
}

bool CSrsListener::start()
{
    m_socket = static_cast<intptr_t>(::socket(AF_INET, SOCK_DGRAM, 0));
    if (m_socket < 0) {
        std::cerr << "dcsswiftbus: failed to create SRS UDP socket" << std::endl;
        return false;
    }

    // SRS may broadcast to 127.255.255.255 and other tools may want the data too
    int reuse = 1;
    setsockopt(static_cast<int>(m_socket), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuse), sizeof(reuse));

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
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // catch both 127.0.0.1 and 127.255.255.255 broadcasts
    addr.sin_port = htons(m_port);
    if (::bind(static_cast<int>(m_socket), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::cerr << "dcsswiftbus: failed to bind SRS UDP port " << m_port
                  << " (SRS client running? close it or use --srs-port)" << std::endl;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&CSrsListener::run, this);
    return true;
}

void CSrsListener::stop()
{
    if (!m_running) { return; }
    m_running = false;
    if (m_thread.joinable()) { m_thread.join(); }
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(m_socket));
#else
    ::close(static_cast<int>(m_socket));
#endif
    m_socket = -1;
}

void CSrsListener::run()
{
    char buffer[65536]; // SRS datagrams are large (full radio/iff state)
    while (m_running) {
        const auto received = ::recvfrom(static_cast<int>(m_socket), buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
        if (received <= 0) { continue; }
        buffer[received] = '\0';
        parseDatagram(std::string(buffer, static_cast<std::size_t>(received)));
    }
}

void CSrsListener::parseDatagram(const std::string &payload)
{
    const json::ValuePtr root = json::parse(payload);
    if (!root || root->type != json::Value::Object) { return; }

    const json::ValuePtr radios = root->get("radios");
    if (!radios || radios->type != json::Value::Array) { return; }

    SrsRadios result;

    // SRS radios: Lua [1] = intercom, [2] = radio 1, [3] = radio 2 -> JSON indices 1 and 2
    auto radioKhz = [&](std::size_t jsonIndex) -> int {
        const json::ValuePtr radio = radios->at(jsonIndex);
        if (!radio) { return 0; }
        const json::ValuePtr freq = radio->get("freq");
        if (!freq || !freq->isNumber()) { return 0; }
        const double hz = freq->number;
        if (hz < 1000000.0) { return 0; } // SRS uses small values (0/1/...) for off/invalid
        return static_cast<int>(std::lround(hz / 1000.0));
    };
    result.com1ActiveKhz = radioKhz(1);
    result.com2ActiveKhz = radioKhz(2);

    if (const json::ValuePtr iff = root->get("iff")) {
        if (const json::ValuePtr status = iff->get("status"); status && status->isNumber()) {
            result.transponderStatus = static_cast<int>(status->number);
        }
        if (const json::ValuePtr mode3 = iff->get("mode3"); mode3 && mode3->isNumber()) {
            // Only a plausible squawk counts: non-zero, four octal digits. Modules whose
            // IFF is unpowered/unsupported report 0 or -1 -> swift GUI keeps the squawk.
            const int code = static_cast<int>(mode3->number);
            const bool octal = code >= 1 && code <= 7777 && (code % 10) <= 7 && (code / 10 % 10) <= 7 &&
                               (code / 100 % 10) <= 7 && (code / 1000 % 10) <= 7;
            if (octal) { result.transponderCode = code; }
        }
    }

    m_state.updateFromSrs(result);
    ++m_packetCount;
}

} // namespace dcsswiftbus
