#include <gtkmm.h>
#include <iostream>
#include <glibmm.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtkmm/enums.h"
#include "gtkmm/gestureclick.h"
#include "gtkmm/label.h"
#include "gtkmm/separator.h"
#include "gtkmm/togglebutton.h"
#include "wp/proxy-interfaces.h"
#include "wp/proxy.h"

#include <pipewire/keys.h>

#include "wireplumber.hpp"


enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

enum class FaceChoice
{
    LAST_CHANGE,
    DEFAULT_SINK,
    DEFAULT_SOURCE,
};

enum class ClickAction
{
    SHOW_MIXER,
    SHOW_FACE,
    MUTE_FACE,
};

static const gchar *DEFAULT_NODE_MEDIA_CLASSES[] = {
    "Audio/Sink",
    "Audio/Source",
};

static VolumeLevel volume_icon_for(double volume)
{
    double max = 1.0;
    auto third = max / 3;
    if (volume == 0)
    {
        return VOLUME_LEVEL_MUTE;
    } else if ((volume > 0) && (volume <= third))
    {
        return VOLUME_LEVEL_LOW;
    } else if ((volume > third) && (volume <= (third * 2)))
    {
        return VOLUME_LEVEL_MED;
    } else if ((volume > (third * 2)) && (volume <= max))
    {
        return VOLUME_LEVEL_HIGH;
    }

    return VOLUME_LEVEL_OOR;
}

WfWpControl::WfWpControl(WpPipewireObject *obj, WayfireWireplumber *parent_widget)
{
    object = obj;
    parent = parent_widget;

    guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));

    scale.set_range(0.0, 1.0);
    scale.set_target_value(0.5);
    scale.set_size_request(300, 0);

    const gchar *name;

    // try to find a name to display
    name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NICK);
    if (!name)
    {
        name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NAME);
    }

    if (!name)
    {
        name = wp_pipewire_object_get_property(object, PW_KEY_NODE_DESCRIPTION);
    }

    if (!name)
    {
        name = "Unnamed";
    }

    name = g_strdup_printf("%s %d", name, id);

    label.set_text(Glib::ustring(name));

    button.set_child(volume_icon);

    attach(label, 0, 0, 2, 1);
    attach(button, 1, 1, 1, 1);
    attach(scale, 0, 1, 1, 1);

    mute_conn = button.signal_toggled().connect(
        [this, id] ()
    {
        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "mute", g_variant_new_boolean(button.get_active()));
        GVariant *v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
    });

    scale.set_user_changed_callback(
        [this, id] ()
    {
        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "volume",
            g_variant_new_double(std::pow(scale.get_target_value(), 3))); // see on_mixer_plugin_loaded
        GVariant *v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
    });

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        dy = parent->invert_scroll ? dy : dy * -1; // for the same scrolling as volume widget, which we will
                                                   // agree it is more intuitive for more people
        double change = 0;
        const double SCROLL_MULT = 0.2; // corrects the scrolling to have the default scroll sensitivity as 1
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * parent->scroll_sensitivity * SCROLL_MULT) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * parent->scroll_sensitivity * SCROLL_MULT) / 100;
        }

        guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));
        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "volume",
            g_variant_new_double(std::pow(get_scale_target_value() + change, 3))); // see
                                                                                   // on_mixer_plugin_loaded
        GVariant *v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    add_controller(scroll_gesture);

    update_gestures();

    // initialise the values
    GVariant *v = NULL;
    g_signal_emit_by_name(WpCommon::mixer_api, "get-volume", id, &v);
    if (!v)
    {
        return;
    }

    gboolean mute  = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);
    set_btn_status_no_callbk(mute);
    set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded

    update_icon();
}

Glib::ustring WfWpControl::get_name()
{
    return label.get_text();
}

void WfWpControl::set_btn_status_no_callbk(bool state)
{
    mute_conn.block(true);
    button.set_active(state);
    mute_conn.block(false);
}

