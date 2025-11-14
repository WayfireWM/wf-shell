#include "network.hpp"
#include <algorithm>
#include <cassert>
#include <glibmm/spawn.h>
#include <gtk-utils.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <giomm.h>
#include <glibmm.h>
#include <gio/gio.h> // sometimes needed for lower-level variant helpers

#include <glibmm/variant.h>
#include <glibmm/varianttype.h>
#include <glib.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>

#include <giomm/init.h>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

struct NetworkInfo
{
  std::string ssid;
  std::string path;
  int strength = 0;     // Wi-Fi signal strength (0–100)
  bool secured = false; // True if the network requires authentication
};

static std::vector<NetworkInfo> get_available_networks()
{
  std::vector<NetworkInfo> result;

  try
  {
    auto connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);

    auto nm_proxy = Gio::DBus::Proxy::create_for_bus_sync(
        Gio::DBus::BusType::SYSTEM,
        "org.freedesktop.NetworkManager",
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.NetworkManager");

    Glib::VariantContainerBase reply = nm_proxy->call_sync("GetDevices");
    auto device_child = reply.get_child(0);
    auto devices_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Glib::DBusObjectPathString>>>(device_child);
    std::vector<Glib::DBusObjectPathString> device_paths = devices_variant.get();

    for (const auto &path : device_paths)
    {
      auto dev_proxy = Gio::DBus::Proxy::create_for_bus_sync(
          Gio::DBus::BusType::SYSTEM,
          "org.freedesktop.NetworkManager",
          path,
          "org.freedesktop.NetworkManager.Device");

      // For glibmm-2.68, we use GetCachedProperty(const Glib::ustring&, Glib::VariantBase&)
      Glib::VariantBase dev_type_variant;
      dev_proxy->get_cached_property(dev_type_variant, "DeviceType"); // ✅ argument order

      guint32 dev_type = 0;
      Glib::Variant<guint32> vtype = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(dev_type_variant);
      dev_type = vtype.get();

      if (dev_type != 2) // NM_DEVICE_TYPE_WIFI
        continue;

      auto wifi_proxy = Gio::DBus::Proxy::create_for_bus_sync(
          Gio::DBus::BusType::SYSTEM,
          "org.freedesktop.NetworkManager",
          path,
          "org.freedesktop.NetworkManager.Device.Wireless");

      Glib::VariantContainerBase aps_reply = wifi_proxy->call_sync("GetAllAccessPoints");
      auto ap_child = aps_reply.get_child(0);
      auto ap_paths_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Glib::DBusObjectPathString>>>(ap_child);
      std::vector<Glib::DBusObjectPathString> ap_paths = ap_paths_variant.get();

      for (const auto &ap_path : ap_paths)
      {
        auto ap_proxy = Gio::DBus::Proxy::create_for_bus_sync(
            Gio::DBus::BusType::SYSTEM,
            "org.freedesktop.NetworkManager",
            ap_path,
            "org.freedesktop.NetworkManager.AccessPoint");

        Glib::VariantBase ssid_var, wpa_var, rsn_var;
        ap_proxy->get_cached_property(ssid_var, "Ssid"); // ✅ reversed order
        ap_proxy->get_cached_property(wpa_var, "WpaFlags");
        ap_proxy->get_cached_property(rsn_var, "RsnFlags");

        std::string ssid;
        if (ssid_var)
        {
          Glib::Variant<std::vector<guint8>> ssid_bytes =
              Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<guint8>>>(ssid_var);
          auto vec = ssid_bytes.get();
          ssid.assign(vec.begin(), vec.end());
        }

        guint32 wpa_flags = 0, rsn_flags = 0;
        if (wpa_var)
          wpa_flags = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(wpa_var).get();
        if (rsn_var)
          rsn_flags = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(rsn_var).get();

        std::string security = (wpa_flags || rsn_flags) ? "Secure" : "Open";

        if (!ssid.empty())
          result.push_back({ssid, security});
      }
    }
  }
  catch (const Glib::Error &ex)
  {
    std::cerr << "D-Bus error in get_available_networks(): " << ex.what() << std::endl;
  }

  return result;
}

std::string WfNetworkConnectionInfo::get_control_center_section(DBusProxy &nm)
{
  Glib::Variant<bool> wifi;
  nm->get_cached_property(wifi, "WirelessEnabled");

  return wifi.get() ? "wifi" : "network";
}

