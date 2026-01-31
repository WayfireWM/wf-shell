extern "C"{
	#include <ddcutil_c_api.h>
	#include <ddcutil_status_codes.h>
}

#include "light.hpp"

class WfLightDdcaControl : public WfLightControl
{
	friend class DdcaSurveillor;

	private:
		DDCA_Display_Ref ref;
};

class DdcaSurveillor {
	private:
		DdcaSurveillor(){
			ddca_enable_verify(true);
			DDCA_Display_Info_List *display_list = NULL;
			ddca_get_display_info_list2(false, &display_list);

			for (int i = 0 ; i < display_list->ct ; i++){
				display_refs.push_back(display_list[i]);
			}
		}

		std::vector<DDCA_Display_Info> display_refs;
}
