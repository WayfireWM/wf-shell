#include <memory>
#include <string>
#include <iostream>
#include <giomm.h>
#include <giomm/dbusproxy.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <glib.h>

#include "locker.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"
#include "fingerprint.hpp"

/**
 * Fingerprint Reader Auth
 *
 * Based on fprintd DBUS Spec:
 * https://fprint.freedesktop.org/fprintd-dev/
 */

WayfireLockerFingerprintPlugin::WayfireLockerFingerprintPlugin() :
    WayfireLockerPlugin("locker/fingerprint")
{}

WayfireLockerFingerprintPlugin::~WayfireLockerFingerprintPlugin()
{
    if (device_proxy)
    {
        device_proxy->call_sync("Release");
    }
}

void WayfireLockerFingerprintPlugin::on_connection(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & name)
{
    /* Bail here if config has this disabled */
    if (!enable)
    {
        return;
    }

    Gio::DBus::Proxy::create(connection,
        "net.reactivated.Fprint",
        "/net/reactivated/Fprint/Manager",
        "net.reactivated.Fprint.Manager",
        [this, connection] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        manager_proxy = Gio::DBus::Proxy::create_finish(result);
        get_device();
    });
}

void WayfireLockerFingerprintPlugin::get_device()
{
    if (device_proxy != nullptr)
    {
        std::cerr << "get_device when device_proxy is not null" << std::endl;
        return;
    }

    try {
        auto default_device = manager_proxy->call_sync("GetDefaultDevice");
        Glib::Variant<Glib::ustring> item_path;
        default_device.get_child(item_path, 0);
        Gio::DBus::Proxy::create(connection,
            "net.reactivated.Fprint",
            item_path.get(),
            "net.reactivated.Fprint.Device",
            sigc::mem_fun(*this, &WayfireLockerFingerprintPlugin::on_device_acquired));
    } catch (Glib::Error & e) /* TODO : Narrow down? */
    {
        hide();
        return;
    }
}

void WayfireLockerFingerprintPlugin::on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result)
{
    device_proxy = Gio::DBus::Proxy::create_finish(result);
    update("Listing fingers...", "system-search-symbolic", "info");
    char *username = getlogin();
    auto reply     = device_proxy->call_sync("ListEnrolledFingers", nullptr,
        Glib::Variant<std::tuple<Glib::ustring>>::create(username));
    Glib::Variant<std::vector<Glib::ustring>> array;
    reply.get_child(array, 0);

    if (array.get_n_children() == 0)
    {
        // Zero fingers for this user.
        show();
        update("No fingerprints enrolled", "dialog-error-symbolic", "bad");
        // Don't hide entirely, allow the user to see this specific fail
        return;
    }

    show();
    update("Fingerprint Device Ready", "system-search-symbolic", "info");
    Glib::Variant<Glib::ustring> finger;
    reply.get_child(finger, 0);

    start_fingerprint_scanning();
}

