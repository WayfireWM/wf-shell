#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <pwd.h>
#include <grp.h>
extern "C"{
    #include <sys/inotify.h>
}
#include <errno.h>

#include "light.hpp"

class WfLightSysfsControl: public WfLightControl
{
    protected:
        std::string path;

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

        std::string get_name(){
            std::string name;
            name = "Integrated display";
            return name;
        }

    public:
        WfLightSysfsControl(WayfireLight *parent, std::string _path) : WfLightControl(parent){
            path = _path;

            scale.set_target_value(get_brightness());
            label.set_text(get_name());

            icons = brightness_display_icons;
        }

        // the permissions have already been checked and *most likely* won’t have changed, so we just read/write

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
            }
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
bool is_group_member(gid_t file_group_id) {
    gid_t current_group = getgid();
    gid_t supplementary_groups[NGROUPS_MAX];
    int n_groups = getgroups(NGROUPS_MAX, supplementary_groups);

    if (current_group == file_group_id)
        return true;

    for (int i = 0; i < n_groups; ++i) {
        if (supplementary_groups[i] == file_group_id)
            return true;
    }

    return false;
}

// let’s assume the file is not owned by the user and only bother with groups
bool is_in_file_group(const std::filesystem::path& file_path) {
    struct stat file_info;
    if (stat(file_path.c_str(), &file_info) != 0) {
        std::cerr << "Failed to stat " << file_path << ".\n";
        return false;
    }

    struct group *file_group = getgrgid(file_info.st_gid);

    if (!file_group) {
        std::cerr << "Failed to fetch owner/group info for " << file_path << ".\n";
        return false;
    }

    if (is_group_member(file_info.st_gid)) {
        return true;
    } else {
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

            // look for present integrated backlights
            const auto path = "/sys/class/backlight";
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
            // proper function and avoid performance loss for "some systems"
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

                    if (event->mask & IN_CLOSE_WRITE){
                        // look for the watch descriptor
                        for (auto & pair : path_wd_to_controls){
                            if (pair.first.second == event->wd){
                                // it changed, so refresh the value of the controls
                                for (auto control : pair.second){
                                    control->set_scale_target_value(control->get_brightness());
                                }
                            }
                        }
                    }

                    if (event->mask & IN_CREATE){

                    }
                    if (event->mask & IN_DELETE){

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
                std::cout << "Can read backlight, but cannot write. Control will only display brightness.\n";

            int wd = inotify_add_watch(fd, path.string().c_str(), IN_CLOSE_WRITE);
            if (wd == -1){
                std::cerr << "Light widget: failed to register inotify watch descriptor.\n";
                return;
            }

            std::pair path_wd = {path, wd};
            path_wd_to_controls.insert({path_wd, {}});

            for (auto widget : widgets)
            {
                auto control = new WfLightSysfsControl(widget, path);
                path_wd_to_controls[path_wd].push_back(control);
                widget->add_control(control);
            }
        }

        void catch_up_widget(WayfireLight* widget){
            for (auto it : path_wd_to_controls){
                auto control = new WfLightSysfsControl(widget, it.first.first.string());
                it.second.push_back(control);
                widget->add_control(control);
            }
        }

        // std::vector<std::filesystem::path> devices;
        std::map<std::pair<std::filesystem::path, int>, std::vector<WfLightSysfsControl*>> path_wd_to_controls;
        std::vector<WayfireLight*> widgets;
        std::thread inotify_thread;

    public:

        void add_widget(WayfireLight *widget){
            widgets.push_back(widget);
            catch_up_widget(widget);
        }
        void rem_widget(WayfireLight *widget){

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