void WfWpControl::set_scale_target_value(double volume)
{
    scale.set_target_value(volume);
}

void WfWpControl::update_icon()
{
    VolumeLevel current = volume_icon_for(get_scale_target_value());

    if (is_muted())
    {
        volume_icon.set_from_icon_name("audio-volume-muted");
        return;
    }

    std::map<VolumeLevel, std::string> icon_name_from_state = {
        {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
        {VOLUME_LEVEL_LOW, "audio-volume-low"},
        {VOLUME_LEVEL_MED, "audio-volume-medium"},
        {VOLUME_LEVEL_HIGH, "audio-volume-high"},
        {VOLUME_LEVEL_OOR, "audio-volume-muted"},
    };

    volume_icon.set_from_icon_name(icon_name_from_state.at(current));
}

double WfWpControl::get_scale_target_value()
{
    return scale.get_target_value();
}

bool WfWpControl::is_muted()
{
    return button.get_active();
}

void WfWpControl::update_gestures()
{
    WfOption<std::string> str_wp_right_click_action{"panel/wp_right_click_action"};
    WfOption<std::string> str_wp_middle_click_action{"panel/wp_middle_click_action"};

    auto mute_action =
        [&] (int count, double x, double y)
    {
        button.set_active(!button.get_active());
    };

    static auto right_click_mute = Gtk::GestureClick::create();
    right_click_mute->set_button(3);
    right_click_mute->signal_pressed().connect(mute_action);

    static auto middle_click_mute = Gtk::GestureClick::create();
    middle_click_mute->set_button(2);
    middle_click_mute->signal_pressed().connect([=] (int count, double x, double y)
    {
        button.set_active(!button.get_active());
    });

    remove_controller(right_click_mute);
    remove_controller(middle_click_mute);

    if ((std::string)str_wp_right_click_action == (std::string)"mute_face")
    {
        add_controller(right_click_mute);
    }

    if ((std::string)str_wp_middle_click_action == (std::string)"mute_face")
    {
        add_controller(middle_click_mute);
    }
}

void WfWpControl::handle_config_reload()
{
    update_gestures();
}

WfWpControl*WfWpControl::copy()
{
    WfWpControl *copy = new WfWpControl(object, parent);
    return copy;
}

WfWpControlDevice::WfWpControlDevice(WpPipewireObject *obj,
    WayfireWireplumber *parent_widget) : WfWpControl(obj, parent_widget)
{
    // for devices (sinks and sources), we determine if they are the default

    attach(default_btn, 1, 0, 1, 1);

    WpProxy *proxy = WP_PROXY(object);

    is_def_icon.set_from_icon_name("emblem-default");
    default_btn.set_child(is_def_icon);

    // we are not using ToggleButton groups because on_default_nodes_changed will be called anyway to set the status of all devices
    def_conn = default_btn.signal_clicked().connect(
        [this, proxy] ()
    {
        // keep the button down when it is selected to prevent inconsistency of visuals with actual status
        if (default_btn.get_active() == false)
        {
            set_def_status_no_callbk(true);
            return;
        }
        const gchar *media_class = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(proxy),
            PW_KEY_MEDIA_CLASS);
        for (guint i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++)
        {
            if (g_strcmp0(media_class, DEFAULT_NODE_MEDIA_CLASSES[i]))
            {
                continue;
            }

            gboolean res = FALSE;
            const gchar *name = wp_pipewire_object_get_property(
                WP_PIPEWIRE_OBJECT(proxy),
                PW_KEY_NODE_NAME);
            if (!name)
            {
                continue;
            }

            g_signal_emit_by_name(WpCommon::default_nodes_api, "set-default-configured-node-name",
                DEFAULT_NODE_MEDIA_CLASSES[i], name, &res);
            if (!res)
            {
                continue;
            }

            wp_core_sync(WpCommon::core, NULL, NULL, NULL);
        }
    });
}

void WfWpControlDevice::set_def_status_no_callbk(bool state)
{
    def_conn.block(true);
    default_btn.set_active(state);
    def_conn.block(false);
}

