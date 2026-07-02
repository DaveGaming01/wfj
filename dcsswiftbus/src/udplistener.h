/*
 * dcsswiftbus - UDP listener for the DCS Export.lua feed
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "state.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace dcsswiftbus {

//! Receives "key=value;key=value;..." datagrams from Export.lua and updates CState
class CUdpListener
{
public:
    CUdpListener(CState &state, std::uint16_t port);
    ~CUdpListener();

    CUdpListener(const CUdpListener &) = delete;
    CUdpListener &operator=(const CUdpListener &) = delete;

    bool start();
    void stop();

    //! Number of datagrams parsed so far
    std::uint64_t packetCount() const { return m_packetCount; }

private:
    void run();
    void parseDatagram(const char *data, std::size_t len);

    CState &m_state;
    std::uint16_t m_port;
    std::atomic<bool> m_running { false };
    std::atomic<std::uint64_t> m_packetCount { 0 };
    std::thread m_thread;
    intptr_t m_socket = -1;
};

} // namespace dcsswiftbus
