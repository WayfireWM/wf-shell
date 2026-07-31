#pragma once
#include <giomm/desktopappinfo.h>
#include <gtkmm.h>

#include "wf-option-wrap.hpp"

class WayfireMenu;
using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;

class WfMenuLayout : public Gtk::LayoutManager
{
  protected:
    Gtk::Allocation search_alloc, logout_alloc, category_alloc, flow_alloc, separator_alloc;

    void allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline) override;
    void measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation, int for_size, int& minimum,
        int& natural, int& minimum_baseline, int& natural_baseline) const override;
    WayfireMenu *menu;
    int limit_width = 0, limit_height = 0;

    WfOption<bool> show_categories{"panel/menu_show_categories"};
    WfOption<int> category_width{"panel/menu_min_category_width"};
    WfOption<int> content_width{"panel/menu_min_content_width"};
    WfOption<int> content_height{"panel/menu_min_content_height"};
    WfOption<std::string> panel_position{"panel/position"};

  public:
    WfMenuLayout(WayfireMenu *menu);

    void set_limit(int x, int y);
};
