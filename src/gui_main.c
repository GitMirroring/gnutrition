/* gui_main.c - Entry point for gnutrition-gtk
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "budget.h"
#include "db.h"
#include "gui.h"
#include "log.h"
#include "i18n.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PROGRAM_NAME "gnutrition-gtk"

/* Get the path to the user's log database.  Uses
   $XDG_DATA_HOME/gnutrition/log.db or ~/.local/share/gnutrition/log.db.
   Returns a dynamically-allocated string.  */
static char *
get_log_path (void)
{
  const char *data_home;
  char *path;
  size_t len;

  data_home = getenv ("XDG_DATA_HOME");
  if (data_home && data_home[0] != '\0')
    {
      len = strlen (data_home) + strlen ("/gnutrition/log.db") + 1;
      path = malloc (len);
      if (!path)
        {
          fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
          return NULL;
        }
      snprintf (path, len, "%s/gnutrition/log.db", data_home);
    }
  else
    {
      const char *home = getenv ("HOME");
      if (!home)
        {
          fprintf (stderr, _("%s: HOME is not set\n"), PROGRAM_NAME);
          return NULL;
        }
      len = strlen (home)
            + strlen ("/.local/share/gnutrition/log.db") + 1;
      path = malloc (len);
      if (!path)
        {
          fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
          return NULL;
        }
      snprintf (path, len, "%s/.local/share/gnutrition/log.db", home);
    }

  return path;
}

/* Ensure the directory containing PATH exists.  */
static int
ensure_dir (const char *path)
{
  char *slash;
  char *copy;

  copy = strdup (path);
  if (!copy)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
      return -1;
    }

  slash = strrchr (copy, '/');
  if (slash)
    {
      char *p;
      *slash = '\0';

      for (p = copy + 1; *p; p++)
        {
          if (*p == '/')
            {
              *p = '\0';
              if (mkdir (copy, 0755) < 0 && errno != EEXIST)
                {
                  fprintf (stderr,
                           _("%s: cannot create directory '%s': %s\n"),
                           PROGRAM_NAME, copy, strerror (errno));
                  free (copy);
                  return -1;
                }
              *p = '/';
            }
        }
      if (mkdir (copy, 0755) < 0 && errno != EEXIST)
        {
          fprintf (stderr, _("%s: cannot create directory '%s': %s\n"),
                   PROGRAM_NAME, copy, strerror (errno));
          free (copy);
          return -1;
        }
    }

  free (copy);
  return 0;
}

int
main (int argc, char **argv)
{
  char *db_path_alloc = NULL;
  const char *db_path = NULL;
  char *log_path;
  sqlite3 *food_db;
  sqlite3 *log_db;
  int calories;
  int exit_status;

  setlocale (LC_ALL, "");
  bindtextdomain ("gnutrition", LOCALEDIR);
  textdomain ("gnutrition");

/* Set default db_path */
  if (!db_path)
    {
      const char *home = getenv ("HOME");
      if (home)
        {
          size_t len = strlen (home) + strlen ("/.local/share/gnutrition/food.db") + 1;
          db_path_alloc = malloc (len);
          if (db_path_alloc)
            {
              snprintf (db_path_alloc, len, "%s/.local/share/gnutrition/food.db", home);
              db_path = db_path_alloc;
            }
        }
      if (!db_path)
        db_path = "food.db";
    }

  food_db = db_open (db_path);

  calories = 2000;

  /* Open the food database.  */
  food_db = db_open (db_path);
  if (!food_db)
    return 1;

  /* Open the user's log database.  */
  log_path = get_log_path ();
  if (!log_path)
    {
      db_close (food_db);
      return 1;
    }

  if (ensure_dir (log_path) < 0)
    {
      db_close (food_db);
      free (log_path);
      return 1;
    }

  log_db = log_open (log_path);
  if (!log_db)
    {
      db_close (food_db);
      free (log_path);
      return 1;
    }

  /* Load saved profile calorie target if available.  */
  {
    struct user_profile saved;
    int rc = log_get_profile (log_db, &saved);
    if (rc == 0 && saved.calorie_target > 0)
      calories = saved.calorie_target;
  }

  exit_status = gui_run (food_db, log_db, calories, argc, argv);

  log_close (log_db);
  db_close (food_db);
  free (log_path);

  return exit_status;
}
