#include "network.hpp"
#include <atomic>
#include <cassert>
#include <gio/gio.h> // sometimes needed for lower-level variant helpers
#include <giomm.h>
#include <giomm/dbusconnection.h>
#include <giomm/dbusproxy.h>
#include <giomm/init.h>
#include <glib.h>
#include <glibmm.h>
#include <glibmm/spawn.h>
#include <glibmm/variant.h>
#include <glibmm/varianttype.h>
#include <gtk-utils.hpp>
#include <gtkmm/gestureclick.h>
#include <gdkmm/rgba.h>
#include <glibmm/markup.h>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

struct NetworkInfo {
  std::string ssid;
  std::string path;
  int strength = 0;     // Wi-Fi signal strength (0–100)
  bool secured = false; // True if the network requires authentication
};
// Map 0-100 strength to GNOME symbolic icon names
static std::string icon_name_for_strength(int value) {
  if (value > 80)
    return "network-wireless-signal-excellent-symbolic";
  if (value > 55)
    return "network-wireless-signal-good-symbolic";
  if (value > 30)
    return "network-wireless-signal-ok-symbolic";
  if (value > 5)
    return "network-wireless-signal-weak-symbolic";
  return "network-wireless-signal-none-symbolic";
}

// Interpolated color without CSS, based on connection strength percent
struct status_color {
  int point;
  Gdk::RGBA rgba;
};

static status_color status_colors[] = {
    {0, Gdk::RGBA{"#ff0000"}},
    {25, Gdk::RGBA{"#ff0000"}},
    {40, Gdk::RGBA{"#ffff55"}},
    {100, Gdk::RGBA{"#00ff00"}},
};

#define MAX_COLORS (sizeof(status_colors) / sizeof(status_color))

static Gdk::RGBA get_color_for_pc(int pc) {
  for (int i = MAX_COLORS - 2; i >= 0; i--) {
    if (status_colors[i].point <= pc) {
      auto &r1 = status_colors[i].rgba;
      auto &r2 = status_colors[i + 1].rgba;

      double a = 1.0 * (pc - status_colors[i].point) /
                 (status_colors[i + 1].point - status_colors[i].point);
      Gdk::RGBA result;
      result.set_rgba(r1.get_red() * (1 - a) + r2.get_red() * a,
                      r1.get_green() * (1 - a) + r2.get_green() * a,
                      r1.get_blue() * (1 - a) + r2.get_blue() * a,
                      r1.get_alpha() * (1 - a) + r2.get_alpha() * a);

      return result;
    }
  }

  return Gdk::RGBA{"#ffffff"};
}

static std::string rgba_to_hex(const Gdk::RGBA &c) {
  auto to_hex = [](double v) {
    int iv = (int)std::round(std::max(0.0, std::min(1.0, v)) * 255.0);
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02X", iv);
    return std::string(buf);
  };
  return std::string("#") + to_hex(c.get_red()) + to_hex(c.get_green()) +
         to_hex(c.get_blue());
}

void WayfireNetworkInfo::trigger_wifi_scan_async(
    std::function<void()> callback) {
  // Use existing connection or create a new one synchronously
  Glib::RefPtr<Gio::DBus::Connection> conn = connection;
  if (!conn) {
    try {
      conn = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
    } catch (const Glib::Error &e) {
      std::cerr << "D-Bus connection error: " << e.what() << std::endl;
      callback();
      return;
    }
  }

  // Create NetworkManager proxy asynchronously
  Gio::DBus::Proxy::create(
      conn, NM_DBUS_NAME, "/org/freedesktop/NetworkManager",
      "org.freedesktop.NetworkManager",
      [callback, conn](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
          auto nm_proxy = Gio::DBus::Proxy::create_finish(result);

          // Get devices to find WiFi device
          nm_proxy->call(
              "GetDevices",
              [callback, conn,
               nm_proxy](const Glib::RefPtr<Gio::AsyncResult> &result) {
                try {
                  Glib::VariantContainerBase reply =
                      nm_proxy->call_finish(result);
                  auto device_child = reply.get_child(0);
                  auto devices_variant = Glib::VariantBase::cast_dynamic<
                      Glib::Variant<std::vector<Glib::DBusObjectPathString>>>(
                      device_child);
                  std::vector<Glib::DBusObjectPathString> device_paths =
                      devices_variant.get();

                  // Find WiFi device and trigger scan
                  auto scan_state = std::make_shared<std::atomic<bool>>(false);
                  auto devices_checked = std::make_shared<std::atomic<int>>(0);
                  int total_devices = device_paths.size();

                  if (total_devices == 0) {
                    callback(); // No devices found
                    return;
                  }

                  for (const auto &path : device_paths) {
                    Gio::DBus::Proxy::create(
                        conn, NM_DBUS_NAME, path,
                        "org.freedesktop.NetworkManager.Device",
                        [callback, conn, path, scan_state, devices_checked,
                         total_devices](
                            const Glib::RefPtr<Gio::AsyncResult> &result) {
                          try {
                            auto dev_proxy =
                                Gio::DBus::Proxy::create_finish(result);

                            // Check device type
                            Glib::VariantBase dev_type_variant;
                            dev_proxy->get_cached_property(dev_type_variant,
                                                           "DeviceType");
                            guint32 dev_type = 0;
                            if (dev_type_variant) {
                              Glib::Variant<guint32> vtype =
                                  Glib::VariantBase::cast_dynamic<
                                      Glib::Variant<guint32>>(dev_type_variant);
                              dev_type = vtype.get();
                            }

                            if (dev_type == 2) // NM_DEVICE_TYPE_WIFI
                            {
                              // Create wireless device proxy and trigger scan
                              Gio::DBus::Proxy::create(
                                  conn, NM_DBUS_NAME, path,
                                  "org.freedesktop.NetworkManager.Device."
                                  "Wireless",
                                  [callback, scan_state, devices_checked,
                                   total_devices](
                                      const Glib::RefPtr<Gio::AsyncResult>
                                          &result) {
                                    try {
                                      auto wifi_proxy =
                                          Gio::DBus::Proxy::create_finish(
                                              result);
                                      // Trigger scan (RequestScan takes (a{sv})
                                      // - a tuple with an empty dict) Create
                                      // empty dictionary a{sv}
                                      std::map<Glib::ustring, Glib::VariantBase>
                                          scan_options;
                                      auto scan_options_variant = Glib::Variant<
                                          std::map<Glib::ustring,
                                                   Glib::VariantBase>>::
                                          create(scan_options);
                                      // Wrap in tuple (a{sv})
                                      std::vector<Glib::VariantBase> args_vec =
                                          {scan_options_variant};
                                      auto args = Glib::VariantContainerBase::
                                          create_tuple(args_vec);

                                      wifi_proxy->call(
                                          "RequestScan",
                                          [callback, wifi_proxy, scan_state](
                                              const Glib::RefPtr<
                                                  Gio::AsyncResult> &result) {
                                            try {
                                              wifi_proxy->call_finish(result);
                                              scan_state->store(true);
                                              // Scan triggered successfully,
                                              // wait a bit for it to complete
                                              Glib::signal_timeout()
                                                  .connect_once(
                                                      [callback]() {
                                                        callback();
                                                      },
                                                      1000); // Wait 1 second
                                                             // for scan to
                                                             // complete
                                            } catch (const Glib::Error &e) {
                                              std::cerr << "RequestScan error: "
                                                        << e.what()
                                                        << std::endl;
                                              // Continue anyway - might have
                                              // already scanned
                                              Glib::signal_timeout()
                                                  .connect_once(
                                                      [callback]() {
                                                        callback();
                                                      },
                                                      500);
                                            }
                                          },
                                          args);
                                    } catch (const Glib::Error &e) {
                                      std::cerr
                                          << "Failed to create wireless proxy: "
                                          << e.what() << std::endl;
                                      int checked = ++(*devices_checked);
                                      if (checked >= total_devices &&
                                          !scan_state->load()) {
                                        callback();
                                      }
                                    }
                                  });
                            } else {
                              int checked = ++(*devices_checked);
                              if (checked >= total_devices &&
                                  !scan_state->load()) {
                                callback(); // No WiFi device found
                              }
                            }
                          } catch (const Glib::Error &e) {
                            int checked = ++(*devices_checked);
                            if (checked >= total_devices &&
                                !scan_state->load()) {
                              callback();
                            }
                          }
                        });
                  }
                } catch (const Glib::Error &e) {
                  std::cerr << "GetDevices error: " << e.what() << std::endl;
                  callback();
                }
              },
              Glib::VariantContainerBase());
        } catch (const Glib::Error &e) {
          std::cerr << "Failed to create NM proxy: " << e.what() << std::endl;
          callback();
        }
      });
}

