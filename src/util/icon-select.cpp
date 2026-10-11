#include <gdkmm/display.h>
#include <gtkmm/icontheme.h>

#include "icon-select.hpp"

std::string preferred_icon(std::string name, bool prefer_symbolic)
{
    if (name.empty() ||
        !prefer_symbolic ||
        ((name.size() >= 9) &&
         (name.compare(name.size() - 9, 9, "-symbolic") == 0)))
    {
        return name;
    }

    auto theme =
        Gtk::IconTheme::get_for_display(Gdk::Display::get_default());

    if (theme && theme->has_icon(name + "-symbolic"))
    {
        return name + "-symbolic";
    }

    return name;
}

std::string icon_from_range(std::map<double, std::vector<std::string>> icons, double value)
{
    for (auto pair : icons)
    {
        if (value <= pair.first)
        {
            auto theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
            if (!theme)
            {
                // let’s stick to standard icons in this case
                return *pair.second.end();
            }

            for (auto name : pair.second)
            {
                if (theme->has_icon(name))
                {
                    return name;
                }
            }
        }
    }

    return "";
}
