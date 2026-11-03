#include "toplevel.hpp"
#include "window-list.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
struct button_width_t
{
    int minimum;
    int natural;
};

button_width_t measure_button(const Gtk::Widget& child, int height, int maximum_width)
{
    int minimum, natural, minimum_baseline, natural_baseline;
    child.measure(Gtk::Orientation::HORIZONTAL, height,
        minimum, natural, minimum_baseline, natural_baseline);

    // Keep the existing square minimum and respect the button's CSS minimum.
    minimum = std::max(minimum, std::max(height, 1));
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
    std::vector<button_width_t> widths;
    int minimum_total = 0;
    int natural_total = 0;

    if (!vertical)
    {
        for (auto child : children)
        {
            auto measured = measure_button(*child, height, user_size.value());
            widths.push_back(measured);
            minimum_total += measured.minimum;
            natural_total += measured.natural;
        }
    }

    const int extra_total     = natural_total - minimum_total;
    const int available_extra = std::max(0,
        std::min(width - minimum_total, extra_total));
    int cumulative_extra = 0;
    int allocated_extra  = 0;
    int offset   = 0;
    size_t index = 0;

    for (auto child : children)
    {
        int size = width;
        if (!vertical)
        {
            const auto& measured = widths[index];
            cumulative_extra += measured.natural - measured.minimum;
            // Share limited space in proportion to each button's content.
            // Cumulative rounding ensures no pixels are lost between buttons.
            int next_extra = extra_total > 0 ?
                static_cast<int>(static_cast<int64_t>(available_extra) *
                    cumulative_extra / extra_total) : 0;
            size = measured.minimum + next_extra - allocated_extra;
            allocated_extra = next_extra;
        }

        int position = offset;
        if (child == top_widget)
        {
            const int extent = vertical ? height : width;
            position = std::max(0, std::min(top_position, extent - size));
        }

        auto alloc = Gtk::Allocation();
        if (vertical)
        {
            alloc.set_width(width);
            alloc.set_height(size);
            alloc.set_x(0);
            alloc.set_y(position);
        } else
        {
            alloc.set_width(size);
            alloc.set_height(height);
            alloc.set_x(position);
            alloc.set_y(0);
        }

        child->size_allocate(alloc, -1);
        // Reserve the dragged button's own width in its logical position.
        offset += size;
        ++index;
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
    if (window_list->is_vertical())
    {
        if (orientation == Gtk::Orientation::VERTICAL)
        {
            int per_child = std::max(for_size, 1);
            minimum = widget.get_children().size() * per_child;
            natural = minimum;
        } else
        {
            minimum = 1;
            natural = 1;
        }
    } else if (orientation == Gtk::Orientation::HORIZONTAL)
    {
        minimum = 0;
        natural = 0;
        for (auto child : widget.get_children())
        {
            auto measured = measure_button(*child, for_size, user_size.value());
            minimum += measured.minimum;
            natural += measured.natural;
        }
    } else
    {
        minimum = 1;
        natural = 1;
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
