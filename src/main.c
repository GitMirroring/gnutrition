/* main.c - Entry point for GNUtrition
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
#include "log.h"
#include "ui.h"
#include "i18n.h"

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PROGRAM_NAME "gnutrition"

static const struct option long_options[] =
{
  {"help",       no_argument,       NULL, 'h'},
  {"version",    no_argument,       NULL, 'V'},
  {"search",     required_argument, NULL, 's'},
  {"info",       required_argument, NULL, 'i'},
  {"log",        required_argument, NULL, 'l'},
  {"quantity",   required_argument, NULL, 'n'},
  {"delete",     required_argument, NULL, 'x'},
  {"edit",       required_argument, NULL, 'e'},
  {"budget",     no_argument,       NULL, 'b'},
  {"calories",   required_argument, NULL, 'c'},
  {"age",        required_argument, NULL, 'a'},
  {"height",     required_argument, NULL, 'H'},
  {"weight",     required_argument, NULL, 'w'},
  {"activity",   required_argument, NULL, 'A'},
  {"date",       required_argument, NULL, 'd'},
  {"db",         required_argument, NULL, 'D'},
  {"profile-db", required_argument, NULL, 'P'},
  {NULL, 0, NULL, 0}
};

static void
print_version (void)
{
  printf (_("GNUtrition %s\n"), PACKAGE_VERSION);
  printf (_("Copyright (C) 2026 Free Software Foundation, Inc.\n"));
  printf (_("License GPLv3+: GNU GPL version 3 or later "
          "<https://gnu.org/licenses/gpl.html>\n"));
  printf (_("This is free software: you are free to change "
          "and redistribute it.\n"));
  printf (_("There is NO WARRANTY, to the extent permitted by law.\n"));
}

static void
print_help (void)
{
  printf (_("Usage: %s [OPTION]...\n"), PROGRAM_NAME);
  printf (_("Track your daily food intake using the USDA Food Pattern "
          "budget system.\n\n"));
  printf (_("With no options, starts the interactive ncurses interface.\n\n"));
  printf (_("Options:\n"));
  printf (_("  -s, --search=QUERY   search for foods matching QUERY\n"));
  printf (_("  -i, --info=CODE      show nutrient info for food CODE\n"));
  printf (_("  -l, --log=CODE       log a food by its code\n"));
  printf (_("  -n, --quantity=NUM   number of servings to log "
          "(default: 1.0)\n"));
  printf (_("  -x, --delete=ID      delete a food log entry by its ID\n"));
  printf (_("  -e, --edit=ID        edit a food log entry (use with "
          "-n and/or -d)\n"));
  printf (_("  -b, --budget         show today's budget summary\n"));
  printf (_("  -c, --calories=KCAL  set daily calorie target "
          "(default: 2000)\n"));
  printf (_("  -a, --age=YEARS      your age in years "
          "(for calorie estimation)\n"));
  printf (_("  -H, --height=CM      your height in centimeters\n"));
  printf (_("  -w, --weight=KG      your weight in kilograms\n"));
  printf (_("  -A, --activity=LEVEL activity level: sedentary, light,\n"));
  printf (_("                         moderate, very-active, "
          "or extra-active\n"));
  printf (_("  -d, --date=DATE      date for log/budget "
          "(default: today)\n"));
  printf (_("  -D, --db=PATH        path to food database "
          "(default: $HOME/.local/share/gnutrition/food.db)\n"));
  printf (_("  -P, --profile-db=PATH\n"));
  printf (_("                       path to profile/log database\n"));
  printf (_("                         (default: "
          "$XDG_DATA_HOME/gnutrition/log.db)\n"));
  printf (_("  -h, --help           display this help and exit\n"));
  printf (_("  -V, --version        output version information and exit\n"));
  printf (_("\nWhen --age, --height, --weight, and --activity are all given,\n"));
  printf (_("the calorie target is estimated using the Mifflin-St Jeor\n"));
  printf (_("equation and saved to your profile for future sessions.\n"));
  printf (_("Use --calories to override the computed estimate.\n"));
  printf (_("\nThe calorie level determines your daily food-group budget\n"));
  printf (_("using the USDA Healthy US-Style Eating Pattern table\n"));
  printf (_("(Dietary Guidelines for Americans, 2020-2025, Appendix 3).\n"));
  printf (_("Values are rounded to the nearest 200 kcal pattern level\n"));
  printf (_("(range: 1000-3200).\n"));
  printf (_("\nYour profile and food log are stored in a separate database\n"));
  printf (_("at $XDG_DATA_HOME/gnutrition/log.db (the USDA food database\n"));
  printf (_("is never modified).\n"));
  printf (_("\nReport bugs to: bug-gnutrition@gnu.org\n"));
  printf (_("GNUtrition home page: "
          "<https://www.gnu.org/software/gnutrition/>\n"));
  printf (_("General help using GNU software: "
          "<https://www.gnu.org/gethelp/>\n"));
}

/* Parse an activity-level name.  Returns -1 if NAME is not
   recognized.  */
