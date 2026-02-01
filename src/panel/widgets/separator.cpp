#include "separator.hpp"

WayfireSeparator::WayfireSeparator(int pixels)
{
    int half = pixels / 2;
    separator.set_margin_start(half);
    separator.set_margin_end(half);
}

void WayfireSeparator::init(Gtk::Box *container)
{
    separator.add_css_class("separator");
    container->append(separator);
}
