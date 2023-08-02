#ifndef TRAY_TRAY_HPP
#define TRAY_TRAY_HPP

#include "item.hpp"
#include "widgets/tray/host.hpp"

#include <widget.hpp>

class WayfireStatusNotifier : public WayfireWidget
{
  private:
    StatusNotifierHost host = StatusNotifierHost(this);

    Gtk::HBox icons_hbox;
    std::map<Glib::ustring, StatusNotifierItem> items;

  public:
    void init(Gtk::HBox *container) override;

    void add_item(const Glib::ustring & service);
    void remove_item(const Glib::ustring & service);
};

#endif
