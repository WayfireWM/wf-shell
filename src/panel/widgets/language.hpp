#ifndef WIDGETS_LANGUAGE_HPP
#define WIDGETS_LANGUAGE_HPP

#include "../widget.hpp"
#include "gtkmm/button.h"
#include "wayfire-shell-unstable-v2-client-protocol.h"
#include <cstdint>
#include <gtkmm/calendar.h>
#include <gtkmm/label.h>
#include <string>
#include <vector>
// #include "wayfire-shell-unstable-v2-client-protocol.h"

struct Language
{
  std::string Name;
  std::string ID;
};

class WayfireLanguage : public WayfireWidget
{
    Gtk::Label label;
    Gtk::Button button;

    zwf_keyboard_lang_manager_v2 *keyboard_lang_manager;
    uint32_t current_language;
    std::vector<Language> available;

  public:
    void init(Gtk::HBox *container) override;
    bool update_label();
    void set_current(uint32_t index);
    void set_available(std::vector<Language> languages);
    void next_language();
    WayfireLanguage(zwf_keyboard_lang_manager_v2 *kbdlayout_manager);
    ~WayfireLanguage();
};

#endif /* end of include guard: WIDGETS_LANGUAGE_HPP */
