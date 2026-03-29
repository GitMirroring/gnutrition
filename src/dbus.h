/* dbus.h - D-Bus interface for GNUtrition
   Copyright (C) 2026 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef DBUS_H
#define DBUS_H

#include <gio/gio.h>
#include <sqlite3.h>

/* Context for the D-Bus service, holding database handles.  */
struct dbus_context
{
  sqlite3 *food_db;
  sqlite3 *log_db;
  int calories;
};

/* Start the D-Bus service on the session bus.  CTX holds the
   database handles used by method implementations.  Returns the
   owner ID (> 0) on success, 0 on failure.  The caller must call
   dbus_service_stop to release resources.  */
guint dbus_service_start (struct dbus_context *ctx);

/* Stop the D-Bus service and release the bus name.  */
void dbus_service_stop (guint owner_id);

#endif /* DBUS_H */
