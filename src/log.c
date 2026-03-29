/* log.c - Food log for GNUtrition
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

#include "log.h"
#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 32

sqlite3 *
log_open (const char *path)
{
  sqlite3 *db;
  int rc;
  char *errmsg;

  rc = sqlite3_open (path, &db);
  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: cannot open log database '%s': %s\n"),
               path, sqlite3_errmsg (db));
      sqlite3_close (db);
      return NULL;
    }

  /* Create the log table if it does not exist.  */
  rc = sqlite3_exec (db,
    "CREATE TABLE IF NOT EXISTS food_log ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  food_code INTEGER NOT NULL,"
    "  description TEXT NOT NULL,"
    "  date TEXT NOT NULL,"
    "  servings REAL NOT NULL DEFAULT 1.0"
    ")",
    NULL, NULL, &errmsg);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: cannot create log table: %s\n"), errmsg);
      sqlite3_free (errmsg);
      sqlite3_close (db);
      return NULL;
    }

  /* Create the profile table if it does not exist.  Only one row
     is ever stored (id = 1).  */
  rc = sqlite3_exec (db,
    "CREATE TABLE IF NOT EXISTS user_profile ("
    "  id INTEGER PRIMARY KEY CHECK (id = 1),"
    "  age_years INTEGER NOT NULL,"
    "  height_cm REAL NOT NULL,"
    "  weight_kg REAL NOT NULL,"
    "  activity_level INTEGER NOT NULL,"
    "  calorie_target INTEGER NOT NULL"
    ")",
    NULL, NULL, &errmsg);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: cannot create profile table: %s\n"),
               errmsg);
      sqlite3_free (errmsg);
      sqlite3_close (db);
      return NULL;
    }

  return db;
}

void
log_close (sqlite3 *db)
{
  if (db)
    sqlite3_close (db);
}

int
log_add (sqlite3 *db, int food_code, const char *description,
         const char *date, double servings)
{
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2 (db,
    "INSERT INTO food_log (food_code, description, date, servings) "
    "VALUES (?1, ?2, ?3, ?4)",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_int (stmt, 1, food_code);
  sqlite3_bind_text (stmt, 2, description, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (stmt, 3, date, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double (stmt, 4, servings);

  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: cannot add log entry: %s\n"),
               sqlite3_errmsg (db));
      return -1;
    }

  return 0;
}

int
log_delete (sqlite3 *db, int id)
{
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2 (db,
    "DELETE FROM food_log WHERE id = ?1",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_int (stmt, 1, id);

  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: cannot delete log entry: %s\n"),
               sqlite3_errmsg (db));
      return -1;
    }

  return 0;
}

int
log_update (sqlite3 *db, int id, const char *date, double servings)
{
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2 (db,
    "UPDATE food_log SET date = ?1, servings = ?2 WHERE id = ?3",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_text (stmt, 1, date, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double (stmt, 2, servings);
  sqlite3_bind_int (stmt, 3, id);

  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: cannot update log entry: %s\n"),
               sqlite3_errmsg (db));
      return -1;
    }

  return 0;
}

/* Append a log_entry to LIST, growing dynamically.  */
static int
log_list_append (struct log_list *list, int id, int food_code,
                 const char *description, const char *date, double servings)
{
  if (list->count >= list->capacity)
    {
      size_t new_cap;
      struct log_entry *tmp;

      new_cap = list->capacity == 0 ? INITIAL_CAPACITY : list->capacity * 2;
      tmp = realloc (list->items, new_cap * sizeof *tmp);
      if (!tmp)
        {
          fprintf (stderr, _("gnutrition: memory exhausted\n"));
          return -1;
        }
      list->items = tmp;
      list->capacity = new_cap;
    }

  list->items[list->count].id = id;
  list->items[list->count].food_code = food_code;
  list->items[list->count].description = strdup (description);
  list->items[list->count].date = strdup (date);
  if (!list->items[list->count].description
      || !list->items[list->count].date)
    {
      fprintf (stderr, _("gnutrition: memory exhausted\n"));
      return -1;
    }
  list->items[list->count].servings = servings;
  list->count++;
  return 0;
}