WfWpControlDevice*WfWpControlDevice::copy()
{
    WfWpControlDevice *copy = new WfWpControlDevice(object, parent);
    return copy;
}

bool WayfireWireplumber::on_popover_timeout(int timer)
{
    popover_timeout.disconnect();
    popover->popdown();
    return false;
}

void WayfireWireplumber::check_set_popover_timeout()
{
    popover_timeout.disconnect();

    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireWireplumber::on_popover_timeout), 0), timeout * 1000);
}

void WayfireWireplumber::reload_config()
{
    WfOption<std::string> str_face_choice{"panel/wp_face_choice"};
    WfOption<std::string> str_wp_left_click_action{"panel/wp_left_click_action"};
    WfOption<std::string> str_wp_right_click_action{"panel/wp_right_click_action"};
    WfOption<std::string> str_wp_middle_click_action{"panel/wp_middle_click_action"};

    if ((std::string)str_face_choice == (std::string)"last_change")
    {
        face_choice = FaceChoice::LAST_CHANGE;
    } else if ((std::string)str_face_choice == (std::string)"default_sink")
    {
        face_choice = FaceChoice::DEFAULT_SINK;
        face = nullptr;
        WpCommon::on_default_nodes_changed(WpCommon::default_nodes_api, NULL);
    } else if ((std::string)str_face_choice == (std::string)"default_source")
    {
        face_choice = FaceChoice::DEFAULT_SOURCE;
        face = nullptr;
        WpCommon::on_default_nodes_changed(WpCommon::default_nodes_api, NULL);
    } else // default if no match
    {
        face_choice = FaceChoice::LAST_CHANGE;
    }

    static auto left_click_gesture = Gtk::GestureClick::create();
    left_click_gesture->set_button(1);
    static auto right_click_gesture = Gtk::GestureClick::create();
    right_click_gesture->set_button(3);
    static auto middle_click_gesture = Gtk::GestureClick::create();
    middle_click_gesture->set_button(2);

    button->remove_controller(left_click_gesture);
    button->remove_controller(right_click_gesture);
    button->remove_controller(middle_click_gesture);

    auto show_mixer_action = [&] (int count, double x, double y)
    {
        if ((popover->get_child() == (Gtk::Widget*)&master_box) && popover->is_visible())
        {
            popover->popdown();
            return;
        }

        if (!popover->is_visible())
        {
            popover->popup();
        }

        if (popover->get_child() != (Gtk::Widget*)&master_box)
        {
            popover->set_child(master_box);
            popover_timeout.disconnect();
        }
    };

    auto show_face_action = [&] (int count, double x, double y)
    {
        if (!face)
        {
            return; // no face means we have nothing to show
        }

        if ((popover->get_child() == face) && popover->is_visible())
        {
            popover->popdown();
            return;
        }

        if (!popover->is_visible())
        {
            popover->popup();
        }

        if (popover->get_child() != face)
        {
            popover->set_child(*face);
            popover_timeout.disconnect();
        }
    };

    // the left click case is a bit special, since it’s supposed to show the popover.
    // (this is also why the mute action is not available for the left click)
    // this is not the prettiest, but the alternative is way worse
    if ((std::string)str_wp_left_click_action == (std::string)"show_mixer")
    {
        left_click_gesture->signal_pressed().connect(
            [&] (int c, double x, double y)
        {
            if (popover->get_child() != (Gtk::Widget*)&master_box)
            {
                popover->set_child(master_box);
                // popdown so that when the click is processed, the popover is down, and thus pops up
                // not the prettiest result, as it visibly closes instead of just replacing, but i’m not sure
                // how to make it better
                popover->popdown();
            }
        });
        button->add_controller(left_click_gesture);
    }

    if ((std::string)str_wp_left_click_action == (std::string)"show_face")
    {
        left_click_gesture->signal_pressed().connect(
            [&] (int c, double x, double y)
        {
            if (!face)
            {
                return;
            }

            if (popover->get_child() != face)
            {
                popover->set_child(*face);
                // popdown so that when the click is processed, the popover is down, and thus pops up
                // not the prettiest result, as it visibly closes instead of just replacing, but i’m not sure
                // how to make it better
                popover->popdown();
            }
        });
        button->add_controller(left_click_gesture);
    }

    if ((std::string)str_wp_right_click_action == (std::string)"show_mixer")
    {
        right_click_gesture->signal_pressed().connect(show_mixer_action);
        button->add_controller(right_click_gesture);
    }

    if ((std::string)str_wp_right_click_action == (std::string)"show_face")
    {
        right_click_gesture->signal_pressed().connect(show_face_action);
        button->add_controller(right_click_gesture);
    }

    if ((std::string)str_wp_right_click_action == (std::string)"mute_face")
    {
        right_click_gesture->signal_pressed().connect(
            [&] (int count, double x, double y)
        {
            if (!face)
            {
                return; // no face means we have nothing to change by clicking
            }

            face->button.set_active(!face->button.get_active());
        });
        button->add_controller(right_click_gesture);
    }

    if ((std::string)str_wp_middle_click_action == (std::string)"show_mixer")
    {
        middle_click_gesture->signal_pressed().connect(show_mixer_action);
        button->add_controller(middle_click_gesture);
    }

    if ((std::string)str_wp_middle_click_action == (std::string)"show_face")
    {
        middle_click_gesture->signal_pressed().connect(show_face_action);
        button->add_controller(middle_click_gesture);
    }

    if ((std::string)str_wp_middle_click_action == (std::string)"mute_face")
    {
        middle_click_gesture->signal_pressed().connect(
            [&] (int count, double x, double y)
        {
            if (!face)
            {
                return; // no face means we have nothing to change by clicking
            }

            face->button.set_active(!face->button.get_active());
        });
        button->add_controller(middle_click_gesture);
    }
}

