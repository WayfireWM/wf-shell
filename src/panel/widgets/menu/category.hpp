#pragma once
#include <string>
#include <giomm/desktopappinfo.h>
#include <glibmm/refptr.h>

using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;

class WfMenuCategory
{
  public:
    WfMenuCategory(std::string name, std::string icon_name);
    std::string get_name();
    std::string get_icon_name();
    std::vector<AppInfo> items;

  private:
    std::string name;
    std::string icon_name;
};
