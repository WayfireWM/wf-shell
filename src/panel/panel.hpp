#pragma once

#include <memory>
#include <wayland-client.h>
#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>

#include "giomm/application.h"
#include "css-config.hpp"
#include "wf-shell-app.hpp"

class WayfirePanel
{
  public:
    WayfirePanel(WayfireOutput *output);

    wl_surface *get_wl_surface();
    Gtk::Window& get_window();
    void handle_config_reload();

  private:
    class impl;
    std::unique_ptr<impl> pimpl;
};

class WayfirePanelApp : public WayfireShellApp
{
  public:
    WayfirePanel *panel_for_wl_output(wl_output *output);
    static WayfirePanelApp& get();
    std::string get_application_name() override;
    Gio::Application::Flags get_extra_application_flags() override;

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to create() */
    static void create(int argc, char **argv);
    ~WayfirePanelApp();

    void on_activate() override;
    void handle_new_output(WayfireOutput *output) override;
    void handle_output_removed(WayfireOutput *output) override;
    void on_config_reload() override;
    void reload_css();

  private:
    WayfirePanelApp();

    class impl;
    std::unique_ptr<impl> priv;
};
