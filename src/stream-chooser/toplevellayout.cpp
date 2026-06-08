#include "toplevellayout.hpp"
void ToplevelLayout::allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline)
{
    Gtk::Widget& widget_not_const = const_cast<Gtk::Widget&>(widget);

    auto overlay = widget_not_const.get_children()[0];
    auto label   = widget_not_const.get_children()[1];

    /* Label */
    auto label_height = std::max(16, label->get_height());
    auto l_alloc = Gtk::Allocation();
    l_alloc.set_height(label_height);
    l_alloc.set_width(width - 24);
    l_alloc.set_y(height - label_height);
    l_alloc.set_x(24);
    label->size_allocate(l_alloc, -1);

    /* Overlay */
    auto o_alloc = Gtk::Allocation();
    o_alloc.set_height(height);
    o_alloc.set_width(width);
    o_alloc.set_y(0);
    o_alloc.set_x(0);
    overlay->size_allocate(o_alloc, -1);
}

void ToplevelLayout::measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation,
    int for_size, int& minimum, int& natural, int& minimum_baseline,
    int& natural_baseline) const
{
    minimum = std::max(for_size, 32);
    natural = std::max(for_size, 32);
    minimum_baseline = -1;
    natural_baseline = -1;
}

Gtk::SizeRequestMode ToplevelLayout::get_request_mode_vfunc(const Gtk::Widget& widget) const
{
    return Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT;
}
