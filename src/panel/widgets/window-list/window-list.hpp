#ifndef WIDGETS_WINDOW_LIST_HPP
#define WIDGETS_WINDOW_LIST_HPP

#include "../../widget.hpp"
#include "panel.hpp"
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

#include <gtkmm.h>

class WayfireToplevel;

class WayfireWindowListLayout : public Gtk::LayoutManager{
  protected:
  void allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline) override{
    int per_child = width / widget.get_children().size();

    // TODO Bad assumptions not based on reality.
    if(per_child < 32)
    {
      per_child = 32;
    }
    if(per_child > 100)
    {
      per_child = 100;
    }
    int count = 0;
    for (auto child: widget.get_children())
    {
      auto alloc = Gtk::Allocation();
      if(child == top_widget){
        alloc.set_x(top_x);
        alloc.set_y(0);
        alloc.set_width(per_child);
        alloc.set_height(height);
        child->size_allocate(alloc,-1);
        continue;
      }
      alloc.set_x(per_child * count);
      alloc.set_y(0);
      alloc.set_width(per_child);
      alloc.set_height(height);
      std::cout<<"PLACED"<<std::endl;
      child->size_allocate(alloc,-1);
    }
  }

  public:
    int top_x = 0;
    Gtk::Widget *top_widget = nullptr;


};

class WayfireWindowListBox : public Gtk::Box
{
    std::shared_ptr<WayfireWindowListLayout> layout = std::make_shared<WayfireWindowListLayout>();

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
    // void forall_vfunc(gboolean, GtkCallback callback, gpointer callback_data) override;
    // void on_size_allocate(Gtk::Allocation& alloc) override;

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
    Gtk::Widget *get_widget_at(int x);
};

class WayfireWindowList : public WayfireWidget
{
  public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WayfireToplevel>> toplevels;

    zwlr_foreign_toplevel_manager_v1 *manager = NULL;
    WayfireOutput *output;
    WayfireWindowListBox box;
    Gtk::ScrolledWindow scrolled_window;

    WayfireWindowList(WayfireOutput *output);
    virtual ~WayfireWindowList();

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);

    wayfire_config *get_config();

    void init(Gtk::Box *container) override;
    void add_output(WayfireOutput *output);

  private:
    int get_default_button_width();
    int get_target_button_width();
};

#endif /* end of include guard: WIDGETS_WINDOW_LIST_HPP */
