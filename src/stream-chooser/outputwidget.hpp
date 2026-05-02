#pragma once
#include "gtkmm/picture.h"
#include <gtkmm.h>
#include <gdkmm.h>

class WayfireChooserOutput : public Gtk::Box
{
    Gtk::Label connector, model;
    Gtk::Picture contents;

    std::shared_ptr<Gdk::Monitor> output;

  public:
    void print();
    WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output);
};