void WayfireNetworkInfo::get_available_networks_async(
    std::function<void(const std::vector<NetworkInfo> &)> callback) {
  // Use shared state to safely track progress across async callbacks
  struct ScanState {
    std::vector<NetworkInfo> all_networks;
    std::atomic<int> devices_processed{0};
    int total_devices = 0;
    std::function<void(const std::vector<NetworkInfo> &)> final_callback;
    std::mutex networks_mutex; // Protect all_networks vector

    ScanState(std::function<void(const std::vector<NetworkInfo> &)> cb)
        : final_callback(cb) {}

    void check_complete() {
      if (devices_processed.load() >= total_devices) {
        final_callback(all_networks);
      }
    }
  };

  auto state = std::make_shared<ScanState>(callback);

  // Use existing connection or create a new one synchronously
  // (The async work is in the D-Bus method calls, not the connection)
  Glib::RefPtr<Gio::DBus::Connection> conn = connection;
  if (!conn) {
    try {
      conn = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
    } catch (const Glib::Error &e) {
      std::cerr << "D-Bus connection error: " << e.what() << std::endl;
      callback({});
      return;
    }
  }

  // Create NetworkManager proxy asynchronously
  Gio::DBus::Proxy::create(
      conn, NM_DBUS_NAME, "/org/freedesktop/NetworkManager",
      "org.freedesktop.NetworkManager",
      [state](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
          auto nm_proxy = Gio::DBus::Proxy::create_finish(result);

          // Call GetDevices asynchronously (no parameters)
          nm_proxy
              ->call(
                  "GetDevices",
                  [state,
                   nm_proxy](const Glib::RefPtr<Gio::AsyncResult> &result) {
                    try {
                      Glib::VariantContainerBase reply =
                          nm_proxy->call_finish(result);
                      auto device_child = reply.get_child(0);
                      auto devices_variant =
                          Glib::VariantBase::cast_dynamic<Glib::Variant<
                              std::vector<Glib::DBusObjectPathString>>>(
                              device_child);
                      std::vector<Glib::DBusObjectPathString> device_paths =
                          devices_variant.get();

                      state->total_devices = device_paths.size();

                      if (state->total_devices == 0) {
                        state->final_callback({});
                        return;
                      }

                      // Process each device asynchronously
                      for (const auto &path : device_paths) {
                        // Create device proxy
                        Gio::
                            DBus::
                                Proxy::
                                    create(
                                        nm_proxy->get_connection(),
                                        NM_DBUS_NAME, path,
                                        "org.freedesktop.NetworkManager.Device",
                                        [state, path, nm_proxy](
                                            const Glib::RefPtr<Gio::AsyncResult>
                                                &result) {
                                          try {
                                            auto dev_proxy =
                                                Gio::DBus::Proxy::create_finish(
                                                    result);

                                            // Check device type
                                            Glib::VariantBase dev_type_variant;
                                            dev_proxy->get_cached_property(
                                                dev_type_variant, "DeviceType");
                                            guint32 dev_type = 0;
                                            if (dev_type_variant) {
                                              Glib::Variant<guint32> vtype =
                                                  Glib::VariantBase::
                                                      cast_dynamic<
                                                          Glib::Variant<
                                                              guint32>>(
                                                          dev_type_variant);
                                              dev_type = vtype.get();
                                            }

                                            if (dev_type !=
                                                2) // NM_DEVICE_TYPE_WIFI
                                            {
                                              state->devices_processed++;
                                              state->check_complete();
                                              return;
                                            }

                                            // Create wireless device proxy
                                            Gio::DBus::Proxy::create(dev_proxy
                                                                         ->get_connection(),
                                                                     NM_DBUS_NAME,
                                                                     path,
                                                                     "org."
                                                                     "freedeskt"
                                                                     "op."
                                                                     "NetworkMa"
                                                                     "nager."
                                                                     "Device."
                                                                     "Wireless",
                                                                     [state,
                                                                      path](const Glib::
                                                                                RefPtr<
                                                                                    Gio::
                                                                                        AsyncResult>
                                                                                    &
                                                                                        result) {
                                                                       try {
                                                                         auto wifi_proxy =
                                                                             Gio::DBus::
                                                                                 Proxy::create_finish(
                                                                                     result);

                                                                         // Get
                                                                         // all
                                                                         // access
                                                                         // points
                                                                         // (no
                                                                         // parameters)
                                                                         wifi_proxy
                                                                             ->call(
                                                                                 "GetAllAccessPoints",
                                                                                 [state,
                                                                                  wifi_proxy](
                                                                                     const Glib::RefPtr<
                                                                                         Gio::
                                                                                             AsyncResult>
                                                                                         &result) {
                                                                                   try {
                                                                                     Glib::VariantContainerBase
                                                                                         aps_reply =
                                                                                             wifi_proxy
                                                                                                 ->call_finish(
                                                                                                     result);
                                                                                     auto ap_child =
                                                                                         aps_reply
                                                                                             .get_child(
                                                                                                 0);
                                                                                     auto ap_paths_variant =
                                                                                         Glib::VariantBase::cast_dynamic<
                                                                                             Glib::Variant<
                                                                                                 std::vector<
                                                                                                     Glib::
                                                                                                         DBusObjectPathString>>>(
                                                                                             ap_child);
                                                                                     std::vector<
                                                                                         Glib::
                                                                                             DBusObjectPathString>
                                                                                         ap_paths =
                                                                                             ap_paths_variant
                                                                                                 .get();

                                                                                     int total_aps =
                                                                                         ap_paths
                                                                                             .size();
                                                                                     auto aps_processed =
                                                                                         std::make_shared<
                                                                                             std::atomic<
                                                                                                 int>>(
                                                                                             0);

                                                                                     if (total_aps ==
                                                                                         0) {
                                                                                       state
                                                                                           ->devices_processed++;
                                                                                       state
                                                                                           ->check_complete();
                                                                                       return;
                                                                                     }

                                                                                     // Process each access point
                                                                                     for (
                                                                                         const auto
                                                                                             &ap_path :
                                                                                         ap_paths) {
                                                                                       Gio::DBus::
                                                                                           Proxy::create(
                                                                                               wifi_proxy
                                                                                                   ->get_connection(),
                                                                                               NM_DBUS_NAME,
                                                                                               ap_path,
                                                                                               "org.freedesktop.NetworkManager.AccessPoint",
                                                                                               [state,
                                                                                                total_aps,
                                                                                                aps_processed](
                                                                                                   const Glib::RefPtr<
                                                                                                       Gio::
                                                                                                           AsyncResult>
                                                                                                       &result) {
                                                                                                 try {
                                                                                                   auto ap_proxy =
                                                                                                       Gio::DBus::
                                                                                                           Proxy::create_finish(
                                                                                                               result);
                                                                                                   if (ap_proxy) {
                                                                                                     Glib::VariantBase
                                                                                                         ssid_var,
                                                                                                         wpa_var,
                                                                                                         rsn_var,
                                                                                                         strength_var;
                                                                                                     ap_proxy
                                                                                                         ->get_cached_property(
                                                                                                             ssid_var,
                                                                                                             "Ssid");
                                                                                                     ap_proxy
                                                                                                         ->get_cached_property(
                                                                                                             wpa_var,
                                                                                                             "WpaFlags");
                                                                                                     ap_proxy
                                                                                                         ->get_cached_property(
                                                                                                             rsn_var,
                                                                                                             "RsnFlags");
                                                                                                     ap_proxy
                                                                                                         ->get_cached_property(
                                                                                                             strength_var,
                                                                                                             "Strength");

                                                                                                     std::string
                                                                                                         ssid;
                                                                                                     if (ssid_var) {
                                                                                                       Glib::Variant<
                                                                                                           std::vector<
                                                                                                               guint8>>
                                                                                                           ssid_bytes = Glib::VariantBase::cast_dynamic<
                                                                                                               Glib::Variant<
                                                                                                                   std::vector<
                                                                                                                       guint8>>>(
                                                                                                               ssid_var);
                                                                                                       auto vec =
                                                                                                           ssid_bytes
                                                                                                               .get();
                                                                                                       ssid.assign(
                                                                                                           vec.begin(),
                                                                                                           vec.end());
                                                                                                     }

                                                                                                     guint32
                                                                                                         wpa_flags =
                                                                                                             0,
                                                                                                         rsn_flags =
                                                                                                             0;
                                                                                                     if (wpa_var)
                                                                                                       wpa_flags =
                                                                                                           Glib::VariantBase::cast_dynamic<
                                                                                                               Glib::Variant<
                                                                                                                   guint32>>(
                                                                                                               wpa_var)
                                                                                                               .get();
                                                                                                     if (rsn_var)
                                                                                                       rsn_flags =
                                                                                                           Glib::VariantBase::cast_dynamic<
                                                                                                               Glib::Variant<
                                                                                                                   guint32>>(
                                                                                                               rsn_var)
                                                                                                               .get();

                                                                                                     int strength =
                                                                                                         0;
                                                                                                     if (strength_var)
                                                                                                       strength =
                                                                                                           Glib::VariantBase::cast_dynamic<
                                                                                                               Glib::Variant<
                                                                                                                   guchar>>(
                                                                                                               strength_var)
                                                                                                               .get();

                                                                                                     bool secured =
                                                                                                         (wpa_flags ||
                                                                                                          rsn_flags);

                                                                                                     if (!ssid.empty()) {
                                                                                                       std::lock_guard<
                                                                                                           std::
                                                                                                               mutex>
                                                                                                           lock(
                                                                                                               state
                                                                                                                   ->networks_mutex);
                                                                                                       state
                                                                                                           ->all_networks
                                                                                                           .push_back(
                                                                                                               {ssid,
                                                                                                                ap_proxy
                                                                                                                    ->get_object_path(),
                                                                                                                strength,
                                                                                                                secured});
                                                                                                     }
                                                                                                   }

                                                                                                   int processed =
                                                                                                       ++(*aps_processed);
                                                                                                   if (processed >=
                                                                                                       total_aps) {
                                                                                                     state
                                                                                                         ->devices_processed++;
                                                                                                     state
                                                                                                         ->check_complete();
                                                                                                   }
                                                                                                 } catch (
                                                                                                     const Glib::Error
                                                                                                         &e) {
                                                                                                   // Ignore errors for individual access points
                                                                                                   int processed =
                                                                                                       ++(*aps_processed);
                                                                                                   if (processed >=
                                                                                                       total_aps) {
                                                                                                     state
                                                                                                         ->devices_processed++;
                                                                                                     state
                                                                                                         ->check_complete();
                                                                                                   }
                                                                                                 }
                                                                                               });
                                                                                     }
                                                                                   } catch (
                                                                                       const Glib::Error
                                                                                           &e) {
                                                                                     std::cerr
                                                                                         << "Error getting access points: "
                                                                                         << e.what()
                                                                                         << std::
                                                                                                endl;
                                                                                     state
                                                                                         ->devices_processed++;
                                                                                     state
                                                                                         ->check_complete();
                                                                                   }
                                                                                 },
                                                                                 Glib::
                                                                                     VariantContainerBase());
                                                                       } catch (
                                                                           const Glib::Error
                                                                               &e) {
                                                                         state
                                                                             ->devices_processed++;
                                                                         state
                                                                             ->check_complete();
                                                                       }
                                                                     });
                                          } catch (const Glib::Error &e) {
                                            state->devices_processed++;
                                            state->check_complete();
                                          }
                                        });
                      }
                    } catch (const Glib::Error &e) {
                      std::cerr
                          << "D-Bus error in get_available_networks_async(): "
                          << e.what() << std::endl;
                      state->final_callback({});
                    }
                  },
                  Glib::VariantContainerBase());
        } catch (const Glib::Error &e) {
          std::cerr << "Failed to create NM proxy: " << e.what() << std::endl;
          state->final_callback({});
        }
      });
}

