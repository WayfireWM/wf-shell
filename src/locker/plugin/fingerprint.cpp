#include <giomm.h>
#include <giomm/dbusproxy.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <memory>
#include <string>
#include <iostream>
#include "../../util/wf-option-wrap.hpp"
#include "glib.h"
#include "locker.hpp"
#include "fingerprint.hpp"


WayfireLockerFingerprintPlugin::WayfireLockerFingerprintPlugin() :
    dbus_name_id(Gio::DBus::own_name(Gio::DBus::BusType::SYSTEM,
        "net.reactivated.Fprint",
        sigc::mem_fun(*this, &WayfireLockerFingerprintPlugin::on_bus_acquired))),
    enable(WfOption<bool>{"locker/fingerprint_enable"})
{}

WayfireLockerFingerprintPlugin::~WayfireLockerFingerprintPlugin()
{
    if (device_proxy)
    {
        device_proxy->call_sync("Release");
    }
}

void WayfireLockerFingerprintPlugin::on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection,
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
        auto variant = manager_proxy->call_sync("GetDevices");
        /* Decant the array from the tuple, count devices */
        Glib::Variant<std::vector<Glib::VariantBase>> array;
        variant.get_child(array, 0);
        if (array.get_n_children() == 0)
        {
            enable = false;
            hide();
            return;
        }

        auto default_device = manager_proxy->call_sync("GetDefaultDevice");
        Glib::Variant<Glib::ustring> item_path;
        default_device.get_child(item_path, 0);
        Gio::DBus::Proxy::create(connection,
            "net.reactivated.Fprint",
            item_path.get(),
            "net.reactivated.Fprint.Device",
            sigc::mem_fun(*this, &WayfireLockerFingerprintPlugin::on_device_acquired));
    });
}

void WayfireLockerFingerprintPlugin::on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result)
{
    device_proxy = Gio::DBus::Proxy::create_finish(result);
    char *username = getlogin();
    update_labels("Listing fingers...");
    auto reply = device_proxy->call_sync("ListEnrolledFingers", nullptr,
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
    device_proxy->signal_signal().connect([this] (const Glib::ustring & sender_name,
                                                  const Glib::ustring & signal_name,
                                                  const Glib::VariantContainerBase & params)
    {
        if (signal_name == "VerifyStatus")
        {
            Glib::Variant<Glib::ustring> mesg;
            Glib::Variant<bool> done;
            params.get_child(mesg, 0);
            params.get_child(done, 1);
            update_labels(mesg.get());
            if (mesg.get() == "verify-match")
            {
                WayfireLockerApp::get().unlock();
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
            }

            if (done.get())
            {
                is_scanning = false;
                device_proxy->call_sync("VerifyStop");
            }
        }
    }, false);
    device_proxy->call_sync("Claim", nullptr, Glib::Variant<std::tuple<Glib::ustring>>::create({username}));
    finger_name = finger.get();

    /* Start fingerprint reader 5 seconds after start
     *  This fixes an issue where the fingerprint reader
     *  is a button which locks the screen */
    Glib::signal_timeout().connect_seconds(
        [this] ()
    {
        this->start_fingerprint_scanning();
        return G_SOURCE_REMOVE;
    }, 5);
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
            Glib::Variant<std::tuple<Glib::ustring>>::create({finger_name}));
    } else
    {
        update_labels("Unable to start fingerprint scan");
    }
}

void WayfireLockerFingerprintPlugin::init()
{}

void WayfireLockerFingerprintPlugin::add_output(int id, Gtk::Grid *grid)
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

    Gtk::Box *box = get_plugin_position(WfOption<std::string>{"locker/fingerprint_position"}, grid);
    box->append(*image);

    box->append(*label);
}

void WayfireLockerFingerprintPlugin::remove_output(int id)
{
    labels.erase(id);
    images.erase(id);
}

bool WayfireLockerFingerprintPlugin::should_enable()
{
    return enable;
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
