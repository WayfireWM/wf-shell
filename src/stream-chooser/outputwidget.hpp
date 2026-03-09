#pragma once
#include <gtkmm.h>
#include <gdkmm.h>

class WayfireChooserOutput : public Gtk::Box
{
    Gtk::Label connector, model;
    Gtk::Image contents;

    std::shared_ptr<Gdk::Monitor> output;

  public:
    void print();
    WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output);
};
