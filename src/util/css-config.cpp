#include <css-config.hpp>
#include <wf-option-wrap.hpp>
#include <sstream>
#include <iostream>

void CssFromConfig::add_provider()
{
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(), provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void CssFromConfig::remove_provider()
{
    Gtk::StyleContext::remove_provider_for_display(Gdk::Display::get_default(), provider);
}

CssFromConfigBool::CssFromConfigBool(std::string option_name, std::string css_true, std::string css_false) :
    option_value{option_name}
{
    provider = Gtk::CssProvider::create();
    option_value.set_callback([=]
    {
        provider->load_from_string(option_value ? css_true : css_false);
    });

    add_provider();
}

CssFromConfigInt::CssFromConfigInt(std::string option_name, std::string css_before, std::string css_after) :
    option_value{option_name}
{
    provider = Gtk::CssProvider::create();
    option_value.set_callback([=]
    {
        // TODO When we go up to c++20 use std::format
        std::stringstream ss;
        ss << css_before << option_value << css_after;
        provider->load_from_string(ss.str());
    });
    std::stringstream ss;
    ss << css_before << option_value << css_after;
    provider->load_from_string(ss.str());

    add_provider();
}

CssFromConfigString::CssFromConfigString(std::string option_name, std::string css_before,
    std::string css_after) :
    option_value{option_name}
{
    provider = Gtk::CssProvider::create();
    option_value.set_callback([=] ()
    {
        // TODO When we go up to c++20 use std::format
        std::stringstream ss;
        ss << css_before << (std::string)option_value << css_after;
        provider->load_from_string(ss.str());
    });
    std::stringstream ss;
    ss << css_before << (std::string)option_value << css_after;
    provider->load_from_string(ss.str());

    add_provider();
}

CssFromConfigFont::CssFromConfigFont(std::string option_name, std::string css_before, std::string css_after) :
    option_value{option_name}
{
    provider = Gtk::CssProvider::create();
    option_value.set_callback([=] ()
    {
        std::stringstream ss;
        std::string font_name = (std::string)option_value;
        ss << css_before << "font: " << font_name << ";" << css_after;
        auto css = ss.str();
        provider->load_from_string(css);
    });

    std::stringstream ss;
    std::string font_name = (std::string)option_value;
    ss << css_before << "font: " << font_name << ";" << css_after;
    auto css = ss.str();
    provider->load_from_string(css);
    add_provider();
}
