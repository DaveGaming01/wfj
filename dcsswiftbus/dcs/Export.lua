-- dcsswiftbus Export.lua
-- Streams own-aircraft state to the dcsswiftbus daemon over UDP so the swift
-- pilot client (thinking it is talking to FlightGear) can fly it on VATSIM.
--
-- Install: copy into  %USERPROFILE%\Saved Games\DCS\Scripts\Export.lua
-- If you already have an Export.lua (SRS, TacView, Helios...), append this
-- file's contents instead - it chains the previous handlers like they do.
--
-- SPDX-License-Identifier: GPL-2.0-or-later

local dcsswiftbus = {
    HOST = "127.0.0.1",
    PORT = 47788,
    INTERVAL = 0.1, -- seconds between updates (10 Hz)
}

do
    local prevLuaExportStart = LuaExportStart
    local prevLuaExportStop = LuaExportStop
    local prevLuaExportActivityNextEvent = LuaExportActivityNextEvent

    local socket = nil
    local udp = nil

    LuaExportStart = function()
        if prevLuaExportStart then prevLuaExportStart() end
        package.path = package.path .. ";.\\LuaSocket\\?.lua"
        package.cpath = package.cpath .. ";.\\LuaSocket\\?.dll"
        socket = require("socket")
        udp = socket.udp()
        udp:settimeout(0)
    end

    LuaExportStop = function()
        if udp then udp:close() end
        udp = nil
        if prevLuaExportStop then prevLuaExportStop() end
    end

    LuaExportActivityNextEvent = function(t)
        local tNext = t + dcsswiftbus.INTERVAL

        if udp then
            local self = LoGetSelfData()
            if self and self.LatLongAlt then
                local rad2deg = 180.0 / math.pi
                local lat = self.LatLongAlt.Lat
                local lon = self.LatLongAlt.Long
                local alt = self.LatLongAlt.Alt                       -- meters MSL
                local agl = LoGetAltitudeAboveGroundLevel() or 0.0    -- meters
                local pitch = (self.Pitch or 0.0) * rad2deg
                local roll = (self.Bank or 0.0) * rad2deg
                local hdg = (self.Heading or 0.0) * rad2deg
                if hdg < 0 then hdg = hdg + 360.0 end

                -- DCS world frame: x = north, y = up, z = east (m/s)
                local vel = LoGetVectorVelocity() or { x = 0, y = 0, z = 0 }
                local gs = math.sqrt(vel.x * vel.x + vel.z * vel.z)

                -- body rotation rates in rad/s: x = roll, y = yaw, z = pitch
                local omega = LoGetAngularVelocity() or { x = 0, y = 0, z = 0 }

                local mech = LoGetMechInfo()
                local gear, flaps, brk = 0.0, 0.0, 0.0
                if mech then
                    if mech.gear and mech.gear.value then gear = mech.gear.value end
                    if mech.flaps and mech.flaps.value then flaps = mech.flaps.value end
                    if mech.speedbrakes and mech.speedbrakes.value then brk = mech.speedbrakes.value end
                end

                local msg = string.format(
                    "name=%s;lat=%.7f;lon=%.7f;alt=%.2f;agl=%.2f;gs=%.2f;" ..
                    "pitch=%.3f;roll=%.3f;hdg=%.3f;" ..
                    "ve=%.3f;vu=%.3f;vn=%.3f;" ..
                    "pr=%.5f;rr=%.5f;yr=%.5f;" ..
                    "gear=%.2f;flaps=%.2f;brk=%.2f",
                    self.Name or "DCS", lat, lon, alt, agl, gs,
                    pitch, roll, hdg,
                    vel.z, vel.y, vel.x,
                    omega.z, omega.x, omega.y,
                    gear, flaps, brk)
                udp:sendto(msg, dcsswiftbus.HOST, dcsswiftbus.PORT)
            end
        end

        if prevLuaExportActivityNextEvent then
            local otherNext = prevLuaExportActivityNextEvent(t)
            if otherNext and otherNext < tNext then tNext = otherNext end
        end
        return tNext
    end
end
