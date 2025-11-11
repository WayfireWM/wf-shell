#include "network.hpp"
#include <glibmm/spawn.h>
#include <cassert>
#include <iostream>
#include <gtk-utils.hpp>
#include <sstream>
#include <vector>
#include <algorithm>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

std::string WfNetworkConnectionInfo::get_control_center_section(DBusProxy& nm)
{
    Glib::Variant<bool> wifi;
    nm->get_cached_property(wifi, "WirelessEnabled");

    return wifi.get() ? "wifi" : "network";
}

void WfNetworkConnectionInfo::spawn_control_center(DBusProxy& nm)
{
    std::string command = "env XDG_CURRENT_DESKTOP=GNOME gnome-control-center ";
    command += get_control_center_section(nm);

    Glib::spawn_command_line_async(command);
}

/* --- ConnectionInfo implementations (same as before) --- */

struct NoConnectionInfo : public WfNetworkConnectionInfo
{
    std::string get_icon_name(WfConnectionState state)
    {
        return "network-offline-symbolic";
    }

    int get_connection_strength() { return 0; }
    std::string get_strength_str() { return "none"; }
    std::string get_ip() { return "127.0.0.1"; }
    virtual ~NoConnectionInfo() {}
};

struct WifiConnectionInfo : public WfNetworkConnectionInfo
{
    WayfireNetworkInfo *widget;
    DBusProxy ap;

    WifiConnectionInfo(const DBusConnection& connection, std::string path,
        WayfireNetworkInfo *widget)
    {
        this->widget = widget;

        ap = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path,
            "org.freedesktop.NetworkManager.AccessPoint");

        if (ap)
        {
            ap->signal_properties_changed().connect(
                sigc::mem_fun(*this, &WifiConnectionInfo::on_properties_changed));
        }
    }

    void on_properties_changed(DBusPropMap changed, DBusPropList invalid)
    {
        bool needs_refresh = false;
        for (auto& prop : changed)
        {
            if (prop.first == STRENGTH)
            {
                needs_refresh = true;
            }
        }

        if (needs_refresh)
        {
            widget->update_icon();
            widget->update_status();
        }
    }

    int get_strength()
    {
        assert(ap);

        Glib::Variant<guchar> vstr;
        ap->get_cached_property(vstr, STRENGTH);

        return vstr.get();
    }

    std::string get_strength_str()
    {
        int value = get_strength();

        if (value > 80) return "excellent";
        if (value > 55) return "good";
        if (value > 30) return "ok";
        if (value > 5)  return "weak";
        return "none";
    }

    virtual std::string get_icon_name(WfConnectionState state)
    {
        if ((state <= CSTATE_ACTIVATING) || (state == CSTATE_DEACTIVATING))
            return "network-wireless-acquiring-symbolic";

        if (state == CSTATE_DEACTIVATED)
            return "network-wireless-disconnected-symbolic";

        if (ap)
            return "network-wireless-signal-" + get_strength_str() + "-symbolic";
        else
            return "network-wireless-no-route-symbolic";
    }

    virtual int get_connection_strength()
    {
        if (ap) return get_strength();
        return 100;
    }

    virtual std::string get_ip() { return "0.0.0.0"; }
    virtual ~WifiConnectionInfo() {}
};

struct EthernetConnectionInfo : public WfNetworkConnectionInfo
{
    DBusProxy ap;
    EthernetConnectionInfo(const DBusConnection& connection, std::string path) {}

    virtual std::string get_icon_name(WfConnectionState state)
    {
        if ((state <= CSTATE_ACTIVATING) || (state == CSTATE_DEACTIVATING))
            return "network-wired-acquiring-symbolic";

        if (state == CSTATE_DEACTIVATED)
            return "network-wired-disconnected-symbolic";

        return "network-wired-symbolic";
    }

    std::string get_connection_name() { return "Ethernet - " + connection_name; }
    std::string get_strength_str() { return "excellent"; }
    virtual int get_connection_strength() { return 100; }
    virtual std::string get_ip() { return "0.0.0.0"; }
    virtual ~EthernetConnectionInfo() {}
};

/* --- connection state helpers --- */

static WfConnectionState get_connection_state(DBusProxy connection)
{
    if (!connection) return CSTATE_DEACTIVATED;

    Glib::Variant<guint32> state;
    connection->get_cached_property(state, "State");
    return (WfConnectionState)state.get();
}

