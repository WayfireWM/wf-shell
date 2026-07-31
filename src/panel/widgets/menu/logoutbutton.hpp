#pragma once
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
class WayfireLogoutUIButton
{
  public:
    Gtk::Box layout;
    Gtk::Image image;
    Gtk::Label label;
    Gtk::Button button;
};
