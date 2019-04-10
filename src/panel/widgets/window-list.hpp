#ifndef WIDGETS_WINDOW_LIST_HPP
#define WIDGETS_WINDOW_LIST_HPP

#include "../widget.hpp"
#include "../../util/display.hpp"
#include "panel.hpp"
#include "toplevel.hpp"

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>

class WayfireToplevel;

class WayfireWindowList : public WayfireWidget
{
    public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WayfireToplevel>> toplevels;

    zwlr_foreign_toplevel_manager_v1 *manager;
    WayfireOutput *output;
    Gtk::HBox box;
    Gtk::ScrolledWindow scrolled_window;

    WayfireWindowList(WayfireOutput *output);
    virtual ~WayfireWindowList();

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);

    WayfireDisplay *get_display();
    wayfire_config *get_config();

    void init(Gtk::HBox *container, wayfire_config *config);
    void add_output(WayfireOutput *output);

    uint button_text_length;

    Gtk::Button **dnd_button_ptr;

    private:
    int32_t last_button_width = 100;
    void on_draw(const Cairo::RefPtr<Cairo::Context>&);

    void set_button_width(int width);
    int get_default_button_width();
};

#endif /* end of include guard: WIDGETS_WINDOW_LIST_HPP */
