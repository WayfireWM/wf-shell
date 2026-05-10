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
    l_alloc.set_width(width);
    l_alloc.set_y(0);
    l_alloc.set_x(0);
    label->size_allocate(l_alloc, -1);

    /* Overlay */
    auto o_alloc = Gtk::Allocation();
    o_alloc.set_height(height - label_height);
    o_alloc.set_width(width);
    o_alloc.set_y(label_height);
    o_alloc.set_x(0);
    overlay->size_allocate(o_alloc, -1);
}

void ToplevelLayout::measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation,
    int for_size, int& minimum, int& natural, int& minimum_baseline,
    int& natural_baseline) const
{
    // Answer 1:1 aspect ratio
    minimum = for_size;
    natural = for_size;
    minimum_baseline = -1;
    natural_baseline = -1;
}

Gtk::SizeRequestMode ToplevelLayout::get_request_mode_vfunc(const Gtk::Widget& widget) const
{
    return Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT;
}