static int
parse_activity (const char *name)
{
  if (strcmp (name, "sedentary") == 0)
    return ACTIVITY_SEDENTARY;
  if (strcmp (name, "light") == 0)
    return ACTIVITY_LIGHT;
  if (strcmp (name, "moderate") == 0)
    return ACTIVITY_MODERATE;
  if (strcmp (name, "very-active") == 0)
    return ACTIVITY_VERY_ACTIVE;
  if (strcmp (name, "extra-active") == 0)
    return ACTIVITY_EXTRA_ACTIVE;
  return -1;
}

/* Return today's date as a dynamically-allocated string.  The caller
   must free the result.  Returns NULL on failure.  */
static char *
get_today (void)
{
  char *buf;
  time_t now;
  struct tm *tm;

  buf = malloc (11);
  if (!buf)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
      return NULL;
    }
  now = time (NULL);
  tm = localtime (&now);
  strftime (buf, 11, "%Y-%m-%d", tm);
  return buf;
}

/* Normalize a date string to ISO 8601 format (YYYY-MM-DD).  Accepts
   the locale's preferred date representation (%x).  Returns a
   dynamically-allocated string on success, or NULL on failure.  */
static char *
normalize_date (const char *input)
{
  struct tm tm;
  char *buf;

  buf = malloc (11);
  if (!buf)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
      return NULL;
    }

  memset (&tm, 0, sizeof tm);
  if (strptime (input, "%x", &tm) != NULL)
    {
      if (strftime (buf, 11, "%Y-%m-%d", &tm) > 0)
        return buf;
    }

  free (buf);
  return NULL;
}

/* Format an ISO 8601 date (YYYY-MM-DD) for display using the
   locale's preferred date representation.  Returns a pointer to a
   static buffer; not reentrant.  */
static const char *
format_date (const char *iso_date)
{
  static char buf[64];
  struct tm tm;

  memset (&tm, 0, sizeof tm);
  if (sscanf (iso_date, "%d-%d-%d",
              &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3)
    return iso_date;
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  if (strftime (buf, sizeof buf, "%x", &tm) == 0)
    return iso_date;
  return buf;
}

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
  char *dir;
  char *slash;
  char *copy;

  copy = strdup (path);
  if (!copy)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
      return -1;
    }

  /* Find the last slash to get the directory part.  */
  slash = strrchr (copy, '/');
  if (slash)
    {
      *slash = '\0';
      dir = copy;

      /* Build the directory path component by component.  */
      {
        char *p;
        for (p = dir + 1; *p; p++)
          {
            if (*p == '/')
              {
                *p = '\0';
                if (mkdir (dir, 0755) < 0 && errno != EEXIST)
                  {
                    fprintf (stderr, _("%s: cannot create directory '%s': %s\n"),
                             PROGRAM_NAME, dir, strerror (errno));
                    free (copy);
                    return -1;
                  }
                *p = '/';
              }
          }
        if (mkdir (dir, 0755) < 0 && errno != EEXIST)
          {
            fprintf (stderr, _("%s: cannot create directory '%s': %s\n"),
                     PROGRAM_NAME, dir, strerror (errno));
            free (copy);
            return -1;
          }
      }
    }

  free (copy);
  return 0;
}

/* Perform a CLI food search.  */
static int
cmd_search (sqlite3 *db, const char *query)
{
  struct food_list results;
  size_t i;

  if (db_search_foods (db, query, &results) < 0)
    return 1;

  if (results.count == 0)
    {
      printf (_("No foods found matching '%s'.\n"), query);
    }
  else
    {
      printf ("%-10s %s\n", _("Code"), _("Description"));
      printf ("%-10s %s\n", "----------",
              "--------------------------------------------");
      for (i = 0; i < results.count; i++)
        printf ("%-10d %s\n", results.items[i].food_code,
                results.items[i].description);
    }

  food_list_free (&results);
  return 0;
}

