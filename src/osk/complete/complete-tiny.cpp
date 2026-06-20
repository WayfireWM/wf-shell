#include "complete-tiny.hpp"
#include "glibmm/main.h"
#include <iostream>
#include <llama.h>
#include <thread>
#include <vector>
#include "osk.hpp"


WayfireOskCompleteTinyLlama::WayfireOskCompleteTinyLlama()
{
    llama_backend_init();
    std::string model_path = llama_file;

    switch_language("en_US", "English (American)");

    auto mparams = llama_model_default_params();
    model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!model)
    {
        std::cerr << "failed loading model " << model_path << "\n";
        return;
    }
}

WayfireOskCompleteTinyLlama::~WayfireOskCompleteTinyLlama()
{
    if (model)
    {
        llama_model_free(model);
        model = nullptr;
    }

    llama_backend_free();
}

void WayfireOskCompleteTinyLlama::switch_language(std::string short_lang, std::string long_lang)
{
    system_prompt = "You are user facing spell checker and autocorrect. Language: " + long_lang +
        ". Do not say ether";
}

void WayfireOskCompleteTinyLlama::get_suggestions(const std::string surrounding_text,
    const std::string partial_word)
{
    std::thread worker([this, surrounding_text, partial_word] ()
    {
        std::vector<std::string> suggestions;
        if (!model)
        {
            return;
        }

        const llama_vocab *vocab = llama_model_get_vocab(model);
        if (!vocab)
        {
            return;
        }

        auto cparams = llama_context_default_params();
        cparams.n_ctx   = 512;
        cparams.n_batch = 512;
        llama_context *local_ctx = llama_init_from_model(model, cparams);
        if (!local_ctx)
        {
            return;
        }

        std::string prompt_instruction;
        if (partial_word.empty())
        {
            prompt_instruction = "What is the next word in this text: " +
                surrounding_text;
        } else
        {
            prompt_instruction =
                "Assuming the last word is not fully typed yet, what is the full last word of this text: " +
                surrounding_text;
        }

        std::string full_prompt = "<|system|>\n" + system_prompt + "</s>\n" +
            "<|user|>\n" + prompt_instruction;

        std::vector<llama_token> tokens;
        tokens.resize(full_prompt.size() + 4);
        int n_tokens =
            llama_tokenize(vocab, full_prompt.c_str(), full_prompt.size(), tokens.data(), tokens.size(), true,
                true);
        tokens.resize(n_tokens);

        if (tokens.empty())
        {
            llama_free(local_ctx);
            Glib::signal_idle().connect([] ()
            {
                WayfireOsk::get().get_window().clear_suggestions();
                return false;
            });
            return;
        }

        llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

        if (llama_decode(local_ctx, batch) == 0)
        {
            auto *logits = llama_get_logits_ith(local_ctx, batch.n_tokens - 1);
            int n_vocab  = llama_vocab_n_tokens(vocab);

            std::vector<std::pair<float, llama_token>> candidates;
            candidates.reserve(n_vocab);
            for (int id = 0; id < n_vocab; ++id)
            {
                candidates.push_back({logits[id], id});
            }

            std::sort(candidates.rbegin(), candidates.rend());
            /* Comparing uint & int, but suggestion_limit config xml says 0 <= suggestion_limit <= 100 */
            for (size_t i = 0; i < candidates.size() && suggestions.size() < suggestion_limit; ++i)
            {
                llama_token tok = candidates[i].second;
                if ((tok == llama_vocab_eos(vocab)) || (tok == llama_vocab_nl(vocab)))
                {
                    continue;
                }

                char buf[128] = {0};
                int len = llama_token_to_piece(vocab, tok, buf, sizeof(buf), 0, true);
                if (len > 0)
                {
                    std::string word(buf, len);
                    word.erase(std::remove(word.begin(), word.end(), '\n'), word.end());

                    Glib::ustring word_utf8 = word;

                    bool valid = false;
                    for (auto c : word_utf8)
                    {
                        if (g_unichar_isalpha(c))
                        {
                            valid = true;
                            break;
                        }
                    }

                    if (!valid)
                    {
                        continue;
                    }

                    auto start = word.find_first_not_of(" \t");
                    auto end   = word.find_last_not_of(" \t");
                    word = (start == std::string::npos) ? "" : word.substr(start, end - start + 1);

                    if (!word.empty() &&
                        (std::find(suggestions.begin(), suggestions.end(), word) == suggestions.end()))
                    {
                        suggestions.push_back(word);
                    }
                }
            }
        }

        llama_free(local_ctx);
        if (suggestions.size() > 0)
        {
            uint64_t current_seq_id = get_next_sequence_id();

            Glib::signal_idle().connect([suggestions, current_seq_id] ()
            {
                WayfireOsk::get().get_window().set_suggestions(suggestions, current_seq_id);
                return false;
            });
        }
    });
    worker.detach();
}
