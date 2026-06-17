/*
 * wf-shell power controller — Linux implementation.
 *
 * Uses systemctl(1) for shutdown, reboot, suspend, hibernate.
 * Uses loginctl(1), dm-tool(1), gdm-control(1), kdmctl(1), sddm(1)
 * for session/switch-user.
 */

#include "power-controller.hpp"

#if defined(WFS_PLATFORM_LINUX)

class LinuxPowerController : public WFPowerController
{
  public:
    Capability query(Action action) override;
};

WFPowerController::Capability LinuxPowerController::query(Action action)
{
    Capability cap{.available = false, .permitted = false, .command = ""};

    switch (action) {
    case Action::Shutdown:
        cap.available = true;
        cap.command = "systemctl poweroff";
        cap.permitted = WFPowerController::check_permission("systemctl poweroff");
        break;

    case Action::Reboot:
        cap.available = true;
        cap.command = "systemctl reboot";
        cap.permitted = WFPowerController::check_permission("systemctl reboot");
        break;

    case Action::Suspend:
        cap.available = true;
        cap.command = "systemctl suspend";
        cap.permitted = WFPowerController::check_permission("systemctl suspend");
        break;

    case Action::Hibernate:
        /* Hibernate is available only if systemd-hibernate is installed. */
        cap.available = WFPowerController::check_permission("systemctl hibernate") ||
                        WFPowerController::check_permission("hibernate");
        if (cap.available) {
            cap.command = "systemctl hibernate";
        }
        cap.permitted = cap.available;
        break;

    case Action::SwitchUser:
        /* Check for a running display manager that supports session switching. */
        cap.available = WFPowerController::check_permission("loginctl show-session 2") ||
                        WFPowerController::check_permission("dm-tool list-seats") ||
                        WFPowerController::check_permission("gdm-control --list-sessions") ||
                        WFPowerController::check_permission("kdmctl list sessions") ||
                        WFPowerController::check_permission("sddm --theme list");
        if (cap.available) {
            /* dm-tool is LightDM's CLI — fall back to it if available. */
            if (WFPowerController::check_permission("dm-tool switch-to-greeter")) {
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

std::unique_ptr<WFPowerController> WFPowerController::create()
{
    return std::make_unique<LinuxPowerController>();
}

#endif /* WFS_PLATFORM_LINUX */
