#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::Box *container)
{
    icons_box.add_css_class("tray");
    update_layout();
    icons_box.set_halign(Gtk::Align::FILL);
    icons_box.set_valign(Gtk::Align::FILL);
    icons_box.set_expand(true);
    icons_box.set_homogeneous(true);
    container->append(icons_box);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_box.append(items.at(service));
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    if (items.count(service) == 0)
    {
        return;
    }

    icons_box.remove(items.at(service));
    items.erase(service);
}

void WayfireStatusNotifier::update_layout()
{
    icons_box.set_column_spacing(spacing);
    icons_box.set_row_spacing(spacing);
    icons_box.set_max_children_per_line(rows_cols);
    icons_box.set_min_children_per_line(rows_cols);
    icons_box.set_selection_mode(Gtk::SelectionMode::NONE);

    WfOption<std::string> panel_position{"panel/position"};

    if ((panel_position.value() == PANEL_POSITION_LEFT) || (panel_position.value() == PANEL_POSITION_RIGHT))
    {
        icons_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    } else
    {
        icons_box.set_orientation(Gtk::Orientation::VERTICAL);
    }
}

void WayfireStatusNotifier::handle_config_reload()
{
    update_layout();
}
