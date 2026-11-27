#pragma once

#include "complete/complete.hpp"
#include "wf-option-wrap.hpp"
#include <llama.h>
#include <string>
class WayfireOskCompleteTinyLlama : public WayfireOskComplete
{
  private:
    llama_model *model = nullptr;
    std::string system_prompt = "";
    WfOption<std::string> llama_file{"osk/llama_file"};
    WfOption<int> suggestion_limit{"osk/suggestion_limit"};

  public:
    WayfireOskCompleteTinyLlama();
    ~WayfireOskCompleteTinyLlama();

    void switch_language(std::string short_land, std::string long_land) override;
    void get_suggestions(const std::string surrounding_text, const std::string partial_word) override;
};
