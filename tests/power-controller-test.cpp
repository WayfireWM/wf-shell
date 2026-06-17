#include <gtest/gtest.h>

#include "power-controller.hpp"

/* ─── WFPowerController::create() ────────────────────────────────────────── */

TEST(PowerControllerCreate, ReturnsNonNull)
{
    auto ctrl = WFPowerController::create();
    EXPECT_NE(ctrl, nullptr) << "create() must return a non-null pointer on this platform";
}

/* ─── WFPowerController::is_root() ───────────────────────────────────────── */

TEST(PowerControllerIsRoot, ReturnsBool)
{
    bool result = WFPowerController::is_root();
    /* Just verify it doesn't crash and returns a bool */
    EXPECT_TRUE(result == true || result == false);
}

/* ─── WFPowerController::check_permission() ──────────────────────────────── */

TEST(PowerControllerCheckPermission, NullptrReturnsFalse)
{
    /* A nullptr command should return false */
    bool result = WFPowerController::check_permission(nullptr);
    EXPECT_FALSE(result);
}

TEST(PowerControllerCheckPermission, EmptyCommandReturnsFalse)
{
    bool result = WFPowerController::check_permission("");
    EXPECT_FALSE(result);
}

/* ─── WFPowerController::query() ─────────────────────────────────────────── */

class PowerControllerQuery : public ::testing::Test
{
  protected:
    std::unique_ptr<WFPowerController> ctrl;

    void SetUp() override
    {
        ctrl = WFPowerController::create();
        ASSERT_NE(ctrl, nullptr);
    }
};

TEST_F(PowerControllerQuery, ShutdownCapabilityIsValid)
{
    auto cap = ctrl->query(WFPowerController::Action::Shutdown);
    EXPECT_TRUE(cap.available) << "Shutdown should be available on all supported platforms";
}

TEST_F(PowerControllerQuery, RebootCapabilityIsValid)
{
    auto cap = ctrl->query(WFPowerController::Action::Reboot);
    EXPECT_TRUE(cap.available) << "Reboot should be available on all supported platforms";
}

TEST_F(PowerControllerQuery, SuspendCapabilityIsValid)
{
    auto cap = ctrl->query(WFPowerController::Action::Suspend);
    EXPECT_TRUE(cap.available) << "Suspend should be available on all supported platforms";
    if (cap.available) {
        EXPECT_EQ(cap.permitted, !cap.command.empty())
            << "permitted should match whether a command was found";
    }
}

TEST_F(PowerControllerQuery, HibernateCapabilityIsValid)
{
    auto cap = ctrl->query(WFPowerController::Action::Hibernate);
    /* Hibernate may or may not be available depending on platform */
    if (cap.available) {
        EXPECT_FALSE(cap.command.empty()) << "command must be set when available";
    }
}

TEST_F(PowerControllerQuery, SwitchUserCapabilityIsValid)
{
    auto cap = ctrl->query(WFPowerController::Action::SwitchUser);
    /* SwitchUser may or may not be available */
    if (cap.available) {
        EXPECT_FALSE(cap.command.empty()) << "command must be set when available";
        EXPECT_TRUE(cap.permitted) << "permitted should be true when available";
    }
}

TEST_F(PowerControllerQuery, AllActionsReturnValidCapability)
{
    auto actions = {
        WFPowerController::Action::Shutdown,
        WFPowerController::Action::Reboot,
        WFPowerController::Action::Suspend,
        WFPowerController::Action::Hibernate,
        WFPowerController::Action::SwitchUser,
    };
    for (auto action : actions) {
        auto cap = ctrl->query(action);
        EXPECT_FALSE(cap.permitted && !cap.available)
            << "permitted can only be true when available is true";
        if (!cap.available) {
            EXPECT_TRUE(cap.command.empty()) << "command must be empty when not available";
        }
    }
}
