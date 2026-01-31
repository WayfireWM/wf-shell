#include <fstream>
#include <iostream>
#include <string>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iomanip>

#include "locker-pin.hpp"

WayfirePinChangeApp::WayfirePinChangeApp() : Gtk::Application("org.wayfire.locker-pin",
        Gio::Application::Flags::NONE)
{
    signal_activate().connect(sigc::mem_fun(*this, &WayfirePinChangeApp::activate));
    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL)
    {
        hash_file = std::string(getenv("HOME")) + "/.config/wf-locker.hash";
    } else
    {
        hash_file = std::string(std::string(config_home) + "/wf-locker.hash");
    }

    std::ifstream f(hash_file);
    if (!f.is_open())
    {
        std::cerr << "No PIN hash set" << std::endl;
        hash = "";
        return;
    }

    std::string s;
    if (!getline(f, s))
    {
        std::cerr << "No PIN hash set" << std::endl;
        hash = "";
        return;
    }

    if (s.length() != 128)
    {
        std::cerr << "Invalid PIN hash" << std::endl;
        hash = "";
        return;
    }

    hash = s;
}

void WayfirePinChangeApp::activate()
{
    add_window(window);
    window.set_child(grid);
    grid.attach(label, 0, 0, 3, 1);

    for (int val = 0; val < 10; val++)
    {
        std::string val_string = std::to_string(val);
        numbers[val].set_label(val_string);
        numbers[val].signal_clicked().connect(
            [this, val_string] ()
        {
            pin_key(val_string);
        });
    }

    cancel.set_label("❌");
    submit.set_label("✔️");
    cancel.signal_clicked().connect(
        [this] ()
    {
        pin_cancel();
    });
    submit.signal_clicked().connect(
        [this] ()
    {
        pin_submit();
    });

    grid.attach(numbers[0], 1, 4);
    grid.attach(numbers[1], 0, 1);
    grid.attach(numbers[2], 1, 1);
    grid.attach(numbers[3], 2, 1);
    grid.attach(numbers[4], 0, 2);
    grid.attach(numbers[5], 1, 2);
    grid.attach(numbers[6], 2, 2);
    grid.attach(numbers[7], 0, 3);
    grid.attach(numbers[8], 1, 3);
    grid.attach(numbers[9], 2, 3);
    grid.attach(submit, 2, 4);
    grid.attach(cancel, 0, 4);

    if (hash.length() > 1)
    {
        stage = Stage::PREVIOUS;
    } else
    {
        stage = Stage::FIRST;
    }

    set_label();
    grid.set_halign(Gtk::Align::CENTER);
    grid.set_valign(Gtk::Align::CENTER);
    window.present();
}

void WayfirePinChangeApp::set_label()
{
    std::string asterisks(pin.length(), '*');

    if (stage == Stage::PREVIOUS)
    {
        label.set_label("Enter current PIN\n" + asterisks);
    } else if (stage == Stage::FIRST)
    {
        label.set_label("Enter new PIN\n" + asterisks);
    } else if (stage == Stage::CONFIRM)
    {
        label.set_label("Confirm new PIN\n" + asterisks);
    }
}

void WayfirePinChangeApp::pin_cancel()
{
    pin = "";
    set_label();
}

void WayfirePinChangeApp::pin_key(std::string key)
{
    pin = pin + key;
    set_label();
}

void WayfirePinChangeApp::pin_submit()
{
    if (pin.length() == 0)
    {
        return;
    }

    if (stage == Stage::PREVIOUS)
    {
        std::string tmphash = sha512(pin);
        if (tmphash == hash)
        {
            stage = Stage::FIRST;
            pin   = "";
            set_label();
            return;
        }

        pin = "";
        set_label();
        return;
    } else if (stage == Stage::FIRST)
    {
        confirm = sha512(pin);
        pin     = "";
        stage   = Stage::CONFIRM;
        set_label();
        return;
    } else if (stage == Stage::CONFIRM)
    {
        std::string tmphash = sha512(pin);
        if (tmphash == confirm)
        {
            std::ofstream out(hash_file);
            out << tmphash;
            out.close();
            exit(0);
        }

        confirm = "";
        pin     = "";
        stage   = Stage::FIRST;
        set_label();
        return;
    }
}

std::string WayfirePinChangeApp::sha512(const std::string input)
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

/* Starting point */
int main(int argc, char **argv)
{
    auto app = new WayfirePinChangeApp();
    app->run();
    exit(0);
}
