#include <dirent.h>
#include <sstream>

#include <cassert>
#include <giomm/icon.h>
#include <glibmm/spawn.h>
#include <iostream>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/stylecontext.h>

#include "menu.hpp"
#include "gtk-utils.hpp"
#include "launchers.hpp"
#include "wf-autohide-window.hpp"

#define MAX_LAUNCHER_NAME_LENGTH 11
const std::string default_icon = ICONDIR "/wayfire.png";

WfMenuCategoryDefinition::WfMenuCategoryDefinition(std::string _name, std::string _icon_name):
    name(_name), icon_name(_icon_name)
{

}

std::string WfMenuCategoryDefinition::get_name(){
    return name;
}

std::string WfMenuCategoryDefinition::get_icon_name(){
    return icon_name;
}

WfMenuCategoryButton::WfMenuCategoryButton(WayfireMenu *_menu, std::string _category, std::string _label, std::string _icon_name) :
    Gtk::Button(), menu(_menu), category(_category), label(_label), icon_name(_icon_name)
{
    m_image.set_from_icon_name(icon_name,
        (Gtk::IconSize)Gtk::ICON_SIZE_LARGE_TOOLBAR);
    m_image.set_pixel_size(32);
    m_label.set_text(label);
    m_label.set_xalign(0.0);

    m_box.pack_start(m_image, false, false, 2);
    m_box.pack_end(m_label, true, true, 2);
    m_box.set_homogeneous(false);

    this->add(m_box);
    this->get_style_context()->add_class("flat");

    this->signal_clicked().connect_notify(
        sigc::mem_fun(this, &WfMenuCategoryButton::on_click));
}

void WfMenuCategoryButton::on_click()
{
    menu->set_category(category);
}

