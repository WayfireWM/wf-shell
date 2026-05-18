#ifndef WF_PANEL_HPP
#define WF_PANEL_HPP

#include <memory>
#include <wayland-client.h>
#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>

#include "wf-shell-app.hpp"
#include "wf-ipc.hpp"

class WayfirePanelApp;
class WayfirePanel
{
  public:
    WayfirePanel(WayfireOutput *output);

    wl_surface *get_wl_surface();
    Gtk::Window& get_window();
    void handle_config_reload();
    void init_widgets();
    void init_layout();
    void unhide_now();
    void set_panel_app(WayfirePanelApp *panel_app);
    std::shared_ptr<WayfireIPC> get_ipc_server_instance();

  private:
    class impl;
    std::unique_ptr<impl> pimpl;
};

class WayfirePanelApp : public WayfireShellApp
{
  public:
    WayfirePanel *panel_for_wl_output(wl_output *output);
    static WayfirePanelApp& get();

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to create() */
    static void create(int argc, char **argv);
    ~WayfirePanelApp();

    void on_activate() override;
    void handle_new_output(WayfireOutput *output) override;
    void handle_output_removed(WayfireOutput *output) override;
    void on_config_reload() override;
    void on_css_reload() override;
    void unhide_now();
    std::shared_ptr<WayfireIPC> get_ipc_server_instance();
    std::shared_ptr<WayfireIPC> ipc_server = nullptr;    

  private:
    WayfirePanelApp(int argc, char **argv);

    void clear_css_rules();
    void add_css_file(std::string file, int priority);

    class impl;
    std::vector<Glib::RefPtr<Gtk::CssProvider>> css_rules;
    std::unique_ptr<impl> priv;
};

#endif /* end of include guard: WF_PANEL_HPP */
