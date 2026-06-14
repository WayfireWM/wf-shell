#include "power-controller.hpp"
#include "platform.hpp"

#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

/* ─── Root check ─────────────────────────────────────────────────────────── */

bool WFPowerController::is_root(void)
{
    return geteuid() == 0;
}

/* ─── Permission check ───────────────────────────────────────────────────── */

/*
 * Check whether the current user can execute a command without elevation.
 * Tries to run `command --help 2>/dev/null` via wordexp expansion.
 * If that fails with EPERM/EACCES, the user lacks permission.
 *
 * Returns true if the command is executable without privilege.
 */
bool WFPowerController::check_permission(const char *command)
{
    if (is_root()) {
        return true;
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

/* ─── Linux implementation ──────────────────────────────────────────────── */

#if defined(WFS_PLATFORM_LINUX)

WFPowerController::Capability WFPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "systemctl poweroff";
        cap.permitted = check_permission("systemctl poweroff");
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "systemctl reboot";
        cap.permitted = check_permission("systemctl reboot");
        break;

    case Action::Suspend:
        cap.available = true;
        cap.command = "systemctl suspend";
        cap.permitted = check_permission("systemctl suspend");
        break;

    case Action::Hibernate:
        /* Hibernate is available only if systemd-hibernate is installed. */
        cap.available = check_permission("systemctl hibernate") ||
                        check_permission("hibernate");
        if (cap.available) {
            cap.command = "systemctl hibernate";
        }
        cap.permitted = cap.available;
        break;

    case Action::SwitchUser:
        /* Check for a running display manager that supports session switching. */
        cap.available = check_permission("loginctl show-session 2") ||
                        check_permission("dm-tool list-seats") ||
                        check_permission("gdm-control --list-sessions") ||
                        check_permission("kdmctl list sessions") ||
                        check_permission("sddm --theme list");
        if (cap.available) {
            /* dm-tool is LightDM's CLI — fall back to it if available. */
            if (check_permission("dm-tool switch-to-greeter")) {
                cap.command = "dm-tool switch-to-greeter";
            } else {
                cap.command = "";
            }
        }
        cap.permitted = cap.available;
        break;
    }

    return cap;
}

/* ─── FreeBSD implementation ────────────────────────────────────────────── */

#elif defined(WFS_PLATFORM_FREEBSD)

/* grp.h provides getgrnam(3). */
#include <grp.h>

static bool in_wheel_group(void)
{
    if (WFPowerController::is_root()) {
        return true;
    }
    gid_t wheel_gid = 0;
    struct group *gr = getgrnam("wheel");
    if (gr) {
        wheel_gid = gr->gr_gid;
    }
    if (wheel_gid == 0) {
        return false;
    }

    int ngroups = 0;
    gid_t groups[64];
    if (getgroups(0, nullptr) < 0) {
        return false;
    }
    getgroups(64, groups);
    for (int i = 0; i < 64; i++) {
        if (groups[i] == wheel_gid) {
            return true;
        }
    }
    return false;
}

WFPowerController::Capability WFPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "/sbin/shutdown -p now";
        cap.permitted = in_wheel_group();
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "/sbin/shutdown -r now";
        cap.permitted = in_wheel_group();
        break;

    case Action::Suspend:
        /* zzz is the FreeBSD suspend utility.  acpiconf -s 3 is the low-level
         * alternative.  Both require no special privilege on ACPI-capable
         * hardware. */
        cap.available = check_permission("zzz") || check_permission("acpiconf -s 3");
        if (cap.available) {
            cap.command = check_permission("zzz") ? "zzz" : "acpiconf -s 3";
        }
        cap.permitted = cap.available;
        break;

    case Action::Hibernate:
        /* FreeBSD does not support hibernation. */
        cap.available = false;
        cap.permitted = false;
        cap.command   = "";
        break;

    case Action::SwitchUser:
        /* No portable FreeBSD command for session switching.
         * Hide the button unless a display manager is detected. */
        cap.available = check_permission("dm-tool list-seats") ||
                        check_permission("gdm-control --list-sessions");
        if (cap.available) {
            cap.command = "dm-tool switch-to-greeter";
        }
        cap.permitted = cap.available;
        break;
    }

    return cap;
}

/* ─── OpenBSD implementation ────────────────────────────────────────────── */

#elif defined(WFS_PLATFORM_OPENBSD)

WFPowerController::Capability WFPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "/sbin/shutdown -p now";
        cap.permitted = is_root();
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "/sbin/shutdown -r now";
        cap.permitted = is_root();
        break;

    case Action::Suspend:
        /* OpenBSD uses zzz for suspend (aliased to apm -z). */
        cap.available = check_permission("zzz");
        if (cap.available) {
            cap.command = "zzz";
        }
        cap.permitted = cap.available;
        break;

    case Action::Hibernate:
        cap.available = false;
        break;

    case Action::SwitchUser:
        /* No portable OpenBSD command. */
        cap.available = false;
        break;
    }

    return cap;
}

/* ─── NetBSD implementation ─────────────────────────────────────────────── */

#elif defined(WFS_PLATFORM_NETBSD)

WFPowerController::Capability WFPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "/sbin/shutdown -p now";
        cap.permitted = is_root();
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "/sbin/shutdown -r now";
        cap.permitted = is_root();
        break;

    case Action::Suspend:
        /* NetBSD uses rtcws to enter suspend, or zzz if available. */
        cap.available = check_permission("zzz") || check_permission("rtcwake");
        if (cap.available) {
            cap.command = check_permission("zzz") ? "zzz" : "rtcwake -s mem";
        }
        cap.permitted = cap.available;
        break;

    case Action::Hibernate:
        /* NetBSD supports hibernation via suspend. */
        cap.available = check_permission("zzz") || check_permission("hibernate");
        if (cap.available) {
            cap.command = "zzz";
        }
        cap.permitted = cap.available;
        break;

    case Action::SwitchUser:
        cap.available = false;
        break;
    }

    return cap;
}

/* ─── Unknown platform ──────────────────────────────────────────────────── */

#else

WFPowerController::Capability WFPowerController::query(Action action)
{
    (void)action;
    return {};
}

#endif
