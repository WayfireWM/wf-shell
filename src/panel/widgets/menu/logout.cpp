#include <gtk4-layer-shell.h>

#include "logout.hpp"

void WayfireLogoutUI::on_logout_click()
{
    ui.hide();
    g_spawn_command_line_async(logout_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_reboot_click()
{
    ui.hide();
    g_spawn_command_line_async(reboot_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_shutdown_click()
{
    ui.hide();
    g_spawn_command_line_async(shutdown_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_suspend_click()
{
    ui.hide();
    g_spawn_command_line_async(suspend_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_hibernate_click()
{
    ui.hide();
    g_spawn_command_line_async(hibernate_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_switchuser_click()
{
    ui.hide();
    g_spawn_command_line_async(switchuser_command.value().c_str(), NULL);
}

void WayfireLogoutUI::on_cancel_click()
{
    ui.hide();
}

#define LOGOUT_BUTTON_SIZE  125
#define LOGOUT_BUTTON_MARGIN 10

void WayfireLogoutUI::create_logout_ui_button(WayfireLogoutUIButton *button, const char *icon,
    const char *label)
{
    button->button.set_size_request(LOGOUT_BUTTON_SIZE, LOGOUT_BUTTON_SIZE);
    button->image.set_from_icon_name(icon);
    button->label.set_text(label);
    button->layout.set_orientation(Gtk::Orientation::VERTICAL);
    button->layout.set_halign(Gtk::Align::CENTER);
    button->layout.append(button->image);
    button->image.set_icon_size(Gtk::IconSize::LARGE);
    button->image.set_vexpand(true);
    button->layout.append(button->label);
    button->button.set_child(button->layout);
}

WayfireLogoutUI::WayfireLogoutUI()
{
    create_logout_ui_button(&suspend, "emblem-synchronizing", "Suspend");
    signals.push_back(suspend.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_suspend_click)));

    main_layout.attach(suspend.button, 0, 0, 1, 1);

    create_logout_ui_button(&hibernate, "weather-clear-night", "Hibernate");
    signals.push_back(hibernate.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_hibernate_click)));
    main_layout.attach(hibernate.button, 1, 0, 1, 1);

    create_logout_ui_button(&switchuser, "system-users", "Switch User");
    signals.push_back(switchuser.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_switchuser_click)));
    main_layout.attach(switchuser.button, 2, 0, 1, 1);

    create_logout_ui_button(&logout, "system-log-out", "Log Out");
    signals.push_back(logout.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_logout_click)));
    main_layout.attach(logout.button, 0, 1, 1, 1);

    create_logout_ui_button(&reboot, "system-reboot", "Reboot");
    signals.push_back(reboot.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_reboot_click)));
    main_layout.attach(reboot.button, 1, 1, 1, 1);

    create_logout_ui_button(&shutdown, "system-shutdown", "Shut Down");
    signals.push_back(shutdown.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_shutdown_click)));
    main_layout.attach(shutdown.button, 2, 1, 1, 1);

    cancel.button.set_size_request(100, 50);
    cancel.button.set_label("Cancel");
    main_layout.attach(cancel.button, 1, 2, 1, 1);
    signals.push_back(cancel.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_cancel_click)));

    main_layout.set_row_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.set_column_spacing(LOGOUT_BUTTON_MARGIN);
    /* Make surfaces layer shell */
    gtk_layer_init_for_window(ui.gobj());
    gtk_layer_set_layer(ui.gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);

    gtk_layer_set_anchor(ui.gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(ui.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(ui.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(ui.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    main_layout.set_valign(Gtk::Align::CENTER);
    box.set_center_widget(main_layout);
    box.set_hexpand(true);
    box.set_vexpand(true);
    ui.set_child(box);
    ui.add_css_class("logout");
    auto display = ui.get_display();
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data("window.logout { background-color: rgba(0, 0, 0, 0.5); }");
    Gtk::StyleContext::add_provider_for_display(display,
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
}

WayfireLogoutUI::~WayfireLogoutUI()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