int
log_get_day (sqlite3 *db, const char *date, struct log_list *results)
{
  sqlite3_stmt *stmt;
  int rc;

  results->items = NULL;
  results->count = 0;
  results->capacity = 0;

  rc = sqlite3_prepare_v2 (db,
    "SELECT id, food_code, description, date, servings "
    "FROM food_log "
    "WHERE date = ?1 "
    "ORDER BY id",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_text (stmt, 1, date, -1, SQLITE_TRANSIENT);

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      int id = sqlite3_column_int (stmt, 0);
      int food_code = sqlite3_column_int (stmt, 1);
      const char *desc = (const char *) sqlite3_column_text (stmt, 2);
      const char *d = (const char *) sqlite3_column_text (stmt, 3);
      double servings = sqlite3_column_double (stmt, 4);
      if (log_list_append (results, id, food_code,
                           desc ? desc : "", d ? d : "", servings) < 0)
        {
          sqlite3_finalize (stmt);
          log_list_free (results);
          return -1;
        }
    }

  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      log_list_free (results);
      return -1;
    }

  return 0;
}

int
log_save_profile (sqlite3 *db, const struct user_profile *profile)
{
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2 (db,
    "INSERT OR REPLACE INTO user_profile "
    "(id, age_years, height_cm, weight_kg, activity_level, calorie_target) "
    "VALUES (1, ?1, ?2, ?3, ?4, ?5)",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_int (stmt, 1, profile->age_years);
  sqlite3_bind_double (stmt, 2, profile->height_cm);
  sqlite3_bind_double (stmt, 3, profile->weight_kg);
  sqlite3_bind_int (stmt, 4, profile->activity_level);
  sqlite3_bind_int (stmt, 5, profile->calorie_target);

  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: cannot save profile: %s\n"),
               sqlite3_errmsg (db));
      return -1;
    }

  return 0;
}

int
log_get_profile (sqlite3 *db, struct user_profile *profile)
{
  sqlite3_stmt *stmt;
  int rc;

  memset (profile, 0, sizeof *profile);

  rc = sqlite3_prepare_v2 (db,
    "SELECT age_years, height_cm, weight_kg, "
    "activity_level, calorie_target "
    "FROM user_profile WHERE id = 1",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  rc = sqlite3_step (stmt);

  if (rc == SQLITE_ROW)
    {
      profile->age_years = sqlite3_column_int (stmt, 0);
      profile->height_cm = sqlite3_column_double (stmt, 1);
      profile->weight_kg = sqlite3_column_double (stmt, 2);
      profile->activity_level = sqlite3_column_int (stmt, 3);
      profile->calorie_target = sqlite3_column_int (stmt, 4);
      sqlite3_finalize (stmt);
      return 0;
    }
  else if (rc == SQLITE_DONE)
    {
      sqlite3_finalize (stmt);
      return 1;  /* No profile saved yet.  */
    }
  else
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return -1;
    }
}

void
log_list_free (struct log_list *list)
{
  size_t i;
  if (!list)
    return;
  for (i = 0; i < list->count; i++)
    {
      free (list->items[i].description);
      free (list->items[i].date);
    }
  free (list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

int
log_get_dates (sqlite3 *db, struct date_list *results)
{
  sqlite3_stmt *stmt;
  int rc;

  results->dates = NULL;
  results->count = 0;
  results->capacity = 0;

  rc = sqlite3_prepare_v2 (db,
    "SELECT DISTINCT date FROM food_log ORDER BY date",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      const char *d = (const char *) sqlite3_column_text (stmt, 0);
      if (results->count >= results->capacity)
        {
          size_t new_cap;
          char **tmp;

          new_cap = results->capacity == 0
                    ? INITIAL_CAPACITY : results->capacity * 2;
          tmp = realloc (results->dates, new_cap * sizeof *tmp);
          if (!tmp)
            {
              fprintf (stderr, _("gnutrition: memory exhausted\n"));
              sqlite3_finalize (stmt);
              date_list_free (results);
              return -1;
            }
          results->dates = tmp;
          results->capacity = new_cap;
        }
      results->dates[results->count] = strdup (d ? d : "");
      if (!results->dates[results->count])
        {
          fprintf (stderr, _("gnutrition: memory exhausted\n"));
          sqlite3_finalize (stmt);
          date_list_free (results);
          return -1;
        }
      results->count++;
    }

  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      date_list_free (results);
      return -1;
    }

  return 0;
}

void
date_list_free (struct date_list *list)
{
  size_t i;
  if (!list)
    return;
  for (i = 0; i < list->count; i++)
    free (list->dates[i]);
  free (list->dates);
  list->dates = NULL;
  list->count = 0;
  list->capacity = 0;
}
