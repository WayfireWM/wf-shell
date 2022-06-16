#ifndef NOTIFICATION_DAEMON_HPP
#define NOTIFICATION_DAEMON_HPP

#include "notification-info.hpp"

#include <set>

class WayfireNotificationCenter;

namespace Daemon
{
void start(WayfireNotificationCenter *center);
void stop();
void connect(WayfireNotificationCenter *center);

const std::map<Notification::id_type, const Notification> &getNotifications();
void removeNotification(Notification::id_type id);
}; // namespace Daemon

#endif
