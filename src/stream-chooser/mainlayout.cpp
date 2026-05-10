#include "mainlayout.hpp"
void MainLayout::allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline)
{
    Gtk::Widget& widget_not_const = const_cast<Gtk::Widget&>(widget);

    auto inner = widget_not_const.get_children()[0];
    auto alloc = Gtk::Allocation();
    alloc.set_height(height / 2);
    alloc.set_width(width / 2);
    alloc.set_y(height / 4);
    alloc.set_x(width / 4);
    inner->size_allocate(alloc, -1);
}

void MainLayout::measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation,
    int for_size, int& minimum, int& natural, int& minimum_baseline,
    int& natural_baseline) const
{
    // Answer 1:1 aspect ratio
    minimum = std::min(for_size, 300);
    natural = std::min(for_size, 2000);
    minimum_baseline = -1;
    natural_baseline = -1;
}

Gtk::SizeRequestMode MainLayout::get_request_mode_vfunc(const Gtk::Widget& widget) const
{
    return Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT;
}
