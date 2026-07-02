/*
 * dcsswiftbus - listener for DCS-SRS export broadcasts (cockpit radios for ~60 modules)
 *
 * The DCS-SRS export script (if installed) broadcasts the full cockpit radio
 * state as JSON to UDP 127.0.0.1:9084 every export tick, using per-module
 * exporters maintained by the SRS project. We piggyback on that instead of
 * reimplementing per-aircraft cockpit logic.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "state.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace dcsswiftbus {

class CSrsListener
{
public:
    CSrsListener(CState &state, std::uint16_t port);
    ~CSrsListener();

    CSrsListener(const CSrsListener &) = delete;
    CSrsListener &operator=(const CSrsListener &) = delete;

    bool start();
    void stop();

    std::uint64_t packetCount() const { return m_packetCount; }

private:
    void run();
    void parseDatagram(const std::string &payload);

    CState &m_state;
    std::uint16_t m_port;
    std::atomic<bool> m_running { false };
    std::atomic<std::uint64_t> m_packetCount { 0 };
    std::thread m_thread;
    intptr_t m_socket = -1;
};

} // namespace dcsswiftbus
