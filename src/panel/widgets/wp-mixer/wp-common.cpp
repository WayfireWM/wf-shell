#include <iostream>
#include <pipewire/keys.h>

#include "wp-common.hpp"
#include "wp/proxy-interfaces.h"

const gchar *DEFAULT_NODE_MEDIA_CLASSES[] = {
    "Audio/Sink",
    "Audio/Source",
};

WpCommon::WpCommon()
{
    // creates the core, object interests and connects signals

    std::cout << "Initialising wireplumber\n";
    wp_init(WP_INIT_PIPEWIRE);
    core = wp_core_new(NULL, NULL, NULL);
    object_manager = wp_object_manager_new();

    // register interests in sinks, sources and streams
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

    // load plugins
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

void WpCommon::on_mixer_plugin_loaded(WpCore *core, GAsyncResult *res, void *data)
{
    instance->mixer_api = wp_plugin_find(core, "mixer-api");
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
    instance->default_nodes_api = wp_plugin_find(core, "default-nodes-api");

    instance->on_all_plugins_loaded();
}

void WpCommon::on_all_plugins_loaded()
{
    // connect signals
    g_signal_connect(
        instance->mixer_api,
        "changed",
        G_CALLBACK(instance->on_mixer_changed),
        NULL);

    g_signal_connect(
        instance->default_nodes_api,
        "changed",
        G_CALLBACK(instance->on_default_nodes_changed),
        NULL);

    g_signal_connect(
        instance->object_manager,
        "object_added",
        G_CALLBACK(instance->on_object_added),
        NULL);

    g_signal_connect(
        instance->object_manager,
        "object-removed",
        G_CALLBACK(instance->on_object_removed),
        NULL);

    wp_core_install_object_manager(instance->core, instance->object_manager);
}

void WpCommon::catch_up_to_current_state(WayfireWpMixer *widget)
{
    // catch up to objects already registered by the manager
    WpIterator *reg_objs = wp_object_manager_new_iterator(object_manager);
    GValue item = G_VALUE_INIT;
    while (wp_iterator_next(reg_objs, &item))
    {
        add_object_to_widget((WpPipewireObject*)g_value_get_object(&item), widget);
        g_value_unset(&item);
    }
}

void WpCommon::add_object_to_widget(WpPipewireObject *object, WayfireWpMixer *widget)
{
    // adds a new control to the appropriate section of a widget

    const std::string_view type{wp_pipewire_object_get_property(object, PW_KEY_MEDIA_CLASS)};

    bool recheck_default = false;
    WfWpControl *control;
    Gtk::Box *which_box;
    if (type == "Audio/Sink")
    {
        which_box = &(widget->sinks_box);
        control   = new WfWpControlDevice(object, widget);
        recheck_default = true;
    } else if (type == "Audio/Source")
    {
        which_box = &(widget->sources_box);
        control   = new WfWpControlDevice(object, widget);
        recheck_default = true;
    } else if (type == "Stream/Output/Audio")
    {
        which_box = &(widget->streams_box);
        control   = new WfWpControl(object, (WayfireWpMixer*)widget);
    } else
    {
        std::cout << "Could not match pipewire object media class, ignoring\n";
        return;
    }

    control->init();
    widget->objects_to_controls.insert({object, std::unique_ptr<WfWpControl>(control)});
    which_box->append((Gtk::Widget&)*control);

    // we added new controls, so maybe one of them is a default
    // also initialises the quick_target for when quick_target choice is a device
    if (recheck_default)
    {
        on_default_nodes_changed(default_nodes_api, NULL);
    }

    // let’s considier this a change and set this as quick_target
    if (widget->quick_target_choice == QuickTargetChoice::LAST_CHANGE)
    {
        widget->set_quick_target_from(control);
        // if the full mixer is not shown, change the popover child
        if ((widget->popover->get_child() !=
             &(widget->master_box)) && widget->popover->is_visible())
        {
            widget->popover->set_child(*widget->quick_target);
        }

        widget->update_icon();
    }
}

void WpCommon::on_object_added(WpObjectManager *manager, gpointer object, gpointer data)
{
    // add to all the registered widgets
    auto *obj = (WpPipewireObject*)object;

    for (auto widget : instance->widgets)
    {
        instance->add_object_to_widget(obj, widget);
    }
}

void WpCommon::on_mixer_changed(gpointer mixer_api, guint id, gpointer data)
{
    // update the visual of the appropriate WfWpControl according to external changes on all widgets

    auto info = instance->get_volume_and_mute(id);

    auto volume = info.first;
    auto mute   = info.second;

    // for each widget
    for (auto widget : instance->widgets)
    {
        WfWpControl *control;

        // first, find the appropiate control
        for (auto & it : widget->objects_to_controls)
        {
            WpPipewireObject *obj = it.first;
            control = it.second.get();
            if (wp_proxy_get_bound_id(WP_PROXY(obj)) == id)
            {
                break;
            }

            // if we are at the end and still no match
            if (it.first == widget->objects_to_controls.end()->first)
            {
                std::cerr << "Wireplumber mixer could not find control for wp object";
                return;
            }
        }

        const auto update_icons = [&] ()
        {
            control->update_icon();
            if (widget->quick_target)
            {
                widget->quick_target->update_icon();
            }

            widget->update_icon();
        };

        bool change = false; // if something changes below

        // if the quick_target controls the same object as the changed, correct it as well
        if (
            widget->quick_target /* not quick-target-less guard */
            // current control and quick_target are for the same wp obj
            && (control->object ==
                widget->quick_target->object))
        {
            widget->quick_target->set_btn_status_no_callbk(mute);
            widget->quick_target->set_scale_target_value(volume);
            change = true;
        }
        // change quick_target if needed
        else if (widget->quick_target_choice == QuickTargetChoice::LAST_CHANGE)
        {
            widget->set_quick_target_from(control);
            change = true;
        }

        // if this control is the source of this change, don’t do anything
        if (control->ignore)
        {
            control->ignore = false;
            update_icons();
            continue;
        }

        // correct the values of the control
        control->set_btn_status_no_callbk(mute);
        control->set_scale_target_value(volume);

        update_icons();

        // if the mixer is currently being displayed, stop there
        if ((widget->popover->get_child() ==
             &(widget->master_box)) && widget->popover->is_visible())
        {
            continue;
        }

        if (widget->quick_target &&
            (!widget->popover->is_visible() ||
             (widget->popover->get_child() != (WfWpControl*)&widget->quick_target)))
        {
            // put the quick_target in the popover and show
            widget->popover->set_child(*widget->quick_target);
            if (widget->popup_on_change && change)
            {
                widget->popover->popup();
            }
        }

        // in all cases that reach here, (re-)schedule hiding
        widget->check_set_popover_timeout();
    }
}

void WpCommon::on_default_nodes_changed(gpointer default_nodes_api, gpointer data)
{
    // silimarly to mixer, update the visuals of the controls on the widgets

    if (!default_nodes_api)
    {
        return;
    }

    std::vector<guint32> defaults;
    guint32 id = SPA_ID_INVALID;

    // for the appropriate classes
    for (guint32 i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++)
    {
        // query the new data
        g_signal_emit_by_name(default_nodes_api, "get-default-node", DEFAULT_NODE_MEDIA_CLASSES[i], &id);
        if (id != SPA_ID_INVALID)
        {
            defaults.push_back(id);
        }
    }

    for (const auto widget : instance->widgets)
    {
        for (const auto & entry : widget->objects_to_controls)
        {
            auto obj  = WP_PIPEWIRE_OBJECT(entry.first);
            auto ctrl = static_cast<WfWpControlDevice*>(entry.second.get());

            guint32 bound_id = wp_proxy_get_bound_id(WP_PROXY(obj));
            if (bound_id == SPA_ID_INVALID)
            {
                continue;
            }

            bool is_default = std::any_of(defaults.begin(), defaults.end(), [bound_id] (guint32 def_id)
            {
                return def_id == bound_id;
            });

            // is it a source or a sink?
            const std::string_view type{wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS)};

            if (!is_default)
            {
                const std::string_view type{wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS)};
                if ((type == "Audio/Sink") || (type == "Audio/Source"))
                {
                    ctrl->set_def_status_no_callbk(false);
                }

                continue;
            }

            // if we’re here, we’re a default of some kind
            ctrl->set_def_status_no_callbk(true);

            if ((widget->quick_target_choice == QuickTargetChoice::DEFAULT_SINK) &&
                (type == "Audio/Sink")
                ||
                (widget->quick_target_choice == QuickTargetChoice::DEFAULT_SOURCE) &&
                (type == "Audio/Source")) // if the settings call for it, refresh the quick_target
            {
                widget->set_quick_target_from(ctrl);
                widget->update_icon();
            }
        }
    }
}

