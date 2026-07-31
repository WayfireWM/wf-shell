#include "wf-autohide-window.hpp"

#include "layout.hpp"
#include "menu.hpp"

WfMenuLayout::WfMenuLayout(WayfireMenu *menu) : menu(menu)
{}

void WfMenuLayout::allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline)
{
    if (menu == nullptr)
    {
        return;
    }

    bool is_top = panel_position.value() == WF_WINDOW_POSITION_TOP;

    Gtk::Widget::Measurements entry_measurements, logout_measurements, separator_measurements;
    entry_measurements     = menu->search_entry.measure(Gtk::Orientation::VERTICAL, width);
    logout_measurements    = menu->box_bottom.measure(Gtk::Orientation::VERTICAL, width);
    separator_measurements = menu->separator.measure(Gtk::Orientation::VERTICAL, width);

    int remaining_height = height -
        (entry_measurements.sizes.minimum +
            logout_measurements.sizes.minimum +
            separator_measurements.sizes.natural);
    if (remaining_height <= 0)
    {
        return;
    }

    search_alloc.set_x(0);
    search_alloc.set_y(is_top ? 0 : height -
        (logout_measurements.sizes.minimum + separator_measurements.sizes.natural +
            entry_measurements.sizes.minimum));
    search_alloc.set_height(entry_measurements.sizes.minimum);
    search_alloc.set_width(width);
    menu->search_entry.size_allocate(search_alloc, -1);

    logout_alloc.set_x(0);
    logout_alloc.set_y(height - logout_measurements.sizes.minimum);
    logout_alloc.set_width(width);
    logout_alloc.set_height(logout_measurements.sizes.minimum);
    menu->box_bottom.size_allocate(logout_alloc, -1);

    separator_alloc.set_x(0);
    separator_alloc.set_y(height -
        (logout_measurements.sizes.minimum + separator_measurements.sizes.natural));
    separator_alloc.set_width(width);
    separator_alloc.set_height(separator_measurements.sizes.natural);
    menu->separator.size_allocate(separator_alloc, -1);

    if (show_categories.value())
    {
        category_alloc.set_x(0);
        category_alloc.set_y(is_top ? entry_measurements.sizes.minimum : 0);
        category_alloc.set_width(category_width);
        category_alloc.set_height(remaining_height);
        menu->category_scrolled_window.size_allocate(category_alloc, -1);

        flow_alloc.set_x(category_width);
        flow_alloc.set_y(is_top ? entry_measurements.sizes.minimum : 0);
        flow_alloc.set_width(width - category_width);
        flow_alloc.set_height(remaining_height);
        menu->app_scrolled_window.size_allocate(flow_alloc, -1);
    } else
    {
        /* Even if we're not having it, allocate some space */
        category_alloc.set_x(0);
        category_alloc.set_y(is_top ? entry_measurements.sizes.minimum : 0);
        category_alloc.set_width(width);
        category_alloc.set_height(remaining_height);
        menu->category_scrolled_window.size_allocate(category_alloc, -1);

        flow_alloc.set_x(0);
        flow_alloc.set_y(is_top ? entry_measurements.sizes.minimum : 0);
        flow_alloc.set_width(width);
        flow_alloc.set_height(remaining_height);
        menu->app_scrolled_window.size_allocate(flow_alloc, -1);
    }
}

void WfMenuLayout::measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation,
    int for_size, int& minimum, int& natural, int& minimum_baseline,
    int& natural_baseline) const
{
    minimum_baseline = -1;
    natural_baseline = -1;
    // What is our preferred width?
    if (orientation == Gtk::Orientation::HORIZONTAL)
    {
        if (limit_width > 0)
        {
            minimum = limit_width;
            natural = limit_width;
            return;
        }

        minimum = category_width + content_width;
        natural = category_width + content_width;
    } else
    {
        if (limit_height > 0)
        {
            minimum = limit_height;
            natural = limit_height;
            return;
        }

        Gtk::Widget::Measurements entry_measurements, logout_measurements, separator_measurements;
        entry_measurements     = menu->search_entry.measure(Gtk::Orientation::VERTICAL, for_size);
        logout_measurements    = menu->box_bottom.measure(Gtk::Orientation::VERTICAL, for_size);
        separator_measurements = menu->separator.measure(Gtk::Orientation::VERTICAL, for_size);

        minimum = separator_measurements.sizes.natural + entry_measurements.sizes.minimum +
            logout_measurements.sizes.minimum + content_height;
        natural = separator_measurements.sizes.natural + entry_measurements.sizes.minimum +
            logout_measurements.sizes.minimum + content_height;
    }
}

void WfMenuLayout::set_limit(int w, int h)
{
    if ((w == limit_width) && (h == limit_height))
    {
        return;
    }

    limit_width  = w;
    limit_height = h;
    menu->popover_layout_box.queue_resize();
}
