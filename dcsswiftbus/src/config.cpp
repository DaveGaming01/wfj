/*
 * dcsswiftbus - configuration (defaults < config file < command line)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace dcsswiftbus {

namespace {

std::string trim(const std::string &s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) { return {}; }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

} // namespace

bool Config::loadFile(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open()) { return false; }

    std::string line;
    int lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;
        const auto comment = line.find('#');
        if (comment != std::string::npos) { line.erase(comment); }
        line = trim(line);
        if (line.empty()) { continue; }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "dcsswiftbus: " << path << ":" << lineNo << ": ignoring line without '='" << std::endl;
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key == "dbus_host") { dbusHost = value; }
        else if (key == "dbus_port") { dbusPort = value; }
        else if (key == "dcs_port") { udpPort = std::atoi(value.c_str()); }
        else if (key == "srs_port") { srsPort = std::atoi(value.c_str()); }
        else if (key == "radios") {
            if (value == "cockpit") { cockpitRadios = true; }
            else if (value == "swift") { cockpitRadios = false; }
            else {
                std::cerr << "dcsswiftbus: " << path << ":" << lineNo
                          << ": radios must be 'cockpit' or 'swift', got '" << value << "'" << std::endl;
            }
        } else {
            std::cerr << "dcsswiftbus: " << path << ":" << lineNo << ": unknown setting '" << key << "'" << std::endl;
        }
    }

    loadedFrom = path;
    return true;
}

bool Config::applyArgs(int argc, char **argv)
{
    // a --config argument wins over the default config file, so find it first
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            if (!loadFile(argv[i + 1])) {
                std::cerr << "dcsswiftbus: cannot read config file " << argv[i + 1] << std::endl;
                return false;
            }
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto nextArg = [&]() -> const char * { return (i + 1 < argc) ? argv[++i] : nullptr; };
        if (arg == "--config") {
            ++i; // handled above
        } else if (arg == "--dbus-host") {
            if (const char *v = nextArg()) { dbusHost = v; }
        } else if (arg == "--dbus-port") {
            if (const char *v = nextArg()) { dbusPort = v; }
        } else if (arg == "--udp-port" || arg == "--dcs-port") {
            if (const char *v = nextArg()) { udpPort = std::atoi(v); }
        } else if (arg == "--srs-port") {
            if (const char *v = nextArg()) { srsPort = std::atoi(v); }
        } else if (arg == "--radios") {
            const char *v = nextArg();
            if (v && std::string(v) == "cockpit") { cockpitRadios = true; }
            else if (v && std::string(v) == "swift") { cockpitRadios = false; }
            else {
                std::cerr << "dcsswiftbus: --radios takes 'cockpit' or 'swift'" << std::endl;
                return false;
            }
        } else {
            std::cout << "usage: dcsswiftbus [options]\n"
                      << "\n"
                      << "  --config <file>          settings file (default: dcsswiftbus.cfg if present)\n"
                      << "  --radios cockpit|swift   who owns COM1/COM2/squawk: the DCS cockpit (via\n"
                      << "                           DCS-SRS broadcasts) or the swift GUI (default: cockpit)\n"
                      << "  --dbus-host <ip>         DBus listen address for swift (default 127.0.0.1)\n"
                      << "  --dbus-port <port>       DBus listen port for swift (default 45003)\n"
                      << "  --dcs-port <port>        UDP port for the DCS Export.lua feed (default 47788)\n"
                      << "  --srs-port <port>        UDP port of the DCS-SRS broadcasts (default 9084)\n";
            return false;
        }
    }
    return true;
}

} // namespace dcsswiftbus
