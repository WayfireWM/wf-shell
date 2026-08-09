#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturelongpress.h>

#include "item.hpp"
#include "menu.hpp"
WfMenuItem::WfMenuItem(WayfireMenu *_menu, Glib::RefPtr<Gio::DesktopAppInfo> app) :
    Gtk::FlowBoxChild(), menu(_menu), app_info(app)
{
    image.set((const Glib::RefPtr<const Gio::Icon>&)app->get_icon());
    image.add_css_class("default-icon");
    label.set_text(app->get_name());
    label.set_ellipsize(Pango::EllipsizeMode::END);

    extra_actions_button.add_css_class("flat");
    extra_actions_button.add_css_class("app-button-extras");
    extra_actions_button.set_direction(Gtk::ArrowType::RIGHT);
    extra_actions_button.set_has_frame(false);

    box.set_expand(false);
    box.add_css_class("app-button");

    auto left_click_g  = Gtk::GestureClick::create();
    auto right_click_g = Gtk::GestureClick::create();
    auto long_press_g  = Gtk::GestureLongPress::create();
    left_click_g->set_button(1);
    right_click_g->set_button(3);
    long_press_g->set_touch_only(true);

    signals.push_back(left_click_g->signal_pressed().connect([=] (int, double, double)
    {
        left_click_g->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    signals.push_back(left_click_g->signal_released().connect(
        [=] (int c, double x, double y)
    {
        on_click();
    }));
    signals.push_back(right_click_g->signal_pressed().connect([=] (int, double, double)
    {
        right_click_g->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    signals.push_back(right_click_g->signal_released().connect(
        [=] (int c, double x, double y)
    {
        extra_actions_button.popup();
    }));
    signals.push_back(long_press_g->signal_pressed().connect(
        [=] (double x, double y)
    {
        extra_actions_button.activate();
        long_press_g->set_state(Gtk::EventSequenceState::CLAIMED);
        left_click_g->set_state(Gtk::EventSequenceState::DENIED);
    }));

    if (menu->menu_list)
    {
        label.set_hexpand(true);
        label.set_halign(Gtk::Align::FILL);
        label.set_halign(Gtk::Align::START);
        label.set_xalign(0.0);
        list_item.set_hexpand(true);
        box.set_hexpand(true);
        set_hexpand(true);
        box.set_orientation(Gtk::Orientation::HORIZONTAL);
        extra_actions_button.set_halign(Gtk::Align::END);
        extra_actions_button.set_icon_name("arrow-right");
        button.add_css_class("flat");

        list_item.append(image);
        list_item.append(label);
        button.set_child(list_item);

        list_item.add_controller(left_click_g);
        list_item.add_controller(right_click_g);
        list_item.add_controller(long_press_g);

        box.append(button);
        box.append(extra_actions_button);

        set_child(box);
    } else
    {
        label.set_max_width_chars(0);
        box.set_orientation(Gtk::Orientation::VERTICAL);
        box.append(image);
        if (app->list_actions().size() == 0)
        {
            button.set_child(box);
            button.add_css_class("flat");
            set_child(button);
        } else
        {
            extra_actions_button.set_child(box);
            extra_actions_button.add_css_class("flat");
            set_child(extra_actions_button);
        }

        box.add_controller(left_click_g);
        box.add_controller(right_click_g);
        box.add_controller(long_press_g);

        box.append(label);
    }

    m_menu  = Gio::Menu::create();
    actions = Gio::SimpleActionGroup::create();
    extra_actions_button.hide();

    for (auto action : app->list_actions())
    {
        std::stringstream ss;
        ss << "app." << action;
        std::string full_action = ss.str();

        auto menu_item = Gio::MenuItem::create(app_info->get_action_name(action), full_action);

        auto action_obj = Gio::SimpleAction::create(action);
        signals.push_back(action_obj->signal_activate().connect(
            [this, action] (Glib::VariantBase vb)
        {
            auto ctx = Gdk::Display::get_default()->get_app_launch_context();
            app_info->launch_action(action, ctx);
            menu->hide_menu();
        }));
        m_menu->append_item(menu_item);
        actions->add_action(action_obj);

        extra_actions_button.show();
    }

    extra_actions_button.set_menu_model(m_menu);
    extra_actions_button.insert_action_group("app", actions);

    set_has_tooltip();
    signals.push_back(signal_query_tooltip().connect([=] (int x, int y, bool key_mode,
                                                          const std::shared_ptr<Gtk::Tooltip>& tooltip) ->
        bool
    {
        tooltip->set_text(app->get_name());
        return true;
    }, false));
}

WfMenuItem::~WfMenuItem()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WfMenuItem::on_click()
{
    auto ctx = Gdk::Display::get_default()->get_app_launch_context();
    app_info->launch(std::vector<Glib::RefPtr<Gio::File>>(), ctx);
    menu->hide_menu();
}

void WfMenuItem::set_search_value(uint32_t value)
{
    search_value = value;
}

uint32_t WfMenuItem::get_search_value()
{
    return search_value;
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

uint32_t WfMenuItem::fuzzy_match(Glib::ustring pattern)
{
    uint32_t match_score = 0;
    Glib::ustring name   = app_info->get_name();
    Glib::ustring long_name = app_info->get_display_name();
    Glib::ustring progr     = app_info->get_executable();

    auto name_lower = name.lowercase();
    auto long_name_lower = long_name.lowercase();
    auto progr_lower     = progr.lowercase();
    auto pattern_lower   = pattern.lowercase();

    if (::fuzzy_match(progr_lower, pattern_lower))
    {
        match_score += 100;
    }

    if (::fuzzy_match(name_lower, pattern_lower))
    {
        match_score += 100;
    }

    if (::fuzzy_match(long_name_lower, pattern_lower))
    {
        match_score += 10;
    }

    return match_score;
}

uint32_t WfMenuItem::matches(Glib::ustring pattern)
{
    uint32_t match_score    = 0;
    Glib::ustring long_name = app_info->get_display_name();
    Glib::ustring name  = app_info->get_name();
    Glib::ustring progr = app_info->get_executable();
    Glib::ustring descr = app_info->get_description();

    auto name_lower = name.lowercase();
    auto long_name_lower = long_name.lowercase();
    auto progr_lower     = progr.lowercase();
    auto descr_lower     = descr.lowercase();
    auto pattern_lower   = pattern.lowercase();

    auto pos = name_lower.find(pattern_lower);
    if (pos != name_lower.npos)
    {
        match_score += 1000 - pos;
    }

    pos = progr_lower.find(pattern_lower);
    if (pos != progr_lower.npos)
    {
        match_score += 1000 - pos;
    }

    pos = long_name_lower.find(pattern_lower);
    if (pos != long_name_lower.npos)
    {
        match_score += 500 - pos;
    }

    pos = descr_lower.find(pattern_lower);
    if (pos != descr_lower.npos)
    {
        match_score += 300 - pos;
    }

    return match_score;
}

bool WfMenuItem::operator <(const WfMenuItem& other)
{
    return Glib::ustring(app_info->get_name()).lowercase() <
           Glib::ustring(other.app_info->get_name()).lowercase();
}
