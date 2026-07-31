#pragma once
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

class WayfireMenu;

class WfMenuCategoryButton : public Gtk::Button
{
  public:
    WfMenuCategoryButton(WayfireMenu *menu, std::string category, std::string label, std::string icon_name);
    ~WfMenuCategoryButton();

  private:
    WayfireMenu *menu;
    Gtk::Box m_box;
    Gtk::Label m_label;
    Gtk::Image m_image;
    sigc::connection sig_click;

    std::string category;
    std::string label;
    std::string icon_name;
    void on_click();
};
