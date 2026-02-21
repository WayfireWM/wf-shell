#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/window.h>
#include <wf-shell-app.hpp>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>
#include <epoxy/gl.h>

#include "background-gl.hpp"
#include "sigc++/connection.h"


class WayfireBackground : public Gtk::Window
{
  private:
    void setup_window();
    WayfireOutput *output;

  public:
    WayfireBackground(WayfireOutput *output);

    ~WayfireBackground();
    Glib::RefPtr<BackgroundGLArea> gl_area;
};

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground>> backgrounds;
    std::string cache_file = "", current_background;
    sigc::connection change_bg_conn;

  public:
    using WayfireShellApp::WayfireShellApp;
    void on_activate() override;
    static void create(int argc, char **argv);

    void handle_new_output(WayfireOutput *output) override;
    void handle_output_removed(WayfireOutput *output) override;
    std::string get_application_name() override;
    Gio::Application::Flags get_extra_application_flags() override
    {
        return Gio::Application::Flags::NON_UNIQUE;
    }

    std::vector<std::string> get_background_list(std::string path);
    void change_background();
    static gboolean sigusr1_handler(void *instance);
    void write_cache(std::string path);
    void reset_cycle_timeout();
    void prep_cache();
};
