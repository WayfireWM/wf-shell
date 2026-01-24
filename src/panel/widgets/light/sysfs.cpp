#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <regex>
#include <pwd.h>
#include <grp.h>
extern "C"{
    #include <sys/inotify.h>
}
#include <errno.h>

#include "light.hpp"

class WfLightSysfsControl: public WfLightControl
{
    friend class SysfsSurveillor;

    // if we exist, it means we can just read/write, as the files and permissions
    // have already been checked and are being monitored with inotify

    protected:
        std::string path, connector_name;

        int get_max(){
            std::ifstream max_file(path + "/max_brightness");
            if (!max_file.is_open()){
                std::cerr << "Failed to get max brightness for device at " << path << '\n';
                return 0;
            }

            int max;
            max_file >> max;
            max_file.close();
            return max;
        }

    public:
        WfLightSysfsControl(WayfireLight *parent, std::string _path) : WfLightControl(parent){
            path = _path;

            // this resolves to something of the sort :
            // /sys/devices/pciXXXX:XX/XXXX:XX:XX.X/XXXX:XX:XX.X/drm/cardX-<connector-name>/<name>
            // what we are intersted in here is the connector name
            std::string realpath = std::filesystem::canonical(path);
            // the offset is constant until cardX, after which we look for the -.
            connector_name = realpath.substr(60, realpath.size());
            connector_name = connector_name.substr(connector_name.find("-") + 1, connector_name.size());
            // then, the connector is what remains until /
            connector_name = connector_name.substr(0, connector_name.find("/"));

            scale.set_target_value(get_brightness());
            label.set_text(get_name());

            icons = brightness_display_icons;
        }

        std::string get_name(){
            return connector_name;
        }

        void set_brightness(double brightness){
            std::ofstream b_file(path + "/brightness");
            if (!b_file.is_open()){
                std::cerr << "Failed to open brightness for device at " << path << '\n';
                return;
            }
            // something of the sort avoids formatting issues with locales
            b_file << std::to_string((int)(brightness * (double)get_max()));
            if (b_file.fail()){
                std::cerr << "Failed to write brightness for device at " << path << '\n';
                return;
            }
            parent->update_icon();
        }

