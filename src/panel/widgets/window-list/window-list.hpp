#ifndef WIDGETS_WINDOW_LIST_HPP
#define WIDGETS_WINDOW_LIST_HPP

#include "../../widget.hpp"
#include "panel.hpp"
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>

class WayfireToplevel;

class WayfireWindowListBox : public Gtk::HBox
{
    Gtk::Widget *top_widget = nullptr;
    int top_x = 0;

    public:
    WayfireWindowListBox();

    /**
     * Set the widget which should always be rendered on top of the other child
     * widgets */
    void set_top_widget(Gtk::Widget *top = nullptr);
    /** Set the absolute position of the top widget */
    void set_top_x(int x);

    /**
     * Override some of Gtk::HBox's built-in layouting functions, so that we
     * support manually dragging a button
     */
    void forall_vfunc(gboolean, GtkCallback callback, gpointer callback_data) override;
    void on_size_allocate(Gtk::Allocation& alloc) override;

    /**
     * @param x the x-axis position, relative to ref
     * @param ref The widget that x is relative to. ref must be a child
     * widget of this box
     * @return the position x, but relative to the box
     */
    int get_absolute_position(int x, Gtk::Widget& ref);

    /** Find the direct child widget at the given box-relative coordinates,
     * ignoring the top widget if possible, i.e if the top widget and some
     * other widget are at the given coordinates, then the bottom widget will
     * be returned
     *
     * @return The direct child widget or none if it doesn't exist
     */
    Gtk::Widget* get_widget_at(int x);

    /**
     * Get the list of widgets sorted from left to right, i.e ignoring the top
     * widget setting
     */
    std::vector<Gtk::Widget*> get_unsorted_widgets();
};

class WayfireWindowList : public WayfireWidget
{
    public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WayfireToplevel>> toplevels;

    zwlr_foreign_toplevel_manager_v1 *manager;
    WayfireOutput *output;
    WayfireWindowListBox box;
    Gtk::ScrolledWindow scrolled_window;

    WayfireWindowList(WayfireOutput *output);
    virtual ~WayfireWindowList();

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);

    wayfire_config *get_config();

    void init(Gtk::HBox *container) override;
    void add_output(WayfireOutput *output);

    private:
    void on_draw(const Cairo::RefPtr<Cairo::Context>&);

    void set_button_width(int width);
    int get_default_button_width();
    int get_target_button_width();
};

#endif /* end of include guard: WIDGETS_WINDOW_LIST_HPP */
