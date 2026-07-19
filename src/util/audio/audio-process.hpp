#pragma once

/** Small process helpers for audio backends (popen read, no shell). */

#include <string>
#include <vector>

namespace wf_audio
{
namespace detail
{

/** Run argv[0] with args; capture stdout. Returns false on spawn/failure. */
bool run_capture(const std::vector<std::string>& argv, std::string& out, int& exit_code);

/** Split lines, trim CR. */
std::vector<std::string> split_lines(const std::string& s);

} // namespace detail
} // namespace wf_audio
