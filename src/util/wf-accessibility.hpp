#include "gtk/gtk.h"
#include <glibmm/extraclassinit.h>

/*
 * Helper init.
 * Sets the CSS element name, replaces tab-navigation, and sets the accessibility role, since we're
 * simulating buttons
 */
class WfAccessibleBoxInit : public Glib::ExtraClassInit
{
  public:
    WfAccessibleBoxInit(const std::string& css_name, GtkAccessibleRole role) :
        Glib::ExtraClassInit(&WfAccessibleBoxInit::class_init_func,
            new ClassData{css_name, role})
    {}

    static gboolean box_button_focus(GtkWidget *widget, GtkDirectionType direction)
    {
        // a Tab goes into a menu
        for (GtkWidget *child = gtk_widget_get_first_child(widget);
             child != nullptr;
             child = gtk_widget_get_next_sibling(child))
        {
            if (GTK_IS_POPOVER(child) && gtk_widget_get_visible(child))
            {
                auto *parent_class = GTK_WIDGET_CLASS(g_type_class_peek_parent(GTK_WIDGET_GET_CLASS(widget)));
                if (parent_class && parent_class->focus)
                {
                    return parent_class->focus(widget, direction);
                }
            }
        }

        if (gtk_widget_is_focus(widget))
        {
            return FALSE;
        }

        // Tab goes across panel
        if (gtk_widget_get_focusable(widget) &&
            gtk_widget_get_sensitive(widget) &&
            gtk_widget_get_visible(widget))
        {
            return gtk_widget_grab_focus(widget);
        }

        return FALSE;
    }

  private:
    struct ClassData
    {
        std::string css_name;
        GtkAccessibleRole role;
    };

    static void class_init_func(void *klass, void *data)
    {
        auto *class_data   = static_cast<ClassData*>(data);
        auto *widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->focus = &WfAccessibleBoxInit::box_button_focus;

        if (!class_data->css_name.empty())
        {
            gtk_widget_class_set_css_name(widget_class, class_data->css_name.c_str());
        }

        gtk_widget_class_set_accessible_role(widget_class, class_data->role);
    }
};