/* Show nutrient information for a food code, scaled by SERVINGS.  */
static int
cmd_info (sqlite3 *db, int food_code, double servings)
{
  struct nutrient_list nutrients;
  struct fped_entry fped;
  size_t i;

  if (db_get_nutrients (db, food_code, &nutrients) < 0)
    return 1;

  if (nutrients.count == 0)
    {
      printf (_("No nutrient data found for food code %d.\n"), food_code);
    }
  else
    {
      printf (_("Nutrient information for food code %d:\n\n"), food_code);
      printf ("%-40s %10s\n", _("Nutrient"), _("Value"));
      printf ("%-40s %10s\n",
              "----------------------------------------",
              "----------");
      for (i = 0; i < nutrients.count; i++)
        printf ("%-40s %10.2f\n", nutrients.items[i].name,
                nutrients.items[i].value * servings);
    }
  nutrient_list_free (&nutrients);

  /* Also show FPED budget cost if available.  */
  if (db_get_fped (db, food_code, &fped) == 0)
    {
      if (servings != 1.0)
        printf (_("\nFood Pattern Equivalents (per 100g x %.1f):\n"),
                servings);
      else
        printf (_("\nFood Pattern Equivalents (per 100g):\n"));
      printf (_("  Vegetables: %.2f cup-eq\n"), fped.vegetables * servings);
      printf (_("  Fruits:     %.2f cup-eq\n"), fped.fruits * servings);
      printf (_("  Grains:     %.2f oz-eq\n"), fped.grains * servings);
      printf (_("  Dairy:      %.2f cup-eq\n"), fped.dairy * servings);
      printf (_("  Protein:    %.2f oz-eq\n"), fped.protein * servings);
      printf (_("  Oils:       %.2f g\n"), fped.oils * servings);
    }

  return 0;
}

/* Show today's budget summary using the food log.  */
static int
cmd_budget (sqlite3 *food_db, sqlite3 *log_db, const char *date,
            int calories)
{
  struct daily_budget budget;
  struct daily_budget consumed;
  struct log_list entries;
  size_t i;

  budget = budget_for_calories (calories);
  memset (&consumed, 0, sizeof consumed);

  if (log_get_day (log_db, date, &entries) < 0)
    return 1;

  printf (_("Food log for %s:\n"), format_date (date));
  if (entries.count == 0)
    {
      printf (_("  (no entries)\n"));
    }
  else
    {
      for (i = 0; i < entries.count; i++)
        {
          struct fped_entry fped;
          printf (_("  %d - %s (%.1f servings)\n"),
                  entries.items[i].food_code,
                  entries.items[i].description,
                  entries.items[i].servings);
          if (db_get_fped (food_db, entries.items[i].food_code,
                           &fped) == 0)
            {
              double s = entries.items[i].servings;
              consumed.vegetables += fped.vegetables * s;
              consumed.fruits += fped.fruits * s;
              consumed.grains += fped.grains * s;
              consumed.dairy += fped.dairy * s;
              consumed.protein += fped.protein * s;
              consumed.oils += fped.oils * s;
            }
        }
    }

  log_list_free (&entries);

  printf ("\n");
  budget_print (&budget, &consumed);
  return 0;
}

