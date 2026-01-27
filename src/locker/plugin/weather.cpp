#include <memory>
#include <iostream>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>

#include "lockergrid.hpp"
#include "weather.hpp"
#include "yyjson.h"


void WayfireLockerWeatherPlugin::update_labels(std::string text)
{
    for (auto& it : weather_widgets)
    {
        ((Gtk::Label *)it.second->get_first_child())->set_markup(text);
    }

    label_contents = text;
}

void WayfireLockerWeatherPlugin::update_icons(std::string path)
{
    for (auto& it : weather_widgets)
    {
        ((Gtk::Image *)it.second->get_first_child()->get_next_sibling())->set(path);
    }

    icon_path = path;
}

void WayfireLockerWeatherPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    weather_widgets.emplace(id, std::shared_ptr<Gtk::Box>(new Gtk::Box()));

    auto weather_widget = weather_widgets[id];
    if (!shown)
    {
        weather_widget->hide();
    }
    auto label = Gtk::Label();
    label.add_css_class("weather");
    label.set_markup(label_contents);
    label.set_justify(Gtk::Justification::CENTER);
    weather_widget->append(label);

    Gtk::Image icon(icon_path);
    icon.add_css_class("weather");
    weather_widget->append(icon);

    grid->attach(*weather_widget, position);
}

void WayfireLockerWeatherPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*weather_widgets[id]);
    weather_widgets.erase(id);
}

void WayfireLockerWeatherPlugin::update_weather()
{
    std::string weather_data_dir;

    weather_data_dir = std::string(getenv("HOME")) + "/.local/share/weather/data";

    std::string file_path = weather_data_dir + "/data.json";

    yyjson_read_flag flg = 0;
    yyjson_read_err err;

    yyjson_doc *doc = yyjson_read_file(file_path.c_str(), flg, NULL, &err);

    if (doc == NULL)
    {
        std::cerr << "Error reading JSON file " << file_path << ": " << err.msg << std::endl;
        hide();
        return;
    }
    show();

    yyjson_val *root_obj = yyjson_doc_get_root(doc);

    if (root_obj && yyjson_is_obj(root_obj))
    {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(root_obj, &iter);
        yyjson_val *key, *val;
        while ((key = yyjson_obj_iter_next(&iter)))
        {
            val = yyjson_obj_iter_get_val(key);

            if (yyjson_get_str(key) == std::string("temp"))
            {
                this->update_labels(yyjson_get_str(val));
            } else if (yyjson_get_str(key) == std::string("icon"))
            {
                update_icons(yyjson_get_str(val));
            }
        }
    }

    yyjson_doc_free(doc);

}

WayfireLockerWeatherPlugin::WayfireLockerWeatherPlugin():
  WayfireLockerPlugin("locker/weather_enable", "locker/weather_position")
{}

void WayfireLockerWeatherPlugin::init()
{
    update_weather();
    timeout = Glib::signal_timeout().connect_seconds(
        [this] ()
    {
        this->update_weather();
        return G_SOURCE_CONTINUE;
    }, 600);
}

void WayfireLockerWeatherPlugin::deinit()
{
    timeout.disconnect();
}

void WayfireLockerWeatherPlugin::hide()
{
    for (auto& it : weather_widgets)
    {
        it.second->hide();
    }

    shown = false;
}

void WayfireLockerWeatherPlugin::show()
{
    for (auto& it : weather_widgets)
    {
        it.second->show();
    }

    shown = true;
}