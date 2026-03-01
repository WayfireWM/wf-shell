#pragma once

#include <gtkmm.h>

#include "../widget.hpp"
#include "wf-popover.hpp"
#include "wf-ipc.hpp"

class WayfireWorkspaceBox;
class WayfireWorkspaceWindow : public Gtk::Widget
{
  public:
    int x = 0, y = 0, w = 10, h = 10;
    int x_index, y_index;
    int id, output_id;
    bool active;
    WayfireWorkspaceBox *ws;
    WayfireWorkspaceWindow()
    {}
    ~WayfireWorkspaceWindow() override
    {}
};

class WayfireWorkspaceSwitcher : public WayfireWidget, public IIPCSubscriber
{
    std::string output_name;
    void set_size();
    void on_event(wf::json_t data) override;
    void switcher_on_event(wf::json_t data);
    void grid_on_event(wf::json_t data);
    void render_workspace(wf::json_t workspace_data, int j, int output_id, int output_width,
        int output_height);
    void process_workspaces(wf::json_t workspace_data);
    void grid_process_workspaces(wf::json_t workspace_data);
    void render_views(wf::json_t views_data);
    void grid_render_views(wf::json_t views_data);
    void add_view(wf::json_t view_data);
    void grid_add_view(wf::json_t view_data);
    void remove_view(wf::json_t view_data);
    void grid_remove_view(wf::json_t view_data);
    void clear_switcher_box();
    void clear_box();
    void get_wsets();
    bool on_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation);
    bool on_grid_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation);

  public:
    Gtk::Box box;
    Gtk::Grid mini_grid;
    Gtk::Box switcher_box;
    Gtk::Grid switch_grid;
    Gtk::Overlay overlay;
    std::pair<double, double> get_scaled_size();
    std::unique_ptr<WayfireMenuButton> button;
    int output_width, output_height;
    void init(Gtk::Box *container) override;
    WayfireWorkspaceSwitcher(WayfireOutput *output);
    ~WayfireWorkspaceSwitcher();
    int grid_width, grid_height;
    int active_view_id;
    std::shared_ptr<IPCClient> ipc_client;
    std::pair<int, int> get_workspace(WayfireWorkspaceBox *ws, WayfireWorkspaceWindow *w);
    std::pair<int, int> grid_get_workspace(WayfireWorkspaceWindow *w);
    int current_ws_x, current_ws_y;
    std::vector<WayfireWorkspaceWindow*> windows;
    WfOption<std::string> workspace_switcher_mode{"panel/workspace_switcher_mode"};
    WfOption<double> workspace_switcher_target_size_opt{"panel/workspace_switcher_target_size"};
    double workspace_switcher_target_size;
    WfOption<bool> workspace_switcher_render_views{"panel/workspace_switcher_render_views"};
};

class WayfireWorkspaceBox : public Gtk::Overlay
{
    WayfireWorkspaceSwitcher *switcher;

  public:
    int x_index, y_index;
    int output_id, output_width, output_height;
    std::pair<int, int> get_scaled_size();
    WayfireWorkspaceBox(WayfireWorkspaceSwitcher *switcher)
    {
        this->switcher = switcher;
    }

    ~WayfireWorkspaceBox() override
    {}
    void on_switch_grid_clicked(int count, double x, double y);
    void on_workspace_clicked(int count, double x, double y);
    bool on_workspace_scrolled(double x, double y);
};