int
main (int argc, char **argv)
{
  int c;
  char *db_path_alloc = NULL;
  const char *db_path = NULL;
  const char *search_query;
  const char *date;
  char *date_alloc;
  char *log_path;
  const char *log_path_explicit;
  int info_code;
  int log_code;
  double log_servings;
  int do_budget;
  int calories;
  int calories_explicit;  /* 1 if --calories was given  */
  int profile_age;
  double profile_height;
  double profile_weight;
  int profile_activity;
  int profile_given;  /* bitmask: 1=age, 2=height, 4=weight, 8=activity  */
  int mode;  /* 0 = interactive, 1 = search, 2 = info, 3 = log,
                4 = budget, 5 = delete, 6 = edit  */
  int delete_id;
  int edit_id;
  int edit_quantity_given;
  int edit_date_given;
  sqlite3 *food_db;
  sqlite3 *log_db;
  int exit_status;

  /* Initialize variables explicitly (GNU Coding Standards).  */
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

  search_query = NULL;
  date = NULL;
  date_alloc = NULL;
  log_path = NULL;
  log_path_explicit = NULL;
  info_code = 0;
  log_code = 0;
  log_servings = 1.0;
  do_budget = 0;
  calories = 2000;
  calories_explicit = 0;
  profile_age = 0;
  profile_height = 0.0;
  profile_weight = 0.0;
  profile_activity = ACTIVITY_SEDENTARY;
  profile_given = 0;
  mode = 0;
  delete_id = 0;
  edit_id = 0;
  edit_quantity_given = 0;
  edit_date_given = 0;
  food_db = NULL;
  log_db = NULL;
  exit_status = 0;

  while ((c = getopt_long (argc, argv, "hVs:i:l:n:x:e:bc:a:H:w:A:d:D:P:",
                           long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          print_help ();
          return 0;

        case 'V':
          print_version ();
          return 0;

        case 's':
          search_query = optarg;
          mode = 1;
          break;

        case 'i':
          info_code = atoi (optarg);
          mode = 2;
          break;

        case 'l':
          log_code = atoi (optarg);
          mode = 3;
          break;

        case 'n':
          {
            char *endp;
            errno = 0;
            log_servings = strtod (optarg, &endp);
            if (errno != 0 || endp == optarg || *endp != '\0'
                || log_servings <= 0.0)
              {
                fprintf (stderr,
                         _("%s: invalid quantity '%s'\n"),
                         PROGRAM_NAME, optarg);
                return 1;
              }
          }
          edit_quantity_given = 1;
          break;

        case 'x':
          delete_id = atoi (optarg);
          mode = 5;
          break;

        case 'e':
          edit_id = atoi (optarg);
          mode = 6;
          break;

        case 'b':
          do_budget = 1;
          mode = 4;
          break;

        case 'c':
          calories = budget_round_to_pattern (atoi (optarg));
          calories_explicit = 1;
          break;

        case 'a':
          profile_age = atoi (optarg);
          profile_given |= 1;
          break;

        case 'H':
          {
            char *endp;
            errno = 0;
            profile_height = strtod (optarg, &endp);
            if (errno != 0 || endp == optarg || *endp != '\0'
                || profile_height <= 0.0)
              {
                fprintf (stderr,
                         _("%s: invalid height '%s'\n"),
                         PROGRAM_NAME, optarg);
                return 1;
              }
          }
          profile_given |= 2;
          break;

        case 'w':
          {
            char *endp;
            errno = 0;
            profile_weight = strtod (optarg, &endp);
            if (errno != 0 || endp == optarg || *endp != '\0'
                || profile_weight <= 0.0)
              {
                fprintf (stderr,
                         _("%s: invalid weight '%s'\n"),
                         PROGRAM_NAME, optarg);
                return 1;
              }
          }
          profile_given |= 4;
          break;

        case 'A':
          profile_activity = parse_activity (optarg);
          if (profile_activity < 0)
            {
              fprintf (stderr, _("%s: unknown activity level '%s'\n"),
                       PROGRAM_NAME, optarg);
              fprintf (stderr, _("Valid levels: sedentary, light, moderate, "
                       "very-active, extra-active\n"));
              return 1;
            }
          profile_given |= 8;
          break;

        case 'd':
          date = optarg;
          edit_date_given = 1;
          break;

        case 'D':
          db_path = optarg;
          break;

        case 'P':
          log_path_explicit = optarg;
          break;

        default:
          fprintf (stderr, _("Try '%s --help' for more information.\n"),
                   PROGRAM_NAME);
          return 1;
        }
    }

  /* Default date to today if not specified.  */
  if (!date)
    {
      date_alloc = get_today ();
      if (!date_alloc)
        return 1;
      date = date_alloc;
    }
  else
    {
      date_alloc = normalize_date (date);
      if (!date_alloc)
        {
          fprintf (stderr, _("%s: invalid date: %s\n"), PROGRAM_NAME, date);
          return 1;
        }
      date = date_alloc;
    }

  /* Suppress unused variable warnings.  */
  (void) do_budget;

  /* Open the food database.  */
  food_db = db_open (db_path);
  if (!food_db)
    {
      free (date_alloc);
      return 1;
    }

  /* Open the user's food log / profile database.  */
  if (log_path_explicit)
    {
      log_path = strdup (log_path_explicit);
      if (!log_path)
        {
          fprintf (stderr, _("%s: memory exhausted\n"), PROGRAM_NAME);
          db_close (food_db);
          free (date_alloc);
          return 1;
        }
    }
  else
    {
      log_path = get_log_path ();
      if (!log_path)
        {
          db_close (food_db);
          free (date_alloc);
          return 1;
        }
    }

  if (ensure_dir (log_path) < 0)
    {
      db_close (food_db);
      free (log_path);
      free (date_alloc);
      return 1;
    }

  log_db = log_open (log_path);
  if (!log_db)
    {
      db_close (food_db);
      free (log_path);
      free (date_alloc);
      return 1;
    }

  /* If all profile fields were given on the command line, compute the
     calorie estimate, save the profile, and use the result (unless
     --calories was also given, which takes precedence).  */
  if (profile_given == 15)  /* all four bits set  */
    {
      struct user_profile prof;
      prof.age_years = profile_age;
      prof.height_cm = profile_height;
      prof.weight_kg = profile_weight;
      prof.activity_level = profile_activity;
      prof.calorie_target = budget_estimate_calories (profile_age,
                                                      profile_height,
                                                      profile_weight,
                                                      profile_activity);
      if (log_save_profile (log_db, &prof) < 0)
        fprintf (stderr, _("%s: warning: could not save profile\n"),
                 PROGRAM_NAME);
      else
        printf (_("Profile saved (estimated %d kcal/day).\n"),
                prof.calorie_target);

      if (!calories_explicit)
        calories = prof.calorie_target;
    }
  else if (profile_given != 0)
    {
      fprintf (stderr, _("%s: --age, --height, --weight, and --activity "
               "must all be given together\n"), PROGRAM_NAME);
      log_close (log_db);
      db_close (food_db);
      free (log_path);
      free (date_alloc);
      return 1;
    }
  else if (!calories_explicit)
    {
      /* No profile args and no --calories: load saved profile.  */
      struct user_profile saved;
      int rc = log_get_profile (log_db, &saved);
      if (rc == 0 && saved.calorie_target > 0)
        calories = saved.calorie_target;
    }

  switch (mode)
    {
    case 0:
      /* Interactive ncurses mode.  */
      exit_status = ui_run (food_db, log_db, calories) < 0 ? 1 : 0;
      break;

    case 1:
      exit_status = cmd_search (food_db, search_query);
      break;

    case 2:
      exit_status = cmd_info (food_db, info_code, log_servings);
      break;

    case 3:
      {
        /* Log a food: first look up the description.  */
        struct food_list results;
        char code_str[32];
        snprintf (code_str, sizeof code_str, "%d", log_code);

        /* Search by exact food code - we query for foods matching
           the code string but we'll match by code.  */
        if (db_search_foods (food_db, "", &results) == 0)
          {
            size_t j;
            const char *desc = _("Unknown food");
            for (j = 0; j < results.count; j++)
              {
                if (results.items[j].food_code == log_code)
                  {
                    desc = results.items[j].description;
                    break;
                  }
              }
            if (log_add (log_db, log_code, desc, date,
                        log_servings) < 0)
              exit_status = 1;
            else
              printf (_("Logged food %d (%s) x%.1f for %s.\n"),
                      log_code, desc, log_servings,
                      format_date (date));
            food_list_free (&results);
          }
        else
          {
            exit_status = 1;
          }
      }
      break;

    case 4:
      exit_status = cmd_budget (food_db, log_db, date, calories);
      break;

    case 5:
      /* Delete a log entry.  */
      if (log_delete (log_db, delete_id) < 0)
        exit_status = 1;
      else
        printf (_("Deleted log entry %d.\n"), delete_id);
      break;

    case 6:
      /* Edit a log entry.  At least -n or -d must be given.  */
      if (!edit_quantity_given && !edit_date_given)
        {
          fprintf (stderr,
                   _("%s: --edit requires --quantity and/or --date\n"),
                   PROGRAM_NAME);
          exit_status = 1;
        }
      else
        {
          /* We need to fetch the current entry to fill in unchanged
             fields.  Use a simple query by iterating the date list
             and entries.  We look up the entry by ID.  */
          struct date_list dl;
          int found = 0;
          if (log_get_dates (log_db, &dl) == 0)
            {
              size_t di;
              for (di = 0; di < dl.count && !found; di++)
                {
                  struct log_list el;
                  if (log_get_day (log_db, dl.dates[di], &el) == 0)
                    {
                      size_t ei;
                      for (ei = 0; ei < el.count; ei++)
                        {
                          if (el.items[ei].id == edit_id)
                            {
                              double new_srv = edit_quantity_given
                                ? log_servings : el.items[ei].servings;
                              const char *new_date = edit_date_given
                                ? date : el.items[ei].date;
                              if (log_update (log_db, edit_id,
                                              new_date, new_srv) < 0)
                                exit_status = 1;
                              else
                                printf (_("Updated log entry %d "
                                          "(%.1f servings, %s).\n"),
                                        edit_id, new_srv,
                                        format_date (new_date));
                              found = 1;
                              break;
                            }
                        }
                      log_list_free (&el);
                    }
                }
              date_list_free (&dl);
            }
          if (!found && exit_status == 0)
            {
              fprintf (stderr,
                       _("%s: log entry %d not found\n"),
                       PROGRAM_NAME, edit_id);
              exit_status = 1;
            }
        }
      break;

    default:
      abort ();
    }

  log_close (log_db);
  db_close (food_db);
  free (log_path);
  free (date_alloc);
  return exit_status;
}