void WayfireNetworkInfo::update_icon()
{
    // Defensive checks to avoid crashes during early init or missing DBus info
    if (!info)
    {
        // ensure info is always valid
        info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
        info->connection_name = "No connection";
        std::cerr << "[network] update_icon: info was null, created NoConnectionInfo" << std::endl;
    }

    // get connection state safely
    WfConnectionState state = CSTATE_DEACTIVATED;
    try
    {
        state = get_connection_state(active_connection_proxy);
    }
    catch (...)
    {
        // shouldn't normally throw, but guard anyway
        std::cerr << "[network] update_icon: exception in get_connection_state()" << std::endl;
        state = CSTATE_DEACTIVATED;
    }

    // Attempt to ask the info object for icon name, but guard unexpected exceptions
    std::string icon_name;
    try
    {
        icon_name = info->get_icon_name(state);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[network] update_icon: exception in get_icon_name(): " << e.what() << std::endl;
        icon_name = "network-offline-symbolic";
    }
    catch (...)
    {
        std::cerr << "[network] update_icon: unknown exception in get_icon_name()" << std::endl;
        icon_name = "network-offline-symbolic";
    }

    // Final sanity: if icon_name is empty, use fallback
    if (icon_name.empty())
        icon_name = "network-offline-symbolic";

    icon.set_from_icon_name(icon_name);
}


/* optional color helper omitted (not used) */

void WayfireNetworkInfo::update_status()
{
    std::string description = info->get_connection_name();
    status.set_text(description);
    button->set_tooltip_text(description);

    status.get_style_context()->remove_class("excellent");
    status.get_style_context()->remove_class("good");
    status.get_style_context()->remove_class("weak");
    status.get_style_context()->remove_class("none");
    if (status_color_opt)
    {
        status.get_style_context()->add_class(info->get_strength_str());
    }
}

void WayfireNetworkInfo::update_active_connection()
{
    Glib::Variant<Glib::ustring> active_conn_path;
    nm_proxy->get_cached_property(active_conn_path, ACTIVE_CONNECTION);

    if (active_conn_path && (active_conn_path.get() != "/"))
    {
        active_connection_proxy = Gio::DBus::Proxy::create_sync(
            connection, NM_DBUS_NAME, active_conn_path.get(),
            "org.freedesktop.NetworkManager.Connection.Active");
    } else
    {
        active_connection_proxy = DBusProxy();
    }

    auto set_no_connection = [=] ()
    {
        info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
        info->connection_name = "No connection";
    };

    if (!active_connection_proxy)
    {
        set_no_connection();
    } else
    {
        Glib::Variant<Glib::ustring> vtype, vobject;
        active_connection_proxy->get_cached_property(vtype, "Type");
        active_connection_proxy->get_cached_property(vobject, "SpecificObject");
        auto type   = vtype.get();
        auto object = vobject.get();

        if (type.find("wireless") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo>(
                new WifiConnectionInfo(connection, object, this));
        } else if (type.find("ethernet") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo>(
                new EthernetConnectionInfo(connection, object));
        } else if (type.find("bluetooth"))
        {
            std::cout << "Unimplemented: bluetooth connection" << std::endl;
            set_no_connection();
        } else
        {
            std::cout << "Unimplemented: unknown connection type" << std::endl;
            set_no_connection();
        }

        Glib::Variant<Glib::ustring> vname;
        active_connection_proxy->get_cached_property(vname, "Id");
        info->connection_name = vname.get();
    }

    update_icon();
    update_status();
}

void WayfireNetworkInfo::on_nm_properties_changed(
    const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    for (auto & prop : properties)
    {
        if (prop.first == ACTIVE_CONNECTION)
        {
            update_active_connection();
        }
    }
}

bool WayfireNetworkInfo::setup_dbus()
{
    auto cancellable = Gio::Cancellable::create();
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
    if (!connection)
    {
        std::cerr << "Failed to connect to dbus" << std::endl;
        return false;
    }

    nm_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME,
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.NetworkManager");
    if (!nm_proxy)
    {
        std::cerr << "Failed to connect to network manager, " <<
            "are you sure it is running?" << std::endl;
        return false;
    }

    nm_proxy->signal_properties_changed().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::on_nm_properties_changed));

    return true;
}

/* --- nmcli helper and parser --- */

struct WifiEntry { std::string ssid; int signal; std::string security; };

static std::string run_cmd_capture_stdout(const std::string& cmd, int *exit_status = nullptr)
{
    std::string stdout_out;
    int status = 0;
    try
    {
        Glib::spawn_command_line_sync(cmd, &stdout_out, nullptr, &status);
    }
    catch (const Glib::SpawnError& e)
    {
        std::cerr << "spawn error: " << e.what() << std::endl;
        if (exit_status) *exit_status = -1;
        return std::string();
    }

    if (exit_status) *exit_status = status;
    return stdout_out;
}