void WayfireLockerFingerprintPlugin::start_fingerprint_scanning()
{
    if (!enable)
    {
        return;
    }

    if (WayfireLockerApp::get().is_locked_out())
    {
        return;
    }

    if (signal)
    {
        signal.disconnect();
    }

    signal = device_proxy->signal_signal().connect([this] (const Glib::ustring & sender_name,
                                                           const Glib::ustring & signal_name,
                                                           const Glib::VariantContainerBase & params)
    {
        if (device_proxy == nullptr)
        {
            return; // Skip close-down messages
        }

        if (signal_name == "VerifyStatus")
        {
            Glib::Variant<Glib::ustring> mesg;
            Glib::Variant<bool> done;
            params.get_child(mesg, 0);
            params.get_child(done, 1);
            bool is_done = done.get();

            update(mesg.get(), "system-search-symbolic", "info");
            if (mesg.get() == "verify-match")
            {
                std::cout << "Match" << std::endl;
                stop_fingerprint_scanning();
                WayfireLockerApp::get().perform_unlock("Fingerprint verified");
                return;
            }

            if (mesg.get() == "verify-no-match")
            {
                std::cout << "No match" << std::endl;
                show();
                update("Invalid fingerprint", "dialog-error-symbolic", "bad");
                stop_fingerprint_scanning();
                WayfireLockerApp::get().recieved_bad_auth();

                /* Reschedule fingerprint scan */
                starting_fingerprint = Glib::signal_timeout().connect(
                    [this] ()
                {
                    this->start_fingerprint_scanning();
                    return G_SOURCE_REMOVE;
                }, 500);
                return;
            } else if (mesg.get() == "verify-unknown-error")
            {
                update("Unknown error", "dialog-error-symbolic", "info");
                stop_fingerprint_scanning();
                /* Reschedule fingerprint scan */
                starting_fingerprint = Glib::signal_timeout().connect(
                    [this] ()
                {
                    this->start_fingerprint_scanning();
                    return G_SOURCE_REMOVE;
                }, 500);
                return;
            } else if (mesg.get() == "verify-disconnected")
            {
                update("Reader disconnected", "dialog-error-symbolic", "bad");
                /* This needs testing on a machine with removable fprint reader */
                device_proxy = nullptr;
                if (signal)
                {
                    signal.disconnect();
                }

                /* Delay, cools down a repeated failing loop */
                finding_new_device = Glib::signal_timeout().connect_seconds(
                    [this] ()
                {
                    get_device();
                    return G_SOURCE_REMOVE;
                }, 2);
                return;
            }

            if (is_done && (device_proxy != nullptr))
            {
                stop_fingerprint_scanning();
            }
        } else if (signal_name == "VerifyFingerSelected")
        {
            /* TODO potentially present this to user to tell them
             *  which fingerprint is being expected */
            Glib::Variant<Glib::ustring> finger_name;
            params.get_child(finger_name, 0);
            std::cout << "Finger : " << finger_name.get() << std::endl;
        }
    }, false);
    if (device_proxy)
    {
        if (starting_fingerprint)
        {
            starting_fingerprint.disconnect();
        }

        try {
            char *username = getlogin();
            device_proxy->call_sync("Claim", nullptr,
                Glib::Variant<std::tuple<Glib::ustring>>::create({username}));
        } catch (Glib::Error & e) /* TODO : Narrow down? */
        {
            stop_fingerprint_scanning();
            std::cout << "Fingerprint device already claimed, try in 3s" << std::endl;
            update("Fingerprint reader busy...", "dialog-error-symbolic", "info");
            Glib::signal_timeout().connect_seconds(
                [this] ()
            {
                this->start_fingerprint_scanning();
                return G_SOURCE_REMOVE;
            }, 3);
            return;
        }

        show();
        update("Use fingerprint to unlock", "process-completed-symbolic", "good");
        device_proxy->call_sync("VerifyStart",
            nullptr,
            Glib::Variant<std::tuple<Glib::ustring>>::create({""}));
    } else
    {
        update("Unable to start fingerprint scan", "dialog-error-symbolic", "bad");
        hide();
    }
}

void WayfireLockerFingerprintPlugin::stop_fingerprint_scanning()
{
    if (starting_fingerprint)
    {
        starting_fingerprint.disconnect();
    }

    if (!device_proxy)
    {
        return;
    }

    if (signal)
    {
        signal.disconnect();
    }

    /* Stop if running. Eventually log or respond to errors
     *   but for now, avoid crashing lockscreen on close-down */
    try {
        device_proxy->call_sync("VerifyStop");
        device_proxy->call_sync("Release");
    } catch (Glib::Error & e)
    {}
}

