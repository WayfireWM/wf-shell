#pragma once

#include <memory>
#include <string>

/**
 * wf-shell power controller.
 *
 * Abstracts OS-specific power-management commands (shutdown, reboot, suspend,
 * hibernate, switch-user).  Callers query availability and permission before
 * showing or activating buttons.
 *
 * WFPowerController is an abstract base class.  Instantiate via create() —
 * the factory returns the platform-specific subclass.
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

    virtual ~WFPowerController() = default;

    /**
     * Query capabilities for a given action on the current platform.
     */
    virtual Capability query(Action action) = 0;

    /**
     * Factory: instantiate the platform-specific controller.
     * The returned object is owned by the caller.
     */
    static std::unique_ptr<WFPowerController> create();

    /** Convenience: is the current user root? */
    static bool is_root(void);

    /**
     * Check whether the current user can execute a command without elevation.
     * Tries to run `command --help 2>/dev/null` via wordexp expansion.
     * If that fails with EPERM/EACCES, the user lacks permission.
     * Returns true if the command is executable without privilege.
     */
    static bool check_permission(const char *command);
};
