#include <dirent.h>

#include <cassert>
#include <giomm/icon.h>
#include <glibmm/spawn.h>
#include <iostream>

#include "menu.hpp"
#include "gtk-utils.hpp"
#include "launchers.hpp"
#include "wf-autohide-window.hpp"

#define MAX_LAUNCHER_NAME_LENGTH 11
const std::string default_icon = ICONDIR "/wayfire.png";

WfMenuMenuItem::WfMenuMenuItem(WayfireMenu* _menu, AppInfo app)
    : Gtk::HBox(), menu(_menu), m_app_info(app)
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

    m_button.add(m_button_box);
    m_button.get_style_context()->add_class("flat");

    /* Wrap the button box into a HBox, with left/right padding.
     * This way, the button doesn't fill the whole area allocated for an entry
     * in the flowbox */
    this->pack_start(m_left_pad);
    this->pack_start(m_button);
    this->pack_start(m_right_pad);

    get_style_context()->add_class("flat");
    m_button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WfMenuMenuItem::on_click));
}

void WfMenuMenuItem::on_click()
{
    m_app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    menu->hide_menu();
}

/* Fuzzy search for pattern in text. We use a greedy algorithm as follows:
 * As long as the pattern isn't matched, try to match the leftmost unmatched
 * character in pattern with the first occurence of this character after the
 * partial match. In the end, we just check if we successfully matched all
 * characters */
static bool fuzzy_match(Glib::ustring text, Glib::ustring pattern)
{
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

bool WfMenuMenuItem::fuzzy_match(Glib::ustring pattern)
{
    Glib::ustring name = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr = m_app_info->get_executable();

    pattern = pattern.lowercase();

    return ::fuzzy_match(progr.lowercase(), pattern)
        || ::fuzzy_match(name.lowercase(), pattern)
        || ::fuzzy_match(long_name.lowercase(), pattern);
}

bool WfMenuMenuItem::matches(Glib::ustring pattern)
{
    Glib::ustring name = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr = m_app_info->get_executable();
    Glib::ustring descr = m_app_info->get_description();

    Glib::ustring text = name.lowercase() + "$"
        + long_name.lowercase() + "$" + progr.lowercase() + "$"
        + descr.lowercase();

    return text.find(pattern.lowercase()) != text.npos;
}

bool WfMenuMenuItem::operator < (const WfMenuMenuItem& other)
{
    return Glib::ustring(m_app_info->get_name()).lowercase()
        < Glib::ustring(other.m_app_info->get_name()).lowercase();
}

void WayfireMenu::load_menu_item(AppInfo app_info)
{
    if (!app_info)
        return;

    auto desktop_app_info = Glib::RefPtr<Gio::DesktopAppInfo>::cast_dynamic(app_info);
    if (desktop_app_info && desktop_app_info->get_nodisplay())
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

static bool ends_with(std::string text, std::string pattern)
{
    if (text.length() < pattern.length())
        return false;

    return text.substr(text.length() - pattern.length()) == pattern;
}

void WayfireMenu::load_menu_items_from_dir(std::string path)
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

        if (ends_with(fullpath, ".desktop"))
            load_menu_item(Gio::DesktopAppInfo::create_from_filename(fullpath));
    }
}

void WayfireMenu::load_menu_items_all()
{
    std::string home_dir = getenv("HOME");
    auto app_list = Gio::AppInfo::get_all();
    for (auto app : app_list)
        load_menu_item(app);

    load_menu_items_from_dir(home_dir + "/Desktop");
}

void WayfireMenu::on_search_changed()
{
    fuzzy_filter = false;
    count_matches = 0;
    flowbox.invalidate_filter();

    /* We got no matches, try to fuzzy-match */
    if (count_matches <= 0 && fuzzy_search_enabled)
    {
        fuzzy_filter = true;
        flowbox.invalidate_filter();
    }
}

