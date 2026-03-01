#include <iostream>
#include <gtkmm.h>
#include <glibmm.h>

#include <wf-option-wrap.hpp>

#include "panel.hpp"
#include "workspace-switcher.hpp"

void WayfireWorkspaceSwitcher::init(Gtk::Box *container)
{
    ipc_client = WayfirePanelApp::get().get_ipc_server_instance()->create_client();

    if (!ipc_client)
    {
        std::cerr << "Failed to connect to ipc. (are ipc and ipc-rules plugins loaded?)";
        return;
    }

    box.add_css_class("workspace-switcher");
    box.add_css_class("flat");

    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_children()[0]->add_css_class("flat");
    button->get_children()[0]->add_css_class("workspace-switcher");

    ipc_client->subscribe(this, {"view-mapped"});
    ipc_client->subscribe(this, {"view-focused"});
    ipc_client->subscribe(this, {"view-unmapped"});
    ipc_client->subscribe(this, {"view-minimized"});
    ipc_client->subscribe(this, {"view-set-output"});
    ipc_client->subscribe(this, {"view-geometry-changed"});
    ipc_client->subscribe(this, {"output-layout-changed"});
    ipc_client->subscribe(this, {"wset-workspace-changed"});

    workspace_switcher_target_size_opt.set_callback([=] () {set_size();});

    auto mode_cb = ([=] ()
    {
        clear_switcher_box();
        if (overlay.get_parent())
        {
            overlay.unparent();
        }

        if (workspace_switcher_mode.value() == "row")
        {
            switcher_box.append(box);
        } else if (workspace_switcher_mode.value() == "grid")
        {
            switcher_box.append(overlay);
        } else // "grid_popover"
        {
            button->get_popover()->set_child(overlay);
            button->set_child(mini_grid);
            switcher_box.append(*button);
        }

        get_wsets();
    });
    workspace_switcher_mode.set_callback(mode_cb);

    workspace_switcher_render_views.set_callback([=] ()
    {
        get_wsets();
    });

    switcher_box.add_css_class("workspace-switcher");

    container->append(switcher_box);
    mode_cb();
}

void WayfireWorkspaceSwitcher::set_size()
{
    double val = workspace_switcher_target_size_opt.value();
    if (val == 0.0)
    {
        val = (double)WfOption<int>{"panel/minimal_height"}.value();
    }

    if (workspace_switcher_mode.value() == "row")
    {
        WfOption<std::string> panel_position{"panel/position"};
        if (panel_position.value() == PANEL_POSITION_LEFT or panel_position.value() == PANEL_POSITION_RIGHT)
        {
            val = val / grid_width;
        }
    } else if (workspace_switcher_mode.value() == "grid")
    {
        val = val / grid_height;
    }

    workspace_switcher_target_size = val;
}

void WayfireWorkspaceSwitcher::get_wsets()
{
    ipc_client->send("{\"method\":\"window-rules/list-wsets\"}", [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error getting wsets list for workspace-switcher widget!" << std::endl;
            return;
        }

        if (workspace_switcher_mode.value() == "row")
        {
            process_workspaces(data);
        } else // "grid"/"grid_popover"
        {
            grid_process_workspaces(data);
        }

        set_size();
    });
}

void WayfireWorkspaceSwitcher::clear_switcher_box()
{
    for (auto child : switcher_box.get_children())
    {
        switcher_box.remove(*child);
    }
}

void WayfireWorkspaceSwitcher::clear_box()
{
    for (auto child : box.get_children())
    {
        box.remove(*child);
    }

    for (auto child : mini_grid.get_children())
    {
        mini_grid.remove(*child);
    }

    for (auto child : switch_grid.get_children())
    {
        switch_grid.remove(*child);
    }

    windows.clear();
}

