#include <gtkmm.h>
#include "toplevel.hpp"
#include <wf-option-wrap.hpp>


class WayfireWindowList;

class WayfireWindowListLayout : public Gtk::LayoutManager
{
  protected:
    void allocate_vfunc(const Gtk::Widget& widget, int width, int height, int baseline) override;
    void measure_vfunc(const Gtk::Widget& widget, Gtk::Orientation orientation, int for_size, int& minimum,
        int& natural, int& minimum_baseline, int& natural_baseline) const override;
    Gtk::SizeRequestMode get_request_mode_vfunc(const Gtk::Widget& widget) const override;
    std::shared_ptr<Gtk::LayoutManager> layout;
    WfOption<int> user_size{"panel/window_list_size"};
    WayfireWindowList* window_list;

  public:
    WayfireWindowListLayout(WayfireWindowList* window_list);
    int top_x = 0;
    Gtk::Widget *top_widget = nullptr;
};
