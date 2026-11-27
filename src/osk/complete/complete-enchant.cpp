#include "complete/complete-enchant.hpp"
#include "glibmm/main.h"
#include "osk.hpp"
#include <iostream>
#include <thread>

WayfireOskCompleteEnchant::WayfireOskCompleteEnchant()
{
    broker = enchant_broker_init();
}

WayfireOskCompleteEnchant::~WayfireOskCompleteEnchant()
{
    if (current_dict)
    {
        enchant_broker_free_dict(broker, current_dict);
    }

    enchant_broker_free(broker);
}

void WayfireOskCompleteEnchant::switch_language(std::string short_lang, std::string long_lang)
{
    if (short_lang == current_locale)
    {
        return;
    }

    if (current_dict)
    {
        enchant_broker_free_dict(broker, current_dict);
        current_dict = nullptr;
    }

    current_dict   = enchant_broker_request_dict(broker, short_lang.c_str());
    current_locale = short_lang;

    if (!current_dict)
    {
        std::cerr << "Warning: System dictionary missing for locale: " << short_lang << "\n";
        current_dict = nullptr;
        return;
    }

    std::cout << "Successfully loaded dictionary pipeline for: " << short_lang << "\n";
    return;
}

void WayfireOskCompleteEnchant::get_suggestions(const std::string surrounding, const std::string partial_word)
{
    auto layout = WayfireOsk::get().get_current_layout();
    /* Only suggest on alpabetical */
    if ((layout.compare("ansi") != 0) && (layout.compare("iso") != 0))
    {
        return;
    }

    std::thread worker([this, partial_word] ()
    {
        std::vector<std::string> results;

        if (current_dict == nullptr)
        {
            std::cout << "No dictionary set" << std::endl;
            return;
        }

        int valid = enchant_dict_check(current_dict, partial_word.c_str(), partial_word.length());
        if (valid == 0)
        {
            results.push_back(partial_word);
        }

        size_t total_suggestions = 0;
        char **suggest_list = enchant_dict_suggest(
            current_dict,
            partial_word.c_str(),
            partial_word.length(),
            &total_suggestions);
        if (suggest_list && (total_suggestions > 0))
        {
            /* Comparing uint & int, but suggestion_limit config xml says 0 <= suggestion_limit <= 100 */
            for (size_t i = 0; i < total_suggestions && results.size() < suggestion_limit; ++i)
            {
                if (suggest_list[i] != nullptr)
                {
                    auto suggest = std::string(suggest_list[i]);
                    if (suggest.length() > partial_word.length())
                    {
                        results.push_back(suggest);
                    }
                }
            }

            enchant_dict_free_string_list(current_dict, suggest_list);
        }

        if (results.size() > 0)
        {
            auto current_seq_id = get_next_sequence_id();
            Glib::signal_idle().connect([results, current_seq_id] ()
            {
                WayfireOsk::get().get_window().set_suggestions(results, current_seq_id);
                return false;
            });
        } else
        {
            std::cout << "No suggestions" << std::endl;
        }
    });

    worker.detach();
}
