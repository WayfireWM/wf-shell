#pragma once
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/revealer.h>
#include <giomm.h>
#include <sigc++/connection.h>

#include "glib.h"
#include "glibmm/main.h"
#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"


std::string substitute_string(const std::string from, const std::string to, const std::string in);
std::string substitute_strings(const std::vector<std::tuple<std::string, std::string>> pairs,
    const std::string in);

class WayfireLockerMPRISWidget : public WayfireLockerTimedRevealer
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

class WayfireLockerMPRISCollective : public WayfireLockerTimedRevealer
{
  private:
    std::map<std::string, Glib::RefPtr<WayfireLockerMPRISWidget>> children;
    Gtk::Box box;

  public:
    void add_child(std::string name, Glib::RefPtr<Gio::DBus::Proxy> proxy);
    void rem_child(std::string name);
    WayfireLockerMPRISCollective():
      WayfireLockerTimedRevealer("locker/mpris_always")
    {
        /* At next chance, force visibility */
        Glib::signal_idle().connect([this] () {
          set_reveal_child(true);
          return G_SOURCE_REMOVE;
        });
        set_child(box);
        box.set_orientation(Gtk::Orientation::VERTICAL);
    }
    void activity() override;
};

class WayfireLockerMPRISPlugin : public WayfireLockerPlugin
{
  private:
    Glib::RefPtr<Gio::DBus::Proxy> manager_proxy;
    std::map<std::string, Glib::RefPtr<Gio::DBus::Proxy>> clients;
    std::map<int, Glib::RefPtr<WayfireLockerMPRISCollective>> widgets;
    sigc::connection signal;
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
