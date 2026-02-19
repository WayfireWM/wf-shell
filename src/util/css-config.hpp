#pragma once

#include <string>
#include <glibmm.h>
#include <gtkmm.h>
#include "wf-option-wrap.hpp"

class CssFromConfig
{
  public:
    Glib::RefPtr<Gtk::CssProvider> provider;

    void add_provider();
    void remove_provider();
};

class CssFromConfigDouble : public CssFromConfig
{
    WfOption<double> option_value;

  public:
    CssFromConfigDouble(std::string config_opt, std::string css_before, std::string css_after);
};

class CssFromConfigBool : public CssFromConfig
{
    WfOption<bool> option_value;

  public:
    CssFromConfigBool(std::string config_opt, std::string css_true, std::string css_false);
};

class CssFromConfigIconSize : public CssFromConfig
{
    WfOption<int> option_value;

  public:
    CssFromConfigIconSize(std::string option_name, std::string css_class);
};

class CssFromConfigString : public CssFromConfig
{
    WfOption<std::string> option_value;

  public:
    CssFromConfigString(std::string config_opt, std::string css_before, std::string css_after);
};

class CssFromConfigFont : public CssFromConfig
{
    WfOption<std::string> option_value;
    std::string css_before, css_after;

  public:
    CssFromConfigFont(std::string config_opt, std::string css_before, std::string css_after);
    void set_from_string();
};

class CssFromConfigInt : public CssFromConfig
{
    WfOption<int> option_value;

  public:
    CssFromConfigInt(std::string config_opt, std::string css_before, std::string css_after);
};
