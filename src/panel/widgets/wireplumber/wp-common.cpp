#include <iostream>
#include <pipewire/keys.h>
#include <utility>

#include "wp-common.hpp"
#include "wireplumber.hpp"
#include "wf-wp-control.hpp"

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

void WpCommon::catch_up_to_current_state(WayfireWireplumber *widget)
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

void WpCommon::add_object_to_widget(WpPipewireObject *object, WayfireWireplumber *widget)
{
    // adds a new control to the appropriate section of a widget

    const std::string_view type{wp_pipewire_object_get_property(object, PW_KEY_MEDIA_CLASS)};

    WfWpControl *control;
    Gtk::Box *which_box;
    if (type == "Audio/Sink")
    {
        which_box = &(widget->sinks_box);

        control = new WfWpControlDevice(object, widget);
    } else if (type == "Audio/Source")
    {
        which_box = &(widget->sources_box);
        control   = new WfWpControlDevice(object, widget);
    } else if (type == "Stream/Output/Audio")
    {
        which_box = &(widget->streams_box);
        control   = new WfWpControl(object, (WayfireWireplumber*)widget);
    } else
    {
        std::cout << "Could not match pipewire object media class, ignoring\n";
        return;
    }

    widget->objects_to_controls.insert({object, std::unique_ptr<WfWpControl>(control)});
    which_box->append((Gtk::Widget&)*control);
    // try to not be faceless
    if (!widget->face)
    {
        widget->face = control->copy();
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

    GVariant *v = NULL;
    // query the new data
    g_signal_emit_by_name(instance->mixer_api, "get-volume", id, &v);
    if (!v)
    {
        return;
    }

    gboolean mute  = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);

    for (auto widget : instance->widgets)
    {
        WfWpControl *control;

        for (auto & it : widget->objects_to_controls)
        {
            WpPipewireObject *obj = it.first;
            control = it.second.get();
            if (wp_proxy_get_bound_id(WP_PROXY(obj)) == id)
            {
                break;
            }
        }

        control->set_btn_status_no_callbk(mute);
        control->set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded
        control->update_icon();

        // correct the face
        // if the face choice is last_change, ensure the control in the popover is the one that was updated,
        // else, if the face does not control the changed object, just don’t change it

        if (
            control->object
            !=
            ((WfWpControl*)(widget->popover->get_child()))->object)
        {
            // in the other two cases, the face choice, if existant, is already in the face
            if (widget->face_choice == FaceChoice::LAST_CHANGE)
            {
                widget->face = control->copy();
            }
        } else
        {
            // update the face’s values; note the WfWpControl constructor already syncs the values
            widget->face->set_btn_status_no_callbk(mute);
            widget->face->set_scale_target_value(std::cbrt(volume)); // see on_mixer_plugin_loaded
        }

        widget->update_icon();

        // if we have the full mixer in the popover
        if ((Gtk::Widget*)&(widget->master_box)
            ==
            (Gtk::Widget*)widget->popover->get_child())
        {
            // if shown, stop there
            if (widget->popover->is_visible())
            {
                return;
            }

            // if hidden, replace by the face
            widget->popover->set_child(*widget->face);
        }

        // if it was hidden and configuration calls for it, show it
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
    // silimarly to mixer, update the visuals of the controls on the widgets

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

    for (auto widget : instance->widgets)
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
            if (is_default)
            {
                ctrl->set_def_status_no_callbk(true);
                continue;
            }

            // if the control is not for a sink or source (non WfWpControlDevice), don’t try to set status
            const std::string_view type{wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS)};

            if ((type == "Audio/Sink") || (type == "Audio/Source"))
            {
                ctrl->set_def_status_no_callbk(false);
            }

            if ( // if the settings call for it, refresh the face
                (
                    (widget->face_choice == FaceChoice::DEFAULT_SINK) &&
                    (type == "Audio/Sink")
                    ||
                    (widget->face_choice == FaceChoice::DEFAULT_SOURCE) &&
                    (type == "Audio/Source")
                ))
            {
                widget->face = ctrl->copy();
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

        // if face points to the removed control we should handle it.
        if (widget->face && (widget->face->object == it->first))
        {
            // reset face to nullptr
            widget->face = nullptr;
        }

        widget->objects_to_controls.erase(it);
    }
}

void WpCommon::add_widget(WayfireWireplumber *widget){
    widgets.push_back(widget);
    catch_up_to_current_state(widget);
}

void WpCommon::rem_widget(WayfireWireplumber *widget){
    widgets.erase(std::find(widgets.begin(), widgets.end(), widget));
}

std::pair<double, bool> WpCommon::get_volume_and_mute(guint32 id){
    GVariant *v = NULL;
    g_signal_emit_by_name(mixer_api, "get-volume", id, &v);
    if (!v)
    {
        // return;
    }
    gboolean mute  = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);
    return std::make_pair(std::cbrt(volume), mute);
}


void WpCommon::set_volume(guint32 id, double volume){
    GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&gvb, "{sv}", "volume",
        g_variant_new_double(std::pow(volume, 3))); // see on_mixer_plugin_loaded
    GVariant *v = g_variant_builder_end(&gvb);
    gboolean res FALSE;
    g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
}

void WpCommon::set_mute(guint32 id, bool state){
    GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&gvb, "{sv}", "mute", g_variant_new_boolean(state));
    GVariant *v = g_variant_builder_end(&gvb);
    gboolean res FALSE;
    g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);

}

gboolean WpCommon::set_default(const gchar* media_class, const gchar* name){
    gboolean res = false;
    g_signal_emit_by_name(WpCommon::default_nodes_api, "set-default-configured-node-name",
        media_class, name, &res);

    wp_core_sync(core, NULL, NULL, NULL);

    return res;
}

WpCommon& WpCommon::get(){
    if (!instance)
    {
        instance = std::unique_ptr<WpCommon>(new WpCommon());
    }
    return *instance;
}
