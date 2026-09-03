#pragma once

#include <unordered_map>
#include <memory>

#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "lockergrid.hpp"

template<class WidgetT>
class WayfireLockerMultiOutputPlugin : public WayfireLockerPlugin
{
  protected:
    std::unordered_map<std::string, std::shared_ptr<WidgetT>> widgets;
    bool shown = true;

    virtual std::shared_ptr<WidgetT> create_widget() = 0;

  public:
    WayfireLockerMultiOutputPlugin(std::string prefix) :
        WayfireLockerPlugin(prefix)
    {}

    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override
    {
        auto widget = create_widget();
        widgets.emplace(id, widget);

        if (!shown)
        {
            widget->hide();
        }

        grid->attach(*widget, position);
    }

    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override
    {
        auto it = widgets.find(id);
        if (it == widgets.end())
        {
            return;
        }

        grid->remove(*it->second);
        widgets.erase(it);
    }

    void hide()
    {
        shown = false;
        for (auto& it : widgets)
        {
            it.second->hide();
        }
    }

    void show()
    {
        shown = true;
        for (auto& it : widgets)
        {
            it.second->show();
        }
    }

    void init() override
    {}
    void deinit() override
    {}
};

template<class InnerWidgetT>
class WayfireLockerTimedWidget : public WayfireLockerTimedRevealer
{
  public:
    InnerWidgetT inner;

    WayfireLockerTimedWidget(std::string always_option, const char *css_class = nullptr) :
        WayfireLockerTimedRevealer(always_option),
        inner("locker")
    {
        set_child(inner);
        if (css_class != nullptr)
        {
            inner.add_css_class(css_class);
        }
    }
};