void WayfireLockerFingerprintPlugin::init()
{
    auto cancellable = Gio::Cancellable::create();
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
    if (!connection)
    {
        std::cerr << "Failed to connect to dbus" << std::endl;
        return;
    }

    Gio::DBus::Proxy::create(
        connection,
        "net.reactivated.Fprint",
        "/net/reactivated/Fprint/Manager",
        "net.reactivated.Fprint.Manager",
        [this] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        auto manager_proxy = Gio::DBus::Proxy::create_finish(result);
        try {
            auto default_device = manager_proxy->call_sync("GetDefaultDevice");
            Glib::Variant<Glib::ustring> item_path;
            default_device.get_child(item_path, 0);
            Gio::DBus::Proxy::create(connection,
                "net.reactivated.Fprint",
                item_path.get(),
                "net.reactivated.Fprint.Device",
                sigc::mem_fun(*this, &WayfireLockerFingerprintPlugin::on_device_acquired));
        } catch (Glib::Error & e) /* TODO : Narrow down? */
        {
            hide();
            return;
        }
    });
}

void WayfireLockerFingerprintPlugin::deinit()
{
    // Ensure we return state to allow other programs to use scanner
    stop_fingerprint_scanning();
    if (signal)
    {
        signal.disconnect();
    }

    if (starting_fingerprint)
    {
        starting_fingerprint.disconnect();
    }

    if (finding_new_device)
    {
        finding_new_device.disconnect();
    }

    device_proxy = nullptr;
}

WayfireLockerFingerprintPluginWidget::WayfireLockerFingerprintPluginWidget(std::string label_contents,
    std::string image_contents,
    std::string color_contents) :
    WayfireLockerTimedRevealer("locker/fingerprint_always")
{
    set_child(box);
    image_print.set_from_icon_name("auth-fingerprint-symbolic");
    image_overlay.set_from_icon_name(image_contents);
    label.set_label(label_contents);
    overlay.set_child(image_print);
    overlay.add_overlay(image_overlay);
    overlay.set_hexpand(false);
    overlay.set_halign(Gtk::Align::CENTER);
    image_overlay.set_halign(Gtk::Align::END);
    image_overlay.set_valign(Gtk::Align::END);

    overlay.add_css_class("fingerprint-overlay");
    image_print.add_css_class("fingerprint-icon");
    image_overlay.add_css_class("fingerprint-overlay-image");
    label.add_css_class("fingerprint-text");
    if (color_contents != "")
    {
        image_overlay.add_css_class(color_contents);
    }

    box.set_orientation(Gtk::Orientation::VERTICAL);
    box.append(overlay);
    box.append(label);
}

void WayfireLockerFingerprintPlugin::lockout_changed(bool lockout)
{
    if (lockout)
    {
        update("Too many attempts", "dialog-error-symbolic", "bad");
        stop_fingerprint_scanning();
    } else
    {
        if (enable && device_proxy)
        {
            show();
            start_fingerprint_scanning();
        }
    }
}

void WayfireLockerFingerprintPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerFingerprintPluginWidget(label_contents, icon_contents,
        color_contents));
    auto widget = widgets[id];
    if (!show_state)
    {
        widget->hide();
    }

    grid->attach(*widget, position);
}

void WayfireLockerFingerprintPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

void WayfireLockerFingerprintPlugin::update(std::string label, std::string image, std::string color)
{
    icon_contents  = image;
    label_contents = label;
    color_contents = color;

    for (auto& it : widgets)
    {
        it.second->label.set_label(label);
        auto widget = &it.second->image_overlay;
        widget->set_from_icon_name(image);
        widget->remove_css_class("info");
        widget->remove_css_class("bad");
        widget->remove_css_class("good");
        widget->add_css_class(color);
    }
}

void WayfireLockerFingerprintPlugin::hide()
{
    show_state = false;
    for (auto & it : widgets)
    {
        it.second->hide();
    }
}

void WayfireLockerFingerprintPlugin::show()
{
    show_state = true;
    for (auto & it : widgets)
    {
        it.second->show();
    }
}
