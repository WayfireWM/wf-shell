#include <filesystem>
#include <fstream>
#include <iostream>
#include <grp.h>
#include <pwd.h>

#include "light.hpp"

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

bool is_in_file_group(const std::filesystem::path& file_path) {
    struct stat file_info;
    if (stat(file_path.c_str(), &file_info) != 0) {
        std::cerr << "Failed to stat " << file_path << ".\n";
        return false;
    }

    // let’s assume the file is not owned by the user and only bother with groups
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

void WayfireLight::setup_fs(){
    // look for integrated backlights
    const auto path = "/sys/class/backlight";
    if (!std::filesystem::exists(path)){
        std::cout << "No backlight directory found for integrated screens, skipping.\n";
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)){
        const std::filesystem::path b_path = entry.path().string() + "brightness";
        const std::filesystem::path max_b_path = entry.path().string() + "max_brightness";

        if (!std::filesystem::exists(b_path)){
            std::cout << "No brightness found for " << entry.path().string() << ", ignoring.\n";
            break;
        }
        if (!std::filesystem::exists(b_path)){
            std::cout << "No max_brightness found for " << entry.path().string() << ", ignoring.\n";
            break;
        }

        auto max_perms = std::filesystem::status(max_b_path).permissions();
        // can the file be read?
        if (!((max_perms & std::filesystem::perms::others_read) == std::filesystem::perms::none)
            || !((is_in_file_group(max_b_path) && !((max_perms & std::filesystem::perms::group_read) == std::filesystem::perms::none))))
            break;

        auto perms = std::filesystem::status(b_path).permissions();
        // can the file be read?
        if (!((perms & std::filesystem::perms::others_read) == std::filesystem::perms::none)
            || !((is_in_file_group(b_path) && !((perms & std::filesystem::perms::group_read) == std::filesystem::perms::none))))
            break;
        // and written?
        if (!((perms & std::filesystem::perms::others_write) == std::filesystem::perms::none)
            || !((is_in_file_group(b_path) && !((perms & std::filesystem::perms::group_write) == std::filesystem::perms::none))))
            std::cout << "Can read backlight, but cannot write. Control will only display brightness.\n";

        controls.emplace_back(std::make_unique<WfLightFsControl>(entry.path()));
    }
}

// the permissions have already been checked, so we just read/write

WfLightFsControl::WfLightFsControl(std::string path) : WfLightControl(){
    scale.set_target_value(get_brightness());

    label.set_text(get_name());

    icons = brightness_display_icons;
}

std::string WfLightFsControl::get_name(){
    std::string name;
    name = "Integrated display";
    return name;
}

int WfLightFsControl::get_max(){
    std::ifstream max_file(path + "max_brightness");
    if (!max_file.is_open()){
        std::cerr << "Failed to get max brightness for device at " << path << '\n';
        return 0;
    }
    int max;
    max_file >> max;
    max_file.close();
    return max;
}

void WfLightFsControl::set_brightness(double brightness){
    std::ofstream b_file(path + "/brightness");
    if (!b_file.is_open()){
        std::cerr << "Failed to open brightness for device at " << path << '\n';
        return;
    }
    b_file << brightness * get_max();
    if (b_file.fail()){
        std::cerr << "Failed to write brightness for device at " << path << '\n';
    }

}

double WfLightFsControl::get_brightness(){
    std::ifstream b_file(path + "max_brightness");
    if (!b_file.is_open()){
        std::cerr << "Failed to get brightness for device at " << path << '\n';
        return 0;
    }
    int brightness;
    b_file >> brightness;
    b_file.close();
    return brightness * get_max();
}
