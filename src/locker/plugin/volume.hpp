#pragma once

#include "multi-output-timed-plugin.hpp"
#include "widget-utils/pulseaudio/control.hpp"

class LockerVolumes : public Gtk::Box
{
    GvcControl volume, mic;

  public:
    LockerVolumes(const std::string& section);
};

class WayfireLockerVolumePluginWidget : public WayfireLockerTimedWidget<LockerVolumes>
{
  public:
    WayfireLockerVolumePluginWidget() :
        WayfireLockerTimedWidget("locker/volume_always")
    {}
};

class WayfireLockerVolumePlugin :
    public WayfireLockerMultiOutputPlugin<WayfireLockerVolumePluginWidget>
{
  private:
    sigc::connection signal;

  protected:
    std::shared_ptr<WayfireLockerVolumePluginWidget> create_widget() override
    {
        return std::make_shared<WayfireLockerVolumePluginWidget>();
    }

  public:
    WayfireLockerVolumePlugin();
};