        double get_brightness(){
            std::ifstream b_file(path + "/brightness");
            if (!b_file.is_open()){
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

// utilities to check permissions
bool is_group_member(gid_t file_group_id)
{
    gid_t current_group = getgid();
    gid_t supplementary_groups[NGROUPS_MAX];
    int n_groups = getgroups(NGROUPS_MAX, supplementary_groups);

    if (current_group == file_group_id)
        return true;

    for (int i = 0; i < n_groups; ++i)
    {
        if (supplementary_groups[i] == file_group_id)
            return true;
    }

    return false;
}

// let’s assume the file is not owned by the user and only bother with groups
bool is_in_file_group(const std::filesystem::path& file_path)
{
    struct stat file_info;
    if (stat(file_path.c_str(), &file_info) != 0)
    {
        std::cerr << "Failed to stat " << file_path << ".\n";
        return false;
    }

    struct group *file_group = getgrgid(file_info.st_gid);

    if (!file_group)
    {
        std::cerr << "Failed to fetch owner/group info for " << file_path << ".\n";
        return false;
    }

    if (is_group_member(file_info.st_gid))
    {
        return true;
    } else
    {
        return false;
    }
}

// singleton that monitors sysfs and calls the necessary functions
// monitors appearance and deletion of backlight devices
// and the brightness of each of them
class SysfsSurveillor {
    private:
        SysfsSurveillor(){
            fd = inotify_init();
            if (fd == -1){
                std::cerr << "Light widget: initialisation of inotify on sysfs failed.\n";
                return;
            }

            const auto path = "/sys/class/backlight";

            wd_additions = inotify_add_watch(fd, path, IN_CREATE);
            wd_additions = inotify_add_watch(fd, path, IN_DELETE);

            // look for present integrated backlights
            for (const auto& entry : std::filesystem::directory_iterator(path)){
                add_dev(entry);
            }

            inotify_thread = std::thread(&SysfsSurveillor::handle_inotify_events, this);
        }

        static inline std::unique_ptr<SysfsSurveillor> instance;

        int fd; // inotify file descriptor

        void handle_inotify_events(){
            // according to the inotify man page, aligning as such ensures
            // proper function and avoids performance loss for "some systems"
            char buf[2048] __attribute__((aligned(__alignof__(struct inotify_event))));
            const struct inotify_event *event;
            ssize_t size;

            for (;;){
                // read, which will block until the next inotify event
                size = read(fd, buf, sizeof(buf));
                if (size == -1 && errno != EAGAIN){
                    std::cerr << "Light widget: error reading inotify event.\n";
                }
                if (size <= 0)
                    break;

                for (char *ptr = buf ; ptr < buf + size ; ptr += sizeof(struct inotify_event) + event->len){
                    event = (const struct inotify_event*) ptr;

                    // a registered brightness file was changed
                    if (event->mask & IN_CLOSE_WRITE){
                        // look for the watch descriptor
                        if (wd_to_path_controls.find(event->wd) != wd_to_path_controls.end()){
                            // update every control
                            for (auto control : wd_to_path_controls[event->wd].second){
                                control->set_scale_target_value(control->get_brightness());
                            }
                        }
                    }

                    // metadata changed, so maybe permissions
                    if (event->mask & IN_ATTRIB){
                        if (wd_to_path_controls.find(event->wd) != wd_to_path_controls.end())
                        {
                            // get the path without which file, just the directory
                            auto path = wd_to_path_controls[event->wd].first;

                            // only recheck if the permissions to brightness or max_brightenss changed
                            if (event->name == (path.string() + "/brightness") ||
                                event->name == (path.string() + "/max_brightness"))
                            {
                                // if we cannot do what’s needed on the device, remove it
                                if (!check_perms(path)){
                                    rem_dev(path);
                                }

                            }
                        }
                    }

                    // a backlight device appeared
                    if (event->mask & IN_CREATE){
                        if (wd_additions == event->wd){
                            if (event->len)
                            {
                                add_dev(event->name);
                            }
                        }
                    }

                    // a backlight device was removed
                    if (event->mask & IN_DELETE){
                        if (wd_removal == event->wd){
                            if (event->len)
                            {
                                rem_dev(event->name);
                            }
                        }
                    }
                }
            }
        }

        bool check_perms(std::filesystem::path path){
            // those are the two files we are interested in,
            // brightness for reading / setting the value (0 to max),
            // and max_brightness for getting the maximum value for this device.
            // we need to be able to read max_brightness and write to brightness.
            const std::filesystem::path b_path = path.string() + "/brightness";
            const std::filesystem::path max_b_path = path.string() + "/max_brightness";

            // verity they exist
            if (!std::filesystem::exists(b_path)){
                std::cout << "No brightness found for " << path.string() << ", ignoring.\n";
                return false;
            }
            if (!std::filesystem::exists(b_path)){
                std::cout << "No max_brightness found for " << path.string() << ", ignoring.\n";
                return false;
            }

            auto max_perms = std::filesystem::status(max_b_path).permissions();
            // can the file be read?
            if (!((max_perms & std::filesystem::perms::others_read) != std::filesystem::perms::none
                || (is_in_file_group(max_b_path) && (max_perms & std::filesystem::perms::group_read) != std::filesystem::perms::none)
            )){
                std::cout << "Cannot read max_brightness file.\n";
                return false;
            }

            auto perms = std::filesystem::status(b_path).permissions();
            // can the file be read?
            if (!((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none
                || (is_in_file_group(b_path) && (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none)
            )){
                std::cout << "Cannot read brightness file.\n";
                return false;
            }
            // and written?
            if (!((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none
                || (is_in_file_group(b_path) && (perms & std::filesystem::perms::group_write) != std::filesystem::perms::none)
            ))
            {
                std::cout << "Can read backlight, but cannot write. Ignoring.\n";
                return false;
            }

            return true;
        }

        void add_dev(std::filesystem::path path){
            if (!check_perms(path))
                return;

            // create a watch descriptor on the brightness file
            int wd = inotify_add_watch(fd, path.string().c_str(), IN_CLOSE_WRITE | IN_ATTRIB);
            if (wd == -1){
                std::cerr << "Light widget: failed to register inotify watch descriptor.\n";
                return;
            }

            wd_to_path_controls.insert({wd, {path, {}}});

            // create a control for each widget, and insert it in the vector we just created
            for (auto widget : widgets)
            {
                auto control = std::make_shared<WfLightSysfsControl>(widget, path);
                wd_to_path_controls[wd].second.push_back(std::shared_ptr<WfLightSysfsControl>(control));
;
                widget->add_control(control);
            }
        }

        void rem_dev(std::filesystem::path path){
            // device was removed, we can remove the entire entry (wd path, controls)
            for (auto it : wd_to_path_controls){
                if (it.second.first == path){
                    auto& controls = it.second.second;
                    for (auto control : controls){
                        controls.erase(find(controls.begin(), controls.end(), control));
                    }
                    wd_to_path_controls.erase(it.first);
                }
            }
        }

        void catch_up_widget(WayfireLight* widget){
            // for each managed device, create a control and add it to the widget and keep track of it
            for (auto& it : wd_to_path_controls){
                auto control = std::make_shared<WfLightSysfsControl>(widget, it.second.first.string());
                it.second.second.push_back(std::shared_ptr<WfLightSysfsControl>(control));
                widget->add_control((std::shared_ptr<WfLightControl>)control);
            }
        }

        // stores the data that goes with the inotify witch descriptor (the int)
        // the controls are all the controls which represent this device, to be updated
        std::map<
            int,
            std::pair<
                std::filesystem::path,
                std::vector<std::shared_ptr<WfLightSysfsControl>>
            >
        > wd_to_path_controls;

        // watch descriptors for files (so, a device) being added or removed
        int wd_additions, wd_removal;

        // managed widgets
        std::vector<WayfireLight*> widgets;

        // thread on which to run handle_inotify_event on loop
        std::thread inotify_thread;

    public:

        void add_widget(WayfireLight *widget){
            widgets.push_back(widget);
            catch_up_widget(widget);
        }

        void rem_widget(WayfireLight *widget){
            // search the controls that belong to the widget we’re removing and erase
            for (auto& it : wd_to_path_controls)
            {
                auto& controls = it.second.second;
                for (auto& control : controls)
                {
                    if (control->parent == widget)
                    {
                        controls.erase(find(controls.begin(), controls.end(), control));
                    }
                }
            }
            widgets.erase(find(widgets.begin(), widgets.end(), widget));
        }

        static SysfsSurveillor& get(){
            if (!instance)
            {
                instance = std::unique_ptr<SysfsSurveillor>(new SysfsSurveillor());
            }
            return *instance;
        }
};

void WayfireLight::setup_sysfs(){
    SysfsSurveillor::get().add_widget(this);
}
void WayfireLight::quit_sysfs(){
    SysfsSurveillor::get().rem_widget(this);
}