void WpCommon::on_object_removed(WpObjectManager *manager, gpointer object, gpointer data)
{
    for (auto widget : instance->widgets)
    {
        auto it = widget->objects_to_controls.find((WpPipewireObject*)object);
        if (it == widget->objects_to_controls.end())
        {
            // shouldn’t happen, but checking to avoid weird situations
            return;
        }

        WfWpControl *control = it->second.get();
        auto *box = (Gtk::Box*)control->get_parent();
        if (box)
        {
            box->remove(*control);
        }

        // if quick_target points to the removed control we should handle it.
        if (widget->quick_target && (widget->quick_target->object == it->first))
        {
            // reset quick_target to nullptr
            widget->quick_target = nullptr;
        }

        widget->objects_to_controls.erase(it);
    }
}

void WpCommon::add_widget(WayfireWpMixer *widget)
{
    widgets.push_back(widget);
    catch_up_to_current_state(widget);
}

void WpCommon::rem_widget(WayfireWpMixer *widget)
{
    widgets.erase(std::find(widgets.begin(), widgets.end(), widget));
}

std::pair<double, bool> WpCommon::get_volume_and_mute(guint32 id)
{
    if (!mixer_api)
    {
        return {0.0, true};
    }

    GVariant *v = NULL;
    g_signal_emit_by_name(mixer_api, "get-volume", id, &v);
    if (!v)
    {
        return {0.0, true};
    }

    gboolean mute  = TRUE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);
    return std::make_pair(std::cbrt(volume), mute);
}

