#include <css-config.hpp>
#include <sstream>
#include <iostream>
#include <regex>

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
    this->css_before = css_before;
    this->css_after  = css_after;
    provider = Gtk::CssProvider::create();
    option_value.set_callback([=] ()
    {
        set_from_string();
    });

    set_from_string();
    add_provider();
}

void CssFromConfigFont::set_from_string()
{
    std::regex matcher("(.*?)(\\d+(?:pt|px|rem|em|))(.*?)");
    std::string font_name = (std::string)option_value;
    std::smatch matches;
    if (std::regex_match(font_name, matches, matcher))
    {
        std::ssub_match before = matches[1];
        std::string size = matches[2].str();
        std::ssub_match after = matches[3];

        std::string unit = "";

        // If we're using a bare number (ie 15) it now needs to be 15pt to match previous behaviour
        if ((size.find("px") == std::string::npos) &&
            (size.find("pt") == std::string::npos) &&
            (size.find("em") == std::string::npos))
        {
            unit = "pt";
        }

        std::stringstream ss;
        ss << css_before << "font: " << size << unit << " " << before.str() << " " << after.str() << ";" <<
            css_after;
        auto css = ss.str();
        provider->load_from_string(css);

        std::cout << "Font " << css << std::endl;
    } else
    {
        std::stringstream ss;
        ss << css_before << "font: 1rem " << font_name << ";" << css_after;
        auto css = ss.str();
        provider->load_from_string(css);
        std::cout << "Font fallback " << css << std::endl;
    }
}