std::pair<int, int> WayfireWorkspaceSwitcher::get_workspace(WayfireWorkspaceBox *ws,
    WayfireWorkspaceWindow *w)
{
    std::pair<int, int> workspace;
    auto size = ws->get_scaled_size();
    double scaled_output_width  = size.first;
    double scaled_output_height = size.second;
    workspace.first  = std::floor((w->x + (w->w / 2)) / scaled_output_width) + this->current_ws_x;
    workspace.second = std::floor((w->y + (w->h / 2)) / scaled_output_height) + this->current_ws_y;
    return workspace;
}

std::pair<int, int> WayfireWorkspaceSwitcher::grid_get_workspace(WayfireWorkspaceWindow *w)
{
    std::pair<int, int> workspace;
    auto size = this->get_scaled_size();
    double scaled_output_width  = size.first;
    double scaled_output_height = size.second;
    workspace.first  = std::floor((w->x + (w->w / 2)) / scaled_output_width) + this->current_ws_x;
    workspace.second = std::floor((w->y + (w->h / 2)) / scaled_output_height) + this->current_ws_y;
    return workspace;
}

bool WayfireWorkspaceSwitcher::on_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation)
{
    if (auto w = static_cast<WayfireWorkspaceWindow*>(widget))
    {
        allocation.set_x(w->x + (this->current_ws_x - w->x_index) * w->ws->get_width());
        allocation.set_y(w->y);
        allocation.set_width(w->w);
        allocation.set_height(w->h);
        return true;
    }

    return false;
}

bool WayfireWorkspaceSwitcher::on_grid_get_child_position(Gtk::Widget *widget, Gdk::Rectangle& allocation)
{
    if (auto w = static_cast<WayfireWorkspaceWindow*>(widget))
    {
        auto size = this->get_scaled_size();
        allocation.set_x(w->x + this->current_ws_x * size.first);
        allocation.set_y(w->y + this->current_ws_y * size.second);
        allocation.set_width(w->w);
        allocation.set_height(w->h);
        return true;
    }

    return false;
}

void WayfireWorkspaceBox::on_switch_grid_clicked(int count, double x, double y)
{
    wf::json_t workspace_switch_request;
    workspace_switch_request["method"] = "vswitch/set-workspace";
    wf::json_t workspace;
    workspace["x"] = this->switcher->current_ws_x = this->x_index;
    workspace["y"] = this->switcher->current_ws_y = this->y_index;
    workspace["output-id"] = this->output_id;
    workspace_switch_request["data"] = workspace;
    this->switcher->ipc_client->send(workspace_switch_request.serialize(), [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error switching workspaces. Is vswitch plugin enabled?" << std::endl;
        }
    });
    for (auto widget : this->switcher->mini_grid.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        if ((ws->x_index == this->x_index) && (ws->y_index == this->y_index))
        {
            ws->remove_css_class("inactive");
            ws->add_css_class("active");
        } else
        {
            ws->add_css_class("inactive");
            ws->remove_css_class("active");
        }
    }
}

void WayfireWorkspaceBox::on_workspace_clicked(int count, double x, double y)
{
    wf::json_t workspace_switch_request;
    workspace_switch_request["method"] = "vswitch/set-workspace";
    wf::json_t workspace;
    workspace["x"] = this->switcher->current_ws_x = this->x_index;
    workspace["y"] = this->switcher->current_ws_y = this->y_index;
    workspace["output-id"] = this->output_id;
    workspace_switch_request["data"] = workspace;
    this->switcher->ipc_client->send(workspace_switch_request.serialize(), [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error switching workspaces. Is vswitch plugin enabled?" << std::endl;
        }
    });
    for (auto widget : this->switcher->box.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        if ((ws->x_index == this->x_index) && (ws->y_index == this->y_index))
        {
            ws->remove_css_class("inactive");
            ws->add_css_class("active");
        } else
        {
            ws->add_css_class("inactive");
            ws->remove_css_class("active");
        }
    }
}

std::pair<double, double> WayfireWorkspaceSwitcher::get_scaled_size()
{
    WfOption<std::string> panel_position{"panel/position"};

    if (panel_position.value() == PANEL_POSITION_LEFT or panel_position.value() == PANEL_POSITION_RIGHT)
    {
        return {
            workspace_switcher_target_size,
            (workspace_switcher_target_size * this->output_height / float(this->output_width))
        };
    } else
    {
        return {
            (workspace_switcher_target_size * this->output_width / float(this->output_height)),
            workspace_switcher_target_size
        };
    }
}

