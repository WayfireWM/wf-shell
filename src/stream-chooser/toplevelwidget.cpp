#include <iostream>

#include "stream-chooser.hpp"
#include "toplevelwidget.hpp"

static void handle_closed(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    WayfireStreamChooserApp::getInstance().remove_toplevel(toplevel);

    /* TODO Clean up */
}

static void handle_done(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->commit();
}

static void handle_title(void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_title(title);
}

static void handle_app_id(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle1,
    const char *app_id)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_app_id(app_id);
}

static void handle_identifier(void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_identifier(identifier);
}

ext_foreign_toplevel_handle_v1_listener listener =
{
    .closed = handle_closed,
    .done   = handle_done,
    .title  = handle_title,
    .app_id = handle_app_id,
    .identifier = handle_identifier,
};

/* Gtk Overlay showing information about a window */
WayfireChooserTopLevel::WayfireChooserTopLevel(ext_foreign_toplevel_handle_v1 *handle)
{
    append(overlay);
    append(label);
    overlay.set_child(screenshot);
    overlay.add_overlay(icon);
    icon.set_halign(Gtk::Align::START);
    icon.set_valign(Gtk::Align::END);
    label.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    label.set_max_width_chars(40);

    ext_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

void WayfireChooserTopLevel::set_title(std::string title)
{
    buffered_title = title;
}

void WayfireChooserTopLevel::set_app_id(std::string app_id)
{
    buffered_app_id = app_id;
}

void WayfireChooserTopLevel::set_identifier(std::string identifier)
{
    buffered_identifier = identifier;
}

void WayfireChooserTopLevel::commit()
{
    if (buffered_app_id != "")
    {
        app_id = buffered_app_id;
        icon.set_from_icon_name(app_id);
        buffered_app_id = "";
    }

    if (buffered_title != "")
    {
        title = buffered_title;
        label.set_label(title);
        buffered_title = "";
    }

    if (buffered_identifier != "")
    {
        identifier = buffered_identifier;
        buffered_identifier = "";
    }
}

WayfireChooserTopLevel::~WayfireChooserTopLevel()
{}

void WayfireChooserTopLevel::print()
{
    std::cout << "Window: " << identifier << std::endl;
    exit(0);
}