std::string WfNetworkConnectionInfo::get_control_center_section(DBusProxy &nm) {
  Glib::Variant<bool> wifi;
  nm->get_cached_property(wifi, "WirelessEnabled");

  return wifi.get() ? "wifi" : "network";
}

void WfNetworkConnectionInfo::spawn_control_center(DBusProxy &nm) {
  std::string command = "env XDG_CURRENT_DESKTOP=GNOME gnome-control-center ";
  command += get_control_center_section(nm);

  Glib::spawn_command_line_async(command);
}

/* --- ConnectionInfo implementations (same as before) --- */

struct NoConnectionInfo : public WfNetworkConnectionInfo {
  std::string get_icon_name(WfConnectionState state) {
    return "network-offline-symbolic";
  }

  int get_connection_strength() { return 0; }
  std::string get_strength_str() { return "none"; }
  std::string get_ip() { return "127.0.0.1"; }
  virtual ~NoConnectionInfo() {}
};

struct WifiConnectionInfo : public WfNetworkConnectionInfo {

  void show_error(const std::string &message);
  WayfireNetworkInfo *widget;
  DBusProxy ap;

  WifiConnectionInfo(const DBusConnection &connection, std::string path,
                     WayfireNetworkInfo *widget) {
    this->widget = widget;

    ap = Gio::DBus::Proxy::create_sync(
        connection, NM_DBUS_NAME, path,
        "org.freedesktop.NetworkManager.AccessPoint");

    if (ap) {
      ap->signal_properties_changed().connect(
          sigc::mem_fun(*this, &WifiConnectionInfo::on_properties_changed));
    }
  }