static std::vector<WifiEntry> parse_nmcli_wifi_list(const std::string& data)
{
    std::vector<WifiEntry> out;
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.empty()) continue;
        // nmcli -t uses ':' as separator. SSIDs may contain ':' rarely but we keep it simple here.
        size_t p1 = line.find(':');
        if (p1 == std::string::npos) continue;
        size_t p2 = line.find(':', p1 + 1);
        if (p2 == std::string::npos) continue;
        std::string ssid = line.substr(0, p1);
        std::string sigs = line.substr(p1 + 1, p2 - p1 - 1);
        std::string sec  = line.substr(p2 + 1);
        int sig = 0;
        try { sig = std::stoi(sigs); } catch (...) { sig = 0; }
        // trim newline\r
        ssid.erase(std::remove_if(ssid.begin(), ssid.end(), [](unsigned char c){ return c == '\r' || c == '\n'; }), ssid.end());
        out.push_back({ssid, sig, sec});
    }
    return out;
}

/* --- Popover handling --- */

void WayfireNetworkInfo::show_wifi_popover()
{
    auto popover = button->get_popover();

    // ensure popover content is set
    if (!popover->get_child())
    {
        pop_list_box.set_orientation(Gtk::Orientation::VERTICAL);
        pop_list_box.set_spacing(4);
        pop_list_box.set_margin(6);

        pop_scrolled.set_child(pop_list_box);
        pop_scrolled.set_min_content_height(200);
        pop_scrolled.set_min_content_width(320);

        popover_box.set_orientation(Gtk::Orientation::VERTICAL);
        popover_box.set_spacing(6);
        popover_box.append(pop_scrolled);

        // prepare password box (hidden until needed)
        pop_pass_box.set_orientation(Gtk::Orientation::VERTICAL);
        pop_pass_box.set_spacing(4);
        pop_pass_box.set_margin_top(6);
        pop_pass_box.set_margin_bottom(6);

        popover->set_child(popover_box);
        popover->set_size_request(320, 240);
        popover->get_style_context()->add_class("network-popover");
    }

    populate_wifi_list();

    // show
    button->set_keyboard_interactive(false);
    popover->popup();
}

void WayfireNetworkInfo::populate_wifi_list()
{
    // clear pop_list_box children
    auto children = pop_list_box.get_children();
    for (auto *c : children)
        pop_list_box.remove(*c);

    // header
    auto header = Gtk::make_managed<Gtk::Label>("Available Wi-Fi networks");
    header->set_margin_bottom(6);
    pop_list_box.append(*header);

    // run nmcli
    int st = 0;
    std::string out = run_cmd_capture_stdout("nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list", &st);
    if (out.empty())
    {
        auto lbl = Gtk::make_managed<Gtk::Label>("Could not list networks (is nmcli installed?)");
        lbl->set_margin(6);
        pop_list_box.append(*lbl);

        auto open_btn = Gtk::make_managed<Gtk::Button>("Open network settings");
        open_btn->signal_clicked().connect([this]() {
            info->spawn_control_center(nm_proxy);
            button->get_popover()->popdown();
        });
        pop_list_box.append(*open_btn);
        return;
    }

    auto entries = parse_nmcli_wifi_list(out);
    if (entries.empty())
    {
        auto lbl = Gtk::make_managed<Gtk::Label>("No networks found");
        lbl->set_margin(6);
        pop_list_box.append(*lbl);
        return;
    }

    // sort by signal desc, then unique SSID
    std::sort(entries.begin(), entries.end(), [](const WifiEntry &a, const WifiEntry &b) {
        return a.signal > b.signal;
    });

    std::string last_ssid;
    for (auto &e : entries)
    {
        if (e.ssid == last_ssid) continue; // dedupe identical SSID rows
        last_ssid = e.ssid;

        std::ostringstream label;
        label << (e.ssid.empty() ? "<hidden>" : e.ssid) << " (" << e.signal << "%)";
        if (!e.security.empty()) label << " [" << e.security << "]";

        auto btn = Gtk::make_managed<Gtk::Button>(label.str());
        btn->set_halign(Gtk::Align::FILL);

        // capture ssid+security by value
        btn->signal_clicked().connect([this, ssid = e.ssid, sec = e.security]() {
            bool secured = (sec.find("WPA") != std::string::npos) ||
                           (sec.find("WEP") != std::string::npos) ||
                           (sec.find("RSN") != std::string::npos);
            if (!secured)
            {
                attempt_connect_ssid(ssid, "");
                button->get_popover()->popdown();
            }
            else
            {
                show_password_prompt_for(ssid, sec);
            }
        });

        pop_list_box.append(*btn);
    }

    // small spacer and status label
    pop_status_label.set_text("");
    pop_status_label.set_margin_top(6);
    pop_list_box.append(pop_status_label);
}