bool WayfireMenu::on_filter(Gtk::FlowBoxChild *child)
{
    auto button = dynamic_cast<WfMenuMenuItem*> (child->get_child());
    assert(button);

    auto text = search_box.get_text();
    bool does_match = this->fuzzy_filter ?
        button->fuzzy_match(text) : button->matches(text);

    if (does_match)
    {
        this->count_matches++;
        return true;
    }

    return false;
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

bool WayfireMenu::update_icon()
{
    std::string icon;
    int size = menu_size / LAUNCHERS_ICON_SCALE;
    if (((std::string) menu_icon).empty())
    {
        icon = default_icon;
    }
    else
    {
        icon = menu_icon;
    }

    button->set_size_request(size, 0);

    auto ptr_pbuff = load_icon_pixbuf_safe(icon,
        size * main_image.get_scale_factor());

    if (!ptr_pbuff.get())
    {
        std::cout << "Loading default icon: " << default_icon << std::endl;
        ptr_pbuff = load_icon_pixbuf_safe(default_icon,
            size * main_image.get_scale_factor());
    }

    if (!ptr_pbuff)
        return false;

    set_image_pixbuf(main_image, ptr_pbuff, main_image.get_scale_factor());
    return true;
}

void WayfireMenu::update_popover_layout()
{
    /* First time updating layout, need to setup everything */
    if (popover_layout_box.get_parent() == nullptr)
    {
        button->get_popover()->add(popover_layout_box);

        flowbox.set_valign(Gtk::ALIGN_START);
        flowbox.set_homogeneous(true);
        flowbox.set_sort_func(sigc::mem_fun(this, &WayfireMenu::on_sort));
        flowbox.set_filter_func(sigc::mem_fun(this, &WayfireMenu::on_filter));

        flowbox_container.add(bottom_pad);
        flowbox_container.add(flowbox);

        scrolled_window.set_min_content_width(500);
        scrolled_window.set_min_content_height(500);
        scrolled_window.add(flowbox_container);

        search_box.property_margin().set_value(20);
        search_box.set_icon_from_icon_name("search", Gtk::ENTRY_ICON_SECONDARY);
        search_box.signal_changed().connect_notify(
            sigc::mem_fun(this, &WayfireMenu::on_search_changed));
    } else
    {
        /* Layout was already initialized, make sure to remove widgets before
         * adding them again */
        popover_layout_box.remove(search_box);
        popover_layout_box.remove(scrolled_window);
    }

    if ((std::string)panel_position == WF_WINDOW_POSITION_TOP)
    {
        popover_layout_box.pack_start(search_box);
        popover_layout_box.pack_start(scrolled_window);
    } else
    {
        popover_layout_box.pack_start(scrolled_window);
        popover_layout_box.pack_start(search_box);
    }

    popover_layout_box.set_focus_chain({&search_box});
    popover_layout_box.show_all();
}

void WayfireMenu::on_logout_click()
{
    g_spawn_command_line_async(std::string(menu_logout_command).c_str(), NULL);
}

void WayfireMenu::init(Gtk::HBox *container)
{
    menu_icon.set_callback([=] () { update_icon(); });
    menu_size.set_callback([=] () { update_icon(); });
    panel_position.set_callback([=] () { update_popover_layout(); });

    button = std::make_unique<WayfireMenuButton> ("panel");
    button->add(main_image);
    button->get_popover()->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    button->get_popover()->signal_show().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_popover_shown));

    if (!update_icon())
        return;

    button->property_scale_factor().signal_changed().connect(
        [=] () {update_icon(); });

    container->pack_start(hbox, false, false);
    hbox.pack_start(*button, false, false);

    logout_button.set_image_from_icon_name("system-shutdown", Gtk::ICON_SIZE_DIALOG);
    logout_button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_logout_click));
    logout_button.property_margin().set_value(20);
    logout_button.set_margin_right(35);
    hbox_bottom.pack_end(logout_button, false, false);
    popover_layout_box.pack_end(hbox_bottom);
    popover_layout_box.pack_end(separator);

    load_menu_items_all();
    update_popover_layout();

    hbox.show();
    main_image.show();
    button->show();
}

void WayfireMenu::hide_menu()
{
    button->set_active(false);
}