std::pair<int, int> WayfireWorkspaceBox::get_scaled_size()
{
    return this->switcher->get_scaled_size();
}

bool WayfireWorkspaceBox::on_workspace_scrolled(double x, double y)
{
    for (auto widget : this->switcher->box.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        if (y > 0)
        {
            if (ws->y_index < ws->switcher->grid_height - 1)
            {
                ws->y_index++;
            }
        } else
        {
            if (ws->y_index > 0)
            {
                ws->y_index--;
            }
        }
    }

    wf::json_t workspace_switch_request;
    workspace_switch_request["method"] = "vswitch/set-workspace";
    wf::json_t workspace;
    workspace["x"] = this->switcher->current_ws_x = this->x_index;
    workspace["y"] = this->switcher->current_ws_y = this->y_index;
    workspace["output-id"] = this->output_id;
    workspace_switch_request["data"] = workspace;
    this->switcher->ipc_client->send(workspace_switch_request.serialize(), [=] (wf::json_t data)
    {
        if (data.serialize().find("error") != std::string::npos)
        {
            std::cerr << data.serialize() << std::endl;
            std::cerr << "Error switching workspaces. Is vswitch plugin enabled?" << std::endl;
        }
    });
    for (auto widget : this->switcher->box.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        if ((ws->x_index == this->x_index) && (ws->y_index == this->y_index))
        {
            ws->remove_css_class("inactive");
            ws->add_css_class("active");
        } else
        {
            ws->add_css_class("inactive");
            ws->remove_css_class("active");
        }
    }

    return false;
}

void WayfireWorkspaceSwitcher::render_workspace(wf::json_t workspace, int j, int output_id, int output_width,
    int output_height)
{
    auto ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
    ws->x_index   = j;
    ws->y_index   = workspace["workspace"]["y"].as_int();
    ws->output_id = output_id;
    ws->output_width  = output_width;
    ws->output_height = output_height;
    ws->add_css_class("workspace");
    if (workspace["workspace"]["x"].as_int() == j)
    {
        ws->add_css_class("active");
        this->current_ws_x = j;
        this->current_ws_y = ws->y_index;
    } else
    {
        ws->add_css_class("inactive");
    }

    ws->signal_get_child_position().connect(sigc::mem_fun(*this,
        &WayfireWorkspaceSwitcher::on_get_child_position), false);
    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(0);
    click_gesture->signal_released().connect(sigc::mem_fun(*ws, &WayfireWorkspaceBox::on_workspace_clicked));
    auto scroll_controller = Gtk::EventControllerScroll::create();
    scroll_controller->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_controller->signal_scroll().connect(sigc::mem_fun(*ws,
        &WayfireWorkspaceBox::on_workspace_scrolled),
        false);
    ws->set_hexpand(false);
    ws->set_vexpand(false);
    auto size = get_scaled_size();
    ws->set_size_request(size.first, size.second);
    ws->add_controller(click_gesture);
    ws->add_controller(scroll_controller);
    box.append(*ws);
    if (workspace_switcher_render_views.value() && (j == this->grid_width - 1))
    {
        ipc_client->send("{\"method\":\"window-rules/list-views\"}", [=] (wf::json_t data)
        {
            if (data.serialize().find("error") != std::string::npos)
            {
                std::cerr << data.serialize() << std::endl;
                std::cerr << "Error getting views list for workspace-switcher widget!" << std::endl;
                return;
            }

            render_views(data);
        });
    }
}

