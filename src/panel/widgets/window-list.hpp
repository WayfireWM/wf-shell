#ifndef WIDGETS_WINDOW_LIST_HPP
#define WIDGETS_WINDOW_LIST_HPP

#include "../widget.hpp"
#include "../../util/display.hpp"
#include "panel.hpp"
#include "toplevel.hpp"

#include <gtkmm/button.h>

class WayfireToplevel;

class WayfireWindowList : public WayfireWidget
{
    public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WayfireToplevel>> toplevels;

    zwlr_foreign_toplevel_manager_v1 *manager;
    WayfireOutput *output;
    Gtk::HBox box;

    WayfireWindowList();
    virtual ~WayfireWindowList();

    static WayfireWindowList& get();

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    WayfirePanel* panel_for_wl_output(wl_output *output);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);

    WayfireWindowList* window_list_for_wl_output(wl_output *output);
    WayfireDisplay *get_display();
    wayfire_config *get_config();

    void init(Gtk::HBox *container, wayfire_config *config);
    void add_output(WayfireOutput *output);

    private:
    static std::unique_ptr<WayfireWindowList> instance;
};

#endif /* end of include guard: WIDGETS_WINDOW_LIST_HPP */
