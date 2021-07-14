#ifndef CUSTOMIZABLES_HPP
#define CUSTOMIZABLES_HPP

#include <vector>
#include <giomm/desktopappinfo.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/hvbox.h>
#include "../widget.hpp"

struct CustomizableInfo
{
    std::string icon;
    std::string label;
    std::string cmd_btn_1;
    std::string cmd_btn_2;
    std::string cmd_btn_3;
    std::string cmd_scr_up;
    std::string cmd_scr_dn;
    std::string cmd_tooltip;

    bool load(std::string icon,
              std::string label,
              std::string cmd_btn_1,
              std::string cmd_btn_2,
              std::string cmd_btn_3,
              std::string cmd_scr_up,
              std::string cmd_scr_dn,
              std::string cmd_tooltip);
    Glib::RefPtr<Gdk::Pixbuf> get_pixbuf(int32_t size);
    void execute(std::string command);
    void execute(guint button);
    void execute(GdkScrollDirection direction);
    void execute(std::string command, std::string *output);
    std::string get_text();
    CustomizableInfo();
    ~CustomizableInfo();
};

struct WfCustomizableButton
{
    int icon_size;
    bool icon_invert;
    std::string customizable_name;
    Gtk::Image image;
    Gtk::Button button;
    CustomizableInfo *info = NULL;

    WfCustomizableButton();
    WfCustomizableButton(const WfCustomizableButton& other) = delete;
    WfCustomizableButton& operator = (const WfCustomizableButton&) = delete;
    ~WfCustomizableButton();

    bool initialize(std::string name,
                    std::string icon,
                    std::string label,
                    std::string cmd_btn_1,
                    std::string cmd_btn_2,
                    std::string cmd_btn_3,
                    std::string cmd_scr_up,
                    std::string cmd_scr_dn,
                    std::string cmd_tooltip);
    bool on_click(GdkEventButton *ev);
    bool on_scroll(GdkEventScroll *ev);
    bool on_enter(GdkEventCrossing* ev);
    bool on_leave(GdkEventCrossing* ev);
    void on_scale();
};

using customizable_container = std::vector<std::unique_ptr<WfCustomizableButton>>;
class WayfireCustomizables: public WayfireWidget
{
    Gtk::HBox box;
    customizable_container customizables;
    customizable_container get_customizables_from_config();

public:
    virtual void init(Gtk::HBox *container);
    virtual void handle_config_reload();
    virtual ~WayfireCustomizables() {};
};

#endif /* end of include guard: CUSTOMIZABLES_HPP */
