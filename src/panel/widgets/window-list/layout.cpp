#include "gtkmm/enums.h"
#include "toplevel.hpp"
#include "window-list.hpp"

#include <algorithm>

namespace
{
struct button_width_t
{
    int minimum;
    int natural;
};

button_width_t measure_button(const Gtk::Widget& child, Gtk::Orientation orientation, int forsize,
    int maximum_width)
{
    int minimum, natural, minimum_baseline, natural_baseline;
    child.measure(orientation, forsize,
        minimum, natural, minimum_baseline, natural_baseline);

    // Keep the existing square minimum and respect the button's CSS minimum.
    minimum = std::max(minimum, std::max(forsize, 1));
    natural = std::max(minimum, std::min(natural, maximum_width));
    return {minimum, natural};
}
}

WayfireWindowListLayout::WayfireWindowListLayout(WayfireWindowList *window_list)
{
    this->window_list = window_list;
}

void WayfireWindowListLayout::allocate_vfunc(const Gtk::Widget& widget,
    int width, int height, int baseline)
{
    Gtk::Widget& widget_not_const = const_cast<Gtk::Widget&>(widget);
    auto children = widget_not_const.get_children();
    if (children.empty())
    {
        return;
    }

    const bool vertical = window_list->is_vertical();

    // TODO Come back some day with rotated text labels as a vertical option
    bool no_labels = window_list->hide_label.value() || vertical;

    // width & height need to switch based on orientation
    int longside  = vertical ? height : width;
    int shortside = vertical ? width : height;

    int per_child = 1;

    if (no_labels)
    {
        per_child = shortside;
    } else
    {
        per_child = longside / children.size();
        int preference = std::max(shortside, user_size.value());
        // Can't be larger than user preference
        per_child = std::min(per_child, preference);
        // Can't be smaller than square
        per_child = std::max(per_child, shortside);
    }

    // Allocate locations and sizes
    int offset = 0;
    for (auto child : children)
    {
        auto alloc = Gtk::Allocation();

        if (vertical)
        {
            alloc.set_width(shortside);
            alloc.set_height(per_child);
            alloc.set_x(0);
            alloc.set_y(offset);
        } else
        {
            alloc.set_width(per_child);
            alloc.set_height(shortside);
            alloc.set_x(offset);
            alloc.set_y(0);
        }

        child->size_allocate(alloc, -1);
        offset += per_child;
    }

    // Update hints
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
    const bool vertical = window_list->is_vertical();

    // TODO Come back some day with rotated text labels as a vertical option
    bool no_labels = window_list->hide_label || vertical;

    // Easy mode, labels are always square, so just count
    if (no_labels)
    {
        if ((vertical && (orientation == Gtk::Orientation::VERTICAL)) ||
            (!vertical && (orientation == Gtk::Orientation::HORIZONTAL)))
        {
            // Measure long side, widget count * icon size
            int per_child = std::max(for_size, 1);
            minimum = widget.get_children().size() * per_child;
            natural = minimum;
        } else
        {
            // No specific size required
            minimum = 1;
            natural = 1;
        }
    } else
    {
        if ((vertical && (orientation == Gtk::Orientation::VERTICAL)) ||
            (!vertical && (orientation == Gtk::Orientation::HORIZONTAL)))
        {
            // When measuring the long side, count up what every widget wants
            // This is only a suggestion that bubbles up
            minimum = 0;
            natural = 0;
            for (auto child : widget.get_children())
            {
                auto measured = measure_button(*child, orientation, for_size, user_size.value());
                minimum += measured.minimum;
                natural += measured.natural;
            }
        } else
        {
            // When measuring the short side, don't suggest a size at all
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
    return (window_list->is_vertical() ?
        Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH :
        Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT);
}