void WayfireWorkspaceSwitcher::process_workspaces(wf::json_t workspace_data)
{
    size_t i = 0;

    this->grid_width  = workspace_data[i]["workspace"]["grid_width"].as_int();
    this->grid_height = workspace_data[i]["workspace"]["grid_height"].as_int();

    for (i = 0; i < workspace_data.size(); i++)
    {
        wf::json_t output_info_request;
        output_info_request["method"] = "window-rules/output-info";
        wf::json_t output_id;
        output_id["id"] = workspace_data[i]["output-id"].as_int();
        output_info_request["data"] = output_id;
        ipc_client->send(output_info_request.serialize(), [=] (wf::json_t output_data)
        {
            if (output_data.serialize().find("error") != std::string::npos)
            {
                std::cerr << output_data.serialize() << std::endl;
                std::cerr << "Error getting output information!" << std::endl;
                return;
            }

            if (this->output_name == output_data["name"].as_string())
            {
                auto output_id     = output_data["id"].as_int();
                auto output_width  = output_data["geometry"]["width"].as_int();
                auto output_height = output_data["geometry"]["height"].as_int();
                for (auto w : windows)
                {
                    if (w->active)
                    {
                        this->active_view_id = w->id;
                        break;
                    }
                }

                clear_box();
                for (int j = 0; j < this->grid_width; j++)
                {
                    render_workspace(workspace_data[i], j, output_id, output_width, output_height);
                }
            }
        });
    }
}

void WayfireWorkspaceSwitcher::grid_process_workspaces(wf::json_t workspace_data)
{
    size_t i = 0;

    this->grid_width  = workspace_data[i]["workspace"]["grid_width"].as_int();
    this->grid_height = workspace_data[i]["workspace"]["grid_height"].as_int();

    for (i = 0; i < workspace_data.size(); i++)
    {
        wf::json_t output_info_request;
        output_info_request["method"] = "window-rules/output-info";
        wf::json_t output_id;
        output_id["id"] = workspace_data[i]["output-id"].as_int();
        output_info_request["data"] = output_id;
        ipc_client->send(output_info_request.serialize(), [=] (wf::json_t output_data)
        {
            if (output_data.serialize().find("error") != std::string::npos)
            {
                std::cerr << output_data.serialize() << std::endl;
                std::cerr << "Error getting output information!" << std::endl;
                return;
            }

            if (this->output_name == output_data["name"].as_string())
            {
                this->output_width  = output_data["geometry"]["width"].as_int();
                this->output_height = output_data["geometry"]["height"].as_int();
                for (auto w : windows)
                {
                    if (w->active)
                    {
                        this->active_view_id = w->id;
                        break;
                    }
                }

                clear_box();
                button->m_popover.set_parent(mini_grid);
                button->m_popover.set_child(overlay);
                overlay.set_child(switch_grid);
                overlay.add_css_class("workspace");
                overlay.signal_get_child_position().connect(sigc::mem_fun(*this,
                    &WayfireWorkspaceSwitcher::on_grid_get_child_position), false);
                for (int j = 0; j < this->grid_height; j++)
                {
                    for (int k = 0; k < this->grid_width; k++)
                    {
                        auto ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
                        ws->output_id = output_data["id"].as_int();
                        ws->set_can_target(false);
                        auto size     = this->get_scaled_size();
                        auto ws_width = size.first / this->grid_width;
                        auto ws_height = size.second / this->grid_height;
                        ws->set_size_request(ws_width, ws_height);
                        ws->add_css_class("workspace");
                        if ((workspace_data[i]["workspace"]["x"].as_int() == k) &&
                            (workspace_data[i]["workspace"]["y"].as_int() == j))
                        {
                            ws->add_css_class("active");
                            this->current_ws_x = k;
                            this->current_ws_y = j;
                        } else
                        {
                            ws->add_css_class("inactive");
                        }

                        ws->x_index = k;
                        ws->y_index = j;
                        mini_grid.attach(*ws, ws->x_index, ws->y_index, 1, 1);

                        ws = Gtk::make_managed<WayfireWorkspaceBox>(this);
                        ws->output_id = output_data["id"].as_int();
                        ws->set_size_request(size.first, size.second);
                        ws->add_css_class("workspace");
                        if ((workspace_data[i]["workspace"]["x"].as_int() == k) &&
                            (workspace_data[i]["workspace"]["y"].as_int() == j))
                        {
                            ws->add_css_class("active");
                            this->current_ws_x = k;
                            this->current_ws_y = j;
                        } else
                        {
                            ws->add_css_class("inactive");
                        }

                        ws->x_index = k;
                        ws->y_index = j;
                        auto popover_click_gesture = Gtk::GestureClick::create();
                        popover_click_gesture->set_button(0);
                        popover_click_gesture->signal_released().connect(sigc::mem_fun(*ws,
                            &WayfireWorkspaceBox::on_switch_grid_clicked));
                        ws->add_controller(popover_click_gesture);
                        switch_grid.attach(*ws, ws->x_index, ws->y_index, 1, 1);
                        if (workspace_switcher_render_views.value() && (j == this->grid_height - 1) &&
                            (k == this->grid_width - 1))
                        {
                            ipc_client->send("{\"method\":\"window-rules/list-views\"}", [=] (wf::json_t data)
                            {
                                if (data.serialize().find("error") != std::string::npos)
                                {
                                    std::cerr << data.serialize() << std::endl;
                                    std::cerr << "Error getting views list for workspace-switcher widget!" <<
                                        std::endl;
                                    return;
                                }

                                grid_render_views(data);
                            });
                        }
                    }
                }
            }
        });
    }
}

