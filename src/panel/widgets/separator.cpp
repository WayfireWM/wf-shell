#include "separator.hpp"

WayfireSeparator::WayfireSeparator(int pixels)
{
    int half = pixels / 2;
    separator.set_margin_start(half);
    separator.set_margin_end(half);
}

void WayfireSeparator::init(Gtk::HBox *container)
{
    separator.get_style_context()->add_class("separator");
    container->pack_start(separator, Gtk::PACK_SHRINK);
    separator.show_all();
}
