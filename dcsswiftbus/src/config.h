/*
 * dcsswiftbus - configuration (defaults < config file < command line)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>

namespace dcsswiftbus {

struct Config {
    std::string dbusHost = "127.0.0.1";
    std::string dbusPort = "45003";
    int udpPort = 47788;
    int srsPort = 9084;
    bool cockpitRadios = true; // false: swift GUI owns COM1/COM2/squawk
    std::string loadedFrom; // config file actually read, empty if none

    //! Merge settings from an INI-style file (key = value, # comments). False if unreadable.
    bool loadFile(const std::string &path);

    //! Apply command line arguments on top. Returns false (after printing usage) on bad args.
    bool applyArgs(int argc, char **argv);
};

} // namespace dcsswiftbus