void WayfireWireplumber::handle_config_reload()
{
    reload_config();
    for (auto [name, control] : objects_to_controls)
    {
        control->handle_config_reload();
    }
}

void WayfireWireplumber::init(Gtk::Box *container)
{
    // sets up the « widget part »

    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("volume");
    button->get_style_context()->add_class("flat");
    button->set_child(main_image);
    button->show();
    button->get_popover()->set_child(master_box);
    popover = button->get_popover();
    popover->set_autohide(false);

    // scroll to change volume
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        if (!face)
        {
            return false; // no face means we have nothing to change by scrolling
        }

        dy = invert_scroll ? dy : dy * -1; // for the same scrolling as volume widget, which we will agree it
                                           // is more intuitive for more people
        double change = 0;
        const double SCROLL_MULT = 0.2; // corrects the scrolling to have the default scroll sensitivity as 1
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * scroll_sensitivity * SCROLL_MULT) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * scroll_sensitivity * SCROLL_MULT) / 100;
        }

        guint32 id = wp_proxy_get_bound_id(WP_PROXY(face->object));
        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "volume",
            g_variant_new_double(std::pow(face->get_scale_target_value() + change, 3))); // see
                                                                                         // on_mixer_plugin_loaded
        GVariant *v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    // config options
    reload_config();

    // boxes hierarchy and labeling
    auto r1 = Gtk::Orientation::HORIZONTAL;
    auto r2 = Gtk::Orientation::VERTICAL;

    master_box.set_orientation(r1);
    // TODO : only show the boxes which have stuff in them
    master_box.append(sinks_box);
    master_box.append(*new Gtk::Separator(r1));
    master_box.append(sources_box);
    master_box.append(*new Gtk::Separator(r1));
    master_box.append(streams_box);
    sinks_box.set_orientation(r2);
    sinks_box.append(*new Gtk::Label("Output devices"));
    sinks_box.append(*new Gtk::Separator(r2));
    sources_box.set_orientation(r2);
    sources_box.append(*new Gtk::Label("Input devices"));
    sources_box.append(*new Gtk::Separator(r2));
    streams_box.set_orientation(r2);
    streams_box.append(*new Gtk::Label("Audio streams"));
    streams_box.append(*new Gtk::Separator(r2));

    /* Setup popover */
    popover->set_child(master_box);
    popover->get_style_context()->add_class("volume-popover");
    popover->add_controller(scroll_gesture);

    /* Setup layout */
    container->append(*button);
    button->set_child(main_image);

    /*
     *   if the core is already set, we are another widget, wether on another monitor or on the same wf-panel.
     * We re-use the core, manager and all other objects
     */

    WpCommon::widgets.push_back(this);

    if (WpCommon::core != nullptr)
    {
        WpCommon::catch_up_to_current_state(this);
        return;
    }

    WpCommon::init_wp();
}