void WayfireNetworkInfo::show_password_prompt_for(const std::string& ssid, const std::string& security)
{
    // clear popover_box and replace scroll with a password UI inside popover_box
    auto popover = button->get_popover();

    // remove all children of popover_box
    auto pb_children = popover_box.get_children();
    for (auto *c : pb_children) popover_box.remove(*c);

    // title
    auto title = Gtk::make_managed<Gtk::Label>("Connect to: " + (ssid.empty() ? "<hidden>" : ssid));
    title->set_margin_bottom(6);
    popover_box.append(*title);

    // password entry
    auto pass_label = Gtk::make_managed<Gtk::Label>("Password:");
    popover_box.append(*pass_label);

    auto entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_visibility(false);
    entry->set_hexpand(true);
    popover_box.append(*entry);

    // buttons box
    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    hbox->set_halign(Gtk::Align::END);
    auto cancel_btn = Gtk::make_managed<Gtk::Button>("Cancel");
    auto connect_btn = Gtk::make_managed<Gtk::Button>("Connect");

    cancel_btn->signal_clicked().connect([this]() {
        // restore list
        auto pb_children2 = popover_box.get_children();
        for (auto *c : pb_children2) popover_box.remove(*c);
        popover_box.append(pop_scrolled);
        populate_wifi_list();
    });

    connect_btn->signal_clicked().connect([this, ssid, entry]() {
        std::string pwd = entry->get_text();
        if (pwd.empty())
        {
            pop_status_label.set_text("Password cannot be empty");
            return;
        }
        pop_status_label.set_text("Connecting...");
        // attempt connect (synchronous call to nmcli; it's quick)
        attempt_connect_ssid(ssid, pwd);
        button->get_popover()->popdown();
    });

    hbox->append(*cancel_btn);
    hbox->append(*connect_btn);
    popover_box.append(*hbox);

    // status label
    pop_status_label.set_text("");
    popover_box.append(pop_status_label);

    popover->present(); // show updated content
}

void WayfireNetworkInfo::attempt_connect_ssid(const std::string& ssid, const std::string& password)
{
    if (ssid.empty())
    {
        std::cerr << "Empty SSID, aborting connect" << std::endl;
        return;
    }

    std::string cmd = "nmcli device wifi connect \"" + ssid + "\"";
    if (!password.empty())
    {
        // Escape double quotes in password naively
        std::string esc = password;
        size_t pos = 0;
        while ((pos = esc.find('"', pos)) != std::string::npos) { esc.replace(pos, 1, "\\\""); pos += 2; }
        cmd += " password \"" + esc + "\"";
    }

    int exit_status = 0;
    std::string output = run_cmd_capture_stdout(cmd, &exit_status);
    if (exit_status != 0)
    {
        std::cerr << "nmcli connect failed: " << output << std::endl;
        // if popover exists, show temporary error
        pop_status_label.set_text("Failed to connect. Check password or settings.");
    }
    else
    {
        pop_status_label.set_text("Connection started.");
    }

    // trigger an update of state (NetworkManager DBus will also update via signals)
    update_active_connection();
}

/* --- widget lifecycle --- */

void WayfireNetworkInfo::on_click()
{
    if ((std::string)click_command_opt != "default")
    {
        Glib::spawn_command_line_async((std::string)click_command_opt);
    } else
    {
        show_wifi_popover();
    }
}

void WayfireNetworkInfo::init(Gtk::Box *container)
{
    if (!setup_dbus())
    {
        enabled = false;
        return;
    }

    // create WayfireMenuButton
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("network");
    button->get_children()[0]->get_style_context()->add_class("flat");

    update_icon();
    button->set_child(icon);
    container->append(*button);

    button->set_tooltip_text("Click to open Wi-Fi selector");

    // connect click to show popover (also support middle-click etc if needed)
    button->signal_clicked().connect(sigc::mem_fun(*this, &WayfireNetworkInfo::on_click));

    button_content.set_valign(Gtk::Align::CENTER);
    button_content.append(icon);
    button_content.append(status);
    button_content.set_spacing(6);

    icon.set_valign(Gtk::Align::CENTER);
    icon.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::update_icon));
    icon.get_style_context()->add_class("network-icon");

    update_active_connection();
    handle_config_reload();
}

void WayfireNetworkInfo::handle_config_reload()
{
    if (status_opt.value() == NETWORK_STATUS_ICON)
    {
        if (status.get_parent())
        {
            button_content.remove(status);
        }
    } else
    {
        if (!status.get_parent())
        {
            button_content.append(status);
        }
    }

    update_icon();
    update_status();
}

WayfireNetworkInfo::~WayfireNetworkInfo()
{}
