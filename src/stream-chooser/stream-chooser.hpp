#pragma once
#include <gtkmm.h>
#include <gdkmm.h>
#include <ext-foreign-toplevel-list-v1-client-protocol.h>
#include <ext-image-capture-source-v1-client-protocol.h>
#include <ext-image-copy-capture-v1-client-protocol.h>

#include "mainlayout.hpp"
#include "outputwidget.hpp"
#include "toplevelwidget.hpp"

using type_signal_resize = sigc::signal<void (int, int)>;


class WayfireStreamChooserApp : public Gtk::Application
{
  private:
    Gtk::Window window;

    Gtk::Box main, buttons;

    Gtk::Label window_label, screen_label, header;

    Gtk::Button done, cancel;
    Gtk::ScrolledWindow scroll_window, scroll_screen;

    wl_registry *registry;
    ext_foreign_toplevel_list_v1 *list;
    Glib::RefPtr<MainLayout> layout;

    type_signal_resize resized_signal;

  public:
    wl_display *display;
    Gtk::Notebook notebook;
    Gtk::FlowBox window_list, screen_list;
    bool has_foreign_toplevel_list = false;
    bool has_image_copy_capture    = false;
    bool has_image_capture_source  = false;
    std::string drm_device_name;
    int drm_fd = -1;
    gbm_device *gbm_device_ptr = nullptr;

    zwp_linux_dmabuf_v1 *dmabuf;
    ext_image_copy_capture_manager_v1 *manager;
    ext_foreign_toplevel_image_capture_source_manager_v1 *toplevel_capture_manager;
    ext_output_image_capture_source_manager_v1 *output_capture_manager;

    std::map<ext_foreign_toplevel_handle_v1*, std::unique_ptr<WayfireChooserTopLevel>> toplevels;
    std::map<std::string, std::unique_ptr<WayfireChooserOutput>> outputs;

    static WayfireStreamChooserApp& getInstance()
    {
        static WayfireStreamChooserApp instance;
        return instance;
    }

    void set_linux_dmabuf(zwp_linux_dmabuf_v1 *dmabuf);
    void set_toplevel_list(ext_foreign_toplevel_list_v1 *list);
    void set_copy_capture_manager(ext_image_copy_capture_manager_v1 *manager);
    void set_toplevel_capture_manager(
        ext_foreign_toplevel_image_capture_source_manager_v1 *toplevel_capture_manager);
    void set_output_capture_manager(ext_output_image_capture_source_manager_v1 *output_capture_manager);
    void add_toplevel(ext_foreign_toplevel_handle_v1 *handle);
    void remove_toplevel(WayfireChooserTopLevel *widget);

    void add_output(std::shared_ptr<Gdk::Monitor> monitor);
    void remove_output(std::string connector);
    void activate();
    WayfireStreamChooserApp();
    ~WayfireStreamChooserApp()
    {
        toplevels.clear();
        outputs.clear();
        if (gbm_device_ptr)
        {
            gbm_device_destroy(gbm_device_ptr);
        }

        if (drm_fd > 0)
        {
            close(drm_fd);
        }
    }

    type_signal_resize signal_resize();
    void resize(int width, int height);
};
