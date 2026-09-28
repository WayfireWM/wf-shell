#include "toplevel.hpp"
#include "window-list.hpp"

WayfireWindowListLayout::WayfireWindowListLayout(WayfireWindowList *window_list)
{
    this->window_list = window_list;
}

void WayfireWindowListLayout::allocate_vfunc(const Gtk::Widget& widget,
    int width, int height, int baseline)
{
    Gtk::Widget& widget_not_const = const_cast<Gtk::Widget&>(widget);
    int child_count = widget.get_children().size();
    if (child_count <= 0)
    {
        return;
    }

    const bool vertical = window_list->is_vertical();

    int per_child;

    if (vertical)
    {
        // Keep window buttons square on a vertical panel.
        per_child = width;
    } else
    {
        per_child = width / child_count;

        // user preference is ignored if too small
        int preference = std::max(height, user_size.value());

        // At minimum use ratio of 1:1, at max use user preference
        per_child = std::max(per_child, height);
        per_child = std::min(per_child, preference);
    }

    int index  = 0;
    auto alloc = Gtk::Allocation();

    if (vertical)
    {
        alloc.set_width(width);
        alloc.set_height(per_child);
        alloc.set_x(0);
    } else
    {
        alloc.set_height(height);
        alloc.set_width(per_child);
        alloc.set_y(0);
    }

    for (auto child : widget_not_const.get_children())
    {
        if (child == top_widget)
        {
            if (vertical)
            {
                alloc.set_y(top_position);
            } else
            {
                alloc.set_x(top_position);
            }

            child->size_allocate(alloc, -1);
            index++;
            continue;
        }

        if (vertical)
        {
            alloc.set_y(per_child * index);
        } else
        {
            alloc.set_x(per_child * index);
        }

        child->size_allocate(alloc, -1);
        index++;
    }

    for (auto& t : window_list->toplevels)
    {
        if (t.second)
        {
            t.second->send_rectangle_hint();
        }
    }
}

void WayfireWindowListLayout::measure_vfunc(const Gtk::Widget& widget,
    Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
    int& minimum_baseline, int& natural_baseline) const
{
    int child_count = widget.get_children().size();

    if (window_list->is_vertical())
    {
        if (orientation == Gtk::Orientation::VERTICAL)
        {
            int per_child = std::max(for_size, 1);

            minimum = child_count * per_child;
            natural = minimum;
        } else
        {
            minimum = 1;
            natural = 1;
        }
    } else
    {
        if (orientation == Gtk::Orientation::HORIZONTAL)
        {
            // Use max of user preference and ratio 1:1
            int per_child = std::max(for_size, user_size.value());

            minimum = child_count * for_size;
            natural = per_child * child_count;
        } else
        {
            minimum = 1;
            natural = 1;
        }
    }

    minimum_baseline = -1;
    natural_baseline = -1;
}

Gtk::SizeRequestMode WayfireWindowListLayout::get_request_mode_vfunc(
    const Gtk::Widget& widget) const
{
    if (window_list->is_vertical())
    {
        return Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH;
    }

    return Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT;
}
