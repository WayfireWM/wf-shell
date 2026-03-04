#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <pwd.h>
#include <grp.h>
extern "C" {
#include <sys/inotify.h>
}
#include <errno.h>

#include "light.hpp"

class WfLightSysfsControl : public WfLightControl
{
    // if we exist, it means we can just read/write, as the files and permissions
    // have already been checked and are being monitored with inotify

  private:
    std::string path, connector_name;

    int get_max()
    {
        std::ifstream max_file(path + "/max_brightness");
        if (!max_file.is_open())
        {
            std::cerr << "Failed to get max brightness for device at " << path << '\n';
            return 0;
        }

        int max;
        max_file >> max;
        max_file.close();
        return max;
    }

  public:
    WfLightSysfsControl(WayfireLight *parent, std::string _path) : WfLightControl(parent)
    {
        path = _path;

        // this resolves to something of the sort :
        ///sys/devices/pciXXXX:XX/XXXX:XX:XX.X/XXXX:XX:XX.X/drm/cardX-<connector-name>/<name>
        // what we are intersted in here is the connector name
        std::string realpath = std::filesystem::canonical(path);
        // the offset is constant until cardX, after which we look for the -.
        connector_name = realpath.substr(60, realpath.size());
        connector_name = connector_name.substr(connector_name.find("-") + 1, connector_name.size());
        // then, the connector is what remains until /
        connector_name = connector_name.substr(0, connector_name.find("/"));

        scale.set_target_value(get_brightness());
        label.set_text(get_name());
    }

    std::string get_connector()
    {
        return connector_name;
    }

    std::string get_name()
    {
        return "Integrated display";
    }

    void set_brightness(double brightness)
    {
        std::ofstream b_file(path + "/brightness");
        if (!b_file.is_open())
        {
            std::cerr << "Failed to open brightness for device at " << path << '\n';
            return;
        }

        // something of the sort avoids formatting issues with locales
        b_file << std::to_string((int)(brightness * (double)get_max()));
        if (b_file.fail())
        {
            std::cerr << "Failed to write brightness for device at " << path << '\n';
            return;
        }

        update_parent_icon();
    }

    double get_brightness()
    {
        std::ifstream b_file(path + "/brightness");
        if (!b_file.is_open())
        {
            std::cerr << "Failed to get brightness for device at " << path << '\n';
            return 0;
        }

        int brightness, max;
        b_file >> brightness;
        b_file.close();
        max = get_max();
        return (((double)brightness + (double)max) / (double)max) - 1;
    }
};

SysfsSurveillor::SysfsSurveillor()
{
    fd = inotify_init();
    if (fd == -1)
    {
        std::cerr << "Light widget: initialisation of inotify on sysfs failed.\n";
        return;
    }

    const auto path = "/sys/class/backlight";

    wd_additions = inotify_add_watch(fd, path, IN_CREATE);
    wd_additions = inotify_add_watch(fd, path, IN_DELETE);

    // look for present integrated backlights
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        add_dev(entry);
    }

    inotify_thread = std::thread(&SysfsSurveillor::handle_inotify_events, this);
}

SysfsSurveillor::~SysfsSurveillor()
{
    // clean up inotify
    close(fd);
    fd = -1;

    // remove controls from every widget
    for (auto& widget : widgets)
    {
        strip_widget(widget);
    }
}

