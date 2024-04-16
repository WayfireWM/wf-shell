#include "separator.hpp"

WayfireSeparator::WayfireSeparator(int pixels)
{
    box.set_size_request(pixels, 1);
    box.set_app_paintable(true);
    box.signal_draw().connect(
        [this] (const Cairo::RefPtr<Cairo::Context> context) -> bool
    {
        auto style = box.get_style_context();
        auto color = style->get_color(Gtk::STATE_FLAG_NORMAL);

        auto width  = box.get_allocated_width();
        auto height = box.get_allocated_height();

        context->translate(width / 2, height / 8);
        context->set_line_width(1.0);
        context->move_to(0, 0);
        context->line_to(0, height - (height / 4));

        context->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
        context->stroke();
        return false;
    });
}

void WayfireSeparator::init(Gtk::HBox *container)
{
    container->pack_start(box);
    box.show_all();
}

