#pragma once

#include <atomic>
#include <cstdint>
#include <string>

/* Abstract definition of an autocompleter
 *  get_suggestions takes text leading up to and a partial word.
 *  Both MAY be blank.
 *
 *  get_suggestions MUST spawn a worker to process data off-thread.
 *
 *  When it finishes processing it has two possible actions:
 *    - WayfireOsk::get().get_window().set_suggestions(results, current_seq_id);
 *    - WayfireOsk::get().get_window().clear_suggestions();
 *
 *  Furthermore, if you are sending a current_seq_id you MUST increment in beforehand. Sending suggestions
 *  with a lower seq_id than
 *  once already accepted will automatically throw it away believing it to be out of date
 */

class WayfireOskComplete
{
  protected:
    std::atomic<uint64_t> sequence_counter{0};
    uint64_t get_next_sequence_id()
    {
        return ++sequence_counter;
    }

  public:
    WayfireOskComplete()
    {}
    ~WayfireOskComplete()
    {}
    virtual void switch_language(std::string short_lang, std::string long_lang) = 0;
    virtual void get_suggestions(const std::string surrounding_text, const std::string partial_word) = 0;
};

class WayfireOskCompleteNull : public WayfireOskComplete
{
    void switch_language(std::string, std::string) override
    {}
    void get_suggestions(const std::string surrounding, const std::string partial_word) override
    {}
};
