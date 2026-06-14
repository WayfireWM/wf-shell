#include "freebsd-network.hpp"

#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <wordexp.h>

#include <glibmm.h>

/*
 * Check whether the current user can run a shell command without elevation.
 * Tries to expand tokens then executes `cmd --help 2>/dev/null`.
 * If that fails with EPERM/EACCES, the user lacks permission.
 */
static bool check_command(const char *cmd)
{
    if (geteuid() == 0) {
        return true; /* root can do anything */
    }

    wordexp_t we = {};
    int err = wordexp(cmd, &we, WRDE_NOCMD);
    if (err != 0) {
        return false;
    }
    if (we.we_wordc == 0) {
        wordfree(&we);
        return false;
    }

    std::string expanded;
    for (size_t i = 0; i < we.we_wordc; i++) {
        if (i > 0) {
            expanded += ' ';
        }
        expanded += we.we_wordv[i];
    }
    expanded += " --help 2>/dev/null";
    wordfree(&we);

    int status = system(expanded.c_str());
    return WIFEXITED(status) && WEXITSTATUS(status) < 128;
}

FreeBSDNetwork::FreeBSDNetwork(std::string path, std::string iface, bool wireless) :
    Network(path, nullptr), is_wireless(wireless)
{
    this->interface = iface;
}

std::string FreeBSDNetwork::get_name()
{
    return interface;
}

std::string FreeBSDNetwork::get_friendly_name()
{
    if (is_wireless) {
        return "Wireless (" + interface + ")";
    }
    return "Ethernet (" + interface + ")";
}

std::vector<std::string> FreeBSDNetwork::get_css_classes()
{
    if (is_wireless) {
        return {"wifi", "medium"};
    }
    return {"ethernet", "good"};
}

bool FreeBSDNetwork::is_active()
{
    /* Walk all interfaces to find our interface */
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return false;
    }

    bool active = false;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        if (ifa->ifa_name != interface) {
            continue;
        }
        /* IFF_UP means interface is enabled.
         * IFF_RUNNING means cable plugged in / link detected.
         * We treat IFF_UP as "available" and IFF_RUNNING as "active". */
        if ((ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING)) {
            active = true;
        }
    }

    freeifaddrs(ifaddr);
    return active;
}

std::string FreeBSDNetwork::icon_for_state(bool active, bool wireless)
{
    if (wireless) {
        return active ? "network-wireless-recommended" : "network-wireless-offline";
    }
    return active ? "network-wired" : "network-wired-offline";
}

std::string FreeBSDNetwork::get_icon_name()
{
    return icon_for_state(is_active(), is_wireless);
}

void FreeBSDNetwork::disconnect()
{
    /* On FreeBSD, bringing an interface down requires root.
     * Attempt it anyway — if the user lacks permission it will silently fail. */
    std::string cmd = "ifconfig " + interface + " down";
    (void)system(cmd.c_str());
    network_altered.emit();
}

bool FreeBSDNetwork::can_toggle()
{
    /* Check if user can run `ifconfig <iface> up` — the privilege needed to bring
     * an interface up or down.  This covers both the direct case (root) and the
     * sudo/doas case. */
    std::string cmd = "ifconfig " + interface + " up";
    return check_command(cmd.c_str());
}
