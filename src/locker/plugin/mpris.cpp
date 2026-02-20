#include <memory>
#include <giomm/asyncresult.h>
#include <giomm/dbusconnection.h>
#include <giomm/dbusproxy.h>
#include <glibmm.h>
#include <wf-option-wrap.hpp>

#include "lockergrid.hpp"
#include "plugin.hpp"
#include "mpris.hpp"

/* Widget of controls for one player on one screen
 * https://specifications.freedesktop.org/mpris/latest/index.html */
WayfireLockerMPRISWidget::WayfireLockerMPRISWidget(std::string name,
    Glib::RefPtr<Gio::DBus::Proxy> proxy) :
    WayfireLockerTimedRevealer("locker/mpris_always"),
    proxy(proxy),
    name(name)
{
    image.add_css_class("albumart");
    add_css_class("mpris");

    kill.set_halign(Gtk::Align::END);
    box.append(image);
    box.append(sidebox);

    sidebox.set_orientation(Gtk::Orientation::VERTICAL);
    sidebox.append(label);
    sidebox.append(controlbox);

    label.set_halign(Gtk::Align::START);
    label.set_valign(Gtk::Align::START);
    label.set_wrap(true);

    controlbox.set_expand(true);
    controlbox.append(prev);
    controlbox.append(playpause);
    controlbox.append(next);
    controlbox.append(kill);
    controlbox.set_valign(Gtk::Align::END);
    controlbox.set_halign(Gtk::Align::START);
    kill.set_hexpand(true);
    kill.set_halign(Gtk::Align::END);

    signals.push_back(next.signal_clicked().connect(
        [proxy] ()
    {
        proxy->call("Next", [proxy] (Glib::RefPtr<Gio::AsyncResult> res) {proxy->call_finish(res);}, nullptr);
    }));

    signals.push_back(prev.signal_clicked().connect(
        [proxy] ()
    {
        proxy->call("Previous",
            [proxy] (Glib::RefPtr<Gio::AsyncResult> res) {proxy->call_finish(res);}, nullptr);
    }));

    signals.push_back(playpause.signal_clicked().connect(
        [proxy] ()
    {
        proxy->call("PlayPause",
            [proxy] (Glib::RefPtr<Gio::AsyncResult> res) {proxy->call_finish(res);}, nullptr);
    }));

    signals.push_back(kill.signal_clicked().connect(
        [proxy] ()
    {
        proxy->call("Stop", [proxy] (Glib::RefPtr<Gio::AsyncResult> res) {proxy->call_finish(res);}, nullptr);
    }));

    signals.push_back(proxy->signal_properties_changed().connect(
        [this] (const Gio::DBus::Proxy::MapChangedProperties& properties,
                const std::vector<Glib::ustring>& invalidated)
    {
        for (auto & it : properties)
        {
            auto [id, value] = it;
            if (id == "PlaybackStatus")
            {
                auto value_string = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(value);
                playbackstatus(value_string.get());
            } else if (id == "Metadata")
            {
                auto value_array = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<std::string,
                    Glib::VariantBase>>>(value);
                metadata(value_array.get());
            } else if (id == "CanGoNext")
            {
                auto value_bool = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value);
                cangonext(value_bool.get());
            } else if (id == "CanGoPrevious")
            {
                auto value_bool = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value);
                cangoprev(value_bool.get());
            } else if (id == "CanControl")
            {
                auto value_bool = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value);
                cancontrol(value_bool.get());
            }
        }
    }));

    Glib::Variant<std::string> playbackstatus_value;
    proxy->get_cached_property(playbackstatus_value, "PlaybackStatus");
    playbackstatus(playbackstatus_value.get());

    Glib::Variant<std::map<std::string, Glib::VariantBase>> metadata_value;
    proxy->get_cached_property(metadata_value, "Metadata");
    metadata(metadata_value.get());

    Glib::Variant<bool> cangonext_value;
    proxy->get_cached_property(cangonext_value, "CanGoNext");
    cangonext(cangonext_value.get());

    Glib::Variant<bool> cangoprev_value;
    proxy->get_cached_property(cangoprev_value, "CanGoPrevious");
    cangoprev(cangoprev_value.get());

    Glib::Variant<bool> cancontrol_value;
    proxy->get_cached_property(cancontrol_value, "CanControl");
    cancontrol(cancontrol_value.get());

    kill.set_icon_name("media-playback-stop");
    playpause.set_icon_name("media-playback-pause");
    prev.set_icon_name("media-skip-backward");
    next.set_icon_name("media-skip-forward");

    set_child(box);
}

WayfireLockerMPRISWidget::~WayfireLockerMPRISWidget()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WayfireLockerMPRISWidget::playbackstatus(std::string value)
{
    if (value == "Stopped")
    {
        box.hide();
        return;
    }

    box.show();
    if (value == "Paused")
    {
        playpause.set_icon_name("media-playback-start");
    } else
    {
        playpause.set_icon_name("media-playback-pause");
    }

    activity();
}

