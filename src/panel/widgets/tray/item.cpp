#include "item.hpp"
#include <iostream>

static std::pair<Glib::ustring, Glib::ustring> name_and_obj_path(const Glib::ustring &service)
{
    const auto slash_ind = service.find('/');
    if (slash_ind != Glib::ustring::npos)
    {
        return {service.substr(0, slash_ind), service.substr(slash_ind)};
    }
    return {service, "/StatusNotifierItem"};
}

StatusNotifierItem::StatusNotifierItem(const Glib::ustring &service)
{
    const auto &[name, path] = name_and_obj_path(service);
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BUS_TYPE_SESSION, name, path, "org.kde.StatusNotifierItem",
                                     [this](const Glib::RefPtr<Gio::AsyncResult> &result) {
                                         item_proxy = Gio::DBus::Proxy::create_for_bus_finish(result);
                                         for (const auto &property_name : item_proxy->get_cached_property_names())
                                         {
                                             Glib::VariantBase property_variant;
                                             item_proxy->get_cached_property(property_variant, property_name);
                                             item_properties.emplace(property_name, property_variant);
                                         }
                                         init_widget();
                                         /*
                                         item_proxy->call(
                                             "org.freedesktop.DBus.Properies.GetAll",
                                             [this](const Glib::RefPtr<Gio::AsyncResult> &result) {
                                                 const auto all_properties = item_proxy->call_finish(result);
                                                 Glib::Variant<decltype(item_properties)> variant;
                                                 all_properties.get_child(variant);
                                                 item_properties = variant.get();
                                                 init_widget();
                                             },
                                             Glib::Variant<std::tuple<Glib::ustring>>::create({"org.kde.StatusNotifierItem"}));
                                             */
                                     });
}

void StatusNotifierItem::init_widget()
{
    update_icon();
}

Glib::RefPtr<Gdk::Pixbuf> extract_pixbuf(const Glib::VariantBase &variant)
{
    using VariantPixbuf = Glib::Variant<std::vector<std::tuple<gint32, gint32, std::vector<guint8>>>>;
    if (!variant.is_of_type(VariantPixbuf::variant_type()))
    {
        return {};
    }
    auto extracted_data = Glib::VariantBase::cast_dynamic<VariantPixbuf>(variant).get();
    if (extracted_data.empty())
    {
        return {};
    }
    auto chosen_image = std::max_element(extracted_data.begin(), extracted_data.end());
    auto &[width, height, data] = *chosen_image;
    std::cout << width << 'x' << height << '\t' << data.size() << " bytes" << std::endl;
    /* argb to rgba */
    for (size_t i = 0; i + 3 < data.size(); i += 4)
    {
        const auto alpha = data[i];
        data[i] = data[i + 1];
        data[i + 1] = data[i + 2];
        data[i + 2] = data[i + 3];
        data[i + 3] = alpha;
    }
    auto *data_ptr = new auto(std::move(data));
    return Gdk::Pixbuf::create_from_data(data_ptr->data(), Gdk::Colorspace::COLORSPACE_RGB, true, 8, width, height,
                                         4 * width, [data_ptr](auto *) { delete data_ptr; });
}

void StatusNotifierItem::update_icon()
{
    const auto status = get_item_property<Glib::ustring>("Status");
    const Glib::ustring icon_type_name = status == "NeedsAttention" ? "AttentionIcon" : "Icon";
    const auto pixmap_data = extract_pixbuf(item_properties[icon_type_name + "Pixmap"]);
    if (pixmap_data)
    {
        set(pixmap_data);
    }
    else
    {
        set_from_icon_name(get_item_property<Glib::ustring>(icon_type_name + "Name"), Gtk::ICON_SIZE_LARGE_TOOLBAR);
    }
}
