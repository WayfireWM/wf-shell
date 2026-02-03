#include <sigc++/functors/mem_fun.h>
#include <memory>
#include <gtkmm.h>

#include "bluetooth.hpp"
#include "manager.hpp"
#include "modem.hpp"
#include "network.hpp"
#include "vpn.hpp"
#include "wifi-ap.hpp"
#include "wifi.hpp"
#include "wired.hpp"
#include "network-widget.hpp"


AccessPointWidget::AccessPointWidget(std::string path_in, std::shared_ptr<AccessPoint> ap_in):
  ap(ap_in),path(path_in)
{
    add_css_class("access-point");
    append(image);
    append(label);

    auto update_ap = [this] () {
            label.set_label(ap->get_ssid());
            image.set_from_icon_name(ap->get_icon_name());
        };
    signal = ap_in->signal_altered().connect(update_ap);
    update_ap();
}

AccessPointWidget::~AccessPointWidget()
{
    signal.disconnect();
}

VPNControlWidget::VPNControlWidget(std::shared_ptr<VpnConfig> config):
  config(config)
{
    append(image);
    append(label);
    label.set_label(config->name);
    image.set_from_icon_name("network-vpn-symbolic");
}

DeviceControlWidget::DeviceControlWidget(std::shared_ptr<Network> network):
    network(network)
{

    add_css_class("device");
    set_halign(Gtk::Align::FILL);
    set_orientation(Gtk::Orientation::VERTICAL);
    append(topbox);
    append(revealer);
    topbox.append(image);
    topbox.append(label);
    revealer.set_child(revealer_box);
    revealer_box.set_orientation(Gtk::Orientation::VERTICAL);
    auto wifi = std::dynamic_pointer_cast<WifiNetwork>(network);
    auto mobile = std::dynamic_pointer_cast<ModemNetwork>(network);
    auto wired = std::dynamic_pointer_cast<WiredNetwork>(network);
    auto bt = std::dynamic_pointer_cast<BluetoothNetwork>(network);
    if(wifi)
    {
        type="wifi";
        /* TODO Sort*/
        for (auto &it : wifi->get_access_points())
        {
            add_access_point(it.second);
        }

        wifi->signal_add_access_point().connect(
            [this] (std::shared_ptr<AccessPoint> ap) {
                add_access_point(ap);
            }
        );

        wifi->signal_remove_access_point().connect(
            [this] (std::shared_ptr<AccessPoint> ap) {
                remove_access_point(ap->get_path());
            }
        );

        auto wifi_cb = 
            [this, wifi] () {
                if (wifi->get_current_access_point_path() == "/")
                {
                    revealer.set_reveal_child(true);
                    topbox.hide();
                } else
                {
                    revealer.set_reveal_child(false);
                    topbox.show();
                }
            };
        network->signal_network_altered().connect(wifi_cb);
        wifi_cb();
    } else if (wired)
    {
        type="wired";
        revealer.hide();
    } else if (bt)
    {
        type="bt";
        revealer.hide();
    } else if (mobile)
    {
        type="mobile";
        revealer.hide();
    } else
    {
        type="broken";
        revealer.hide();
    }
    
    /* Click toggles connection on/off */
    auto click = Gtk::GestureClick::create();
    click->signal_released().connect(
        [network] (int, double, double) {
            network->toggle();
        }
    );
    label.add_controller(click);

    /* Set label and image based on friendly info */
    auto network_change_cb = [this, network] () {
            image.set_from_icon_name(network->get_icon_symbolic());
            label.set_label(network->get_name());
            if (network->is_active())
            {
                label.add_css_class("active");
            } else
            {
                label.remove_css_class("active");
            }
        };
    network->signal_network_altered().connect(network_change_cb);
    network_change_cb();

}

void DeviceControlWidget::remove_access_point(std::string path){
    auto widget = access_points[path];
    if (widget)
    {
        revealer_box.remove(*widget);
        access_points.erase(path);
    }
}

void DeviceControlWidget::add_access_point(std::shared_ptr<AccessPoint> ap)
{
    if (!ap || ap->get_ssid()=="")
    {
        return;
    }
    auto path = ap->get_path();
    if (path == "/")
    {
        return;
    }
    access_points.emplace(path, new AccessPointWidget(path,  ap));
    auto widget = access_points[path];
    auto click = Gtk::GestureClick::create();
    
    click->signal_released().connect(
        [this, path] (int, double, double) {
            selected_access_point(path);
        }
    );
    widget->add_controller(click);
    revealer_box.append(*access_points[path]);
}

void DeviceControlWidget::selected_access_point(std::string path)
{
    auto wifi = std::dynamic_pointer_cast<WifiNetwork>(network);
    if (!wifi)
    {
        std::cerr << "Cannot select AP on non-wifi device" << std::endl;
    }
    if (path == "/")
    {
        return;
    }
    if (wifi->get_current_access_point_path() == path)
    {
        wifi->disconnect();
    } else 
    {
        wifi->connect(path);    
    }
}

