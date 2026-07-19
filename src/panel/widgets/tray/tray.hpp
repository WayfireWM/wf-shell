#pragma once

#include "item.hpp"
#include "wf-option-wrap.hpp"
#include "widgets/tray/host.hpp"

#include <widget.hpp>

class WayfireStatusNotifier : public WayfireWidget
{
  private:
    StatusNotifierHost host = StatusNotifierHost(this);

    Gtk::FlowBox icons_box;
    std::map<Glib::ustring, StatusNotifierItem> items;

    WfOption<int> spacing{"panel/tray_spacing"};
    WfOption<int> rows_cols{"panel/tray_rows_cols"};

    void update_layout();
    void handle_config_reload();

  public:
    void init(Gtk::Box *container) override;

    void add_item(const Glib::ustring & service);
    void remove_item(const Glib::ustring & service);
};