WfMenuMenuItem::WfMenuMenuItem(WayfireMenu *_menu, AppInfo app) :
    Gtk::HBox(), menu(_menu), m_app_info(app)
{
    m_image.set((const Glib::RefPtr<const Gio::Icon>&)app->get_icon(),
        (Gtk::IconSize)Gtk::ICON_SIZE_LARGE_TOOLBAR);
    m_image.set_pixel_size(48);

    Glib::ustring name = app->get_name();

    if (name.length() > MAX_LAUNCHER_NAME_LENGTH)
    {
        name = name.substr(0, MAX_LAUNCHER_NAME_LENGTH - 2) + "..";
    }

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
        j    = 0; // the first unmatched character in text

    while (i < pattern.length() && j < text.length())
    {
        /* Found a match, advance both pointers */
        if (pattern[i] == text[j])
        {
            ++i;
            ++j;
        } else
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
    Glib::ustring progr     = m_app_info->get_executable();

    pattern = pattern.lowercase();

    return ::fuzzy_match(progr.lowercase(), pattern) ||
           ::fuzzy_match(name.lowercase(), pattern) ||
           ::fuzzy_match(long_name.lowercase(), pattern);
}

bool WfMenuMenuItem::matches(Glib::ustring pattern)
{
    Glib::ustring name = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr     = m_app_info->get_executable();
    Glib::ustring descr     = m_app_info->get_description();

    Glib::ustring text = name.lowercase() + "$" +
        long_name.lowercase() + "$" + progr.lowercase() + "$" +
        descr.lowercase();

    return text.find(pattern.lowercase()) != text.npos;
}

bool WfMenuMenuItem::operator <(const WfMenuMenuItem& other)
{
    return Glib::ustring(m_app_info->get_name()).lowercase() <
           Glib::ustring(other.m_app_info->get_name()).lowercase();
}

void WayfireMenu::load_menu_item(AppInfo app_info)
{
    if (!app_info)
    {
        return;
    }

    auto desktop_app_info = Glib::RefPtr<Gio::DesktopAppInfo>::cast_dynamic(app_info);
    if (desktop_app_info && desktop_app_info->get_nodisplay())
    {
        return;
    }

    auto name = app_info->get_name();
    auto exec = app_info->get_executable();
    /* If we don't have the following, then the entry won't be useful anyway,
     * so we should skip it */
    if (name.empty() || !app_info->get_icon() || exec.empty())
    {
        return;
    }

    /* Already created such a launcher, skip */
    if (loaded_apps.count({name, exec}))
    {
        return;
    }

    loaded_apps.insert({name, exec});

    /* Check if this has a 'OnlyShownIn' for a different desktop env 
       If so, we throw it in a pile at the bottom just to be safe */
    if (!desktop_app_info->get_show_in("wayfire")){
        add_category_app("Hidden", desktop_app_info);
        return;
    }
    add_category_app("All", desktop_app_info);

    /* Split the Categories, iterate to place into submenus */
    std::stringstream categories_stream(desktop_app_info->get_categories());
    std::string segment;

    while(std::getline(categories_stream, segment, ';'))
    {
        add_category_app(segment, desktop_app_info);
    }
}

void WayfireMenu::add_category_app(std::string category, Glib::RefPtr<Gio::DesktopAppInfo> app)
{
    // Filter for allowed categories
    bool allowed = false;
    for (const auto& [key, value]: category_definitions)
    {
        if(key == category){
            allowed = true;
        }
    }
    if (!allowed){
        return;
    }
    items[category].push_back(app);
}

void WayfireMenu::populate_menu_categories(){
    // Ensure the category list is empty
    for (auto child : category_box.get_children())
    {
        gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }
    // Iterate allowed categories in order
    for (auto category: category_order){
        if (category_definitions.count(category) == 1){
            if (items[category].size() > 0 ){
                auto icon_name = category_definitions[category]->get_icon_name();
                auto name = category_definitions[category]->get_name();
                auto category_button = new WfMenuCategoryButton(this, category, name, icon_name);
                category_box.pack_start(*category_button);
            }
        } else {
            std::cerr << "Category in order without Category Definition : " << category << std::endl;
        }
    }
    category_box.show_all();
}

void WayfireMenu::populate_menu_items(std::string category)
{
    // Ensure the flowbox is empty
    for (auto child : flowbox.get_children())
    {
        gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }
    for (auto app_info: items[category]){
        auto app = new WfMenuMenuItem(this, app_info);
        flowbox.add(*app);
    }
    flowbox.show_all();
}

static bool ends_with(std::string text, std::string pattern)
{
    if (text.length() < pattern.length())
    {
        return false;
    }

    return text.substr(text.length() - pattern.length()) == pattern;
}

void WayfireMenu::load_menu_items_from_dir(std::string path)
{
    /* Expand path */
    auto dir = opendir(path.c_str());
    if (!dir)
    {
        return;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = path + "/" + file->d_name;

        if (ends_with(fullpath, ".desktop"))
        {
            load_menu_item(Gio::DesktopAppInfo::create_from_filename(fullpath));
        }
    }
}

void WayfireMenu::load_menu_items_all()
{
    std::string home_dir = getenv("HOME");
    auto app_list = Gio::AppInfo::get_all();
    for (auto app : app_list)
    {
        load_menu_item(app);
    }

    load_menu_items_from_dir(home_dir + "/Desktop");
}

void WayfireMenu::on_search_changed()
{
    auto value = search_box.get_text();
    if(value.length() == 0){
        /* Text has been unset, show categories again */
        populate_menu_items(category);
        category_scrolled_window.show();
        app_scrolled_window.set_min_content_width(int(menu_min_content_width));
    }else{
        /* User is filtering, hide categories, ignore chosen category */
        populate_menu_items("All");
        category_scrolled_window.hide();
        app_scrolled_window.set_min_content_width(int(menu_min_content_width) + int(menu_min_category_width));
    }

    fuzzy_filter  = false;
    count_matches = 0;
    flowbox.invalidate_filter();

    /* We got no matches, try to fuzzy-match */
    if ((count_matches <= 0) && fuzzy_search_enabled)
    {
        fuzzy_filter = true;
        flowbox.invalidate_filter();
    }
}

bool WayfireMenu::on_filter(Gtk::FlowBoxChild *child)
{
    auto button = dynamic_cast<WfMenuMenuItem*>(child->get_child());
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

bool WayfireMenu::on_sort(Gtk::FlowBoxChild *a, Gtk::FlowBoxChild *b)
{
    auto b1 = dynamic_cast<WfMenuMenuItem*>(a->get_child());
    auto b2 = dynamic_cast<WfMenuMenuItem*>(b->get_child());
    assert(b1 && b2);

    return *b2 < *b1;
}

void WayfireMenu::on_popover_shown()
{
    search_box.set_text("");
    flowbox.unselect_all();
}

bool WayfireMenu::update_icon()
{
    std::string icon;
    int size = menu_size / LAUNCHERS_ICON_SCALE;
    if (((std::string)menu_icon).empty())
    {
        icon = default_icon;
    } else
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
    {
        return false;
    }

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

        scroll_pair.add(category_scrolled_window);
        scroll_pair.add(app_scrolled_window);
        scroll_pair.set_homogeneous(false);

        app_scrolled_window.set_min_content_width(int(menu_min_content_width));
        app_scrolled_window.set_min_content_height(int(menu_min_content_height));
        app_scrolled_window.add(flowbox_container);

        category_scrolled_window.set_min_content_width(int(menu_min_category_width));
        category_scrolled_window.set_min_content_height(int(menu_min_content_height));
        category_scrolled_window.add(category_box);

        search_box.property_margin().set_value(20);
        search_box.set_icon_from_icon_name("search", Gtk::ENTRY_ICON_SECONDARY);
        search_box.signal_changed().connect_notify(
            sigc::mem_fun(this, &WayfireMenu::on_search_changed));
    } else
    {
        /* Layout was already initialized, make sure to remove widgets before
         * adding them again */
        popover_layout_box.remove(search_box);
        popover_layout_box.remove(scroll_pair);
    }

    if ((std::string)panel_position == WF_WINDOW_POSITION_TOP)
    {
        popover_layout_box.pack_start(search_box);
        popover_layout_box.pack_start(scroll_pair);
    } else
    {
        popover_layout_box.pack_start(scroll_pair);
        popover_layout_box.pack_start(search_box);
    }

    popover_layout_box.set_focus_chain({&search_box});
    popover_layout_box.show_all();
}

void WayfireLogoutUI::on_logout_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(logout_command).c_str(), NULL);
}