void WfNetworkConnectionInfo::spawn_control_center(DBusProxy &nm)
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

  void scan_networks_async();
  void update_network_list(const std::vector<std::string> &networks);

  void show_error(const std::string &message);
  WayfireNetworkInfo *widget;
  DBusProxy ap;

  WifiConnectionInfo(const DBusConnection &connection, std::string path,
                     WayfireNetworkInfo *widget)
  {
    this->widget = widget;

    ap = Gio::DBus::Proxy::create_sync(
        connection, NM_DBUS_NAME, path,
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
    for (auto &prop : changed)
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

    if (value > 80)
      return "excellent";
    if (value > 55)
      return "good";
    if (value > 30)
      return "ok";
    if (value > 5)
      return "weak";
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
    if (ap)
      return get_strength();
    return 100;
  }

  virtual std::string get_ip() { return "0.0.0.0"; }
  virtual ~WifiConnectionInfo() {}
};

struct EthernetConnectionInfo : public WfNetworkConnectionInfo
{
  DBusProxy ap;
  EthernetConnectionInfo(const DBusConnection &connection, std::string path) {}

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
  if (!connection)
    return CSTATE_DEACTIVATED;

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
    std::cerr
        << "[network] update_icon: info was null, created NoConnectionInfo"
        << std::endl;
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
    std::cerr << "[network] update_icon: exception in get_connection_state()"
              << std::endl;
    state = CSTATE_DEACTIVATED;
  }

  // Attempt to ask the info object for icon name, but guard unexpected
  // exceptions
  std::string icon_name;
  try
  {
    icon_name = info->get_icon_name(state);
  }
  catch (const std::exception &e)
  {
    std::cerr << "[network] update_icon: exception in get_icon_name(): "
              << e.what() << std::endl;
    icon_name = "network-offline-symbolic";
  }
  catch (...)
  {
    std::cerr << "[network] update_icon: unknown exception in get_icon_name()"
              << std::endl;
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
  }
  else
  {
    active_connection_proxy = DBusProxy();
  }

  auto set_no_connection = [=]()
  {
    info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
    info->connection_name = "No connection";
  };

  if (!active_connection_proxy)
  {
    set_no_connection();
  }
  else
  {
    Glib::Variant<Glib::ustring> vtype, vobject;
    active_connection_proxy->get_cached_property(vtype, "Type");
    active_connection_proxy->get_cached_property(vobject, "SpecificObject");
    auto type = vtype.get();
    auto object = vobject.get();

    if (type.find("wireless") != type.npos)
    {
      info = std::unique_ptr<WfNetworkConnectionInfo>(
          new WifiConnectionInfo(connection, object, this));
    }
    else if (type.find("ethernet") != type.npos)
    {
      info = std::unique_ptr<WfNetworkConnectionInfo>(
          new EthernetConnectionInfo(connection, object));
    }
    else if (type.find("bluetooth"))
    {
      std::cout << "Unimplemented: bluetooth connection" << std::endl;
      set_no_connection();
    }
    else
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
    const Gio::DBus::Proxy::MapChangedProperties &properties,
    const std::vector<Glib::ustring> &invalidated)
{
  for (auto &prop : properties)
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
  connection =
      Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
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
    std::cerr << "Failed to connect to network manager, "
              << "are you sure it is running?" << std::endl;
    return false;
  }

  nm_proxy->signal_properties_changed().connect(
      sigc::mem_fun(*this, &WayfireNetworkInfo::on_nm_properties_changed));

  return true;
}

/* --- iwd helper and parser --- */

struct WifiEntry
{
  std::string ssid;
  int signal;
  std::string security;
};

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
  // Clear the list box
  for (auto *child : pop_list_box.get_children())
    pop_list_box.remove(*child);

  // Header
  auto header = Gtk::make_managed<Gtk::Label>("Available Wi-Fi networks");
  header->set_margin_bottom(6);
  pop_list_box.append(*header);

  // Show scanning message
  pop_status_label.set_text("Scanning for networks…");
  pop_list_box.set_sensitive(false);

  // Launch network scan in background
  std::thread([this]()
              {
      auto networks = get_available_networks();

      Glib::signal_idle().connect_once([this, networks]() {
          if (networks.empty()) {
              auto lbl = Gtk::make_managed<Gtk::Label>("No networks found");
              lbl->set_margin(6);
              pop_list_box.append(*lbl);
          } else {
              for (auto &net : networks) {
                  std::ostringstream label;
                  label << net.ssid << " (" << net.strength << "%)";
                  if (net.secured)
                      label << " [Secured]";

                  auto btn = Gtk::make_managed<Gtk::Button>(label.str());
                  btn->set_halign(Gtk::Align::FILL);

                  // Capture SSID and secured flag
                  btn->signal_clicked().connect([this, ssid = net.ssid, secured = net.secured]() {
                      if (!secured) {
                          attempt_connect_ssid(ssid, "");
                          button->get_popover()->popdown();
                      } else {
                          show_password_prompt_for(ssid);
                      }
                  });

                  pop_list_box.append(*btn);
              }
          }

          pop_list_box.set_sensitive(true);
          pop_status_label.set_text(""); // Clear scanning message
      }); })
      .detach();

  // Status label at the bottom
  pop_status_label.set_margin_top(6);
  pop_list_box.append(pop_status_label);
}

