#include <iostream>

#include "outputwidget.hpp"
#include "stream-chooser.hpp"

WayfireChooserOutput::WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output) : output(output)
{
    append(contents);
    append(model);
    append(connector);
    set_size_request(150, 150);
    set_valign(Gtk::Align::FILL);
    set_halign(Gtk::Align::FILL);

    /* TODO Contents. We should probably grab screenshots of each output and display them */

    model.set_label(output->get_model());
    connector.set_label(output->get_connector());

    set_orientation(Gtk::Orientation::VERTICAL);

    output->signal_invalidate().connect([=]
    {
        WayfireStreamChooserApp::getInstance().remove_output(output->get_connector());
    });
}

void WayfireChooserOutput::print()
{
    std::cout << "Monitor: " << output->get_connector() << std::endl;
    exit(0);
}
