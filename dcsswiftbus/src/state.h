/*
 * dcsswiftbus - shared own-aircraft state fed by DCS Export.lua via UDP
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace dcsswiftbus {

//! Own aircraft state as last reported by DCS (SI units as sent on the wire)
struct OwnAircraft {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeMslM = 0.0;
    double heightAglM = 0.0;
    double groundSpeedMs = 0.0;
    double pitchDeg = 0.0;
    double rollDeg = 0.0;
    double trueHeadingDeg = 0.0;
    double velocityEastMs = 0.0;
    double velocityUpMs = 0.0;
    double velocityNorthMs = 0.0;
    double pitchRateRadS = 0.0;
    double rollRateRadS = 0.0;
    double yawRateRadS = 0.0;
    double gearDeployRatio = 0.0;
    double flapsDeployRatio = 0.0;
    double speedBrakeRatio = 0.0;
    std::string aircraftName = "DCS";
};

//! Radio/transponder state owned by swift (set via DBus, echoed back on reads)
struct Avionics {
    int com1ActiveKhz = 122800;
    int com1StandbyKhz = 121500;
    int com2ActiveKhz = 124850;
    int com2StandbyKhz = 121500;
    int transponderCode = 2000;
    int transponderMode = 1; // 0-2 standby, >2 active (FG convention)
    bool transponderIdent = false;
    double com1Volume = 1.0;
    double com2Volume = 1.0;
};

class CState
{
public:
    void updateFromDcs(const OwnAircraft &aircraft)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_aircraft = aircraft;
        m_lastUpdate = std::chrono::steady_clock::now();
    }

    OwnAircraft aircraft() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_aircraft;
    }

    Avionics avionics() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_avionics;
    }

    template <typename F>
    void modifyAvionics(F &&func)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        func(m_avionics);
    }

    //! Considered paused when DCS stopped sending (mission paused/ended)
    bool isStale() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_lastUpdate.time_since_epoch().count() == 0) { return true; }
        return std::chrono::steady_clock::now() - m_lastUpdate > std::chrono::seconds(3);
    }

private:
    mutable std::mutex m_mutex;
    OwnAircraft m_aircraft;
    Avionics m_avionics;
    std::chrono::steady_clock::time_point m_lastUpdate;
};

} // namespace dcsswiftbus
