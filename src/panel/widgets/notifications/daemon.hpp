#ifndef NOTIFICATION_DAEMON_HPP
#define NOTIFICATION_DAEMON_HPP

#include "notification-info.hpp"
#include <glibmm/object.h>

#include <set>

namespace Daemon
{
enum CloseReason : guint32
{
    Expired = 1,
    Dismissed = 2,
    MethodCalled = 3,
    Undefined = 4,
};

using notification_signal = sigc::signal<void(Notification::id_type)>;
notification_signal signalNotificationNew();
notification_signal signalNotificationReplaced();
notification_signal signalNotificationClosed();

sigc::signal<void> signalDaemonStopped();

void start();
void stop();

const std::map<Notification::id_type, const Notification> &getNotifications();
void closeNotification(Notification::id_type id, CloseReason reason);
void invokeAction(Notification::id_type id, const std::string &action_key);
}; // namespace Daemon

#endif
