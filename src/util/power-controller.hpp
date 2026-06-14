#pragma once

#include <string>

/**
 * wf-shell power controller.
 *
 * Abstracts OS-specific power-management commands (shutdown, reboot, suspend,
 * hibernate, switch-user).  Callers query availability and permission before
 * showing or activating buttons.
 *
 * All OS-specific logic lives here — no #ifdefs in callers.
 */

class WFPowerController
{
  public:
    /** Power actions available in wf-shell. */
    enum class Action {
        Shutdown,
        Reboot,
        Suspend,
        Hibernate,
        SwitchUser,
    };

    /**
     * Describes whether a power action can be shown and executed.
     */
    struct Capability {
        bool available;   /**< True if the OS supports this action at all. */
        bool permitted;  /**< True if the current user can execute it. */

        /** Shell command to spawn.  Empty if !available. */
        std::string command;
    };

    /**
     * Query capabilities for a given action on the current platform.
     */
    static Capability query(Action action);

    /** Convenience: is the current user root? */
    static bool is_root(void);

  private:
    static bool check_permission(const char *command);
};
