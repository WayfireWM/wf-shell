#include <gtest/gtest.h>

#include "network/network-backend.hpp"
#include "platform.hpp"

/* ─── FreeBSDNetworkBackend construction ─────────────────────────────────── */

TEST(FreeBSDNetworkBackend, DefaultConstructs)
{
    FreeBSDNetworkBackend backend;
    /* Just verify it doesn't throw */
}

TEST(FreeBSDNetworkBackend, DevicesMapIsInitiallyEmptyExceptSentinel)
{
    FreeBSDNetworkBackend backend;
    auto& devices = backend.devices();
    /* Should contain the NullNetwork sentinel at "/" */
    EXPECT_EQ(devices.size(), 1u);
    EXPECT_NE(devices.find("/"), devices.end());
}

/* ─── FreeBSDNetworkBackend lifecycle ───────────────────────────────────── */

TEST(FreeBSDNetworkBackend, ConnectDoesNotThrow)
{
    FreeBSDNetworkBackend backend;
    EXPECT_NO_THROW(backend.connect());
}

TEST(FreeBSDNetworkBackend, DisconnectAfterConnectDoesNotThrow)
{
    FreeBSDNetworkBackend backend;
    backend.connect();
    EXPECT_NO_THROW(backend.disconnect());
}

TEST(FreeBSDNetworkBackend, DoubleDisconnectDoesNotThrow)
{
    FreeBSDNetworkBackend backend;
    backend.connect();
    backend.disconnect();
    EXPECT_NO_THROW(backend.disconnect());
}

/* ─── wf_platform_name() ─────────────────────────────────────────────────── */

TEST(PlatformName, IsNonNull)
{
    const char* name = wf_platform_name();
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST(PlatformName, IsKnownPlatform)
{
    const char* name = wf_platform_name();
    bool known = (std::strcmp(name, "linux") == 0) ||
                 (std::strcmp(name, "freebsd") == 0) ||
                 (std::strcmp(name, "openbsd") == 0) ||
                 (std::strcmp(name, "netbsd") == 0);
    EXPECT_TRUE(known) << "Platform name '" << name << "' is not recognised";
}
