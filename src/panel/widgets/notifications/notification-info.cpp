#include "notification-info.hpp"

#include <gdkmm.h>

template <class T>
static void iterTo(Glib::VariantIter &iter, T &to)
{
    Glib::Variant<T> var;
    iter.next_value(var);
    to = var.get();
}

template <class K>
static K getHint(const std::map<std::string, Glib::VariantBase> &map, const std::string &key)
{
    if (map.count(key) != 0)
    {
        const auto &val = map.at(key);
        if (val.is_of_type(Glib::Variant<K>::variant_type()))
            return Glib::VariantBase::cast_dynamic<Glib::Variant<K>>(map.at(key)).get();
    }
    return K();
}

Glib::RefPtr<Gdk::Pixbuf> pixbufFromVariant(const Glib::VariantBase &variant)
{
    if (!variant.is_of_type(Glib::VariantType("iiibiiay")))
        throw std::invalid_argument("Cannot create pixbuf from variant.");

    auto iter = Glib::VariantIter(variant);
    gint32 width;
    gint32 height;
    gint32 rowstride;
    bool has_alpha;
    gint32 bits_per_sample;
    gint32 channels;

    iterTo(iter, width);
    iterTo(iter, height);
    iterTo(iter, rowstride);
    iterTo(iter, has_alpha);
    iterTo(iter, bits_per_sample);
    iterTo(iter, channels);

    Glib::VariantBase data_var;
    iter.next_value(data_var);

    // for integer positive A, floor((A + 7) / 8) = ceil(A / 8)
    ulong pixel_size = (channels * bits_per_sample + 7) / 8;
    if (data_var.get_size() != ((ulong)height - 1) * (ulong)rowstride + (ulong)width * pixel_size)
        throw std::invalid_argument("Cannot create pixbuf from variant: expected data size doesn't equal actual one.");
    const auto *data = (guint8 *)(g_memdup2(data_var.get_data(), data_var.get_size()));
    return Gdk::Pixbuf::create_from_data(data, Gdk::COLORSPACE_RGB, has_alpha, bits_per_sample, width, height,
                                         rowstride);
}

Notification::Hints::Hints(const std::map<std::string, Glib::VariantBase> &map)
{
    action_icons = getHint<bool>(map, "actions-icons");
    category = getHint<std::string>(map, "category");
    desktop_entry = getHint<std::string>(map, "desktop-entry");
    if (map.count("image-data") != 0)
        image_data = pixbufFromVariant(map.at("image-data"));
    else if (map.count("icon_data") != 0)
        image_data = pixbufFromVariant(map.at("image_data"));
    image_path = getHint<std::string>(map, "image-path");
    if (image_path.empty())
        image_path = getHint<std::string>(map, "image_path");
    resident = getHint<bool>(map, "resident");
    sound_file = getHint<std::string>(map, "sound-file");
    sound_name = getHint<std::string>(map, "sound-name");
    suppress_sound = getHint<bool>(map, "suppress-sound");
    transient = getHint<bool>(map, "transient");
    x = getHint<gint32>(map, "x");
    y = getHint<gint32>(map, "y");
    urgency = getHint<guint8>(map, "urgency");
}

Notification::Notification(const Glib::VariantContainerBase &parameters, const Glib::ustring &sender)
{
    static const auto REQUIRED_TYPE = Glib::VariantType("(susssasa{sv}i)");
    if (!parameters.is_of_type(REQUIRED_TYPE))
        throw std::invalid_argument("NotificationInfo: parameters type must be (susssasa{sv}i)");

    Glib::VariantBase params_var;
    parameters.get_normal_form(params_var);
    auto iter = Glib::VariantIter(params_var);
    iterTo(iter, app_name);
    iterTo(iter, id);
    if (id == 0)
        id = ++Notification::notifications_count;
    iterTo(iter, app_icon);
    iterTo(iter, summary);
    iterTo(iter, body);
    iterTo(iter, actions);

    std::map<std::string, Glib::VariantBase> hints_map;
    iterTo(iter, hints_map);
    hints = Hints(hints_map);

    additional_info.recv_time = std::time(nullptr);
    additional_info.sender = sender;
}