void SysfsSurveillor::handle_inotify_events()
{
    // according to the inotify man page, aligning as such ensures
    // proper function and avoids performance loss for "some systems"
    char buf[2048] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t size;

    for (;;)
    {
        // read, which will block until the next inotify event
        size = read(fd, buf, sizeof(buf));
        if (size == -1)
        {
            if (errno != EAGAIN)
            {
                std::cerr << "Light widget: error reading inotify event.\n";
            } else
            {
                break;
            }
        }

        for (char *ptr = buf; ptr < buf + size; ptr += sizeof(struct inotify_event) + event->len)
        {
            event = (const struct inotify_event*)ptr;

            // a registered brightness file was changed
            if (event->mask & IN_CLOSE_WRITE)
            {
                // look for the watch descriptor
                if (wd_to_path_controls.find(event->wd) != wd_to_path_controls.end())
                {
                    // update every control
                    auto controls = wd_to_path_controls[event->wd].second;
                    Glib::signal_idle().connect_once([controls] ()
                    {
                        for (auto control : controls)
                        {
                            control->set_scale_target_value(control->get_brightness());
                        }
                    });
                }
            }

            // metadata changed, so maybe permissions
            if (event->mask & IN_ATTRIB)
            {
                if (wd_to_path_controls.find(event->wd) != wd_to_path_controls.end())
                {
                    // get the path without which file, just the directory
                    auto path = wd_to_path_controls[event->wd].first;

                    // only recheck if the permissions to brightness or max_brightenss changed
                    if ((event->name == (path.string() + "/brightness")) ||
                        (event->name == (path.string() + "/max_brightness")))
                    {
                        // if we cannot do what’s needed on the device, remove it
                        if (!check_perms(path))
                        {
                            rem_dev(path);
                        }
                    }
                }
            }

            // a backlight device appeared
            if (event->mask & IN_CREATE)
            {
                if ((wd_additions == event->wd) && event->len)
                {
                    std::string name = event->name;
                    Glib::signal_idle().connect_once([this, name] ()
                    {
                        add_dev(std::filesystem::path("/sys/class/backlight") / name);
                    });
                }
            }

            // a backlight device was removed
            if (event->mask & IN_DELETE)
            {
                if ((wd_removal == event->wd) && event->len)
                {
                    std::string name = event->name;
                    Glib::signal_idle().connect_once([this, name] ()
                    {
                        rem_dev(std::filesystem::path("/sys/class/backlight") / name);
                    });
                }
            }
        }
    }
}

bool SysfsSurveillor::check_perms(std::filesystem::path path)
{
    // those are the two files we are interested in,
    // brightness for reading / setting the value (0 to max),
    // and max_brightness for getting the maximum value for this device.
    // we need to be able to read max_brightness and write to brightness.
    const std::filesystem::path b_path     = path.string() + "/brightness";
    const std::filesystem::path max_b_path = path.string() + "/max_brightness";

    if (access(b_path.c_str(), R_OK | W_OK))
    {
        std::cerr << "Cannot read/write brightness for " << path << ", ignoring.\n";
        return false;
    }

    if (access(max_b_path.c_str(), R_OK))
    {
        std::cerr << "Cannot read max_brightness for " << path << ", ignoring.\n";
        return false;
    }

    return true;
}

void SysfsSurveillor::add_dev(std::filesystem::path path)
{
    if (!check_perms(path))
    {
        return;
    }

    // create a watch descriptor on the brightness file
    int wd = inotify_add_watch(fd, path.string().c_str(), IN_CLOSE_WRITE | IN_ATTRIB);
    if (wd == -1)
    {
        std::cerr << "Light widget: failed to register inotify watch descriptor.\n";
        return;
    }

    wd_to_path_controls.insert({wd, {path, {}}});

    // create a control for each widget, and insert it in the vector we just created
    for (auto widget : widgets)
    {
        auto control = std::make_shared<WfLightSysfsControl>(widget, path);
        wd_to_path_controls[wd].second.push_back(control);

        widget->add_control(control);
    }
}

void SysfsSurveillor::rem_dev(std::filesystem::path path)
{
    // device was removed, we can remove the entire entry (wd path, controls)
    for (auto it : wd_to_path_controls)
    {
        if (it.second.first == path)
        {
            auto& controls = it.second.second;
            for (auto control : controls)
            {
                control->get_parent()->rem_control(control.get());
            }

            wd_to_path_controls.erase(it.first);
        }
    }
}

void SysfsSurveillor::catch_up_widget(WayfireLight *widget)
{
    // for each managed device, create a control and add it to the widget and keep track of it
    for (auto& it : wd_to_path_controls)
    {
        auto control = std::make_shared<WfLightSysfsControl>(widget, it.second.first.string());
        it.second.second.push_back(control);
        widget->add_control(control);
    }
}

void SysfsSurveillor::strip_widget(WayfireLight *widget)
{
    for (auto& [wd, path_controls] : wd_to_path_controls)
    {
        auto& controls = path_controls.second;
        controls.erase(std::remove_if(controls.begin(), controls.end(),
            [widget] (const std::shared_ptr<WfLightControl>& c)
        {
            return c->get_parent() == widget;
        }),
            controls.end());
    }
}

SysfsSurveillor& SysfsSurveillor::get()
{
    if (!instance)
    {
        instance = std::unique_ptr<SysfsSurveillor>(new SysfsSurveillor());
    }

    return *instance;
}