void WayfireWorkspaceSwitcher::add_view(wf::json_t view_data)
{
    if (view_data["type"].as_string() != "toplevel")
    {
        return;
    }

    if (view_data["output-name"].as_string() != this->output_name)
    {
        return;
    }

    if (view_data["minimized"].as_bool())
    {
        return;
    }

    auto v = Gtk::make_managed<WayfireWorkspaceWindow>();
    v->add_css_class("view");
    v->add_css_class(view_data["app-id"].as_string());
    v->id = view_data["id"].as_int();
    if (view_data["activated"].as_bool() || (v->id == this->active_view_id))
    {
        v->add_css_class("active");
        v->active = true;
        this->active_view_id = v->id;
    } else
    {
        v->add_css_class("inactive");
        v->active = false;
    }

    v->output_id = view_data["output-id"].as_int();
    auto x = view_data["geometry"]["x"].as_int();
    auto y = view_data["geometry"]["y"].as_int();
    auto w = view_data["geometry"]["width"].as_int();
    auto h = view_data["geometry"]["height"].as_int();
    for (auto widget : box.get_children())
    {
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
        for (auto child : widget->get_children())
        {
            WayfireWorkspaceWindow *window = (WayfireWorkspaceWindow*)child;
            if (window->id == v->id)
            {
                return;
            }
        }

        auto size     = ws->get_scaled_size();
        double width  = size.first;
        double height = size.second;

        v->x = x * (width / float(ws->output_width));
        v->y = y * (height / float(ws->output_height));
        v->w = (w / float(ws->output_width)) * width;
        v->h = (h / float(ws->output_height)) * height;
        std::pair<int, int> workspace;
        workspace  = get_workspace(ws, v);
        v->x_index = workspace.first;
        v->y_index = workspace.second;
        if ((v->x_index == ws->x_index) && (v->y_index == ws->y_index))
        {
            v->ws = ws;
            v->set_can_target(false);
            // add to workspace box
            ws->add_overlay(*v);
            windows.push_back(v);

            return;
        }
    }
}