void WayfireNetworkInfo::show_password_prompt_for(const std::string &ssid)
{
  // Clear popover content
  for (auto *child : popover_box.get_children())
    popover_box.remove(*child);

  auto popover = button->get_popover();

  // Title
  auto title = Gtk::make_managed<Gtk::Label>("Connect to: " + ssid);
  title->set_margin_bottom(6);
  popover_box.append(*title);

  // Password entry
  auto pass_label = Gtk::make_managed<Gtk::Label>("Password:");
  popover_box.append(*pass_label);

  auto entry = Gtk::make_managed<Gtk::Entry>();
  entry->set_visibility(false);
  entry->set_hexpand(true);
  popover_box.append(*entry);

  // Buttons
  auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  hbox->set_halign(Gtk::Align::END);

  auto cancel_btn = Gtk::make_managed<Gtk::Button>("Cancel");
  cancel_btn->signal_clicked().connect([this]()
                                       {
      // Restore Wi-Fi list
      for (auto *child : popover_box.get_children())
          popover_box.remove(*child);
      popover_box.append(pop_scrolled);
      populate_wifi_list(); });

  auto connect_btn = Gtk::make_managed<Gtk::Button>("Connect");
  connect_btn->signal_clicked().connect([this, ssid, entry, popover]()
                                        {
                                          std::string pwd = entry->get_text();
                                          if (pwd.empty())
                                          {
                                            pop_status_label.set_text("Password cannot be empty");
                                            return;
                                          }
                                          pop_status_label.set_text("Connecting...");
                                          attempt_connect_ssid(ssid, pwd);
                                          popover->popdown(); // ✅ now captured
                                        });

  hbox->append(*cancel_btn);
  hbox->append(*connect_btn);
  popover_box.append(*hbox);

  // Status label
  pop_status_label.set_text("");
  popover_box.append(pop_status_label);

  // Show updated popover
  popover->present();
}

void WayfireNetworkInfo::attempt_connect_ssid(const std::string &ssid,
                                              const std::string &password)
{
  if (ssid.empty() || ssid.find_first_not_of(" \t\r\n") == std::string::npos)
  {
    pop_status_label.set_text("Invalid SSID");
    return;
  }

  try
  {
    auto bus = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
    auto nm = Gio::DBus::Proxy::create_sync(bus,
                                            "org.freedesktop.NetworkManager",
                                            "/org/freedesktop/NetworkManager",
                                            "org.freedesktop.NetworkManager");

    // --- Find Wi-Fi device ---
    Glib::VariantBase devices_var;
    nm->get_cached_property(devices_var, "Devices");
    if (!devices_var)
    {
      pop_status_label.set_text("No devices");
      return;
    }

    auto dev_array = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Glib::DBusObjectPathString>>>(devices_var);
    std::string wifi_device_path;
    for (const auto &path : dev_array.get())
    {
      auto dev_proxy = Gio::DBus::Proxy::create_sync(bus, "org.freedesktop.NetworkManager", path, "org.freedesktop.NetworkManager.Device");
      if (!dev_proxy)
        continue;
      Glib::VariantBase type_v;
      dev_proxy->get_cached_property(type_v, "DeviceType");
      auto type_var = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(type_v);
      if (type_var && type_var.get() == 2)
      {
        wifi_device_path = path;
        break;
      }
    }

    if (wifi_device_path.empty())
    {
      pop_status_label.set_text("No Wi-Fi device");
      return;
    }

    // --- Build settings using glibmm (correct types) ---

    // SSID as byte array 'ay'
    std::vector<guint8> ssid_bytes(ssid.begin(), ssid.end());
    auto ssid_variant = Glib::Variant<std::vector<guint8>>::create(ssid_bytes);

    // UUID
    gchar *uuid_c = g_uuid_string_random();
    Glib::ustring uuid(uuid_c);
    g_free(uuid_c);

    // ----- connection (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> connection_map;
    connection_map["type"] = Glib::Variant<Glib::ustring>::create("802-11-wireless");
    connection_map["id"] = Glib::Variant<Glib::ustring>::create(ssid);
    connection_map["uuid"] = Glib::Variant<Glib::ustring>::create(uuid);

    // ----- 802-11-wireless (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> wifi_map;
    wifi_map["ssid"] = ssid_variant;
    wifi_map["mode"] = Glib::Variant<Glib::ustring>::create("infrastructure");

    // ----- 802-11-wireless-security (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> sec_map;
    bool use_security = !password.empty();
    if (use_security)
    {
      sec_map["key-mgmt"] = Glib::Variant<Glib::ustring>::create("wpa-psk");
      sec_map["psk"] = Glib::Variant<Glib::ustring>::create(password);
    }

    // ------------------------
    // TOP-LEVEL SETTINGS (a{sa{sv}})
    // ------------------------
    std::map<
        Glib::ustring,
        std::map<Glib::ustring, Glib::VariantBase>>
        settings_map;

    settings_map["connection"] = connection_map;
    settings_map["802-11-wireless"] = wifi_map;
    if (use_security)
      settings_map["802-11-wireless-security"] = sec_map;

    auto settings = Glib::Variant<std::map<
        Glib::ustring,
        std::map<Glib::ustring, Glib::VariantBase>>>::create(settings_map);

    // ------------------------
    // Object paths (o)
    // ------------------------
    auto device_path = Glib::Variant<Glib::DBusObjectPathString>::create(wifi_device_path);

    // Access point path is "/" → NM autoselects AP matching SSID
    auto ap_path = Glib::Variant<Glib::DBusObjectPathString>::create("/");

    // ------------------------
    // FINAL TUPLE (a{sa{sv}}, o, o)
    // ------------------------
    std::vector<Glib::VariantBase> args_vec = {
        settings,
        device_path,
        ap_path};

    auto args = Glib::VariantContainerBase::create_tuple(args_vec);

    // ------------------------
    // CALL NetworkManager
    // ------------------------
    nm->call_sync("AddAndActivateConnection", args);

    pop_status_label.set_text("Connecting...");
    update_active_connection();
  }
  catch (const Glib::Error &e)
  {
    std::cerr << "Connect failed: " << e.what() << std::endl;
    pop_status_label.set_text("Failed: " + Glib::ustring(e.what()));
  }
  catch (...)
  {
    pop_status_label.set_text("Unknown error");
  }
}

