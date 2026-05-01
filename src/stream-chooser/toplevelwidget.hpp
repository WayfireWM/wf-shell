#pragma once
#include <gtkmm.h>
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "glibmm/refptr.h"
#include "toplevellayout.hpp"

class WayfireChooserTopLevel : public Gtk::Box
{
  private:
    Gtk::Overlay overlay;
    Gtk::Image icon;
    Gtk::Picture screenshot;
    Gtk::Label label;

    std::string buffered_title = "", title = "";
    std::string buffered_app_id = "", app_id = "";
    std::string buffered_identifier = "", identifier = "";
    Glib::RefPtr<ToplevelLayout> layout;

  public:
    ext_foreign_toplevel_handle_v1 *handle;
    WayfireChooserTopLevel(ext_foreign_toplevel_handle_v1 *handle);
    ~WayfireChooserTopLevel();
    void commit();
    void set_app_id(std::string app_id);
    void set_title(std::string title);
    void set_identifier(std::string identifier);
    void print();
};
