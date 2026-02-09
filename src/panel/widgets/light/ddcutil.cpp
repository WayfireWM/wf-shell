#include <ddcutil_types.h>
#include <iostream>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

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
		}

		std::string get_name(){
			std::string name;
			name = "display";
			return name;
		}

        void set_brightness(double brightness){
			DDCA_Display_Handle handle;
			DDCA_Status status = ddca_open_display2(ref, false, &handle);
			show_err("open display", status);

			uint16_t value = (uint16_t)(get_max() * brightness);
			uint8_t sh = value >> 8;
			uint8_t sl = value & 0xFF;
			status = ddca_set_non_table_vcp_value(handle, VCP_BRIGHTNESS_CODE, sh, sl);
			show_err("set brigthness", status);
			ddca_close_display(handle);
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

	auto status = ddca_start_watch_displays(DDCA_EVENT_CLASS_DISPLAY_ALL);

	status = ddca_register_display_status_callback(on_new_display);

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

void DdcaSurveillor::on_new_display(DDCA_Display_Status_Event event){
	std::cout << "new display\n";
	// 	ddca_redetect_displays();
	// 	ddca_get_display_refs(false, );
}

void DdcaSurveillor::catch_up_widget(WayfireLight *widget){
    for (auto info : displays_info){
        auto control = std::make_shared<WfLightDdcaControl>(widget, info->dref);
        // it.second.second.push_back(std::shared_ptr<WfLightSysfsControl>(control));
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