/* --- widget lifecycle --- */

void WayfireNetworkInfo::on_click()
{
  // Instantly show popover
  show_wifi_popover();

  // Show scanning message
  pop_status_label.set_text("Scanning for networks…");
  pop_list_box.set_sensitive(false);
  while (auto child = pop_list_box.get_first_child())
    pop_list_box.remove(*child);

  // Launch scanning in background
  std::thread([this]()
              {
        auto networks = get_available_networks();

        Glib::signal_idle().connect_once([this, networks]()
        {
            std::vector<std::string> ssids;
            for (auto &w : networks)
                ssids.push_back(w.ssid);

            update_network_list(ssids);  // <-- pass ssids, not networks
            pop_list_box.set_sensitive(true);
        }); })
      .detach();
}

void WayfireNetworkInfo::init(Gtk::Box *container)
{
  // --- Setup the popover initial UI ---
  m_popover_box.set_margin(10);
  m_popover_box.set_spacing(6);
  m_popover_box.append(m_status_label);
  m_popover_box.append(m_spinner);
  m_popover.set_child(m_popover_box);
  m_spinner.start();

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
  button->signal_clicked().connect(
      sigc::mem_fun(*this, &WayfireNetworkInfo::on_click));

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
  }
  else
  {
    if (!status.get_parent())
    {
      button_content.append(status);
    }
  }

  update_icon();
  update_status();
}

void WayfireNetworkInfo::update_network_list(
    const std::vector<std::string> &networks)
{
  while (auto child = pop_list_box.get_first_child())
    pop_list_box.remove(*child);

  if (networks.empty())
  {
    pop_status_label.set_text("No networks found.");
    return;
  }

  pop_status_label.set_text("");

  for (const auto &ssid : networks)
  {
    auto row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto label = Gtk::make_managed<Gtk::Label>(ssid);
    label->set_hexpand(true);
    row->append(*label);

    // Use a Button as a clickable row (GTK4 way)
    auto btn = Gtk::make_managed<Gtk::Button>();
    btn->set_child(*row);
    btn->add_css_class("wifi-row");

    btn->signal_clicked().connect([this, ssid]()
                                  {
                                    show_password_prompt_for(ssid); // adapt security as needed
                                  });

    pop_list_box.append(*btn);
  }

  pop_list_box.show();
}

void WayfireNetworkInfo::show_error(const std::string &message)
{
  m_status_label.set_text("Error: " + message);
  m_spinner.stop();
}

WayfireNetworkInfo::~WayfireNetworkInfo() {}