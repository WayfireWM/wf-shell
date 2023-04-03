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
    Glib::ustring app_name;
    id_type id;
    Glib::ustring app_icon;
    Glib::ustring summary;
    Glib::ustring body;
    std::vector<Glib::ustring> actions;
    gint32 expire_time;

    struct Hints
    {
        bool action_icons;
        Glib::ustring category;
        Glib::ustring desktop_entry;
        Glib::RefPtr<Gdk::Pixbuf> image_data;
        Glib::ustring image_path;
        bool resident;
        Glib::ustring sound_file;
        Glib::ustring sound_name;
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
