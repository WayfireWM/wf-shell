#include "volume.hpp"

LockerVolumes::LockerVolumes(const std::string& section) :
    volume(StreamRole::Sink, section),
    mic(StreamRole::Source, section)
{
    append(volume);
    volume.set_orientation(Gtk::Orientation::VERTICAL);
    append(mic);
    mic.set_orientation(Gtk::Orientation::VERTICAL);
}

WayfireLockerVolumePlugin::WayfireLockerVolumePlugin() :
    WayfireLockerMultiOutputPlugin("locker/volume")
{}
