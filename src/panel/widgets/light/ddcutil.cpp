#include <ddcutil_types.h>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#include <iostream>
#include <thread>

#include "light.hpp"

#define VCP_BRIGHTNESS_CODE 0x10

void show_err(std::string location, DDCA_Status status){
	// if (!status)
		std::cerr << location << " :" << ddca_rc_name(status) << " : " << ddca_rc_desc(status) << "\n";
}


class WfLightDdcaControl : public WfLightControl
{
	private:
		DDCA_Display_Ref ref;
		int max;
		std::atomic<double> brightness_wish;
		std::atomic<bool>   wish_pending{false};

		int get_max(){
			return max;
		}

	public:
		WfLightDdcaControl(WayfireLight *parent, DDCA_Display_Ref _ref) : WfLightControl(parent){
			ref = _ref;

			DDCA_Display_Handle handle;
			DDCA_Status status = ddca_open_display2(ref, false, &handle);
			show_err("open display", status);

			DDCA_Non_Table_Vcp_Value value;
			status = ddca_get_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, &value);
			max = value.mh << 8 | value.ml;
			ddca_close_display(handle);
			scale.set_target_value(get_brightness());
            label.set_text(get_name());
		}

		std::string get_name(){
			std::string name;
			name = "display";
			return name;
		}

        void set_brightness(double brightness){
			brightness_wish = brightness;
			bool expected = false;

			// set the value in another thread to not block panel, as it can take a while
			if (wish_pending.compare_exchange_strong(expected, true))
			{
				std::thread(writevcp, this).detach();
			}
		}

		static void writevcp(WfLightDdcaControl* control)
		{
			double target = control->brightness_wish.load();

			DDCA_Display_Handle handle;
			DDCA_Status status = ddca_open_display2(control->ref, false, &handle);
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
			if (control->brightness_wish.load() != target)
			{
				writevcp(control);
			}

			control->wish_pending.store(false);
        }

        double get_brightness(){
			DDCA_Display_Handle handle;
			DDCA_Status status = ddca_open_display2(ref, false, &handle);
			show_err("open display", status);

			DDCA_Non_Table_Vcp_Value value;
			status = ddca_get_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, &value);
			show_err("get brightness", status);
			ddca_close_display(handle);
			return value.sh << 8 | value.sl;
	    }
};

DdcaSurveillor::DdcaSurveillor(){
	// watch for new valid monitors

	// when ddcutil implements DDCA_EVENT_CLASS_DPMS (unimpl in 2.2.0), we can handle it
	auto status = ddca_start_watch_displays(DDCA_EVENT_CLASS_DISPLAY_CONNECTION);
	status = ddca_register_display_status_callback(on_display_change);

	ddca_enable_verify(true);
	DDCA_Display_Info_List *display_list = NULL;
	ddca_get_display_info_list2(false, &display_list);

	for (int i = 0 ; i < display_list->ct ; i++){
		displays_info.push_back(&display_list->info[i]);
	}
}

DdcaSurveillor::~DdcaSurveillor(){
	ddca_stop_watch_displays(true);
}


void DdcaSurveillor::on_display_change(DDCA_Display_Status_Event event){
	std::cout << "new display\n";
	// 	ddca_redetect_displays();
	// 	ddca_get_display_refs(false, );
}

void DdcaSurveillor::catch_up_widget(WayfireLight *widget){
    for (auto info : displays_info){
        auto control = std::make_shared<WfLightDdcaControl>(widget, info->dref);
        widget->add_control((std::shared_ptr<WfLightControl>)control);
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