void WayfireWorkspaceSwitcher::grid_add_view(wf::json_t view_data)
{
    if (view_data["type"].as_string() != "toplevel")
    {
        return;
    }

    if (view_data["output-name"].as_string() != this->output_name)
    {
        return;
    }

    if (view_data["minimized"].as_bool())
    {
        return;
    }

    auto v = Gtk::make_managed<WayfireWorkspaceWindow>();
    v->add_css_class("view");
    v->add_css_class(view_data["app-id"].as_string());
    v->id = view_data["id"].as_int();
    if (view_data["activated"].as_bool() || (v->id == this->active_view_id))
    {
        v->add_css_class("active");
        v->active = true;
        this->active_view_id = v->id;
    } else
    {
        v->add_css_class("inactive");
        v->active = false;
    }

    v->output_id = view_data["output-id"].as_int();
    auto x = view_data["geometry"]["x"].as_int();
    auto y = view_data["geometry"]["y"].as_int();
    auto w = view_data["geometry"]["width"].as_int();
    auto h = view_data["geometry"]["height"].as_int();
    for (auto widget : overlay.get_children())
    {
        WayfireWorkspaceWindow *window = (WayfireWorkspaceWindow*)widget;
        if (window->id == v->id)
        {
            return;
        }

        auto size     = this->get_scaled_size();
        double width  = size.first;
        double height = size.second;

        v->x = x * (width / float(this->output_width));
        v->y = y * (height / float(this->output_height));
        v->w = (w / float(this->output_width)) * width;
        v->h = (h / float(this->output_height)) * height;
        std::pair<int, int> workspace;
        workspace  = grid_get_workspace(v);
        v->x_index = workspace.first;
        v->y_index = workspace.second;
        WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)&overlay;
        v->ws = ws;
        v->set_can_target(false);
        // add to workspace box
        overlay.add_overlay(*v);
        windows.push_back(v);

        return;
    }
}

void WayfireWorkspaceSwitcher::remove_view(wf::json_t view_data)
{
    for (auto w : this->windows)
    {
        if (w->id == view_data["id"].as_int())
        {
            for (auto widget : box.get_children())
            {
                WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)widget;
                if (w->get_parent() == ws)
                {
                    ws->remove_overlay(*w);
                    auto elem = std::remove(windows.begin(), windows.end(), w);
                    windows.erase(elem, windows.end());
                    return;
                }
            }
        }
    }
}

void WayfireWorkspaceSwitcher::grid_remove_view(wf::json_t view_data)
{
    for (auto w : this->windows)
    {
        if (w->id == view_data["id"].as_int())
        {
            for (auto widget : overlay.get_children())
            {
                WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
                overlay.remove_overlay(*w);
                auto elem = std::remove(windows.begin(), windows.end(), w);
                windows.erase(elem, windows.end());
                return;
            }
        }
    }
}

void WayfireWorkspaceSwitcher::render_views(wf::json_t views_data)
{
    for (size_t i = 0; i < views_data.size(); i++)
    {
        add_view(views_data[i]);
    }

    for (auto w : windows)
    {
        if (w->active)
        {
            w->reference();
            w->ws->remove_overlay(*w);
            w->ws->add_overlay(*w);
            w->unreference();
            w->add_css_class("active");
        }
    }
}

void WayfireWorkspaceSwitcher::grid_render_views(wf::json_t views_data)
{
    for (size_t i = 0; i < views_data.size(); i++)
    {
        grid_add_view(views_data[i]);
    }

    for (auto w : windows)
    {
        if (w->gobj() == GTK_WIDGET(switch_grid.gobj()))
        {
            continue;
        }

        if (w->active)
        {
            w->reference();
            w->ws->remove_overlay(*w);
            w->ws->add_overlay(*w);
            w->unreference();
            w->add_css_class("active");
        }
    }
}

void WayfireWorkspaceSwitcher::on_event(wf::json_t data)
{
    if (workspace_switcher_mode.value() == "row")
    {
        switcher_on_event(data);
    } else // "grid"/"grid_popover"
    {
        grid_on_event(data);
    }
}

