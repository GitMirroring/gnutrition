/* gui.h - GTK 3 user interface for GNUtrition
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

#ifndef GUI_H
#define GUI_H

#include <sqlite3.h>

/* Run the GTK 3 graphical interface.
   FOOD_DB is the USDA food database; LOG_DB is the user's food log.
   CALORIES is the initial daily calorie target.
   ARGC and ARGV are passed to gtk_init.
   Returns 0 on normal exit, -1 on error.  */
int gui_run (sqlite3 *food_db, sqlite3 *log_db, int calories,
             int argc, char **argv);

#endif /* GUI_H */
