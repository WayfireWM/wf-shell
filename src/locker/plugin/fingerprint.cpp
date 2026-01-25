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
#include "fingerprint.hpp"

/**
 * Fingerprint Reader Auth
 *
 * Based on fprintd DBUS Spec:
 * https://fprint.freedesktop.org/fprintd-dev/
 */

WayfireLockerFingerprintPlugin::WayfireLockerFingerprintPlugin() :
    WayfireLockerPlugin("locker/fingerprint_enable", "locker/fingerprint_position")
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

void WayfireLockerFingerprintPlugin::on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result)
{
    device_proxy = Gio::DBus::Proxy::create_finish(result);
    update_labels("Listing fingers...");
    char *username = getlogin();
    auto reply     = device_proxy->call_sync("ListEnrolledFingers", nullptr,
        Glib::Variant<std::tuple<Glib::ustring>>::create(username));
    Glib::Variant<std::vector<Glib::ustring>> array;
    reply.get_child(array, 0);

    if (array.get_n_children() > 0)
    {
        // User has at least one fingerprint on file!
        show();
        update_labels("Fingerprint Ready");
        update_image("fingerprint");
    } else
    {
        // Zero fingers for this user.
        show();
        update_labels("No fingerprints enrolled");
        update_image("nofingerprint");
        // Don't hide entirely, allow the user to see this specific fail
        return;
    }

    Glib::Variant<Glib::ustring> finger;
    reply.get_child(finger, 0);
    update_labels("Finger print device found");

    /* Attach a listener now, useful when we start scanning */
    signal = device_proxy->signal_signal().connect([this] (const Glib::ustring & sender_name,
                                                  const Glib::ustring & signal_name,
                                                  const Glib::VariantContainerBase & params)
    {
        std::cout << signal_name << " " << device_proxy << std::endl;
        if (device_proxy==nullptr)
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

            update_labels(mesg.get());
            if (mesg.get() == "verify-match")
            {
                WayfireLockerApp::get().perform_unlock();
            }

            if (mesg.get() == "verify-no-match")
            {
                /* Reschedule fingerprint scan */
                Glib::signal_timeout().connect_seconds(
                    [this] ()
                {
                    this->start_fingerprint_scanning();
                    return G_SOURCE_REMOVE;
                }, 5);
                show();
                update_image("nofingerprint");
                update_labels("Invalid fingerprint");
            } else if (mesg.get() == "verify-unknown-error")
            {
                is_done = true;
                /* Reschedule fingerprint scan */
                Glib::signal_timeout().connect_seconds(
                    [this] ()
                {
                    this->start_fingerprint_scanning();
                    return G_SOURCE_REMOVE;
                }, 5);
            }

            if (is_done)
            {
                is_scanning = false;
                device_proxy->call_sync("VerifyStop");
            }
        } else if (signal_name == "VerifyFingerSelected")
        {
            Glib::Variant<Glib::ustring> finger_name;
            params.get_child(finger_name,0);
            std::cout << "Finger : " << finger_name.get() << std::endl;
        }
    }, false);
    claim_device();
}

void WayfireLockerFingerprintPlugin::claim_device()
{
    try {
        char *username = getlogin();
        device_proxy->call_sync("Claim", nullptr,
            Glib::Variant<std::tuple<Glib::ustring>>::create({username}));
        /* Start scanning in 5 seconds */
        Glib::signal_timeout().connect_seconds(
            [this] ()
        {
            this->start_fingerprint_scanning();
            return G_SOURCE_REMOVE;
        }, 5);
    } catch (Glib::Error & e) /* TODO : Narrow down? */
    {
        std::cout << "Fingerprint device already claimed, try in 5s" << std::endl;
        update_labels("Fingerprint reader busy...");
        Glib::signal_timeout().connect_seconds(
            [this] ()
        {
            this->claim_device();
            return G_SOURCE_REMOVE;
        }, 5);
    }

    /* Start fingerprint reader 5 seconds after start
     *  This fixes an issue where the fingerprint reader
     *  is a button which locks the screen  */
}

void WayfireLockerFingerprintPlugin::release_device()
{
    device_proxy->call_sync("Release");
}

void WayfireLockerFingerprintPlugin::start_fingerprint_scanning()
{
    if (!enable)
    {
        return;
    }

    if (device_proxy && !is_scanning)
    {
        show();
        update_labels("Use fingerprint to unlock");
        is_scanning = true;
        device_proxy->call_sync("VerifyStart",
            nullptr,
            Glib::Variant<std::tuple<Glib::ustring>>::create({""}));
    } else
    {
        update_labels("Unable to start fingerprint scan");
    }
}

void WayfireLockerFingerprintPlugin::stop_fingerprint_scanning()
{
    device_proxy->call_sync("VerifyStop");
    is_scanning = false;
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
    if(is_scanning)
    {
        stop_fingerprint_scanning();
    }
    release_device();
    if(signal)
    {
        signal.disconnect();
    }
    device_proxy = nullptr;
}

void WayfireLockerFingerprintPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    labels.emplace(id, std::shared_ptr<Gtk::Label>(new Gtk::Label()));
    images.emplace(id, std::shared_ptr<Gtk::Image>(new Gtk::Image()));

    auto image = images[id];
    auto label = labels[id];

    if (!show_state)
    {
        image->hide();
        label->hide();
    }

    image->set_from_icon_name("fingerprint");
    label->set_label("No Fingerprint device found");

    image->add_css_class("fingerprint-icon");
    label->add_css_class("fingerprint-text");

    grid->attach(*image, position);
    grid->attach(*label, position);
}

void WayfireLockerFingerprintPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*labels[id]);
    grid->remove(*images[id]);
    labels.erase(id);
    images.erase(id);
}

void WayfireLockerFingerprintPlugin::update_image(std::string image)
{
    for (auto& it : images)
    {
        it.second->set_from_icon_name(image);
    }

    icon_contents = image;
}

void WayfireLockerFingerprintPlugin::update_labels(std::string text)
{
    for (auto& it : labels)
    {
        it.second->set_label(text);
    }

    label_contents = text;
}

void WayfireLockerFingerprintPlugin::hide()
{
    show_state = false;
    for (auto& it : labels)
    {
        it.second->hide();
    }

    for (auto& it : images)
    {
        it.second->hide();
    }
}

void WayfireLockerFingerprintPlugin::show()
{
    show_state = true;
    for (auto& it : labels)
    {
        it.second->show();
    }

    for (auto& it : images)
    {
        it.second->show();
    }
}
