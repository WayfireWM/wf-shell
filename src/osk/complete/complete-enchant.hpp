#pragma once
#include "complete.hpp"
#include "wf-option-wrap.hpp"
#include <string>
#include <enchant-2/enchant.h>

class WayfireOskCompleteEnchant : public WayfireOskComplete
{
  private:
    EnchantBroker *broker;
    EnchantDict *current_dict  = nullptr;
    std::string current_locale = "";
    WfOption<int> suggestion_limit{"osk/suggestion_limit"};

  public:
    WayfireOskCompleteEnchant();
    ~WayfireOskCompleteEnchant();

    void switch_language(std::string short_lang, std::string long_lang);
    void get_suggestions(const std::string surrounding, const std::string partial_word);
};
