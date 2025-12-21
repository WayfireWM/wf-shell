#include <ddcutil_types.h>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#include <iostream>
#include <thread>

#include "brightness.hpp"
#include "panel.hpp"

#define VCP_BRIGHTNESS_CODE 0x10

void show_err(std::string location, DDCA_Status status)
{
    if (!status && (status != DDCRC_OK))
    {
        std::cerr << location << " :" << ddca_rc_name(status) << " : " << ddca_rc_desc(status) << "\n";
    }
}

class WfLightDdcaControl : public WfLightControl
{
  private:
    DDCA_Display_Ref ref;
    DDCA_Display_Info2 *info = nullptr;
    std::string connector    = "...";
    int max;
    double brightness_wish;
    bool wish_pending = false;

    int get_max()
    {
        return max;
    }

  public:
    WfLightDdcaControl(WayfireBrightness *parent, DDCA_Display_Ref _ref) : WfLightControl(parent)
    {
        ref = _ref;
        DDCA_Status status;

        // we want Info2 for the connector name
        status = ddca_get_display_info2(ref, &info);
        show_err("open display", status);

        assert(info != nullptr);
        // drm_card_connector is something like cardX-<connector-name>
        connector = std::string(info->drm_card_connector);
        connector = connector.substr(connector.find("-") + 1, connector.size());

        DDCA_Display_Handle handle;
        ddca_open_display2(info->dref, false, &handle);

        DDCA_Non_Table_Vcp_Value value;
        status = ddca_get_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, &value);
        max    = value.mh << 8 | value.ml;
        ddca_close_display(handle);
        scale.set_target_value(get_brightness());
        update_parent_icon();
        label.set_text(get_name());

        // MCCS doesn’t have anything to track changes to feature codes, so we
        // read the current value and adjust the visual when mapped. New thread
        // because MCCS actions can take quite some time and would stall the UI
        signal_map().connect([this] ()
        {
            std::thread([this] ()
            {
                set_scale_target_value(get_brightness());
            }).detach();
        });
    }

    ~WfLightDdcaControl()
    {
        free(info);
    }

    std::string get_connector()
    {
        return connector;
    }

    std::string get_name()
    {
        auto name = std::string(info->model_name);

        if (name == "")
        {
            name = "Unnamed " + connector + " display";
        }

        return name;
    }

    void set_brightness(double brightness)
    {
        brightness_wish = brightness;

        // set the value in another thread to not block panel, as it can take a while
        // even if we accidentally have another call go through, it’s no biggie
        if (!wish_pending)
        {
            wish_pending = true;
            std::thread(writevcp, this).detach();
        }

        parent->cancel_popover_timeout();
        update_parent_icon();

        for (auto control : DdcaSurveillor::get().ref_to_controls[ref])
        {
            control->set_scale_target_value(brightness);
        }
    }

    static void writevcp(WfLightDdcaControl *control)
    {
        double target = control->brightness_wish;

        DDCA_Display_Handle handle;
        DDCA_Status status = ddca_open_display2(control->info->dref, false, &handle);
        show_err("open display", status);

        uint16_t value = (uint16_t)(control->get_max() * target);
        uint8_t sh     = value >> 8;
        uint8_t sl     = value & 0xFF;
        status = ddca_set_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, sh, sl);
        show_err("set brigthness", status);
        ddca_close_display(handle);

        // value changed again, re-run
        // for this to be infinite recursion, the user needs to constantly keep
        // moving the slider before the write is done, should be safe enough
        if (control->brightness_wish != target)
        {
            writevcp(control);
        }

        control->wish_pending = false;
    }

    double get_brightness()
    {
        DDCA_Display_Handle handle;
        DDCA_Status status = ddca_open_display2(info->dref, false, &handle);
        show_err("open display", status);

        DDCA_Non_Table_Vcp_Value value;
        status = ddca_get_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, &value);
        show_err("get brightness", status);
        ddca_close_display(handle);
        return (value.sh << 8 | value.sl) / 100.0;
    }
};

DdcaSurveillor::DdcaSurveillor()
{
    // ddcutil watcher threads have a tendency to segfault, so we use the gdk monitors
    // this means we can’t tweak parameters and we have to entirely re-scan the displays

    mon_ch_sig = WayfirePanelApp::get().signal_monitor_list_changed().connect([=] ()
    {
        std::cout << "Monitors changed" << std::endl;
        rescan_mccs_monitors(0, 0, 0);
    });

    // waiting for idle means DdcaSurveillor will be initialised
    Glib::signal_idle().connect_once([=] () { rescan_mccs_monitors(0, 0, 0); });
}

DdcaSurveillor::~DdcaSurveillor()
{
    mon_ch_sig.disconnect();
    clean_controls();
}

void DdcaSurveillor::clean_controls()
{
    for (auto widget : instance->widgets)
    {
        strip_widget(widget);
    }
}

void DdcaSurveillor::rescan_mccs_monitors(const int pos, const int rem, const int add)
{
    ddca_redetect_displays();
    // remove monitor controls from the widgets
    instance->clean_controls();
    auto display = Gdk::Display::get_default();

    DDCA_Display_Info_List *display_list = NULL;
    ddca_get_display_info_list2(false, &display_list);

    for (int i = 0; i < display_list->ct; i++)
    {
        auto ref = display_list->info[i].dref;
        instance->ref_to_controls[ref];

        for (auto widget : instance->widgets)
        {
            auto control = std::make_shared<WfLightDdcaControl>(widget, ref);
            instance->ref_to_controls[ref].push_back(control);
            widget->add_control(control);
        }
    }

    ddca_free_display_info_list(display_list);
}

void DdcaSurveillor::catch_up_widget(WayfireBrightness *widget)
{
    /* NOOP */
}

void DdcaSurveillor::strip_widget(WayfireBrightness *widget)
{
    for (auto& [ref, controls] : ref_to_controls)
    {
        controls.erase(std::remove_if(controls.begin(), controls.end(),
            [widget] (const std::shared_ptr<WfLightControl>& c)
        {
            if (c->get_parent() == widget)
            {
                widget->rem_control(c);
                return true;
            }

            return false;
        }),
            controls.end());
    }
}

DdcaSurveillor& DdcaSurveillor::get()
{
    if (!instance)
    {
        instance = std::unique_ptr<DdcaSurveillor>(new DdcaSurveillor());
    }

    return *instance;
}