  void on_properties_changed(DBusPropMap changed, DBusPropList invalid) {
    bool needs_refresh = false;
    for (auto &prop : changed) {
      if (prop.first == STRENGTH) {
        needs_refresh = true;
      }
    }

    if (needs_refresh) {
      widget->update_icon();
      widget->update_status();
    }
  }

  int get_strength() {
    assert(ap);

    Glib::Variant<guchar> vstr;
    ap->get_cached_property(vstr, STRENGTH);

    return vstr.get();
  }

  std::string get_strength_str() {
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

  virtual std::string get_icon_name(WfConnectionState state) {
    if ((state <= CSTATE_ACTIVATING) || (state == CSTATE_DEACTIVATING))
      return "network-wireless-acquiring-symbolic";

    if (state == CSTATE_DEACTIVATED)
      return "network-wireless-disconnected-symbolic";

    if (ap)
      return "network-wireless-signal-" + get_strength_str() + "-symbolic";
    else
      return "network-wireless-no-route-symbolic";
  }

  virtual int get_connection_strength() {
    if (ap)
      return get_strength();
    return 100;
  }

  virtual std::string get_ip() { return "0.0.0.0"; }
  virtual ~WifiConnectionInfo() {}
};

struct EthernetConnectionInfo : public WfNetworkConnectionInfo {
  DBusProxy ap;
  EthernetConnectionInfo(const DBusConnection &connection, std::string path) {}

