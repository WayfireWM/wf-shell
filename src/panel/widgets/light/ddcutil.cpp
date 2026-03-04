#include <ddcutil_types.h>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#include <iostream>
#include <thread>

#include "light.hpp"

#define VCP_BRIGHTNESS_CODE 0x10

void show_err(std::string location, DDCA_Status status){
    if (!status && status != DDCRC_OK)
        std::cerr << location << " :" << ddca_rc_name(status) << " : " << ddca_rc_desc(status) << "\n";
}

class WfLightDdcaControl : public WfLightControl
{
    private:
        DDCA_Display_Ref ref;
        DDCA_Display_Info2 *info;
        std::string connector;
        int max;
        double brightness_wish;
        bool wish_pending = false;

        int get_max(){
            return max;
        }

    public:
        WfLightDdcaControl(WayfireLight *parent, DDCA_Display_Ref _ref) : WfLightControl(parent){
            ref = _ref;

            DdcaSurveillor::get().ref_to_controls.at(ref).push_back((std::shared_ptr<WfLightControl>)this);

            DDCA_Status status;

            // we want Info2 for the connector name
            status = ddca_get_display_info2(ref, &info);

            // drm_card_connector is something like cardX-<connector-name>
            connector = std::string(info->drm_card_connector);
            connector = connector.substr(connector.find("-") + 1, connector.size());

            DDCA_Display_Handle handle;
            ddca_open_display2(info->dref, false, &handle);
            show_err("open display", status);

            DDCA_Non_Table_Vcp_Value value;
            status = ddca_get_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, &value);
            max = value.mh << 8 | value.ml;
            ddca_close_display(handle);
            scale.set_target_value(get_brightness());
            update_parent_icon();
            label.set_text(get_name());
        }

        std::string get_connector()
        {
            return connector;
        }

        std::string get_name(){
            return std::string(info->model_name);
        }

        void set_brightness(double brightness){
            brightness_wish = brightness;

            // set the value in another thread to not block panel, as it can take a while
            // even if we accidentally have another call go through, it’s no biggie
            if (!wish_pending)
            {
                wish_pending = true;
                std::thread(writevcp, this).detach();
            }

            update_parent_icon();

            // we don’t really have a good way to track changes to the monitor’s brightness, so here,
            // we don’t update on external changes and just update the controls of the other widgets
            // see watching for VCP changes : https://github.com/rockowitz/ddcutil/issues/589
            for (auto control : DdcaSurveillor::get().ref_to_controls[ref])
            {
                control->set_scale_target_value(brightness);
            }
        }

        static void writevcp(WfLightDdcaControl* control)
        {
            double target = control->brightness_wish;

            DDCA_Display_Handle handle;
            DDCA_Status status = ddca_open_display2(control->info->dref, false, &handle);
            show_err("open display", status);

            uint16_t value = (uint16_t)(control->get_max() * target);
            uint8_t sh = value >> 8;
            uint8_t sl = value & 0xFF;
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

        double get_brightness(){
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

DdcaSurveillor::DdcaSurveillor(){
    DDCA_Status status;
    status = ddca_init2(NULL, DDCA_SYSLOG_ERROR, DDCA_INIT_OPTIONS_NONE, NULL);
    show_err("ddca init", status);

    ddca_enable_dynamic_sleep(true);

    // watch for new valid monitors
    // when ddcutil implements DDCA_EVENT_CLASS_DPMS, we can handle it
    status = ddca_register_display_status_callback(on_display_change);
    show_err("register callback", status);
    status = ddca_start_watch_displays(DDCA_EVENT_CLASS_DISPLAY_CONNECTION);
    show_err("start watch displays", status);

    ddca_enable_verify(false);
    DDCA_Display_Info_List *display_list = NULL;
    ddca_get_display_info_list2(false, &display_list);

    for (int i = 0 ; i < display_list->ct ; i++){
        ref_to_controls[display_list->info[i].dref];
    }
}

DdcaSurveillor::~DdcaSurveillor()
{
    ddca_stop_watch_displays(false);
}

void DdcaSurveillor::on_display_change(DDCA_Display_Status_Event event)
{
    if (event.event_type == DDCA_EVENT_DISPLAY_CONNECTED)
    {
        DdcaSurveillor::get().ref_to_controls[event.dref];
        for (auto& widget : DdcaSurveillor::get().widgets)
        {
            auto control = std::make_shared<WfLightDdcaControl>(widget, event.dref);
            widget->add_control(control);
        }
    } else if (event.event_type == DDCA_EVENT_DISPLAY_DISCONNECTED)
    {
        DdcaSurveillor::get().ref_to_controls.erase(event.dref);
    }
}

void DdcaSurveillor::catch_up_widget(WayfireLight *widget){
    for (auto pair : ref_to_controls)
    {
        auto control = std::make_shared<WfLightDdcaControl>(widget, pair.first);
        widget->add_control(control);
    }
}

void DdcaSurveillor::strip_widget(WayfireLight *widget){
}

DdcaSurveillor& DdcaSurveillor::get(){
    if (!instance)
    {
        instance = std::unique_ptr<DdcaSurveillor>(new DdcaSurveillor());
    }
    return *instance;
}
