#pragma once

#include <memory>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

class WfToplevelIcon
{
  public:
    WfToplevelIcon(zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    ~WfToplevelIcon();
    void set_app_id(std::string app_id);
    void set_title(std::string title);
    void set_state(uint32_t state);
    void close();

    class impl;

  private:
    std::unique_ptr<impl> pimpl;
};

namespace IconProvider
{
/* Loads custom app_id -> icon file mappings from the section
* They have the format icon_mapping_<app_id> = <icon file> */
void load_custom_icons();
}