  virtual std::string get_icon_name(WfConnectionState state) {
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

static WfConnectionState get_connection_state(DBusProxy connection) {
  if (!connection)
    return CSTATE_DEACTIVATED;

  Glib::Variant<guint32> state;
  connection->get_cached_property(state, "State");
  return (WfConnectionState)state.get();
}

void WayfireNetworkInfo::update_icon() {
  // Defensive checks to avoid crashes during early init or missing DBus info
  if (!info) {
    // ensure info is always valid
    info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
    info->connection_name = "No connection";
    std::cerr
        << "[network] update_icon: info was null, created NoConnectionInfo"
        << std::endl;
  }

  // get connection state safely
  WfConnectionState state = CSTATE_DEACTIVATED;
  try {
    state = get_connection_state(active_connection_proxy);
  } catch (...) {
    // shouldn't normally throw, but guard anyway
    std::cerr << "[network] update_icon: exception in get_connection_state()"
              << std::endl;
    state = CSTATE_DEACTIVATED;
  }

  // Attempt to ask the info object for icon name, but guard unexpected
  // exceptions
  std::string icon_name;
  try {
    icon_name = info->get_icon_name(state);
  } catch (const std::exception &e) {
    std::cerr << "[network] update_icon: exception in get_icon_name(): "
              << e.what() << std::endl;
    icon_name = "network-offline-symbolic";
  } catch (...) {
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

void WayfireNetworkInfo::update_status() {
  std::string description = info->get_connection_name();
  button->set_tooltip_text(description);

  if (status_color_opt) {
    int pc = 0;
    try {
      pc = info->get_connection_strength();
    } catch (...) {
      pc = 0;
    }
    auto rgba = get_color_for_pc(pc);
    auto hex = rgba_to_hex(rgba);
    auto escaped = Glib::Markup::escape_text(description);
    status.set_use_markup(true);
    status.set_markup("<span foreground='" + hex + "'>" + escaped +
                      "</span>");
  } else {
    status.set_use_markup(false);
    status.set_text(description);
  }
}

void WayfireNetworkInfo::update_active_connection() {

  Glib::Variant<Glib::ustring> active_conn_path;
  nm_proxy->get_cached_property(active_conn_path, ACTIVE_CONNECTION);

  if (active_conn_path && (active_conn_path.get() != "/")) {
    active_connection_proxy = Gio::DBus::Proxy::create_sync(
        connection, NM_DBUS_NAME, active_conn_path.get(),
        "org.freedesktop.NetworkManager.Connection.Active");
  } else {
    active_connection_proxy = DBusProxy();
  }

  auto set_no_connection = [=]() {
    info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
    info->connection_name = "No connection";
  };

  if (!active_connection_proxy) {
    set_no_connection();
    // No active connection → ensure no SSID is highlighted as connected
    current_ap_path = Glib::DBusObjectPathString{""};
  } else {
    Glib::Variant<Glib::ustring> vtype, vobject;
    active_connection_proxy->get_cached_property(vtype, "Type");
    active_connection_proxy->get_cached_property(vobject, "SpecificObject");
    auto type = vtype.get();
    auto object = vobject.get();

    if (type.find("wireless") != type.npos) {
      info = std::unique_ptr<WfNetworkConnectionInfo>(
          new WifiConnectionInfo(connection, object, this));
      current_ap_path =
          Glib::DBusObjectPathString{object}; // remember the connected AP path
    } else if (type.find("ethernet") != type.npos) {
      info = std::unique_ptr<WfNetworkConnectionInfo>(
          new EthernetConnectionInfo(connection, object));
      current_ap_path = Glib::DBusObjectPathString{""};
    } else if (type.find("bluetooth") != type.npos) {
      std::cout << "Unimplemented: bluetooth connection" << std::endl;
      set_no_connection();
      current_ap_path = Glib::DBusObjectPathString{""};
    } else {
      std::cout << "Unimplemented: unknown connection type" << std::endl;
      set_no_connection();
      current_ap_path = Glib::DBusObjectPathString{""};
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
    const std::vector<Glib::ustring> &invalidated) {
  for (auto &prop : properties) {
    if (prop.first == ACTIVE_CONNECTION) {
      update_active_connection();
      auto pop = button ? button->get_popover() : nullptr;
      if (pop && pop->get_visible()) {
        populate_wifi_list();
      }
    }
  }
}

bool WayfireNetworkInfo::setup_dbus() {
  auto cancellable = Gio::Cancellable::create();
  connection =
      Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
  if (!connection) {
    std::cerr << "Failed to connect to dbus" << std::endl;
    return false;
  }

  nm_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME,
                                           "/org/freedesktop/NetworkManager",
                                           "org.freedesktop.NetworkManager");
  if (!nm_proxy) {
    std::cerr << "Failed to connect to network manager, "
              << "are you sure it is running?" << std::endl;
    return false;
  }

  nm_proxy->signal_properties_changed().connect(
      sigc::mem_fun(*this, &WayfireNetworkInfo::on_nm_properties_changed));

  return true;
}

/* --- Popover handling --- */
void WayfireNetworkInfo::show_wifi_popover() {
  auto popover = button->get_popover();
  // ensure popover content is set
  if (!popover->get_child()) {
    // in network.cpp constructor or init method
    pop_list_box.set_orientation(Gtk::Orientation::VERTICAL);
    pop_list_box.set_spacing(4);
    pop_list_box.set_margin(6);
    pop_scrolled.set_child(pop_list_box);
    pop_scrolled.set_min_content_height(350);
    pop_scrolled.set_min_content_width(260);
    popover_box.set_orientation(Gtk::Orientation::VERTICAL);
    popover_box.set_spacing(6);
    popover_box.append(pop_scrolled);
    // prepare password box (hidden until needed)
    pop_pass_box.set_orientation(Gtk::Orientation::VERTICAL);
    pop_pass_box.set_spacing(4);
    pop_pass_box.set_margin_top(6);
    pop_pass_box.set_margin_bottom(6);
    popover->set_child(popover_box);
    popover->set_size_request(260, 350);
    popover->get_style_context()->add_class("network-popover");
  }

  // Always restore the main network list view (in case we're coming from
  // password prompt) Remove all children from popover_box
  for (auto *child : popover_box.get_children())
    popover_box.remove(*child);

  // Restore the scrolled window with network list
  popover_box.append(pop_scrolled);

  populate_wifi_list();

  // show
  button->set_keyboard_interactive(false);
  popover->popup();
}

void WayfireNetworkInfo::populate_wifi_list() {
  // Clear the list box
  for (auto *child : pop_list_box.get_children())
    pop_list_box.remove(*child);

  // Header
  auto header = Gtk::make_managed<Gtk::Label>("Available WiFi Networks");
  // header->set_margin_bottom(0);
  pop_list_box.append(*header);

  // Show scanning message
  pop_status_label.set_text("Scanning for networks…");
  pop_list_box.set_sensitive(false);
  trigger_wifi_scan_async([this]() {
    // Launch network scan asynchronously using D-Bus async interface
    get_available_networks_async([this](
                                     const std::vector<NetworkInfo> &networks) {
      if (networks.empty()) {
        auto lbl = Gtk::make_managed<Gtk::Label>("No networks found");
        lbl->set_margin(6);
        pop_list_box.append(*lbl);
      } else {
        // Find connected entry (if any) so we can render it first
        int connected_index = -1;
        if (!current_ap_path.empty()) {
          for (size_t i = 0; i < networks.size(); ++i) {
            if (Glib::ustring{networks[i].path} == current_ap_path) {
              connected_index = static_cast<int>(i);
              break;
            }
          }
        }

        auto render_row = [this](const NetworkInfo &net, bool is_connected) {
          auto row =
              Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
          row->set_halign(Gtk::Align::FILL);
          row->set_hexpand(true);

          auto sig_img = Gtk::make_managed<Gtk::Image>();
          sig_img->set_from_icon_name(icon_name_for_strength(net.strength));
          sig_img->set_pixel_size(16);
          row->append(*sig_img);

          auto name_lbl = Gtk::make_managed<Gtk::Label>(net.ssid);
          name_lbl->set_halign(Gtk::Align::START);
          name_lbl->set_hexpand(true);
          if (is_connected) {
            auto escaped = Glib::Markup::escape_text(net.ssid);
            name_lbl->set_use_markup(true);
            name_lbl->set_markup("<span foreground='#33c15e' weight='bold'>" +
                                 escaped + "</span>");
          }
          row->append(*name_lbl);

          if (is_connected) {
            // Info button (shows details of current connection)
            auto info_btn = Gtk::make_managed<Gtk::Button>();
            info_btn->add_css_class("flat");
            info_btn->set_tooltip_text("Connection details");
            auto info_img = Gtk::make_managed<Gtk::Image>();
            info_img->set_from_icon_name("dialog-information-symbolic");
            info_img->set_pixel_size(14);
            info_btn->set_child(*info_img);
            info_btn->signal_clicked().connect([this]() { show_connected_details(); });
            row->append(*info_btn);

            // Disconnect button
            auto disc_btn = Gtk::make_managed<Gtk::Button>();
            disc_btn->add_css_class("flat");
            disc_btn->set_tooltip_text("Disconnect");
            auto disc_img = Gtk::make_managed<Gtk::Image>();
            disc_img->set_from_icon_name("network-offline-symbolic");
            disc_img->set_pixel_size(14);
            disc_btn->set_child(*disc_img);
            disc_btn->signal_clicked().connect(
                [this]() { disconnect_current_network(); });
            row->append(*disc_btn);

            pop_list_box.append(*row);
          } else {
            if (net.secured) {
              auto lock_img = Gtk::make_managed<Gtk::Image>();
              lock_img->set_from_icon_name(
                  "network-wireless-encrypted-symbolic");
              lock_img->set_pixel_size(12);
              row->append(*lock_img);
            }

            auto btn = Gtk::make_managed<Gtk::Button>();
            btn->set_child(*row);
            btn->set_halign(Gtk::Align::FILL);

            btn->signal_clicked().connect(
                [this, ssid = net.ssid, secured = net.secured]() {
                  if (!secured) {
                    attempt_connect_ssid(ssid, "");
                    button->get_popover()->popdown();
                  } else {
                    show_password_prompt_for(ssid);
                  }
                });

            pop_list_box.append(*btn);
          }
        };

        // 1) Render connected (if found)
        if (connected_index >= 0) {
          render_row(networks[connected_index], true);
        }

        // 2) Render the rest
        for (size_t i = 0; i < networks.size(); ++i) {
          if (static_cast<int>(i) == connected_index)
            continue;
          render_row(networks[i], false);
        }
      }

      pop_list_box.set_sensitive(true);
      pop_status_label.set_text(""); // Clear scanning message
    });
  });
  // Status label at the bottom
  pop_status_label.set_margin_top(6);
  pop_list_box.append(pop_status_label);
}

void WayfireNetworkInfo::show_password_prompt_for(const std::string &ssid) {
  // Clear popover content
  for (auto *child : popover_box.get_children())
    popover_box.remove(*child);

  auto popover = button->get_popover();

  // Title
  auto title = Gtk::make_managed<Gtk::Label>("Connect to: " + ssid);
  title->set_margin_bottom(12);
  title->set_margin_top(12);
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
  cancel_btn->signal_clicked().connect([this]() {
    // Restore Wi-Fi list
    for (auto *child : popover_box.get_children())
      popover_box.remove(*child);
    popover_box.append(pop_scrolled);
    populate_wifi_list();
  });

  auto connect_btn = Gtk::make_managed<Gtk::Button>("Connect");
  connect_btn->signal_clicked().connect([this, ssid, entry, popover]() {
    std::string pwd = entry->get_text();
    if (pwd.empty()) {
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
                                              const std::string &password) {
  if (ssid.empty() || ssid.find_first_not_of(" \t\r\n") == std::string::npos) {
    pop_status_label.set_text("Invalid SSID");
    return;
  }

  try {
    auto bus = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
    auto nm = Gio::DBus::Proxy::create_sync(
        bus, "org.freedesktop.NetworkManager",
        "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager");

    // --- Find Wi-Fi device ---
    Glib::VariantBase devices_var;
    nm->get_cached_property(devices_var, "Devices");
    if (!devices_var) {
      pop_status_label.set_text("No devices");
      return;
    }

    auto dev_array = Glib::VariantBase::cast_dynamic<
        Glib::Variant<std::vector<Glib::DBusObjectPathString>>>(devices_var);
    std::string wifi_device_path;
    for (const auto &path : dev_array.get()) {
      auto dev_proxy = Gio::DBus::Proxy::create_sync(
          bus, "org.freedesktop.NetworkManager", path,
          "org.freedesktop.NetworkManager.Device");
      if (!dev_proxy)
        continue;
      Glib::VariantBase type_v;
      dev_proxy->get_cached_property(type_v, "DeviceType");
      auto type_var =
          Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(type_v);
      if (type_var && type_var.get() == 2) {
        wifi_device_path = path;
        break;
      }
    }

    if (wifi_device_path.empty()) {
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
    connection_map["type"] =
        Glib::Variant<Glib::ustring>::create("802-11-wireless");
    connection_map["id"] = Glib::Variant<Glib::ustring>::create(ssid);
    connection_map["uuid"] = Glib::Variant<Glib::ustring>::create(uuid);

    // ----- 802-11-wireless (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> wifi_map;
    wifi_map["ssid"] = ssid_variant;
    wifi_map["mode"] = Glib::Variant<Glib::ustring>::create("infrastructure");

    // ----- 802-11-wireless-security (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> sec_map;
    bool use_security = !password.empty();
    if (use_security) {
      sec_map["key-mgmt"] = Glib::Variant<Glib::ustring>::create("wpa-psk");
      sec_map["psk"] = Glib::Variant<Glib::ustring>::create(password);
    }

    // ------------------------
    // TOP-LEVEL SETTINGS (a{sa{sv}})
    // ------------------------
    std::map<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>>
        settings_map;

    settings_map["connection"] = connection_map;
    settings_map["802-11-wireless"] = wifi_map;
    if (use_security)
      settings_map["802-11-wireless-security"] = sec_map;

    auto settings = Glib::Variant<
        std::map<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>>>::
        create(settings_map);

    // ------------------------
    // Object paths (o)
    // ------------------------
    auto device_path =
        Glib::Variant<Glib::DBusObjectPathString>::create(wifi_device_path);

    // Access point path is "/" → NM autoselects AP matching SSID
    auto ap_path = Glib::Variant<Glib::DBusObjectPathString>::create("/");

    // ------------------------
    // FINAL TUPLE (a{sa{sv}}, o, o)
    // ------------------------
    std::vector<Glib::VariantBase> args_vec = {settings, device_path, ap_path};

    auto args = Glib::VariantContainerBase::create_tuple(args_vec);

    // ------------------------
    // CALL NetworkManager
    // ------------------------
    nm->call_sync("AddAndActivateConnection", args);

    pop_status_label.set_text("Connecting...");
    update_active_connection();
  } catch (const Glib::Error &e) {
    std::cerr << "Connect failed: " << e.what() << std::endl;
    pop_status_label.set_text("Failed: " + Glib::ustring(e.what()));
  } catch (...) {
    pop_status_label.set_text("Unknown error");
  }
}

/* --- widget lifecycle --- */

void WayfireNetworkInfo::on_click() {
  // Instantly show popover
  show_wifi_popover();
}

void WayfireNetworkInfo::init(Gtk::Box *container) {
  // --- Setup the popover initial UI ---

  if (!setup_dbus()) {
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

  // Add right-click to open GNOME Control Center
  auto right_click = Gtk::GestureClick::create();
  right_click->set_button(3); // secondary button
  right_click->signal_pressed().connect([this](int n_press, double /*x*/, double /*y*/)
                                        {
                                          if (n_press == 1)
                                          {
                                            if (nm_proxy && info)
                                            {
                                              info->spawn_control_center(nm_proxy);
                                            }
                                          }
                                        });
  button->add_controller(right_click);

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

void WayfireNetworkInfo::handle_config_reload() {
  if (status_opt.value() == NETWORK_STATUS_ICON) {
    if (status.get_parent()) {
      button_content.remove(status);
    }
  } else {
    if (!status.get_parent()) {
      button_content.append(status);
    }
  }

  update_icon();
  update_status();
}

void WayfireNetworkInfo::disconnect_current_network() {
  if (!nm_proxy)
    return;

  if (!active_connection_proxy)
    return;

  const Glib::ustring path = active_connection_proxy->get_object_path();
  if (path.empty())
    return;

  nm_proxy->call(
      "DeactivateConnection",
      [this](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
          nm_proxy->call_finish(result);
          pop_status_label.set_text("Disconnecting…");
          Glib::signal_timeout().connect_once(
              [this]() {
                pop_status_label.set_text("");
                auto pop = button ? button->get_popover() : nullptr;
                if (pop && pop->get_visible())
                  populate_wifi_list();
              },
              600);
        } catch (const Glib::Error &e) {
          pop_status_label.set_text("Failed to disconnect: " +
                                    Glib::ustring(e.what()));
        }
      },
      Glib::VariantContainerBase::create_tuple(
          Glib::Variant<Glib::DBusObjectPathString>::create(
              Glib::DBusObjectPathString{path})));
}

// Helper: convert IPv4 prefix length (e.g. 24) to dotted netmask (255.255.255.0)
static std::string prefix_to_netmask(int prefix) {
  if (prefix < 0 || prefix > 32)
    return "";
  uint32_t mask = prefix == 0 ? 0 : (~0u << (32 - prefix));
  return std::to_string((mask >> 24) & 0xFF) + "." +
         std::to_string((mask >> 16) & 0xFF) + "." +
         std::to_string((mask >> 8) & 0xFF) + "." +
         std::to_string(mask & 0xFF);
}

void WayfireNetworkInfo::show_connected_details() {
  if (!active_connection_proxy) {
    return; // nothing to show
  }

  auto popover = button->get_popover();
  if (!popover)
    return;

  // Clear existing content
  for (auto *child : popover_box.get_children())
    popover_box.remove(*child);

  auto title = Gtk::make_managed<Gtk::Label>("Network Details");
  title->set_margin_top(8);
  title->set_margin_bottom(8);
  title->set_use_markup(true);
  title->set_markup("<b>Network Details</b>");
  popover_box.append(*title);

  auto content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
  content->set_margin_start(8);
  content->set_margin_end(8);
  content->set_margin_bottom(8);

  auto add_row = [content](const std::string &label, const std::string &value) {
    auto h = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto l = Gtk::make_managed<Gtk::Label>(label + ":");
    l->set_xalign(0);
    l->set_width_chars(13);
    auto v = Gtk::make_managed<Gtk::Label>(value.empty() ? "—" : value);
    v->set_xalign(0);
    v->set_selectable(true);
    h->append(*l);
    h->append(*v);
    content->append(*h);
  };

  // Basic info
  std::string ssid = info ? info->connection_name : "";
  add_row("SSID", ssid);

  // Wireless specific (AP proxy)
  std::string security;
  std::string frequency_str;
  std::string strength_str;
  if (!current_ap_path.empty()) {
    try {
      auto ap_proxy = Gio::DBus::Proxy::create_sync(
          connection, NM_DBUS_NAME, current_ap_path, "org.freedesktop.NetworkManager.AccessPoint");
      if (ap_proxy) {
        Glib::VariantBase wpa_var, rsn_var, freq_var, strength_var;
        ap_proxy->get_cached_property(wpa_var, "WpaFlags");
        ap_proxy->get_cached_property(rsn_var, "RsnFlags");
        ap_proxy->get_cached_property(freq_var, "Frequency");
        ap_proxy->get_cached_property(strength_var, "Strength");
        guint32 wpa_flags = 0, rsn_flags = 0; guchar strength = 0; guint32 freq = 0;
        if (wpa_var)
          wpa_flags = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(wpa_var).get();
        if (rsn_var)
          rsn_flags = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(rsn_var).get();
        if (strength_var)
          strength = Glib::VariantBase::cast_dynamic<Glib::Variant<guchar>>(strength_var).get();
        if (freq_var)
          freq = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(freq_var).get();

        security = (wpa_flags || rsn_flags) ? "Secured (WPA/WPA2)" : "Open";
        strength_str = std::to_string((int)strength) + "%";
        if (freq)
          frequency_str = std::to_string(freq) + " MHz";
      }
    } catch (const Glib::Error &e) {
      security = "Unknown";
    }
  }
  add_row("Security", security);
  add_row("Strength", strength_str);
  add_row("Frequency", frequency_str);

  // Active connection IP config objects
  auto get_ip4_details = [this]() {
    struct IP4Info { std::string addr; int prefix = -1; std::string gateway; std::vector<std::string> dns; } info; 
    try {
      Glib::Variant<Glib::ustring> ip4_path_var;
      active_connection_proxy->get_cached_property(ip4_path_var, "Ip4Config");
      auto path = ip4_path_var.get();
      if (!path.empty() && path != "/") {
        auto ip4_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path, "org.freedesktop.NetworkManager.IP4Config");
        if (ip4_proxy) {
          Glib::VariantBase addr_data_var;
          ip4_proxy->get_cached_property(addr_data_var, "AddressData");
          if (addr_data_var) {
            // AddressData: a( a{sv} ) -> represented as Variant of std::vector<std::map<Glib::ustring, Glib::VariantBase>> maybe
            // We'll attempt dynamic casts
            typedef std::map<Glib::ustring, Glib::VariantBase> Dict;
            auto array_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Dict>>>(addr_data_var);
            auto vec = array_variant.get();
            if (!vec.empty()) {
              auto &first = vec.front();
              auto it_addr = first.find("address");
              auto it_prefix = first.find("prefix");
              auto it_gateway = first.find("gateway");
              if (it_addr != first.end())
                info.addr = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it_addr->second).get();
              if (it_prefix != first.end())
                info.prefix = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(it_prefix->second).get();
              if (it_gateway != first.end())
                info.gateway = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it_gateway->second).get();
            }
          }
          Glib::VariantBase dns_var;
          ip4_proxy->get_cached_property(dns_var, "NameserverData");
          if (dns_var) {
            typedef std::map<Glib::ustring, Glib::VariantBase> Dict;
            auto dns_vec = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Dict>>>(dns_var).get();
            for (auto &entry : dns_vec) {
              auto it_addr = entry.find("address");
              if (it_addr != entry.end()) {
                info.dns.push_back(Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it_addr->second).get());
              }
            }
          }
        }
      }
    } catch (...) {}
    return info;
  };

  auto ip4 = get_ip4_details();
  add_row("IPv4 Address", ip4.addr);
  std::string netmask = ip4.prefix >= 0 ? prefix_to_netmask(ip4.prefix) : "";
  add_row("IPv4 Prefix", ip4.prefix >= 0 ? std::to_string(ip4.prefix) : "");
  add_row("Subnet Mask", netmask);
  add_row("Gateway", ip4.gateway);
  if (!ip4.dns.empty()) {
    std::string dns_join;
    for (size_t i = 0; i < ip4.dns.size(); ++i) {
      if (i)
        dns_join += ", ";
      dns_join += ip4.dns[i];
    }
    add_row("DNS", dns_join);
  } else {
    add_row("DNS", "");
  }

  // IPv6 (simplified)
  try {
    Glib::Variant<Glib::ustring> ip6_path_var;
    active_connection_proxy->get_cached_property(ip6_path_var, "Ip6Config");
    auto path6 = ip6_path_var.get();
    std::string ipv6_addr;
    if (!path6.empty() && path6 != "/") {
      auto ip6_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path6, "org.freedesktop.NetworkManager.IP6Config");
      if (ip6_proxy) {
        Glib::VariantBase addr6_var;
        ip6_proxy->get_cached_property(addr6_var, "AddressData");
        if (addr6_var) {
          typedef std::map<Glib::ustring, Glib::VariantBase> Dict;
          auto array_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Dict>>>(addr6_var);
          auto vec = array_variant.get();
          if (!vec.empty()) {
            auto &first = vec.front();
            auto it = first.find("address");
            if (it != first.end()) {
              ipv6_addr = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(it->second).get();
            }
          }
        }
      }
    }
    add_row("IPv6 Address", ipv6_addr);
  } catch (...) {
    add_row("IPv6 Address", "");
  }

  popover_box.append(*content);

  // Action buttons
  auto actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  actions->set_halign(Gtk::Align::END);

  auto back_btn = Gtk::make_managed<Gtk::Button>("Back");
  back_btn->add_css_class("flat");
  back_btn->signal_clicked().connect([this]() {
    // restore list
    for (auto *child : popover_box.get_children())
      popover_box.remove(*child);
    popover_box.append(pop_scrolled);
    populate_wifi_list();
  });
  actions->append(*back_btn);

  auto close_btn = Gtk::make_managed<Gtk::Button>("Close");
  close_btn->add_css_class("flat");
  close_btn->signal_clicked().connect([this]() { button->get_popover()->popdown(); });
  actions->append(*close_btn);

  popover_box.append(*actions);
  pop_status_label.set_text("");
  popover_box.append(pop_status_label);
  popover->present();
}

WayfireNetworkInfo::~WayfireNetworkInfo() {}