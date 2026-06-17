/*
 * FreeBSD network backend — ifconfig polling via getifaddrs(3).
 *
 * Each poll cycle (every 3 seconds) walks the interface list, diffs against
 * the previous state, and emits device_added / device_removed signals.
 */

#include "network-backend.hpp"
#include "null.hpp"

#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>

#include <map>
#include <string>

#include <glibmm.h>

/*
 * Detect whether an interface is a wireless interface on FreeBSD.
 * Wireless interfaces are typically wlanN, which is a virtual interface
 * layered over a physical device (e.g. iwm0, iwn0, urtwn0).
 * The actual physical interface does not expose Wi-Fi scan capabilities.
 */
static bool is_wireless_iface(const char *iface)
{
    return (strncmp(iface, "wlan", 4) == 0) && (iface[4] >= '0') && (iface[4] <= '9');
}

/* ─── FreeBSDNetworkBackend ──────────────────────────────────────────────── */

FreeBSDNetworkBackend::FreeBSDNetworkBackend()
{
    /* Placeholder null device — represents "no connection" */
    all_devices.emplace("/", std::make_shared<NullNetwork>());
}

void FreeBSDNetworkBackend::connect()
{
    /* Initial scan */
    refresh_devices();

    /* Poll every 3 seconds for interface changes */
    poll_timer = Glib::signal_timeout().connect(
        [this]() -> bool {
            refresh_devices();
            return true;
        },
        3000);

    /* Tell listeners that the "NM" equivalent started */
    signal_nm_start.emit();
}

void FreeBSDNetworkBackend::refresh_devices()
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        return;
    }

    std::map<std::string, std::string> current; // interface name → path

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        /* Only consider AF_INET (IPv4) — present on any up interface.
         * Skip loopback (lo0). */
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (strncmp(ifa->ifa_name, "lo", 2) == 0) {
            continue;
        }

        std::string iface = ifa->ifa_name;
        std::string path = "/org/freedesktop/NetworkManager/Devices/freebsd/" + iface;

        current[iface] = path;

        if (all_devices.find(path) == all_devices.end()) {
            /* New interface */
            bool wireless = is_wireless_iface(iface.c_str());
            auto net = std::make_shared<FreeBSDNetwork>(path, iface, wireless);
            all_devices.emplace(path, net);
            signal_device_added.emit(all_devices[path]);
        }
    }

    freeifaddrs(ifaddr);

    /* Remove interfaces that disappeared */
    std::vector<std::string> removed;
    for (const auto &[path, _] : all_devices) {
        if (path == "/") {
            continue;
        }
        /* Extract interface name from path */
        std::string iface = path.substr(path.rfind('/') + 1);
        if (current.find(iface) == current.end()) {
            removed.push_back(path);
        }
    }
    for (const auto& path : removed) {
        signal_device_removed.emit(all_devices[path]);
        all_devices.erase(path);
    }

    /* Re-emit network altered on each device to refresh icon state */
    for (auto& [path, dev] : all_devices) {
        if (path != "/") {
            dev->signal_network_altered().emit();
        }
    }
}

void FreeBSDNetworkBackend::disconnect()
{
    if (poll_timer.connected()) {
        poll_timer.disconnect();
    }
}
