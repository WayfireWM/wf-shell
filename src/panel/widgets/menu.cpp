#include <dirent.h>
#include <sys/stat.h>

#include <cassert>
#include <glibmm.h>
#include <giomm/icon.h>
#include <glibmm/spawn.h>

#include "menu.hpp"
#include "config.hpp"
#include "gtk-utils.hpp"

#define MAX_LAUNCHER_NAME_LENGTH 11

WfMenuMenuItem::WfMenuMenuItem(WayfireMenu* _menu, AppInfo app)
    : Gtk::Button(), menu(_menu), m_app_info(app)
{
    m_image.set((const Glib::RefPtr<const Gio::Icon>&) app->get_icon(),
        (Gtk::IconSize)Gtk::ICON_SIZE_LARGE_TOOLBAR);
    m_image.set_pixel_size(48);

    Glib::ustring name = app->get_name();
    set_tooltip_text(app->get_name());

    if (name.length() > MAX_LAUNCHER_NAME_LENGTH)
        name = name.substr(0, MAX_LAUNCHER_NAME_LENGTH - 2) + "..";

    m_label.set_text(name);
    m_button_box.pack_start(m_image, false, false);
    m_button_box.pack_end(m_label, false, false);

    add(m_button_box);
    get_style_context()->add_class("flat");
    signal_clicked().connect_notify(
        sigc::mem_fun(this, &WfMenuMenuItem::on_click));
}

void WfMenuMenuItem::on_click()
{
    std::string command = m_app_info->get_commandline();
    Glib::spawn_command_line_async("/bin/bash -c \'" + command + "\'");

    menu->focus_lost();
}

bool WfMenuMenuItem::matches(Glib::ustring pattern)
{
    Glib::ustring text = m_app_info->get_name();
    text = text.lowercase();
    pattern = pattern.lowercase();

    /* Fuzzy search for pattern in text. We use a greedy algorithm as follows:
     * As long as the pattern isn't matched, try to match the leftmost unmatched
     * character in pattern with the first occurence of this character after the
     * partial match. In the end, we just check if we successfully matched all
     * characters */

    size_t i = 0, // next character in pattern to match
           j = 0; // the first unmatched character in text

    while (i < pattern.length() && j < text.length())
    {
        /* Found a match, advance both pointers */
        if (pattern[i] == text[j])
        {
            ++i;
            ++j;
        }
        else
        {
            /* Try to match current unmatched character in pattern with the next
             * character in text */
            ++j;
        }
    }

    /* If this happens, then we have already matched all characters */
    return i == pattern.length();
}

bool WfMenuMenuItem::operator < (const WfMenuMenuItem& other)
{
    return Glib::ustring(m_app_info->get_name()).lowercase()
        < Glib::ustring(other.m_app_info->get_name()).lowercase();
}

void WayfireMenu::load_menu_item(std::string file)
{
    auto app_info = Gio::DesktopAppInfo::create_from_filename(file);
    if (!app_info)
        return;

    auto name = app_info->get_name();
    auto exec = app_info->get_executable();
    /* If we don't have the following, then the entry won't be useful anyway,
     * so we should skip it */
    if (name.empty() || !app_info->get_icon() || exec.empty())
        return;

    /* Already created such a launcher, skip */
    if (loaded_apps.count({name, exec}))
        return;
    loaded_apps.insert({name, exec});

    items.push_back(std::unique_ptr<WfMenuMenuItem>(
                        new WfMenuMenuItem(this, app_info)));
    flowbox.add(*items.back());
}

void WayfireMenu::load_menu_items(std::string path)
{
    /* Expand path */
    auto dir = opendir(path.c_str());
    if (!dir)
        return;

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
            continue;

        auto fullpath = path + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) != 0)
            continue;

        if (S_ISDIR(next.st_mode)) {
            load_menu_items(fullpath);
        } else {
            load_menu_item(std::string(fullpath));
        }
    }
}

void WayfireMenu::on_search_changed()
{
    flowbox.invalidate_filter();
}

bool WayfireMenu::on_filter(Gtk::FlowBoxChild *child)
{
    auto button = dynamic_cast<WfMenuMenuItem*> (child->get_child());
    assert(button);
    return button->matches(search_box.get_text());
}

bool WayfireMenu::on_sort(Gtk::FlowBoxChild* a, Gtk::FlowBoxChild* b)
{
    auto b1 = dynamic_cast<WfMenuMenuItem*> (a->get_child());
    auto b2 = dynamic_cast<WfMenuMenuItem*> (b->get_child());
    assert(b1 && b2);

    return *b2 < *b1;
}

void WayfireMenu::on_popover_shown()
{
    flowbox.unselect_all();
}

void WayfireMenu::init(Gtk::HBox *container, wayfire_config *config)
{
    int32_t panel_size = *config->get_section("panel")->get_option("panel_thickness", "48");
    int32_t default_size = panel_size * 0.7;
    int32_t base_size = *config->get_section("panel")->get_option("launcher_size", std::to_string(default_size));

    base_size = std::min(base_size, panel_size);

    menu_button.add(main_image);
    menu_button.set_direction(Gtk::ARROW_DOWN);
    menu_button.set_popover(popover);
    menu_button.get_style_context()->add_class("flat");
    menu_button.set_size_request(base_size, 0);

    popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    popover.signal_show().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_popover_shown));

    auto ptr_pbuff = Gdk::Pixbuf::create_from_file(ICONDIR "/wayfire.png",
        base_size * main_image.get_scale_factor(),
        base_size * main_image.get_scale_factor());
    if (!ptr_pbuff)
        return;

    set_image_pixbuf(main_image, ptr_pbuff, main_image.get_scale_factor());

    container->pack_start(hbox, Gtk::PACK_SHRINK, 0);
    hbox.pack_start(menu_button, Gtk::PACK_SHRINK, 0);

    std::string home_dir = getenv("HOME");

    load_menu_items(home_dir + "/.local/share/applications");
    load_menu_items(home_dir + "/Desktop/");
    load_menu_items("/usr/share/applications");

    flowbox.set_valign(Gtk::ALIGN_START);
    flowbox.set_homogeneous(true);
    flowbox.set_sort_func(sigc::mem_fun(this, &WayfireMenu::on_sort));
    flowbox.set_filter_func(sigc::mem_fun(this, &WayfireMenu::on_filter));

    flowbox_container.add(flowbox);
    flowbox_container.add(bottom_pad);

    scrolled_window.add(flowbox_container);
    scrolled_window.set_min_content_width(500);
    scrolled_window.set_min_content_height(500);

    search_box.property_margin().set_value(20);
    search_box.set_icon_from_icon_name("search", Gtk::ENTRY_ICON_SECONDARY);
    search_box.signal_changed().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_search_changed));

    box.pack_start(search_box);
    box.pack_start(scrolled_window);
    box.show_all();

    popover.add(box);
    popover.show_all();
}

void WayfireMenu::focus_lost()
{
    menu_button.set_active(false);
}
