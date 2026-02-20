#include "spacing.hpp"

WayfireSpacing::WayfireSpacing(int pixels)
{
    box.set_size_request(pixels, 1);
}

void WayfireSpacing::init(Gtk::Box *container)
{
    box.add_css_class("spacing");
    container->append(box);
}
