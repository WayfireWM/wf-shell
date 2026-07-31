#include "categorybutton.hpp"
#include "menu.hpp"

WfMenuCategoryButton::WfMenuCategoryButton(WayfireMenu *_menu, std::string _category, std::string _label,
    std::string _icon_name) :
    Gtk::Button(), menu(_menu), category(_category), label(_label), icon_name(_icon_name)
{
    m_image.set_from_icon_name(icon_name);
    m_image.set_pixel_size(32);
    m_label.set_text(label);
    m_label.set_xalign(0.0);
    m_label.add_css_class("default-icon");


    m_box.append(m_image);
    m_box.append(m_label);
    m_box.set_homogeneous(false);

    this->set_child(m_box);
    this->add_css_class("flat");
    this->add_css_class("app-category");

    sig_click = this->signal_clicked().connect(
        sigc::mem_fun(*this, &WfMenuCategoryButton::on_click));
}

WfMenuCategoryButton::~WfMenuCategoryButton()
{
    sig_click.disconnect();
}

void WfMenuCategoryButton::on_click()
{
    menu->set_category(category);
}