void WayfireWorkspaceSwitcher::switcher_on_event(wf::json_t data)
{
    if (data["event"].as_string() == "view-geometry-changed")
    {
        for (auto child : box.get_children())
        {
            WayfireWorkspaceBox *ws = (WayfireWorkspaceBox*)child;
            for (auto widget : child->get_children())
            {
                WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
                if (w->id == data["view"]["id"].as_int())
                {
                    ws->remove_overlay(*w);
                    auto elem = std::remove(windows.begin(), windows.end(), w);
                    windows.erase(elem, windows.end());
                }
            }
        }

        add_view(data["view"]);
    } else if (data["event"].as_string() == "view-mapped")
    {
        add_view(data["view"]);
    } else if ((data["event"].as_string() == "view-focused") && data["view"].is_object())
    {
        if (data["view"]["type"].as_string() != "toplevel")
        {
            return;
        }

        for (auto child : box.get_children())
        {
            for (auto widget : child->get_children())
            {
                WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
                if (w->id == data["view"]["id"].as_int())
                {
                    w->remove_css_class("inactive");
                    w->add_css_class("active");
                    w->active = true;
                    this->active_view_id = w->id;
                } else
                {
                    w->remove_css_class("active");
                    w->add_css_class("inactive");
                    w->active = false;
                }
            }
        }

        for (auto w : windows)
        {
            if (w->active)
            {
                w->reference();
                w->ws->remove_overlay(*w);
                w->ws->add_overlay(*w);
                w->unreference();
                w->add_css_class("active");
                this->active_view_id = w->id;
            }
        }
    } else if (data["event"].as_string() == "view-unmapped")
    {
        remove_view(data["view"]);
    } else if (data["event"].as_string() == "view-set-output")
    {
        add_view(data["view"]);
    } else if ((data["event"].as_string() == "output-layout-changed") ||
               (data["event"].as_string() == "wset-workspace-changed") ||
               (data["event"].as_string() == "view-minimized"))
    {
        get_wsets();
    }
}

void WayfireWorkspaceSwitcher::grid_on_event(wf::json_t data)
{
    if (data["event"].as_string() == "view-geometry-changed")
    {
        for (auto widget : overlay.get_children())
        {
            WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
            if (w->id == data["view"]["id"].as_int())
            {
                overlay.remove_overlay(*w);
                auto elem = std::remove(windows.begin(), windows.end(), w);
                windows.erase(elem, windows.end());
            }
        }

        grid_add_view(data["view"]);
    } else if (data["event"].as_string() == "view-mapped")
    {
        grid_add_view(data["view"]);
    } else if ((data["event"].as_string() == "view-focused") && data["view"].is_object())
    {
        if (data["view"]["type"].as_string() != "toplevel")
        {
            return;
        }

        for (auto widget : overlay.get_children())
        {
            if (widget->gobj() == GTK_WIDGET(switch_grid.gobj()))
            {
                continue;
            }

            WayfireWorkspaceWindow *w = (WayfireWorkspaceWindow*)widget;
            if (w->id == data["view"]["id"].as_int())
            {
                w->remove_css_class("inactive");
                w->add_css_class("active");
                w->active = true;
                this->active_view_id = w->id;
            } else
            {
                w->remove_css_class("active");
                w->add_css_class("inactive");
                w->active = false;
            }
        }

        for (auto w : windows)
        {
            if (w->gobj() == GTK_WIDGET(switch_grid.gobj()))
            {
                continue;
            }

            if (w->active)
            {
                w->reference();
                w->ws->remove_overlay(*w);
                w->ws->add_overlay(*w);
                w->unreference();
                w->add_css_class("active");
                this->active_view_id = w->id;
            }
        }
    } else if (data["event"].as_string() == "view-unmapped")
    {
        grid_remove_view(data["view"]);
    } else if (data["event"].as_string() == "view-set-output")
    {
        grid_add_view(data["view"]);
    } else if ((data["event"].as_string() == "output-layout-changed") ||
               (data["event"].as_string() == "wset-workspace-changed") ||
               (data["event"].as_string() == "view-minimized"))
    {
        get_wsets();
    }
}

WayfireWorkspaceSwitcher::WayfireWorkspaceSwitcher(WayfireOutput *output)
{
    this->output_name = output->monitor->get_connector();
    switcher_box.set_vexpand(false);
    switcher_box.set_valign(Gtk::Align::CENTER);
}

WayfireWorkspaceSwitcher::~WayfireWorkspaceSwitcher()
{
    if (ipc_client)
    {
        ipc_client->unsubscribe(this);
    }

    clear_box();
}