void WayfireWireplumber::update_icon()
{
    VolumeLevel current = volume_icon_for(face->get_scale_target_value());

    if (!face || face->is_muted())
    {
        main_image.set_from_icon_name("audio-volume-muted");
        return;
    }

    std::map<VolumeLevel, std::string> icon_name_from_state = {
        {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
        {VOLUME_LEVEL_LOW, "audio-volume-low"},
        {VOLUME_LEVEL_MED, "audio-volume-medium"},
        {VOLUME_LEVEL_HIGH, "audio-volume-high"},
        {VOLUME_LEVEL_OOR, "audio-volume-muted"},
    };

    main_image.set_from_icon_name(icon_name_from_state.at(current));
}

void WpCommon::init_wp()
{
    // creates the core, object interests and connects signals

    std::cout << "Initialising wireplumber\n";
    wp_init(WP_INIT_PIPEWIRE);
    core = wp_core_new(NULL, NULL, NULL);
    object_manager = wp_object_manager_new();

    // register interests in  sinks, sources and streams
    WpObjectInterest *sink_interest = wp_object_interest_new_type(WP_TYPE_NODE);
    wp_object_interest_add_constraint(
        sink_interest,
        WP_CONSTRAINT_TYPE_PW_PROPERTY,
        "media.class",
        WP_CONSTRAINT_VERB_EQUALS,
        g_variant_new_string("Audio/Sink"));

    wp_object_manager_add_interest_full(object_manager, sink_interest);

    WpObjectInterest *source_interest = wp_object_interest_new_type(WP_TYPE_NODE);
    wp_object_interest_add_constraint(
        source_interest,
        WP_CONSTRAINT_TYPE_PW_PROPERTY,
        "media.class",
        WP_CONSTRAINT_VERB_EQUALS,
        g_variant_new_string("Audio/Source"));

    wp_object_manager_add_interest_full(object_manager, source_interest);

    WpObjectInterest *stream_interest = wp_object_interest_new_type(WP_TYPE_NODE);
    wp_object_interest_add_constraint(
        stream_interest,
        WP_CONSTRAINT_TYPE_PW_PROPERTY,
        "media.class",
        WP_CONSTRAINT_VERB_EQUALS,
        g_variant_new_string("Stream/Output/Audio"));
    wp_object_manager_add_interest_full(object_manager, stream_interest);

    wp_core_load_component(
        core,
        "libwireplumber-module-mixer-api",
        "module",
        NULL,
        NULL,
        NULL,
        (GAsyncReadyCallback)on_mixer_plugin_loaded,
        NULL);

    wp_core_connect(core);
}

void WpCommon::catch_up_to_current_state(WayfireWireplumber *widget)
{
    // catch up to objects already registered by the manager
    WpIterator *reg_objs = wp_object_manager_new_iterator(object_manager);
    GValue item = G_VALUE_INIT;
    while (wp_iterator_next(reg_objs, &item))
    {
        on_object_added(object_manager, (gpointer)g_value_get_object(&item), &widget);
        g_value_unset(&item);
    }
}

void WpCommon::on_mixer_plugin_loaded(WpCore *core, GAsyncResult *res, void *data)
{
    mixer_api = wp_plugin_find(core, "mixer-api");
    // as far as i understand, this should set the mixer api to use linear scale
    // however, it doesn’t and i can’t find what is wrong.
    // for now, we calculate manually
    // g_object_set(mixer_api, "scale", 0, NULL); // set to linear

    wp_core_load_component(
        core,
        "libwireplumber-module-default-nodes-api",
        "module",
        NULL,
        NULL,
        NULL,
        (GAsyncReadyCallback)on_default_nodes_plugin_loaded,
        NULL);
}

void WpCommon::on_default_nodes_plugin_loaded(WpCore *core, GAsyncResult *res, void *data)
{
    default_nodes_api = wp_plugin_find(core, "default-nodes-api");

    on_all_plugins_loaded();
}

void WpCommon::on_all_plugins_loaded()
{
    g_signal_connect(
        mixer_api,
        "changed",
        G_CALLBACK(WpCommon::on_mixer_changed),
        NULL);

    g_signal_connect(
        default_nodes_api,
        "changed",
        G_CALLBACK(WpCommon::on_default_nodes_changed),
        NULL);

    g_signal_connect(
        object_manager,
        "object_added",
        G_CALLBACK(on_object_added),
        NULL);

    g_signal_connect(
        object_manager,
        "object-removed",
        G_CALLBACK(on_object_removed),
        NULL);

    wp_core_install_object_manager(core, object_manager);
}

void WpCommon::on_object_added(WpObjectManager *manager, gpointer object, gpointer data)
{
    // adds a new widget to the appropriate section

    WpPipewireObject *obj = (WpPipewireObject*)object;

    const gchar *type = wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS);

    for (auto widget : widgets)
    {
        WfWpControl *control;
        Gtk::Box *which_box;
        if (g_strcmp0(type, "Audio/Sink") == 0)
        {
            which_box = &(widget->sinks_box);
            control   = new WfWpControlDevice(obj, widget);
        } else if (g_strcmp0(type, "Audio/Source") == 0)
        {
            which_box = &(widget->sources_box);
            control   = new WfWpControlDevice(obj, widget);
        } else if (g_strcmp0(type, "Stream/Output/Audio") == 0)
        {
            which_box = &(widget->streams_box);
            control   = new WfWpControl(obj, (WayfireWireplumber*)widget);
        } else
        {
            std::cout << "Could not match pipewire object media class, ignoring\n";
            return;
        }

        widget->objects_to_controls.insert({obj, control});
        which_box->append((Gtk::Widget&)*control);
    }
}

