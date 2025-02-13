#include <string>
#include <glibmm.h>
#include <gtkmm.h>
#include <wf-option-wrap.hpp>

class CssFromConfig {
    public:
        Glib::RefPtr<Gtk::CssProvider> provider;

        void add_provider();
        void remove_provider();
};

class CssFromConfigBool : public CssFromConfig{
    WfOption<bool> option_value;
    public:
    CssFromConfigBool(std::string config_opt, std::string css_true, std::string css_false);
};

class CssFromConfigString : public CssFromConfig{
    WfOption<std::string> option_value;
    public:
    CssFromConfigString(std::string config_opt, std::string css_before, std::string css_after);
};

class CssFromConfigInt : public CssFromConfig{
    WfOption<int> option_value;
    public:
    CssFromConfigInt(std::string config_opt, std::string css_before, std::string css_after);
};