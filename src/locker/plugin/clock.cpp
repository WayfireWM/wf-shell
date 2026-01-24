#include <memory>
#include <glibmm.h>
#include <gtkmm/box.h>

#include "lockergrid.hpp"
#include "clock.hpp"


void WayfireLockerClockPlugin::update_labels(std::string text)
{
    for (auto& it : labels)
    {
        it.second->set_markup(text);
    }

    label_contents = text;
}

void WayfireLockerClockPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    labels.emplace(id, std::shared_ptr<Gtk::Label>(new Gtk::Label()));
    auto label = labels[id];
    label->add_css_class("clock");
    label->set_markup(label_contents);
    label->set_justify(Gtk::Justification::CENTER);

    grid->attach(*label, position);
}

void WayfireLockerClockPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*labels[id]);
    labels.erase(id);
}

void WayfireLockerClockPlugin::update_time()
{
    auto time = Glib::DateTime::create_now_local();
    auto text = time.format((std::string)format);

    /* Sometimes GLib::DateTime will add leading spaces. This results in
     * unevenly balanced padding around the text, which looks quite bad.
     *
     * This could be circumvented with the modifiers the user passes to the
     * format string, * but to remove the requirement that the user does
     * something fancy, we just remove any leading spaces. */
    int i = 0;
    while (i < (int)text.length() && text[i] == ' ')
    {
        i++;
    }

    this->update_labels(text.substr(i));
}

WayfireLockerClockPlugin::WayfireLockerClockPlugin():
  WayfireLockerPlugin("locker/clock_enable", "locker/clock_position")
{}

void WayfireLockerClockPlugin::init()
{
    timeout = Glib::signal_timeout().connect_seconds(
        [this] ()
    {
        this->update_time();
        return G_SOURCE_CONTINUE;
    }, 1);
}

void WayfireLockerClockPlugin::deinit()
{
    timeout.disconnect();
}