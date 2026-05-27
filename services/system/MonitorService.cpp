#include "services/system/MonitorService.h"

#include <dbus/dbus.h>
#include <iostream>
#include <thread>

bool MonitorService::isSessionIdle() {
  DBusError err;
  dbus_error_init(&err);
  DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return false;
  }
  DBusMessage *msg = dbus_message_new_method_call(
      "org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core",
      "org.gnome.Mutter.IdleMonitor", "GetIdletime");
  if (!msg) {
    dbus_connection_unref(conn);
    return false;
  }
  DBusMessage *reply =
      dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
  dbus_message_unref(msg);
  if (dbus_error_is_set(&err) || !reply) {
    dbus_error_free(&err);
    dbus_connection_unref(conn);
    return false;
  }
  dbus_uint64_t idle_ms = 0;
  if (dbus_message_get_args(reply, &err, DBUS_TYPE_UINT64, &idle_ms,
                            DBUS_TYPE_INVALID)) {
    dbus_message_unref(reply);
    dbus_connection_unref(conn);
    return (idle_ms > 60000);
  }
  dbus_error_free(&err);
  dbus_message_unref(reply);
  dbus_connection_unref(conn);
  return false;
}

void MonitorService::turnOnDisplay() {
  std::system("kscreen-doctor --dpms on 2>/dev/null");
}

void MonitorService::turnOffDisplay() {
  std::system("kscreen-doctor --dpms off 2>/dev/null");
}
