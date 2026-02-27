#pragma once
#include "network/network.hpp"
#include "network/settings.hpp"
#include "sigc++/connection.h"
#include <giomm.h>
#include <memory>

/* 256 + 512, Has *some* kind of password security */
#define NM_SOME_WPA_SECURITY 0x300


using type_signal_access_point_altered = sigc::signal<void (void)>;
class AccessPoint
{
  private:
    std::string ap_path;
    type_signal_access_point_altered access_point_altered;
    unsigned char strength;
    unsigned int security_flags;
    unsigned int freq;
    std::string ssid = "";
    std::vector<sigc::connection> signals;
    bool saved_password;

  public:

    std::string get_path();
    std::shared_ptr<Gio::DBus::Proxy> access_point_proxy;
    AccessPoint(std::string path, std::shared_ptr<Gio::DBus::Proxy> access_point_proxy);
    ~AccessPoint();
    std::string get_ssid();
    std::string strength_string();
    std::string get_icon_name();
    std::string get_security_icon_name();
    std::string get_band_name();
    std::vector<std::string> get_css_classes();
    type_signal_access_point_altered signal_altered();
    unsigned char get_strength();
    bool has_saved_password();
    void set_has_saved_password(bool);
};
