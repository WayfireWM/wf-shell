/*
 * wf-shell power controller — BSD implementations (FreeBSD / OpenBSD / NetBSD).
 *
 * FreeBSD:   /sbin/shutdown, zzz / acpiconf
 * OpenBSD:   /sbin/shutdown, zzz
 * NetBSD:    /sbin/shutdown, zzz / rtcwake
 */

#include "power-controller.hpp"

#include <unistd.h>

#if defined(WFS_PLATFORM_FREEBSD)

#include <grp.h>

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

    gid_t groups[64];
    int ngroups = getgroups(64, groups);
    if (ngroups < 0) {
        return false;
    }
    for (int i = 0; i < ngroups; i++) {
        if (groups[i] == wheel_gid) {
            return true;
        }
    }
    return false;
}

class FreeBSDPowerController : public WFPowerController
{
  public:
    Capability query(Action action) override;
};

WFPowerController::Capability FreeBSDPowerController::query(Action action)
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
        cap.available = WFPowerController::check_permission("zzz") ||
                        WFPowerController::check_permission("acpiconf -s 3");
        if (cap.available) {
            cap.command = WFPowerController::check_permission("zzz")
                              ? "zzz"
                              : "acpiconf -s 3";
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
        cap.available = WFPowerController::check_permission("dm-tool list-seats") ||
                        WFPowerController::check_permission("gdm-control --list-sessions");
        if (cap.available) {
            cap.command = "dm-tool switch-to-greeter";
        }
        cap.permitted = cap.available;
        break;
    }

    return cap;
}

std::unique_ptr<WFPowerController> WFPowerController::create()
{
    return std::make_unique<FreeBSDPowerController>();
}

#elif defined(WFS_PLATFORM_OPENBSD)

class OpenBSDPowerController : public WFPowerController
{
  public:
    Capability query(Action action) override;
};

WFPowerController::Capability OpenBSDPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "/sbin/shutdown -p now";
        cap.permitted = WFPowerController::is_root();
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "/sbin/shutdown -r now";
        cap.permitted = WFPowerController::is_root();
        break;

    case Action::Suspend:
        /* OpenBSD uses zzz for suspend (aliased to apm -z). */
        cap.available = WFPowerController::check_permission("zzz");
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

std::unique_ptr<WFPowerController> WFPowerController::create()
{
    return std::make_unique<OpenBSDPowerController>();
}

#elif defined(WFS_PLATFORM_NETBSD)

class NetBSDPowerController : public WFPowerController
{
  public:
    Capability query(Action action) override;
};

WFPowerController::Capability NetBSDPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "/sbin/shutdown -p now";
        cap.permitted = WFPowerController::is_root();
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "/sbin/shutdown -r now";
        cap.permitted = WFPowerController::is_root();
        break;

    case Action::Suspend:
        /* NetBSD uses rtcws to enter suspend, or zzz if available. */
        cap.available = WFPowerController::check_permission("zzz") ||
                        WFPowerController::check_permission("rtcwake");
        if (cap.available) {
            cap.command = WFPowerController::check_permission("zzz") ? "zzz" : "rtcwake -s mem";
        }
        cap.permitted = cap.available;
        break;

    case Action::Hibernate:
        /* NetBSD supports hibernation via suspend. */
        cap.available = WFPowerController::check_permission("zzz") ||
                        WFPowerController::check_permission("hibernate");
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

std::unique_ptr<WFPowerController> WFPowerController::create()
{
    return std::make_unique<NetBSDPowerController>();
}

#else

/* Stub for unknown platform — required so the translation unit compiles. */
std::unique_ptr<WFPowerController> WFPowerController::create()
{
    return nullptr;
}

#endif /* WFS_PLATFORM_* */
