#pragma once

#include "network.hpp"
#include <string>

/**
 * FreeBSDNetwork — a Network subclass backed by ifconfig(8) polling.
 *
 * Used on FreeBSD when NetworkManager is not available.  Detects interface
 * status by checking IFF_UP and IFF_RUNNING flags from getifaddrs(3).
 */
class FreeBSDNetwork : public Network
{
  public:
    /**
     * @param path       D-Bus-style object path, e.g. "/org/freedesktop/NetworkManager/Devices/0"
     * @param interface  The OS interface name, e.g. "em0", "wlan0"
     * @param is_wireless True if the interface is wireless
     */
    FreeBSDNetwork(std::string path, std::string interface, bool is_wireless);

    std::string get_name() override;
    std::string get_icon_name() override;
    std::vector<std::string> get_css_classes() override;
    std::string get_friendly_name() override;
    bool is_active();
    void disconnect();
    bool can_toggle() override;

  private:
    bool is_wireless;

    static std::string icon_for_state(bool active, bool wireless);
};
