#include "notification-info.hpp"

#include <gdkmm.h>

namespace
{
template<class... T>
void extractValues(const Glib::VariantBase & variant, T&... values)
{
    std::tie(values...) = Glib::VariantBase::cast_dynamic<Glib::Variant<std::tuple<T...>>>(variant).get();
}

template<class K>
K getHint(const std::map<std::string, Glib::VariantBase> & map, const std::string & key)
{
    if (map.count(key) != 0)
    {
        const auto & val = map.at(key);
        if (val.is_of_type(Glib::Variant<K>::variant_type()))
        {
            return Glib::VariantBase::cast_dynamic<Glib::Variant<K>>(val).get();
        }
    }

    return K();
}

Glib::RefPtr<Gdk::Pixbuf> pixbufFromVariant(const Glib::VariantBase & variant)
{
    gint32 width;
    gint32 height;
    gint32 rowstride;
    bool has_alpha;
    gint32 bits_per_sample;
    gint32 channels;
    std::vector<guint8> data;
    extractValues(variant, width, height, rowstride, has_alpha, bits_per_sample, channels, data);

    // for integer positive A, floor((A + 7) / 8) = ceil(A / 8)
    gulong pixel_size = (channels * bits_per_sample + 7) / 8;
    if (data.size() != ((gulong)height - 1) * (gulong)rowstride + (gulong)width * pixel_size)
    {
        throw std::invalid_argument(
            "Cannot create pixbuf from variant: expected data size doesn't equal actual one.");
    }

    return Gdk::Pixbuf::create_from_data(
        data.data(), Gdk::COLORSPACE_RGB, has_alpha, bits_per_sample, width, height,
        rowstride);
}
}  // namespace

Notification::Hints::Hints(const std::map<std::string, Glib::VariantBase> & map)
{
    action_icons = getHint<bool>(map, "actions-icons");
    category     = getHint<Glib::ustring>(map, "category");
    desktop_entry = getHint<Glib::ustring>(map, "desktop-entry");
    if (map.count("image-data") != 0)
    {
        image_data = pixbufFromVariant(map.at("image-data"));
    } else if (map.count("icon_data") != 0)
    {
        image_data = pixbufFromVariant(map.at("icon_data"));
    }

    image_path = getHint<Glib::ustring>(map, "image-path");
    if (image_path.empty())
    {
        image_path = getHint<Glib::ustring>(map, "image_path");
    }

    resident   = getHint<bool>(map, "resident");
    sound_file = getHint<Glib::ustring>(map, "sound-file");
    sound_name = getHint<Glib::ustring>(map, "sound-name");
    suppress_sound = getHint<bool>(map, "suppress-sound");
    transient = getHint<bool>(map, "transient");
    x = getHint<gint32>(map, "x");
    y = getHint<gint32>(map, "y");
    urgency = getHint<guint8>(map, "urgency");
}

Notification::Notification(const Glib::VariantContainerBase & parameters, const Glib::ustring & sender)
{
    std::map<std::string, Glib::VariantBase> hints_map;
    extractValues(parameters, app_name, id, app_icon, summary, body, actions, hints_map, expire_time);
    if (id == 0)
    {
        id = ++Notification::notifications_count;
    }

    hints = Hints(hints_map);

    additional_info.recv_time = std::time(nullptr);
    additional_info.sender    = sender;
}
