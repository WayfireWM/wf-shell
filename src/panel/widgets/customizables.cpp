#include <giomm/file.h>
#include <glibmm/spawn.h>
#include <gdkmm/pixbuf.h>
#include <iostream>
#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include "customizables.hpp"

// create a customizable widget from label,icon and commands

bool CustomizableInfo::load(std::string icon,
                            std::string label,
                            std::string cmd_btn_1,
                            std::string cmd_btn_2,
                            std::string cmd_btn_3,
                            std::string cmd_scr_up,
                            std::string cmd_scr_dn,
                            std::string cmd_tooltip)
{
    this->icon = icon;
    this->label = label;
    this->cmd_btn_1 = cmd_btn_1;
    this->cmd_btn_2 = cmd_btn_2;
    this->cmd_btn_3 = cmd_btn_3;
    this->cmd_scr_up = cmd_scr_up;
    this->cmd_scr_dn = cmd_scr_dn;
    this->cmd_tooltip = cmd_tooltip;
    return load_icon_pixbuf_safe(icon, 24).get() != nullptr;
}

Glib::RefPtr<Gdk::Pixbuf> CustomizableInfo::get_pixbuf(int32_t size)
{
    return Gdk::Pixbuf::create_from_file(icon, size, size);
}

std::string CustomizableInfo::get_text()
{
    return label;
}

// Execute a shell command
void CustomizableInfo::execute(std::string command)
{
    if (command.length() != 0)
    {
        Glib::spawn_command_line_async("/bin/bash -c \'" + command + "\'");
    }
}

// Mouse-button command dispatch
void CustomizableInfo::execute(guint button)
{
    std::string command;
    switch(button)
    {
    case GDK_BUTTON_PRIMARY:
        command = cmd_btn_1;
        break;
    case GDK_BUTTON_MIDDLE:
        command = cmd_btn_2;
        break;
    case GDK_BUTTON_SECONDARY:
        command = cmd_btn_3;
        break;
    default:                    // …?
        command = "";
    }
    execute(command);
}

// Mouse-wheel command dispatch
void CustomizableInfo::execute(GdkScrollDirection direction)
{
    std::string command;
    switch(direction)
    {
    case GDK_SCROLL_UP:
        command = cmd_scr_up;
        break;
    case GDK_SCROLL_DOWN:
        command = cmd_scr_dn;
        break;
    default:            // GDK_SCROLL_LEFT, GDK_SCROLL_RIGHT, GDK_SCROLL_SMOOTH
        command = "";
        break;
    }
    execute(command);
}

// Execute a shell command and retrieve its output
void CustomizableInfo::execute(std::string command, std::string *output)
{
    std::string stdout;
    std::string stderr;
    int exit_status;
    if (command.length() != 0)
    {
        Glib::spawn_command_line_sync("/bin/bash -c \'" + command + "\'",
                                      &stdout,
                                      &stderr,
                                      &exit_status);
        if (exit_status == 0)
        {
            *output = stdout.substr(0, stdout.length() - 1);
        } else
        {
            *output = stderr.substr(0, stderr.length() - 1);
        }
    }
}

CustomizableInfo::CustomizableInfo() {}
CustomizableInfo::~CustomizableInfo() {}

bool WfCustomizableButton::initialize(std::string name,
                                      std::string icon,
                                      std::string label,
                                      std::string cmd_btn_1,
                                      std::string cmd_btn_2,
                                      std::string cmd_btn_3,
                                      std::string cmd_scr_up,
                                      std::string cmd_scr_dn,
                                      std::string cmd_tooltip)
{
    customizable_name = name;
    info = new CustomizableInfo();
    if (!info->load(icon, label, cmd_btn_1, cmd_btn_2, cmd_btn_3, cmd_scr_up, cmd_scr_dn, cmd_tooltip))
    {
        std::cerr << "Failed to load custom widget " << label << std::endl;
        return false;
    }
    button.add(image);
    button.set_events(Gdk::SCROLL_MASK | Gdk::BUTTON_PRESS_MASK); //  | Gdk::SMOOTH_SCROLL_MASK
    button.signal_button_press_event().connect(sigc::mem_fun(this, &WfCustomizableButton::on_click));
    button.signal_button_release_event().connect(sigc::mem_fun(this, &WfCustomizableButton::on_click));
    button.signal_scroll_event().connect(sigc::mem_fun(this, &WfCustomizableButton::on_scroll));
    button.signal_enter_notify_event().connect(sigc::mem_fun(this, &WfCustomizableButton::on_enter));
    // button.signal_leave_notify_event().connect(sigc::mem_fun(this, &WfCustomizableButton::on_leave));
    button.set_valign(Gtk::ALIGN_CENTER);
    button.get_style_context()->add_class("flat");
    image.set_valign(Gtk::ALIGN_CENTER);
    button.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(this, &WfCustomizableButton::on_scale));
    button.set_tooltip_text(info->get_text());
    on_scale();
    return true;
}

bool WfCustomizableButton::on_click(GdkEventButton *ev)
{
    if (ev->type == GDK_BUTTON_RELEASE)
    {
        info->execute(ev->button);
    }
    if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
    {
        /* touch will generate button_press, but not enter notify */
    }
    return true;
}

bool WfCustomizableButton::on_scroll(GdkEventScroll *ev)
{
    info->execute(ev->direction);
    return true;
}

bool WfCustomizableButton::on_enter(GdkEventCrossing* ev)
{
    std::string output;
    if (info->cmd_tooltip.length() > 0)
    {
        info->execute(info->cmd_tooltip, &output);
        button.set_tooltip_text(output);
    }
    return true;
}