void WpCommon::on_mixer_changed(gpointer mixer_api, guint id, gpointer data)
{
    // update the visual of the appropriate WfWpControl according to external changes

    GVariant *v = NULL;
    // ask the mixer-api for the up-to-date data
    g_signal_emit_by_name(WpCommon::mixer_api, "get-volume", id, &v);
    if (!v)
    {
        return;
    }

    gboolean mute  = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);

    for (auto widget : widgets)
    {
        WfWpControl *control;

        for (auto & it : widget->objects_to_controls)
        {
            WpPipewireObject *obj = it.first;
            control = it.second;
            if (wp_proxy_get_bound_id(WP_PROXY(obj)) == id)
            {
                break;
            }
        }

        control->set_btn_status_no_callbk(mute);
        control->set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded
        control->update_icon();

        if ((widget->face_choice == FaceChoice::DEFAULT_SINK)
            ||
            (widget->face_choice == FaceChoice::DEFAULT_SOURCE))
        {
            if (
                control->object
                !=
                ((WfWpControl*)(widget->popover->get_child()))->object)
            {
                widget->face->set_btn_status_no_callbk(mute);
                widget->face->set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded
                widget->update_icon();
            }

            if (!widget->popup_on_change)
            {
                return;
            }

            // if it was hidden, show it
            if (!widget->popover->is_visible())
            {
                widget->button->set_active(true);
            }

            return;
            // in all cases, (re-)schedule hiding
            widget->check_set_popover_timeout();
        }

        // if we have the *full* mixer in the popover
        if ((Gtk::Widget*)&(widget->master_box)
            ==
            (Gtk::Widget*)widget->popover->get_child())
        {
            // if shown, stop there
            if (widget->popover->is_visible())
            {
                return;
            }

            // if hidden, replace by the popover
            widget->face = control->copy();
            widget->popover->set_child(*widget->face);
        }

        // ensure the control in the popover is the one that was updated
        if (
            control->object
            !=
            ((WfWpControl*)(widget->popover->get_child()))->object)
        {
            widget->face = control->copy();
            widget->popover->set_child(*widget->face);
        } else
        {
            // update the face’s values ; else because the WfWpControl constructor already syncs the values
            widget->face->set_btn_status_no_callbk(mute);
            widget->face->set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded
            widget->update_icon();
        }

        // if it was hidden, show it
        if (widget->popup_on_change && !widget->popover->is_visible())
        {
            widget->button->set_active(true);
        }

        // in all cases, (re-)schedule hiding
        widget->check_set_popover_timeout();
    }
}