void WayfireLogoutUI::on_reboot_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(reboot_command).c_str(), NULL);
}

void WayfireLogoutUI::on_shutdown_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(shutdown_command).c_str(), NULL);
}

void WayfireLogoutUI::on_suspend_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(suspend_command).c_str(), NULL);
}

void WayfireLogoutUI::on_hibernate_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(hibernate_command).c_str(), NULL);
}

void WayfireLogoutUI::on_switchuser_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(switchuser_command).c_str(), NULL);
}

void WayfireLogoutUI::on_cancel_click()
{
    ui.hide();
    bg.hide();
}

#define LOGOUT_BUTTON_SIZE  125
#define LOGOUT_BUTTON_MARGIN 10

void WayfireLogoutUI::create_logout_ui_button(WayfireLogoutUIButton *button, const char *icon,
    const char *label)
{
    button->button.set_size_request(LOGOUT_BUTTON_SIZE, LOGOUT_BUTTON_SIZE);
    button->image.set_from_icon_name(icon, Gtk::ICON_SIZE_DIALOG);
    button->label.set_text(label);
    button->layout.pack_start(button->image, true, false);
    button->layout.pack_start(button->label, true, false);
    button->button.add(button->layout);
}

WayfireLogoutUI::WayfireLogoutUI()
{
    create_logout_ui_button(&suspend, "emblem-synchronizing", "Suspend");
    suspend.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_suspend_click));
    top_layout.pack_start(suspend.button, true, false);

    create_logout_ui_button(&hibernate, "weather-clear-night", "Hibernate");
    hibernate.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_hibernate_click));
    top_layout.pack_start(hibernate.button, true, false);

    create_logout_ui_button(&switchuser, "system-users", "Switch User");
    switchuser.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_switchuser_click));
    top_layout.pack_start(switchuser.button, true, false);

    create_logout_ui_button(&logout, "system-log-out", "Log Out");
    logout.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_logout_click));
    middle_layout.pack_start(logout.button, true, false);

    create_logout_ui_button(&reboot, "system-reboot", "Reboot");
    reboot.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_reboot_click));
    middle_layout.pack_start(reboot.button, true, false);

    create_logout_ui_button(&shutdown, "system-shutdown", "Shut Down");
    shutdown.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_shutdown_click));
    middle_layout.pack_start(shutdown.button, true, false);

    cancel.button.set_size_request(100, 50);
    cancel.button.set_label("Cancel");
    bottom_layout.pack_start(cancel.button, true, false);
    cancel.button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireLogoutUI::on_cancel_click));

    top_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    middle_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    bottom_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.add(top_layout);
    main_layout.add(middle_layout);
    main_layout.add(bottom_layout);
    /* Work around spacing bug for immediate child of window */
    hspacing_layout.pack_start(main_layout, true, false, LOGOUT_BUTTON_MARGIN);
    vspacing_layout.pack_start(hspacing_layout, true, false, LOGOUT_BUTTON_MARGIN);
    /* Make surfaces layer shell */
    gtk_layer_init_for_window(bg.gobj());
    gtk_layer_set_layer(bg.gobj(), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_exclusive_zone(bg.gobj(), -1);
    gtk_layer_init_for_window(ui.gobj());
    gtk_layer_set_layer(ui.gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    ui.add(vspacing_layout);
    bg.set_opacity(0.5);
    auto css_provider  = Gtk::CssProvider::create();
    auto style_context = Gtk::StyleContext::create();
    bg.set_name("logout_background");
    css_provider->load_from_data("window#logout_background { background-color: black; }");
    style_context->add_provider_for_screen(Gdk::Screen::get_default(),
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void WayfireMenu::on_logout_click()
{
    button->get_popover()->hide();
    if (!std::string(menu_logout_command).empty())
    {
        g_spawn_command_line_async(std::string(menu_logout_command).c_str(), NULL);
        return;
    }

    /* If no command specified for logout, show our own logout window */
    logout_ui->ui.present();
    logout_ui->ui.show_all();
    logout_ui->bg.show_all();

    /* Set the background window to the same size of the screen */
    auto gdk_screen = Gdk::Screen::get_default();
    logout_ui->bg.set_size_request(gdk_screen->get_width(), gdk_screen->get_height());
}

void WayfireMenu::refresh()
{
    loaded_apps.clear();
    items.clear();
    for (auto child : flowbox.get_children())
    {
        gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }

    load_menu_items_all();
    populate_menu_categories();
    populate_menu_items("All");
    flowbox.show_all();
}

static void app_info_changed(GAppInfoMonitor *gappinfomonitor, gpointer user_data)
{
    WayfireMenu *menu = (WayfireMenu*)user_data;

    menu->refresh();
}

void WayfireMenu::init(Gtk::HBox *container)
{

    /* https://specifications.freedesktop.org/menu-spec/latest/apa.html#main-category-registry
     * Using the 'Main' categories, with names and icons assigned
     * Any Categories in .desktop files that are not in this list are ignored */
    category_definitions["All"] = std::make_unique<WfMenuCategoryDefinition>("All", "applications-other");
    category_definitions["Network"] = std::make_unique<WfMenuCategoryDefinition>("Internet", "applications-internet");
    category_definitions["Education"] = std::make_unique<WfMenuCategoryDefinition>("Education", "applications-education");
    category_definitions["Office"] = std::make_unique<WfMenuCategoryDefinition>("Office", "applications-office");
    category_definitions["Development"] = std::make_unique<WfMenuCategoryDefinition>("Development", "applications-development");
    category_definitions["Graphics"] = std::make_unique<WfMenuCategoryDefinition>("Graphics", "applications-graphics");
    category_definitions["AudioVideo"] = std::make_unique<WfMenuCategoryDefinition>("Multimedia", "applications-multimedia");
    category_definitions["Game"] = std::make_unique<WfMenuCategoryDefinition>("Games", "applications-games");
    category_definitions["Science"] = std::make_unique<WfMenuCategoryDefinition>("Science", "applications-science");
    category_definitions["Settings"] = std::make_unique<WfMenuCategoryDefinition>("Settings", "preferences-desktop");
    category_definitions["System"] = std::make_unique<WfMenuCategoryDefinition>("System", "applications-system");
    category_definitions["Utility"] = std::make_unique<WfMenuCategoryDefinition>("Accessories", "applications-utilities");
    category_definitions["Hidden"] = std::make_unique<WfMenuCategoryDefinition>("Other Desktops", "user-desktop");

    output->toggle_menu_signal().connect(sigc::mem_fun(this, &WayfireMenu::toggle_menu));

    menu_icon.set_callback([=] () { update_icon(); });
    menu_size.set_callback([=] () { update_icon(); });
    panel_position.set_callback([=] () { update_popover_layout(); });

    button = std::make_unique<WayfireMenuButton>("panel");
    button->add(main_image);
    button->get_popover()->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    button->get_popover()->signal_show().connect_notify(
        sigc::mem_fun(this, &WayfireMenu::on_popover_shown));

    if (!update_icon())
    {
        return;
    }

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

    logout_ui = std::make_unique<WayfireLogoutUI>();

    load_menu_items_all();
    update_popover_layout();
    populate_menu_categories();
    populate_menu_items("All");

    app_info_monitor_changed_handler_id =
        g_signal_connect(app_info_monitor, "changed", G_CALLBACK(app_info_changed), this);

    hbox.show();
    main_image.show();
    button->show();
}

void WayfireMenu::toggle_menu()
{
    if (button->get_active())
    {
        button->set_active(false);
    } else
    {
        button->set_active(true);
    }
}

void WayfireMenu::hide_menu()
{
    button->set_active(false);
}

void WayfireMenu::set_category(std::string in_category)
{
    category = in_category;
    populate_menu_items(in_category);
}
