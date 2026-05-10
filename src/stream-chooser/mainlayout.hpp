#pragma once

#include <gtkmm.h>
#include <wf-option-wrap.hpp>


class MainLayout : public Gtk::LayoutManager
{
  protected:
    void allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline) override;
    void measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation, int for_size, int& minimum,
        int& natural, int& minimum_baseline, int& natural_baseline) const override;
    Gtk::SizeRequestMode get_request_mode_vfunc(const Gtk::Widget& widget) const override;
    std::shared_ptr<Gtk::LayoutManager> layout;

  public:
    MainLayout()
    {}
};
