#pragma once
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/revealer.h>
#include <giomm.h>

#include "plugin.hpp"
#include "lockergrid.hpp"


std::string substitute_string(const std::string from, const std::string to, const std::string in);
std::string substitute_strings(const std::vector<std::tuple<std::string, std::string>> pairs,
    const std::string in);

class WayfireLockerMPRISWidget : public Gtk::Revealer
{
  private:
    Glib::RefPtr<Gio::DBus::Proxy> proxy;
    std::string name;
    void playbackstatus(std::string value);
    void metadata(std::map<std::string, Glib::VariantBase> value);
    void cangonext(bool value);
    void cangoprev(bool value);
    void cancontrol(bool value);
    std::vector<sigc::connection> signals;

  public:
    Gtk::Label label;
    Gtk::Button next, prev, playpause, kill;
    Gtk::Box box, controlbox, sidebox;
    Gtk::Image image;
    std::string image_path = "";

    WayfireLockerMPRISWidget(std::string name, Glib::RefPtr<Gio::DBus::Proxy> proxy);
    ~WayfireLockerMPRISWidget();
};

class WayfireLockerMPRISCollective : public Gtk::Box
{
  private:
    std::map<std::string, Glib::RefPtr<WayfireLockerMPRISWidget>> children;

  public:
    void add_child(std::string name, Glib::RefPtr<Gio::DBus::Proxy> proxy);
    void rem_child(std::string name);
    WayfireLockerMPRISCollective()
    {
        set_orientation(Gtk::Orientation::VERTICAL);
    }
};

class WayfireLockerMPRISPlugin : public WayfireLockerPlugin
{
  private:
    Glib::RefPtr<Gio::DBus::Proxy> manager_proxy;
    std::map<std::string, Glib::RefPtr<Gio::DBus::Proxy>> clients;
    std::map<int, Glib::RefPtr<WayfireLockerMPRISCollective>> widgets;

  public:
    WayfireLockerMPRISPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    void add_client(std::string client);
    void rem_client(std::string client);

    gulong hook_play, hook_pause, hook_stop, hook_metadata;
};