NetworkControlWidget::NetworkControlWidget()
{
    add_css_class("network-control-center");
    set_orientation(Gtk::Orientation::VERTICAL);
    network_manager = NetworkManager::getInstance();

    network_manager->signal_device_added().connect(
        sigc::mem_fun(*this, &NetworkControlWidget::add_device)
    );

    network_manager->signal_device_removed().connect(
        sigc::mem_fun(*this, &NetworkControlWidget::remove_device)
    );

    /* Fill already existing devices */

    for (auto &device : network_manager->get_all_devices())
    {
        add_device(device.second);
    }
    top.append(global_networking);
    top.append(wifi_networking);
    top.append(mobile_networking);
    append(top);
    append(wire_box);
    append(mobile_box);
    append(wifi_box);
    append(bt_box);
    append(vpn_box);

    mobile_networking.set_halign(Gtk::Align::END);
    wifi_networking.set_halign(Gtk::Align::END);

    global_networking.set_label("Networking");
    wifi_networking.set_label("Wifi");
    mobile_networking.set_label("Mobile");

    signal_network = global_networking.signal_toggled().connect(
        [this] () {
            network_manager->networking_global_set(global_networking.get_active());
        }
    );

    signal_wifi = wifi_networking.signal_toggled().connect(
        [this] () {
            network_manager->wifi_global_set(wifi_networking.get_active());
        }
    );

    signal_mobile = mobile_networking.signal_toggled().connect(
        [this] () {
            network_manager->mobile_global_set(mobile_networking.get_active());
        }
    );

    /* Global killswitches from NM proper */
    auto global_cb = 
        [this] () {
            auto [wifi_soft, wifi_hard] = network_manager->wifi_global_enabled();
            auto [mobile_soft, mobile_hard] = network_manager->mobile_global_enabled();
            auto global = network_manager->networking_global_enabled();
            if (!wifi_hard)
            {
                wifi_networking.set_label("Wifi ✈");
                signal_wifi.block(true);
                wifi_networking.set_active(false);
                signal_wifi.unblock();
            } else if(!wifi_soft)
            {
                wifi_networking.set_label("Wifi");
                signal_wifi.block(true);
                wifi_networking.set_active(false);
                signal_wifi.unblock();
            } else {
                wifi_networking.set_label("Wifi");
                signal_wifi.block(true);
                wifi_networking.set_active(true);
                signal_wifi.unblock();
            }
            if(!mobile_hard)
            {
                mobile_networking.set_label("Mobile ✈");
                signal_mobile.block(true);
                mobile_networking.set_active(false);
                signal_mobile.unblock();
            } else if(!mobile_soft)
            {
                mobile_networking.set_label("Mobile");
                signal_mobile.block(true);
                mobile_networking.set_active(false);
                signal_mobile.unblock();
            } else {
                mobile_networking.set_label("Mobile");
                signal_mobile.block(true);
                mobile_networking.set_active(true);
                signal_mobile.unblock();
            }
            signal_network.block(true);
            global_networking.set_active(global);
            signal_network.unblock();
        };
    
    network_manager->signal_global_toggle().connect(global_cb);
    global_cb();

    for (auto &it : network_manager->get_all_vpns())
    {
        add_vpn(it.second);
    }
    
}

void NetworkControlWidget::add_vpn(std::shared_ptr<VpnConfig> config)
{
    auto widget = std::make_shared<VPNControlWidget>(config);
    vpn_widgets.emplace(config->path, widget);

    auto click = Gtk::GestureClick::create();
    click->signal_released().connect(
        [this, config] (int, double, double) {
            auto primary = network_manager->get_primary_network();
            if (primary->has_vpn)
            {
                network_manager->deactivate_connection(primary->get_path());
            } else {
                network_manager->activate_connection(config->path, "/", "/");
            }
        }
    );
    widget->add_controller(click);
    vpn_box.append(*widget);
}

void NetworkControlWidget::remove_vpn(std::string path)
{
    auto widget = vpn_widgets[path];
    vpn_box.remove(*widget);
    vpn_widgets.erase(path);
}

void NetworkControlWidget::add_device(std::shared_ptr<Network> network)
{
    /* GUI doesn't want our null-device */
    if (network->get_path() == "/")
    {
        return;
    }
    auto new_controller = std::make_shared<DeviceControlWidget>(network);
    widgets.emplace(network->get_path(), new_controller);
    auto widget = widgets[network->get_path()];
    if (widget->type == "wifi")
    {
        wifi_box.append(*widget);
    } else if (widget->type == "mobile")
    {
        mobile_box.append(*widget);
    } else if (widget->type == "wired")
    {
        wire_box.append(*widget);
    } else if (widget->type == "bt")
    {
        bt_box.append(*widget);
    } else {
        std::cout << "Unknown network type : " << widget->type << std::endl;
    }
    
}

void NetworkControlWidget::remove_device(std::shared_ptr<Network> network)
{
    auto widget = widgets[network->get_path()];
    /* TODO Remove warnings by checking? */
    wifi_box.remove(*widget);
    mobile_box.remove(*widget);
    wire_box.remove(*widget);
    vpn_box.remove(*widget);
    bt_box.remove(*widget);
    widgets.erase(network->get_path());
}