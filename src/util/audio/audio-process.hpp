#pragma once

/**
 * Process / filesystem helpers for audio backends.
 *
 * Production code uses real fork+exec and access(2).
 * Unit tests install temporary hooks via ProcessHooks so every branch
 * of FreeBSD/Linux backends can be exercised without a live stack.
 *
 * TAOCP: keep side effects at the boundary; pure parsers live in
 * audio-parse.hpp and never call these.
 */

#include <functional>
#include <string>
#include <vector>

namespace wf_audio
{
namespace detail
{

/** Run argv[0] with args; capture combined stdout/stderr. */
bool run_capture(const std::vector<std::string>& argv, std::string& out, int& exit_code);

/** Split lines, trim CR. Pure. */
std::vector<std::string> split_lines(const std::string& s);

/** True if path exists (access F_OK). */
bool path_exists(const std::string& path);

/** Read entire text file; empty string on failure. */
std::string read_text_file(const std::string& path);

/**
 * Overridable I/O for unit tests. Null members fall back to real impl.
 * Reset with reset_process_hooks() after each test.
 */
struct ProcessHooks
{
    std::function<bool(const std::vector<std::string>&, std::string&, int&)> run_capture;
    std::function<bool(const std::string&)> path_exists;
    std::function<std::string(const std::string&)> read_text_file;
};

ProcessHooks& process_hooks();
void reset_process_hooks();

/** Real implementations (also used when hooks are empty). */
bool run_capture_real(const std::vector<std::string>& argv, std::string& out, int& exit_code);
bool path_exists_real(const std::string& path);
std::string read_text_file_real(const std::string& path);

} // namespace detail
} // namespace wf_audio
