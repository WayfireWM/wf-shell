#pragma once
#include <gtkmm.h>
#include <gdkmm.h>
#include <ext-foreign-toplevel-list-v1-client-protocol.h>

#include "outputwidget.hpp"
#include "toplevelwidget.hpp"


class WayfireStreamChooserApp : public Gtk::Application
{
  private:
    Gtk::Window window;
    Gtk::Notebook notebook;

    Gtk::Box main, buttons;

    Gtk::Label window_label, screen_label, header;

    Gtk::FlowBox window_list, screen_list;
    Gtk::Button done, cancel;
    WayfireStreamChooserApp();

    wl_display *display;
    wl_registry *registry;
    ext_foreign_toplevel_list_v1 *list;

  public:
    bool has_image_copy_capture = false;

    std::map<ext_foreign_toplevel_handle_v1*, std::unique_ptr<WayfireChooserTopLevel>> toplevels;
    std::map<std::string, std::unique_ptr<WayfireChooserOutput>> outputs;

    static WayfireStreamChooserApp& getInstance()
    {
        static WayfireStreamChooserApp instance;
        return instance;
    }

    void set_toplevel_list(ext_foreign_toplevel_list_v1 *list);
    void add_toplevel(ext_foreign_toplevel_handle_v1 *handle);
    void remove_toplevel(WayfireChooserTopLevel *widget);

    void add_output(std::shared_ptr<Gdk::Monitor> monitor);
    void remove_output(std::string connector);
    void activate();
    WayfireStreamChooserApp(WayfireStreamChooserApp const&) = delete;
    void operator =(WayfireStreamChooserApp const&) = delete;
};
