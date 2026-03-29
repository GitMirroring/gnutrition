/* log.h - Food log for GNUtrition
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

#ifndef LOG_H
#define LOG_H

#include <sqlite3.h>
#include <stddef.h>

/* A single log entry.  */
struct log_entry
{
  int id;
  int food_code;
  char *description;
  char *date;      /* ISO 8601 date string YYYY-MM-DD  */
  double servings;
};

/* A dynamically-sized list of log entries.  */
struct log_list
{
  struct log_entry *items;
  size_t count;
  size_t capacity;
};

/* User profile for calorie estimation.  Stored in the log database
   so it persists across sessions.  The calorie_target is computed
   from the other fields via budget_estimate_calories() and cached
   here so the program can use it without re-prompting.  */
struct user_profile
{
  int age_years;
  double height_cm;
  double weight_kg;
  int activity_level;   /* enum activity_level from budget.h  */
  int calorie_target;   /* computed and rounded to pattern level  */
};

/* Open (or create) the user's food log database at PATH.
   Returns NULL on failure.  */
sqlite3 *log_open (const char *path);

/* Close the log database.  */
void log_close (sqlite3 *db);

/* Add a food to the log.  DATE should be in YYYY-MM-DD format.
   Returns 0 on success, -1 on error.  */
int log_add (sqlite3 *db, int food_code, const char *description,
             const char *date, double servings);

/* Delete a log entry by its ID.  Returns 0 on success, -1 on error.  */
int log_delete (sqlite3 *db, int id);

/* Update the servings and date of a log entry identified by ID.
   DATE should be in YYYY-MM-DD format.  Returns 0 on success, -1 on
   error.  */
int log_update (sqlite3 *db, int id, const char *date, double servings);

/* Retrieve all log entries for DATE.  Returns 0 on success, -1 on
   error.  The caller must free the result with log_list_free.  */
int log_get_day (sqlite3 *db, const char *date, struct log_list *results);

/* Free resources held by a log_list.  */
void log_list_free (struct log_list *list);

/* A dynamically-sized list of date strings (YYYY-MM-DD).  */
struct date_list
{
  char **dates;
  size_t count;
  size_t capacity;
};

/* Retrieve all distinct dates that have log entries, sorted in
   ascending order.  Returns 0 on success, -1 on error.  The caller
   must free the result with date_list_free.  */
int log_get_dates (sqlite3 *db, struct date_list *results);

/* Free resources held by a date_list.  */
void date_list_free (struct date_list *list);

/* Save the user's profile to the log database.  Only one profile
   row is stored; subsequent calls overwrite the previous one.
   Returns 0 on success, -1 on error.  */
int log_save_profile (sqlite3 *db, const struct user_profile *profile);

/* Load the user's profile from the log database.  Returns 0 on
   success, 1 if no profile exists yet, -1 on error.  */
int log_get_profile (sqlite3 *db, struct user_profile *profile);

#endif /* LOG_H */
