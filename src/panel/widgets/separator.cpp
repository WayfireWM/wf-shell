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

    update_layout();
}

void WayfireSeparator::update_layout()
{
    WfOption<std::string> panel_position{"panel/position"};

    if (panel_position.value() == PANEL_POSITION_LEFT or panel_position.value() == PANEL_POSITION_RIGHT)
    {
        separator.set_orientation(Gtk::Orientation::VERTICAL);
    } else
    {
        separator.set_orientation(Gtk::Orientation::HORIZONTAL);
    }
}

void WayfireSeparator::handle_config_reload()
{
    update_layout();
}