bool WfCustomizableButton::on_leave(GdkEventCrossing* ev)
{
    return true;
}

/* Because icons can have different sizes, we need to use a Gdk:Pixbuf
 * to convert them to the appropriate size. However, Gdk::Pixbuf operates
 * in absolute pixel size, so this doesn't work nicely with scaled outputs.
 *
 * To get around the problem, we first create the Pixbuf with a scaled size,
 * then convert it to a cairo_surface with the appropriate scale, and use this
 * cairo surface as the source for the Gtk::Image */
void WfCustomizableButton::on_scale()
{
    int scale = image.get_scale_factor();

    // hold a reference to the RefPtr
    auto ptr_pbuff = info->get_pixbuf(icon_size * image.get_scale_factor());
    if (!ptr_pbuff)
    {
        return;
    }
    if (icon_invert)
    {
        invert_pixbuf(ptr_pbuff);
    }
    set_image_pixbuf(image, ptr_pbuff, scale);
}

WfCustomizableButton::WfCustomizableButton()
{
    /* I tried to set these as class variables (static),
       but got linking error…
    */
    icon_size = WfOption<int> {"panel/customizables_size"};
    icon_invert = WfOption<bool> {"panel/customizables_invert"};
}

WfCustomizableButton::~WfCustomizableButton()
{
    delete info;
}

static bool begins_with(const std::string& string, const std::string& prefix)
{
    return string.size() >= prefix.size() &&
        string.substr(0, prefix.size()) == prefix;
}

customizable_container WayfireCustomizables::get_customizables_from_config()
{
    auto section = WayfireShellApp::get().config.get_section("panel");
    const std::string custom_label_prefix = "custom_label_";
    const std::string custom_icon_prefix = "custom_icon_";
    const std::string custom_btn1_prefix = "custom_btn1_cmd_";
    const std::string custom_btn2_prefix = "custom_btn2_cmd_";
    const std::string custom_btn3_prefix = "custom_btn3_cmd_";
    const std::string custom_scr_up_prefix = "custom_scr_up_cmd_";
    const std::string custom_scr_dn_prefix = "custom_scr_dn_cmd_";
    const std::string custom_tooltip_cmd = "custom_tooltip_cmd_";

    customizable_container customizables;
    auto try_push_customizable = [&customizables] (const std::string name,
                                                   const std::string icon,
                                                   const std::string label,
                                                   const std::string cmd_btn_1,
                                                   const std::string cmd_btn_2,
                                                   const std::string cmd_btn_3,
                                                   const std::string cmd_scr_up,
                                                   const std::string cmd_scr_dn,
                                                   const std::string cmd_tooltip)
    {
        auto customizable = new WfCustomizableButton();
        if (customizable->initialize(name, icon, label, cmd_btn_1, cmd_btn_2, cmd_btn_3,
                                     cmd_scr_up, cmd_scr_dn, cmd_tooltip))
        {
            customizables.push_back(std::unique_ptr<WfCustomizableButton>(customizable));
        } else
        {
            delete customizable;
        }
    };
 
    /* This one needs a label
       custom_label_<name> =
       custom_icon_<name> =
       custom_btn_[1-3]_cmd_<name> =
       custom_scr_[up|dn]_cmd_<name> =
       custom_tooltip_cmd_<name> =
    */
    for (auto opt : section->get_registered_options())
    {
        /* we have a custom widget */
        if (begins_with(opt->get_name(), custom_label_prefix))
        {
            /* extract customizable name, i.e the string after the prefix */
            auto customizable_name = opt->get_name().substr(custom_label_prefix.size());
            auto label_option = section->get_option(custom_label_prefix + customizable_name);
            /* look for the corresponding icon… */
            auto icon_option = section->get_option_or(custom_icon_prefix + customizable_name);
            /* and corresponding commands. */
            auto cmd_btn_1_option = section->get_option_or(custom_btn1_prefix + customizable_name);
            auto cmd_btn_2_option = section->get_option_or(custom_btn2_prefix + customizable_name);
            auto cmd_btn_3_option = section->get_option_or(custom_btn3_prefix + customizable_name);
            auto cmd_scr_up_option = section->get_option_or(custom_scr_up_prefix + customizable_name);
            auto cmd_scr_dn_option = section->get_option_or(custom_scr_dn_prefix + customizable_name);
            auto cmd_tooltip_option = section->get_option_or(custom_tooltip_cmd + customizable_name);
            try_push_customizable(opt->get_value_str(),
                                  (icon_option)?icon_option->get_value_str():"",
                                  label_option->get_value_str(),
                                  (cmd_btn_1_option)?cmd_btn_1_option->get_value_str():"",
                                  (cmd_btn_2_option)?cmd_btn_2_option->get_value_str():"",
                                  (cmd_btn_3_option)?cmd_btn_3_option->get_value_str():"",
                                  (cmd_scr_up_option)?cmd_scr_up_option->get_value_str():"",
                                  (cmd_scr_dn_option)?cmd_scr_dn_option->get_value_str():"",
                                  (cmd_tooltip_option)?cmd_tooltip_option->get_value_str():"");
        }
    }
    return customizables;
}

void WayfireCustomizables::init(Gtk::HBox *container)
{
    container->pack_start(box, Gtk::PACK_SHRINK);   // false, false);
    handle_config_reload();
}

void WayfireCustomizables::handle_config_reload()
{
    box.set_spacing(WfOption<int> {"panel/customizables_spacing"});
    customizables = get_customizables_from_config();
    for (auto& c : customizables)
    {
        box.pack_start(c->button, false, false);
    }
    box.show_all();
}
