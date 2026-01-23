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

            std::string realpath = std::filesystem::canonical(path);
            // this resolves to something of the sort :
            // /sys/devices/pciXXXX:XX/XXXX:XX:XX.X/XXXX:XX:XX.X/drm/cardX-<connector-name>/<name>
            // what we are intersted in here is the connector name
            std::regex pattern(R"(/card\d+-([^/]+)/)");
            std::smatch match;

            if (std::regex_search(realpath, match, pattern)){
                connector_name = match[1].str();
            } else // we failed :(
            {
                connector_name = "";
            }

            scale.set_target_value(get_brightness());
            label.set_text(get_name());

            icons = brightness_display_icons;
        }

        std::string get_name(){
            return connector_name;
        }

        // the permissions have already been checked and are being monitored, so we just read/write

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
            if (!std::filesystem::exists(path)){
                std::cout << "No backlight directory found for integrated screens, skipping.\n";
                return;
            }

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
                            for (auto control : wd_to_path_controls[event->wd].second){
                                control->set_scale_target_value(control->get_brightness());
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

                    // metadata changed, so maybe permissions
                    if (event->mask & IN_ATTRIB){

                    }
                }

            }
        }

        void add_dev(std::filesystem::path path){
            const std::filesystem::path b_path = path.string() + "/brightness";
            const std::filesystem::path max_b_path = path.string() + "/max_brightness";

            if (!std::filesystem::exists(b_path)){
                std::cout << "No brightness found for " << path.string() << ", ignoring.\n";
                return;
            }
            if (!std::filesystem::exists(b_path)){
                std::cout << "No max_brightness found for " << path.string() << ", ignoring.\n";
                return;
            }

            auto max_perms = std::filesystem::status(max_b_path).permissions();
            // can the file be read?
            if (!((max_perms & std::filesystem::perms::others_read) != std::filesystem::perms::none
                || (is_in_file_group(max_b_path) && (max_perms & std::filesystem::perms::group_read) != std::filesystem::perms::none)
            )){
                std::cout << "Cannot read max_brightness file.\n";
                return;
            }

            auto perms = std::filesystem::status(b_path).permissions();
            // can the file be read?
            if (!((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none
                || (is_in_file_group(b_path) && (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none)
            )){
                std::cout << "Cannot read brightness file.\n";
                return;
            }
            // and written?
            if (!((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none
                || (is_in_file_group(b_path) && (perms & std::filesystem::perms::group_write) != std::filesystem::perms::none)
            ))
            {
                std::cout << "Can read backlight, but cannot write. Ignoring.\n";
                return;
            }

            int wd = inotify_add_watch(fd, path.string().c_str(), IN_CLOSE_WRITE);
            if (wd == -1){
                std::cerr << "Light widget: failed to register inotify watch descriptor.\n";
                return;
            }

            wd_to_path_controls.insert({wd, {path, {}}});

            for (auto widget : widgets)
            {
                auto control = std::make_shared<WfLightSysfsControl>(widget, path);
                wd_to_path_controls[wd].second.push_back(std::shared_ptr<WfLightSysfsControl>(control));
;
                widget->add_control(control);
            }
        }

        void rem_dev(std::filesystem::path path){
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
            for (auto& it : wd_to_path_controls){
                auto control = std::make_shared<WfLightSysfsControl>(widget, it.second.first.string());
                it.second.second.push_back(std::shared_ptr<WfLightSysfsControl>(control));
                widget->add_control((std::shared_ptr<WfLightControl>)control);
            }
        }

        std::map<
            int,
            std::pair<
                std::filesystem::path,
                std::vector<std::shared_ptr<WfLightSysfsControl>>
            >
        > wd_to_path_controls;
        int wd_additions, wd_removal;
        std::vector<WayfireLight*> widgets;
        std::thread inotify_thread;

    public:

        void add_widget(WayfireLight *widget){
            widgets.push_back(widget);
            catch_up_widget(widget);
        }

        void rem_widget(WayfireLight *widget){
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