gboolean WpCommon::set_volume(guint32 id, double volume)
{
    using Vol = std::map<Glib::ustring, Glib::Variant<double>>;
    Vol vol;
    vol["volume"] = Glib::Variant<double>::create(std::pow(volume, 3));
    auto vol_v   = Glib::Variant<Vol>::create(vol);
    gboolean res = FALSE; // ignored for now
    g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, vol_v.gobj(), &res);
    return res;
}

gboolean WpCommon::set_mute(guint32 id, bool state)
{
    using Mute = std::map<Glib::ustring, Glib::Variant<bool>>;
    Mute mute;
    mute["mute"] = Glib::Variant<bool>::create(state);
    auto mute_v  = Glib::Variant<Mute>::create(mute);
    gboolean res = FALSE; // ignored for now
    g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, mute_v.gobj(), &res);
    return res;
}

gboolean WpCommon::set_default(WpPipewireObject *object)
{
    auto proxy = WP_PROXY(object);
    const gchar *media_class = wp_pipewire_object_get_property(
        WP_PIPEWIRE_OBJECT(proxy),
        PW_KEY_MEDIA_CLASS);
    for (guint i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++)
    {
        // only for the media class our object is in
        if (media_class == DEFAULT_NODE_MEDIA_CLASSES[i])
        {
            continue;
        }

        const gchar *name = wp_pipewire_object_get_property(
            WP_PIPEWIRE_OBJECT(proxy),
            PW_KEY_NODE_NAME);
        if (!name)
        {
            continue;
        }

        gboolean res = false;
        g_signal_emit_by_name(WpCommon::default_nodes_api, "set-default-configured-node-name",
            media_class, name, &res);

        wp_core_sync(core, NULL, NULL, NULL);

        return res;
    }

    return false;
}

void WpCommon::re_evaluate_def_nodes()
{
    if (default_nodes_api)
    {
        on_default_nodes_changed(default_nodes_api, NULL);
    }
}

WpCommon& WpCommon::get()
{
    if (!instance)
    {
        instance = std::unique_ptr<WpCommon>(new WpCommon());
    }

    return *instance;
}
