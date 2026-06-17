/*
 * wf-shell power controller — shared utilities.
 *
 * is_root() and check_permission() are used by all platform backends.
 * They live here (compiled unconditionally) rather than duplicated per-platform.
 */

#include "power-controller.hpp"

#include <cstdlib>
#include <string>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool WFPowerController::is_root(void)
{
    return geteuid() == 0;
}

bool WFPowerController::check_permission(const char *command)
{
    if (is_root()) {
        return true;
    }

    if (!command) {
        return false;
    }

    /* Expand any shell tokens in the command string (e.g. $HOME). */
    wordexp_t we = {};
    int err = wordexp(command, &we, WRDE_NOCMD);
    if (err != 0) {
        return false;
    }

    if (we.we_wordc == 0) {
        wordfree(&we);
        return false;
    }

    std::string expanded;
    for (size_t i = 0; i < we.we_wordc; i++) {
        if (i > 0) {
            expanded += ' ';
        }
        expanded += we.we_wordv[i];
    }
    expanded += " --help";
    wordfree(&we);

    /* Redirect stderr so we only see the exit code. */
    expanded += " 2>/dev/null";

    int exit_code = system(expanded.c_str());
    return WIFEXITED(exit_code) && WEXITSTATUS(exit_code) < 128;
}
