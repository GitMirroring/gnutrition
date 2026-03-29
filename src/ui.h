/* ui.h - ncurses user interface for GNUtrition
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

#ifndef UI_H
#define UI_H

#include <sqlite3.h>

/* Run the interactive ncurses interface.
   FOOD_DB is the USDA food database; LOG_DB is the user's food log.
   CALORIES is the daily calorie target for the food-group budget.
   Returns 0 on normal exit, -1 on error.  */
int ui_run (sqlite3 *food_db, sqlite3 *log_db, int calories);

#endif /* UI_H */
