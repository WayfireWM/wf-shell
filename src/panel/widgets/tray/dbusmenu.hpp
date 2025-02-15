#ifndef TRAY_DBUSMENU_HPP
#define TRAY_DBUSMENU_HPP

#include <giomm.h>
#include <gtkmm.h>
#include <glibmm.h>
#include <gmodule.h>

#include <wf-option-wrap.hpp>
#include <libdbusmenu-glib/dbusmenu-glib.h>

#include <optional>

using type_signal_action_group = sigc::signal<void(void)>;

class DbusMenuModel : public Gio::Menu
{
    DbusmenuClient * client;
    std::string prefix;
    type_signal_action_group signal;

    std::string label_to_action_name(std::string, int counter);

  public:
    explicit DbusMenuModel();
    ~DbusMenuModel();
    void connect(const Glib::ustring & service, const Glib::ustring & menu_path, const Glib::ustring & prefix);
    void layout_updated(DbusmenuMenuitem * item);
    void reconstitute(DbusmenuMenuitem * item);
    int iterate_children(Gio::Menu * parent_menu, DbusmenuMenuitem * parent, int counter);
    Glib::RefPtr<Gio::SimpleActionGroup> get_action_group();

    Glib::RefPtr<Gio::SimpleActionGroup> actions;

    type_signal_action_group signal_action_group();
};

#endif
