#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <iostream>
#include <memory>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/evp.h>


#include "plugin.hpp"
#include "locker.hpp"
#include "lockergrid.hpp"
#include "pin.hpp"


/*
 *  To set the PIN Hash required to enable this plugin, try running
 *  `echo -n "1234" | sha512sum | head -c 128 > ~/.config/wf-locker.hash`
 *
 *  Replace the numbers inside the echo quote. There must be one or more digits,
 *  and any non-digit will render it impossible to unlock.
 */

PinPad::PinPad()
{
    for (int count = 0; count < 10; count++)
    {
        std::string number = std::to_string(count);
        numbers[count].add_css_class("pinpad-number");
        numbers[count].add_css_class("pinpad-button");
        numbers[count].set_label(number);
        numbers[count].signal_clicked().connect(
            [number] ()
        {
            auto plugin = WayfireLockerApp::get().get_plugin("pin");
            auto plugin_cast = std::dynamic_pointer_cast<WayfireLockerPinPlugin>(plugin);
            plugin_cast->add_digit(number);
        });
    }

    bsub.set_label("✔️");
    bcan.set_label("❌");
    bsub.add_css_class("pinpad-submit");
    bsub.add_css_class("pinpad-button");
    bcan.add_css_class("pinpad-cancel");
    bcan.add_css_class("pinpad-button");
    bcan.signal_clicked().connect(
        [] ()
    {
        auto plugin = WayfireLockerApp::get().get_plugin("pin");
        auto plugin_cast = std::dynamic_pointer_cast<WayfireLockerPinPlugin>(plugin);
        plugin_cast->reset_pin();
    });
    bsub.signal_clicked().connect(
        [] ()
    {
        auto plugin = WayfireLockerApp::get().get_plugin("pin");
        auto plugin_cast = std::dynamic_pointer_cast<WayfireLockerPinPlugin>(plugin);
        plugin_cast->submit_pin();
    });
    label.add_css_class("pinpad-current");
}

PinPad::~PinPad()
{}

void PinPad::init()
{
    attach(label, 0, 0, 3);
    attach(numbers[1], 0, 1);
    attach(numbers[2], 1, 1);
    attach(numbers[3], 2, 1);
    attach(numbers[4], 0, 2);
    attach(numbers[5], 1, 2);
    attach(numbers[6], 2, 2);
    attach(numbers[7], 0, 3);
    attach(numbers[8], 1, 3);
    attach(numbers[9], 2, 3);
    attach(bsub, 2, 4);
    attach(numbers[0], 1, 4);
    attach(bcan, 0, 4);
    set_vexpand(true);
    set_column_homogeneous(true);
    set_row_homogeneous(true);
}

WayfireLockerPinPlugin::WayfireLockerPinPlugin():
    WayfireLockerPlugin("locker/pin_enable", "locker/pin_position")
{
    if (!enable)
    {
        return;
    }

    /* TODO ... */
    // if (cmdline_config.has_value())
    // {
    // return cmdline_config.value();
    // }
    std::string config_dir;

    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL)
    {
        config_dir = std::string(getenv("HOME")) + "/.config";
    } else
    {
        config_dir = std::string(config_home);
    }

    std::ifstream f(config_dir + "/wf-locker.hash");
    if (!f.is_open())
    {
        std::cerr << "No PIN hash set" << std::endl;
        disabled = true;
        return;
    }

    std::string s;
    if (!getline(f, s))
    {
        std::cerr << "No PIN hash set" << std::endl;
        disabled = true;
        return;
    }

    if (s.length() != 128)
    {
        std::cerr << "Invalid PIN hash" << std::endl;
        disabled = true;
        return;
    }

    pinhash = s;
}

void WayfireLockerPinPlugin::init()
{}

void WayfireLockerPinPlugin::deinit()
{}

void WayfireLockerPinPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    if(disabled)
    {
        return;
    }
    pinpads.emplace(id, new PinPad());
    auto pinpad = pinpads[id];
    pinpad->add_css_class("pinpad");
    pinpad->init();
    grid->attach(*pinpad, position);
    update_labels(); /* Update all to set this one? maybe overkill */
}

void WayfireLockerPinPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    if(disabled)
    {
        return;
    }
    grid->remove(*pinpads[id]);
    pinpads.erase(id);
}

void WayfireLockerPinPlugin::add_digit(std::string digit)
{
    pin = pin + digit;
    update_labels();
}

void WayfireLockerPinPlugin::reset_pin()
{
    pin = "";
    update_labels();
}

void WayfireLockerPinPlugin::update_labels()
{
    std::string asterisks(pin.length(), '*');
    for (auto& it : pinpads)
    {
        it.second->label.set_label(asterisks);
    }
}

void WayfireLockerPinPlugin::submit_pin()
{
    auto hash = sha512(pin);
    if (hash == pinhash)
    {
        WayfireLockerApp::get().perform_unlock();
    }

    pin = "";
    update_labels();
}

std::string WayfireLockerPinPlugin::sha512(const std::string input)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_length = 0;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
    EVP_DigestInit(mdctx, EVP_sha512());
    EVP_DigestUpdate(mdctx, input.c_str(), input.size());
    EVP_DigestFinal(mdctx, hash, &hash_length);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;

    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}