void WpCommon::on_default_nodes_changed(gpointer default_nodes_api, gpointer data)
{
    std::vector<guint32> defaults;
    guint32 id = SPA_ID_INVALID;

    for (guint32 i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++)
    {
        g_signal_emit_by_name(default_nodes_api, "get-default-node", DEFAULT_NODE_MEDIA_CLASSES[i], &id);
        if (id != SPA_ID_INVALID)
        {
            defaults.push_back(id);
        }
    }

    for (auto widget : widgets)
    {
        for (const auto & entry : widget->objects_to_controls)
        {
            auto obj  = WP_PIPEWIRE_OBJECT(entry.first);
            auto ctrl = (WfWpControlDevice*)entry.second;

            guint32 bound_id = wp_proxy_get_bound_id(WP_PROXY(obj));
            if (bound_id == SPA_ID_INVALID)
            {
                continue;
            }

            bool is_default = std::any_of(defaults.begin(), defaults.end(), [bound_id] (guint32 def_id)
            {
                return def_id == bound_id;
            });
            if (is_default)
            {
                ctrl->set_def_status_no_callbk(true);
                continue;
            }

            // if the control is not for a sink or source (non WfWpControlDevice), don’t try to set status
            const gchar *type = wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS);

            if ((g_strcmp0(type, "Audio/Sink") == 0) || (g_strcmp0(type, "Audio/Source") == 0))
            {
                ctrl->set_def_status_no_callbk(false);
            }

            if ((widget->face_choice == FaceChoice::DEFAULT_SINK)
                &&
                (g_strcmp0(type, "Audio/Sink") == 0))
            {
                widget->face = ctrl;
            }

            if ((widget->face_choice == FaceChoice::DEFAULT_SOURCE)
                &&
                (g_strcmp0(type, "Audio/Source") == 0))
            {
                widget->face = ctrl;
            }
        }
    }
}

void WpCommon::on_object_removed(WpObjectManager *manager, gpointer object, gpointer data)
{
    for (auto widget : widgets)
    {
        auto it = widget->objects_to_controls.find((WpPipewireObject*)object);
        if (it == widget->objects_to_controls.end())
        {
            // shouldn’t happen, but checking to avoid weird situations
            return;
        }

        WfWpControl *control = it->second;
        Gtk::Box *box = (Gtk::Box*)control->get_parent();
        box->remove(*control);

        delete control;
        widget->objects_to_controls.erase((WpPipewireObject*)object);
    }
}

WayfireWireplumber::~WayfireWireplumber()
{
    WpCommon::widgets.erase(std::find(WpCommon::widgets.begin(), WpCommon::widgets.end(), this));
    gtk_widget_unparent(GTK_WIDGET(popover->gobj()));
    popover_timeout.disconnect();
}
