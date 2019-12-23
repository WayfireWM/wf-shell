#include "spacing.hpp"

WayfireSpacing::WayfireSpacing(int pixels)
{
    box.set_size_request(pixels, 1);
}

void WayfireSpacing::init(Gtk::HBox *container)
{
    container->pack_start(box);
    box.show_all();
}
