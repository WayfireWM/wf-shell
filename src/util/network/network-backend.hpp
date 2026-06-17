#pragma once

#include <memory>
#include <sigc++/connection.h>

#include "freebsd-network.hpp"

/**
 * NetworkBackend — abstract interface for platform-specific network backends.
 *
 * wf-shell supports two backends:
 *  - Linux / etc.: NetworkManager over D-Bus (event-driven)
 *  - FreeBSD:        ifconfig(8) polling via getifaddrs(3)
 *
 * The factory in NetworkManager's constructor creates the appropriate backend.
 */
class NetworkBackend
{
  public:
    using SignalDeviceAdded   = sigc::signal<void (std::shared_ptr<Network>)>;
    using SignalDeviceRemoved = sigc::signal<void (std::shared_ptr<Network>)>;
    using SignalNmStart       = sigc::signal<void (void)>;
    using SignalNmStop        = sigc::signal<void (void)>;
    using DeviceMap           = std::map<std::string, std::shared_ptr<Network>>;

    virtual ~NetworkBackend() = default;

    /** Connect / initialise the backend.  Starts D-Bus watching or polling. */
    virtual void connect() = 0;

    /** Stop the backend.  Called by NetworkManager's destructor. */
    virtual void disconnect() = 0;

    /**
     * Returns a reference to the current device map.
     * Callers read from it to sync their own view.
     */
    virtual DeviceMap& devices() = 0;

    /** Signal: emitted when a network device appears. */
    SignalDeviceAdded signal_device_added;

    /** Signal: emitted when a network device disappears. */
    SignalDeviceRemoved signal_device_removed;

    /** Signal: emitted when NetworkManager starts (Linux only). */
    SignalNmStart signal_nm_start;

    /** Signal: emitted when NetworkManager stops (Linux only). */
    SignalNmStop signal_nm_stop;
};

/**
 * FreeBSDNetworkBackend — ifconfig-based polling backend for FreeBSD.
 *
 * Uses getifaddrs(3) to detect IPv4 interfaces and exposes them as
 * FreeBSDNetwork objects.  Polls every 3 seconds.
 */
class FreeBSDNetworkBackend : public NetworkBackend
{
  public:
    FreeBSDNetworkBackend();
    void connect() override;
    void disconnect() override;
    DeviceMap& devices() override { return all_devices; }

  private:
    void refresh_devices();

    DeviceMap all_devices;
    sigc::connection poll_timer;
};
