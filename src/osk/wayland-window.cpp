#include "wayland-window.hpp"
#include "gtkmm/enums.h"
#include "osk.hpp"
#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <gtkmm.h>
#include <gdk/wayland/gdkwayland.h>
#include <wayland-client.h>
#include <gtk4-layer-shell.h>
#include <gtkmm/headerbar.h>

#include <gdkmm/display.h>
#include <gdkmm/seat.h>
#include "display.hpp"


int32_t WaylandWindow::check_anchor(std::string anchor)
{
    std::transform(anchor.begin(), anchor.end(), anchor.begin(), ::tolower);

    int32_t parsed_anchor = -1;
    if (anchor.compare("top") == 0)
    {
        parsed_anchor = GTK_LAYER_SHELL_EDGE_TOP;
    } else if (anchor.compare("bottom") == 0)
    {
        parsed_anchor = GTK_LAYER_SHELL_EDGE_BOTTOM;
    } else if (anchor.compare("left") == 0)
    {
        parsed_anchor = GTK_LAYER_SHELL_EDGE_LEFT;
    } else if (anchor.compare("right") == 0)
    {
        parsed_anchor = GTK_LAYER_SHELL_EDGE_RIGHT;
    }

    return parsed_anchor;
}

void WaylandWindow::init(int width, int height, std::string anchor)
{
    gtk_layer_init_for_window(this->gobj());
    gtk_layer_set_layer(this->gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(this->gobj(), "keyboard");
    gtk_layer_set_anchor(this->gobj(),
        GTK_LAYER_SHELL_EDGE_BOTTOM, false);
    gtk_layer_set_anchor(this->gobj(),
        GTK_LAYER_SHELL_EDGE_TOP, false);
    gtk_layer_set_anchor(this->gobj(),
        GTK_LAYER_SHELL_EDGE_RIGHT, false);
    gtk_layer_set_anchor(this->gobj(),
        GTK_LAYER_SHELL_EDGE_LEFT, false);
    auto layer_anchor = check_anchor(anchor);
    if (layer_anchor > -1)
    {
        gtk_layer_set_anchor(this->gobj(),
            (GtkLayerShellEdge)layer_anchor, true);
    }

    if (exclusion)
    {
        gtk_layer_auto_exclusive_zone_enable(this->gobj());
    } else
    {
        gtk_layer_set_exclusive_zone(this->gobj(), -1);
    }

    this->set_size_request(width, height);
    this->show();
    auto gdk_window = this->get_surface()->gobj();
    auto surface    = gdk_wayland_surface_get_wl_surface(gdk_window);

    if (surface && WaylandDisplay::get().zwf_manager)
    {
        this->wf_surface = zwf_shell_manager_v2_get_wf_surface(
            WaylandDisplay::get().zwf_manager, surface);
    }
}

void WaylandWindow::init_headerbar(int headerbar_size)
{
    std::vector<Gtk::Button*> buttons = {
        &top_button, &bottom_button, &close_button
    };

    const int button_size = 0.8 * headerbar_size;
    for (auto& button : buttons)
    {
        button->get_style_context()->add_class("image-button");
        button->set_size_request(button_size, button_size);
        button->set_margin_bottom(OSK_SPACING);
        button->set_margin_top(OSK_SPACING);
        button->set_margin_start(OSK_SPACING);
        button->set_margin_end(OSK_SPACING);
    }

    close_button.set_image_from_icon_name("window-close-symbolic");
    signals.push_back(close_button.signal_clicked().connect([=] ()
    {
        std::exit(0);
    }, true));

    top_button.set_image_from_icon_name("pan-up-symbolic");
    signals.push_back(top_button.signal_clicked().connect([=] ()
    {
        gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, false);
    }, true));

    bottom_button.set_image_from_icon_name("pan-down-symbolic");
    signals.push_back(bottom_button.signal_clicked().connect([=] ()
    {
        gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_TOP, false);
        gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    }, true));

    layout_select.set_icon_name("input-keyboard-symbolic");

    /* setup headerbar layout */
    headerbar_box.set_size_request(-1, headerbar_size);
    headerbar_box.append(top_button);
    headerbar_box.append(bottom_button);
    headerbar_box.append(suggestions);
    headerbar_box.append(layout_select);
    headerbar_box.append(close_button);

    /* Init layout options */
    std::filesystem::path build_dir = LAYOUT_DIR;
    auto action_group  = Gio::SimpleActionGroup::create();
    auto select_action = Gio::SimpleAction::create("select_file", Glib::VARIANT_TYPE_STRING);
    select_action->signal_activate().connect([=] (const Glib::VariantBase& parameter)
    {
        if (parameter && parameter.is_of_type(Glib::VARIANT_TYPE_STRING))
        {
            auto path_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(parameter);
            Glib::ustring base_name = path_variant.get();
            WayfireOsk::get().user_selected_layout(base_name);
        }
    });
    action_group->add_action(select_action);
    insert_action_group("ui_menu", action_group);

    auto menu_model = Gio::Menu::create();

    auto menu_item = Gio::MenuItem::create("Automatic", "ui_menu.select_file");

    menu_item->set_action_and_target("ui_menu.select_file", Glib::Variant<Glib::ustring>::create(
        ""));

    menu_model->append_item(menu_item);

    if (std::filesystem::exists(build_dir) && std::filesystem::is_directory(build_dir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(build_dir))
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".xml"))
            {
                Glib::ustring base_name = entry.path().stem().string();

                auto menu_item = Gio::MenuItem::create(base_name, "ui_menu.select_file");

                menu_item->set_action_and_target("ui_menu.select_file", Glib::Variant<Glib::ustring>::create(
                    base_name));

                menu_model->append_item(menu_item);
            }
        }
    }

    layout_select.set_menu_model(menu_model);
    layout_select.get_popover()->set_autohide(false);

    suggestions.set_hexpand(true);

    layout_box.set_orientation(Gtk::Orientation::VERTICAL);
    layout_box.append(headerbar_box);
    layout_box.set_spacing(OSK_SPACING);
    this->set_child(layout_box);
}

WaylandWindow::WaylandWindow(int width, int height, std::string anchor, int headerbar_size) :
    Gtk::Window()
{
    init_headerbar(headerbar_size);
    /* setup gtk layer shell */
    init(width, height, anchor);
}

WaylandWindow::~WaylandWindow()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WaylandWindow::set_widget(Gtk::Widget& w)
{
    if (current_widget)
    {
        this->layout_box.remove(*current_widget);
    }

    this->layout_box.append(w);
    current_widget = &w;

    w.set_margin_bottom(OSK_SPACING);
    w.set_margin_start(OSK_SPACING);
    w.set_margin_end(OSK_SPACING);
    this->show();
}

void WaylandWindow::clear_suggestions()
{
    for (auto child : suggestions.get_children())
    {
        suggestions.remove(*child);
    }
}

void WaylandWindow::set_suggestions(std::vector<std::string> all, uint64_t seq_id)
{
    if (seq_id < last_suggestion)
    {
        return;
    }

    clear_suggestions();
    for (auto sugg : all)
    {
        auto button = new Gtk::Button();
        button->set_label(sugg);
        button->signal_clicked().connect([=] ()
        {
            WayfireOsk::get().get_device().accept_suggestion(sugg);
        });
        suggestions.append(*button);
    }
}
