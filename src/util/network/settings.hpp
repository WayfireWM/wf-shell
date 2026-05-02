#pragma once
#include <giomm.h>
class NetworkSettings
{
  private:
    std::shared_ptr<Gio::DBus::Proxy> proxy;
    std::string setting_name = "";
    std::string ssid = "";

  public:
    NetworkSettings(std::string path, std::shared_ptr<Gio::DBus::Proxy> proxy);
    void signal(const Glib::ustring&, const Glib::ustring&, const Glib::VariantContainerBase&);
    void read_contents();

    std::string get_ssid();
    std::string get_name();
};