/* https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/ */
void WayfireLockerMPRISWidget::metadata(std::map<std::string, Glib::VariantBase> value)
{
    std::string title = "", album = "", artist = "", art = "";
    for (auto & it : value)
    {
        std::string id = it.first;
        if (id == "xesam:title")
        {
            title = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(it.second).get();
        } else if (id == "xesam:album")
        {
            album = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(it.second).get();
        } else if (id == "xesam:artist")
        {
            auto artists =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::string>>>(it.second).get();
            if (artists.size() > 0)
            {
                artist = artists[0];
            }
        } else if (id == "mpris:artUrl")
        {
            art = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(it.second).get();
        }
    }

    if (art.length() < 8)
    {
        image_path = "";
        image.hide();
    } else
    {
        art = art.substr(7);
        if (art != image_path)
        {
            image.show();
            image.set(art);
            image_path = art;
        }
    }

    std::vector<std::tuple<std::string, std::string>> pairs = {
        {"%track", title},
        {"%album", album},
        {"%artist", artist},
        {"%n", "\n"}
    };
    Glib::ustring output =
        substitute_strings(pairs, (std::string)WfOption<std::string>{"locker/mpris_format"});
    label.set_label(output);
    activity();
}

void WayfireLockerMPRISWidget::cangonext(bool value)
{
    if (value)
    {
        next.show();
    } else
    {
        next.hide();
    }
}

void WayfireLockerMPRISWidget::cangoprev(bool value)
{
    if (value)
    {
        prev.show();
    } else
    {
        prev.hide();
    }
}

void WayfireLockerMPRISWidget::cancontrol(bool value)
{
    if (value)
    {
        controlbox.show();
    } else
    {
        controlbox.hide();
    }
}

void WayfireLockerMPRISCollective::add_child(std::string id, Glib::RefPtr<Gio::DBus::Proxy> proxy)
{
    children.emplace(id, new WayfireLockerMPRISWidget(id, proxy));
    box.append(*children[id]);
}

void WayfireLockerMPRISCollective::rem_child(std::string id)
{
    box.remove(*children[id]);
    children.erase(id);
}

WayfireLockerMPRISPlugin::WayfireLockerMPRISPlugin() :
    WayfireLockerPlugin("locker/mpris")
{}

void WayfireLockerMPRISPlugin::init()
{
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        [this] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        // Got a dbus proxy
        manager_proxy = Gio::DBus::Proxy::create_finish(result);
        auto val = manager_proxy->call_sync("ListNames");
        Glib::Variant<std::vector<std::string>> list;
        val.get_child(list, 0);
        auto l2 = list.get();
        for (auto t : l2)
        {
            if (t.substr(0, 23) == "org.mpris.MediaPlayer2.")
            {
                add_client(t);
            }
        }

        /* https://dbus.freedesktop.org/doc/dbus-java/api/org/freedesktop/DBus.NameOwnerChanged.html */
        signal = manager_proxy->signal_signal().connect(
            [this] (const Glib::ustring & sender_name,
                    const Glib::ustring & signal_name,
                    const Glib::VariantContainerBase & params)
        {
            if (signal_name == "NameOwnerChanged")
            {
                Glib::Variant<std::string> to, from, name;
                params.get_child(name, 0);
                params.get_child(to, 1);
                params.get_child(from, 2);
                if (name.get().substr(0, 23) == "org.mpris.MediaPlayer2.")
                {
                    if (to.get() == "")
                    {
                        add_client(name.get());
                    } else if (from.get() == "")
                    {
                        rem_client(name.get());
                    }
                }
            }
        });
    });
}

void WayfireLockerMPRISPlugin::deinit()
{
    if (signal)
    {
        signal.disconnect();
    }

    manager_proxy = nullptr;
}

void WayfireLockerMPRISPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

void WayfireLockerMPRISPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerMPRISCollective());

    auto collective = widgets[id];
    for (auto & it : clients)
    {
        collective->add_child(it.first, it.second);
    }

    grid->attach(*collective, position);
}

std::string substitute_string(const std::string from, const std::string to, const std::string in)
{
    std::string output = in;
    std::string::size_type position;
    while ((position = output.find(from)) != std::string::npos)
    {
        output.replace(position, from.length(), to);
    }

    return output;
}

std::string substitute_strings(const std::vector<std::tuple<std::string, std::string>> pairs,
    const std::string in)
{
    std::string output = in;
    for (auto & it : pairs)
    {
        const auto [from, to] = it;
        output = substitute_string(from, to, output);
    }

    return output;
}

void WayfireLockerMPRISPlugin::add_client(std::string path)
{
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION,
        path,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        [this, path] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        auto proxy = Gio::DBus::Proxy::create_finish(result);
        clients.emplace(path, proxy);
        for (auto & it : widgets)
        {
            it.second->add_child(path, proxy);
        }
    });
}

void WayfireLockerMPRISPlugin::rem_client(std::string path)
{
    clients.erase(path);
    for (auto & it : widgets)
    {
        it.second->rem_child(path);
    }
}

void WayfireLockerMPRISCollective::activity()
{
    set_reveal_child(true);
    for (auto & it : children)
    {
        it.second->activity();
    }
}
