#ifndef NOTIFICATION_INFO_HPP
#define NOTIFICATION_INFO_HPP

#include <gtkmm/image.h>
#include <map>
#include <string>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

struct Notification
{
    using id_type = guint32;
    std::string app_name;
    id_type id;
    std::string app_icon;
    std::string summary;
    std::string body;
    std::vector<std::string> actions;
    gint32 expire_time;

    struct Hints
    {
        bool action_icons;
        std::string category;
        std::string desktop_entry;
        Glib::RefPtr<Gdk::Pixbuf> image_data;
        std::string image_path;
        bool resident;
        std::string sound_file;
        std::string sound_name;
        bool suppress_sound;
        bool transient;
        gint32 x, y;
        guint8 urgency;
        Hints() = default;

        private:
        explicit Hints(const std::map<std::string, Glib::VariantBase> &map);
        friend Notification;

    } hints;

    struct AdditionalInfo
    {
        /// when the notification was received
        std::time_t recv_time;
        Glib::ustring sender;
    } additional_info;

    explicit Notification(const Glib::VariantContainerBase &parameters, const Glib::ustring &sender);

    private:
    inline static guint notifications_count = 0;
};

#endif
