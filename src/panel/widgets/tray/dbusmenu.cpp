#include "dbusmenu.hpp"

#include <iostream>
#include <time.h>

static void menu_updated(DbusmenuMenuitem * item, gpointer user_data)
{
    DbusMenuModel *menu = (DbusMenuModel*)user_data;

    menu->layout_updated(item);
}

DbusMenuModel::DbusMenuModel()
{
    actions = Gio::SimpleActionGroup::create();
}

type_signal_action_group DbusMenuModel::signal_action_group()
{
    return signal;
}


void DbusMenuModel::connect(const Glib::ustring & dbus_name, const Glib::ustring & menu_path, const Glib::ustring & pref)
{
    prefix = pref;
    client = dbusmenu_client_new(dbus_name.c_str(), menu_path.c_str());
    auto gclient = G_OBJECT(client);
    g_signal_connect(
        gclient,
        DBUSMENU_CLIENT_SIGNAL_LAYOUT_UPDATED,
        G_CALLBACK(menu_updated),
        this
    );
    auto root = dbusmenu_client_get_root(client);
    reconstitute(root);
}

void DbusMenuModel::reconstitute(DbusmenuMenuitem * rootItem)
{
    remove_all();
    auto action_list = actions->list_actions();
    for(auto action_name : action_list){
        actions->remove_action(action_name);
    }
    iterate_children(this, rootItem,0 );

    signal.emit(); // Tell the SNI that the menu actions have changed. Seemed necessary, as if it was taking a copy not a ref
}

int DbusMenuModel::iterate_children(Gio::Menu * parent_menu, DbusmenuMenuitem * parent, int stupid_count)
{
    if (DBUSMENU_IS_MENUITEM(parent))
    {
        auto list = dbusmenu_menuitem_get_children(parent);
        auto current_section = Gio::Menu::create();
        for(; list != NULL ; list = g_list_next(list) )
        {
            auto child = DBUSMENU_MENUITEM(list->data);
            std::cout << "dbus menu..." << std::endl;
            auto label = dbusmenu_menuitem_property_get(child, DBUSMENU_MENUITEM_PROP_LABEL);
            if (label == nullptr)
            {
                auto item = Gio::MenuItem::create(current_section);
                item->set_section(current_section);
                current_section = Gio::Menu::create();
                parent_menu->append_item(item);
                continue;
            }
            std::string menutype = "";
            std::string icon_name = "";
            std::string toggle_type = "";
            if (dbusmenu_menuitem_property_exist(child, DBUSMENU_MENUITEM_PROP_TYPE))
            {
                menutype = std::string(dbusmenu_menuitem_property_get(child, DBUSMENU_MENUITEM_PROP_TYPE));
            }
            bool enabled = dbusmenu_menuitem_property_get_bool(child, DBUSMENU_MENUITEM_PROP_ENABLED);
            if (dbusmenu_menuitem_property_exist(child, DBUSMENU_MENUITEM_PROP_ICON_NAME))
            {
                icon_name = dbusmenu_menuitem_property_get(child, DBUSMENU_MENUITEM_PROP_ICON_NAME);
            }
            if (dbusmenu_menuitem_property_exist(child, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE))
            {
                toggle_type = dbusmenu_menuitem_property_get(child, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE);
            }
            int toggle_state = dbusmenu_menuitem_property_get_int(child, DBUSMENU_MENUITEM_PROP_TOGGLE_STATE);
            bool has_children = dbusmenu_menuitem_get_children(child) != NULL;
            std::cout << ">> " << label << " type: " << menutype <<  " enabled: " << (enabled ? "true" : "false")  <<" icon: " << icon_name <<  " toggle_type: " << toggle_type << " has_children: " << has_children <<  std::endl;

            if(has_children)
            {
                auto submenu = Gio::Menu::create();
                stupid_count = iterate_children(submenu.get(), child, stupid_count);
                current_section->append_submenu(label, submenu);
            } else
            {
                auto item = Gio::MenuItem::create(label, "a");
                if(icon_name!="")
                {
                    item->set_icon(Gio::Icon::create(icon_name));
                }
                if (enabled)
                {
                    auto action_name = label_to_action_name(label, stupid_count);
                    std::cout << "Action generation " << prefix << "." << action_name << std::endl;
                    std::stringstream ss;
                    ss << prefix << "." << action_name;
                    std::string action_string = ss.str();
                    item->set_action(action_string);
                    actions->add_action(action_name, [=] ()
                      {
                        GVariant * data = g_variant_new_int32(0);
                        dbusmenu_menuitem_handle_event(child, "clicked", data, 0);
                      });
                }
                current_section->append_item(item);

            }
            stupid_count++;
        }
        auto item = Gio::MenuItem::create(current_section);
        item->set_section(current_section);
        current_section = Gio::Menu::create();
        parent_menu->append_item(item);
    }else
    {
        std::cout << "DBUS iterating non-menu-item " << std::endl;
    }
    return stupid_count;
}


void DbusMenuModel::layout_updated(DbusmenuMenuitem * item)
{
    auto root = dbusmenu_client_get_root(client);
    reconstitute(root);
}

Glib::RefPtr<Gio::SimpleActionGroup> DbusMenuModel::get_action_group()
{
    return actions;
}


/*
  Strip none alphabetical characters and add a unique numeric ID.
  Keeping alphacharacters can help with debugging but isn't strictly necessary.

  Once we're sure nothing really stupid is happening here, we could have a letter followed by unique number.
 */
std::string DbusMenuModel::label_to_action_name(std::string label, int numeric)
{
    std::string ret = label;
    transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
    for (int i = 0; i < ret.size(); i++) {
        if (ret[i] < 'a'
            || ret[i] > 'z') {
            ret.erase(i, 1);
            i--;
        }
    }
    std::stringstream stream;
    stream << ret << numeric;
    return stream.str();
}