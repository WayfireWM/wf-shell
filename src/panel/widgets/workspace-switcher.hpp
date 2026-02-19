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
    void on_event(wf::json_t data) override;
    void switcher_on_event(wf::json_t data);
    void popover_on_event(wf::json_t data);
    void render_workspace(wf::json_t workspace_data, int j, int output_id, int output_width,
        int output_height);
    void process_workspaces(wf::json_t workspace_data);
    void popover_process_workspaces(wf::json_t workspace_data);
    void render_views(wf::json_t views_data);
    void popover_render_views(wf::json_t views_data);
    void add_view(wf::json_t view_data);
    void popover_add_view(wf::json_t view_data);
    void remove_view(wf::json_t view_data);
    void popover_remove_view(wf::json_t view_data);
    void clear_switcher_box();
    void clear_box();
    void get_wsets();
    bool on_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation);
    bool on_popover_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation);

  public:
    Gtk::Box box;
    Gtk::Grid grid;
    Gtk::Box switcher_box;
    Gtk::Grid popover_grid;
    Gtk::Overlay overlay;
    double get_scaled_width();
    std::unique_ptr<WayfireMenuButton> button;
    int output_width, output_height;
    void init(Gtk::Box *container) override;
    WayfireWorkspaceSwitcher(WayfireOutput *output);
    ~WayfireWorkspaceSwitcher();
    int grid_width, grid_height;
    int active_view_id;
    std::shared_ptr<IPCClient> ipc_client;
    std::pair<int, int> get_workspace(WayfireWorkspaceBox *ws, WayfireWorkspaceWindow *w);
    std::pair<int, int> popover_get_workspace(WayfireWorkspaceWindow *w);
    int current_ws_x, current_ws_y;
    std::vector<WayfireWorkspaceWindow*> windows;
    WfOption<std::string> workspace_switcher_mode{"panel/workspace_switcher_mode"};
    WfOption<double> workspace_switcher_target_height_opt{"panel/workspace_switcher_target_height"};
    double workspace_switcher_target_height;
    WfOption<bool> workspace_switcher_render_views{"panel/workspace_switcher_render_views"};
};

class WayfireWorkspaceBox : public Gtk::Overlay
{
    WayfireWorkspaceSwitcher *switcher;

  public:
    int x_index, y_index;
    int output_id, output_width, output_height;
    int get_scaled_width();
    WayfireWorkspaceBox(WayfireWorkspaceSwitcher *switcher)
    {
        this->switcher = switcher;
    }

    ~WayfireWorkspaceBox() override
    {}
    void on_popover_grid_clicked(int count, double x, double y);
    void on_workspace_clicked(int count, double x, double y);
    bool on_workspace_scrolled(double x, double y);
};
