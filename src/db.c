/* db.c - Database access layer for GNUtrition
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

#include "db.h"
#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64

sqlite3 *
db_open (const char *path)
{
  sqlite3 *db;
  int rc;

  /* Note: We rely on SQLite's internal handling of mmap. SQLite attempts
     to mmap and falls back to read/write if it fails, satisfying the
     requirement to handle filesystems that don't support mmap.  */
  rc = sqlite3_open_v2 (path, &db, SQLITE_OPEN_READONLY, NULL);
  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: cannot open database '%s': %s\n"),
               path, sqlite3_errmsg (db));
      sqlite3_close (db);
      return NULL;
    }
  return db;
}

void
db_close (sqlite3 *db)
{
  if (db)
    sqlite3_close (db);
}

/* Append a food_item to LIST, growing dynamically.  Returns 0 on
   success, -1 on allocation failure.  */
static int
food_list_append (struct food_list *list, int code, const char *desc)
{
  if (list->count >= list->capacity)
    {
      size_t new_cap;
      struct food_item *tmp;

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

  list->items[list->count].food_code = code;
  list->items[list->count].description = strdup (desc);
  if (!list->items[list->count].description)
    {
      fprintf (stderr, _("gnutrition: memory exhausted\n"));
      return -1;
    }
  list->count++;
  return 0;
}

int
db_search_foods (sqlite3 *db, const char *query, struct food_list *results)
{
  sqlite3_stmt *stmt;
  int rc;
  char *pattern;
  size_t qlen;

  results->items = NULL;
  results->count = 0;
  results->capacity = 0;

  /* Build a LIKE pattern: %query%.  */
  qlen = strlen (query);
  pattern = malloc (qlen + 3);
  if (!pattern)
    {
      fprintf (stderr, _("gnutrition: memory exhausted\n"));
      return -1;
    }
  pattern[0] = '%';
  memcpy (pattern + 1, query, qlen);
  pattern[qlen + 1] = '%';
  pattern[qlen + 2] = '\0';

  rc = sqlite3_prepare_v2 (db,
    "SELECT \"Food code\", \"Main food description\" "
    "FROM foods "
    "WHERE \"Main food description\" LIKE ?1 "
    "ORDER BY \"Main food description\" "
    "LIMIT 100",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      free (pattern);
      return -1;
    }

  sqlite3_bind_text (stmt, 1, pattern, -1, SQLITE_TRANSIENT);

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      int code = sqlite3_column_int (stmt, 0);
      const char *desc = (const char *) sqlite3_column_text (stmt, 1);
      if (food_list_append (results, code, desc ? desc : "") < 0)
        {
          sqlite3_finalize (stmt);
          free (pattern);
          food_list_free (results);
          return -1;
        }
    }

  sqlite3_finalize (stmt);
  free (pattern);

  if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      food_list_free (results);
      return -1;
    }

  return 0;
}

/* Append a nutrient_info to LIST, growing dynamically.  */
static int
nutrient_list_append (struct nutrient_list *list, const char *name,
                      double value)
{
  if (list->count >= list->capacity)
    {
      size_t new_cap;
      struct nutrient_info *tmp;

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

  list->items[list->count].name = strdup (name);
  if (!list->items[list->count].name)
    {
      fprintf (stderr, _("gnutrition: memory exhausted\n"));
      return -1;
    }
  list->items[list->count].value = value;
  list->count++;
  return 0;
}

/* Fatty acid shorthand-to-common-name mapping.  The keys are the
   cleaned column names (after newline removal) produced by the USDA
   FNDDS import.  */
static const struct { const char *key; const char *name; } fa_names[] =
{
  { "4:0(g)",       "Butanoic acid (4:0) (g)" },
  { "6:0(g)",       "Hexanoic acid (6:0) (g)" },
  { "8:0(g)",       "Octanoic acid (8:0) (g)" },
  { "10:0(g)",      "Decanoic acid (10:0) (g)" },
  { "12:0(g)",      "Dodecanoic acid (12:0) (g)" },
  { "14:0(g)",      "Tetradecanoic acid (14:0) (g)" },
  { "16:0(g)",      "Hexadecanoic acid (16:0) (g)" },
  { "18:0(g)",      "Octadecanoic acid (18:0) (g)" },
  { "16:1(g)",      "Hexadecenoic acid (16:1) (g)" },
  { "18:1(g)",      "Octadecenoic acid (18:1) (g)" },
  { "20:1(g)",      "Eicosenoic acid (20:1) (g)" },
  { "22:1(g)",      "Docosenoic acid (22:1) (g)" },
  { "18:2(g)",      "Octadecadienoic acid (18:2) (g)" },
  { "18:3(g)",      "Octadecatrienoic acid (18:3) (g)" },
  { "18:4(g)",      "Octadecatetraenoic acid (18:4) (g)" },
  { "20:4(g)",      "Eicosatetraenoic acid (20:4) (g)" },
  { "20:5 n-3(g)",  "EPA (20:5 n-3) (g)" },
  { "22:5 n-3(g)",  "DPA (22:5 n-3) (g)" },
  { "22:6 n-3(g)",  "DHA (22:6 n-3) (g)" },
};

#define NUM_FA_NAMES (sizeof fa_names / sizeof fa_names[0])

/* Build a clean nutrient name from a column name that may contain
   embedded newlines (an artifact of multi-line Excel headers).
   Fatty-acid shorthand codes are expanded to their common names.  */
static char *
clean_column_name (const char *raw)
{
  char *clean;
  size_t i, j, len;

  len = strlen (raw);
  clean = malloc (len + 1);
  if (!clean)
    {
      fprintf (stderr, _("gnutrition: memory exhausted\n"));
      return NULL;
    }

  for (i = 0, j = 0; i < len; i++)
    {
      if (raw[i] == '\n')
        continue;
      clean[j++] = raw[i];
    }
  clean[j] = '\0';

  /* Replace fatty acid shorthand with a readable name.  */
  for (i = 0; i < NUM_FA_NAMES; i++)
    {
      if (strcmp (clean, fa_names[i].key) == 0)
        {
          char *expanded = strdup (fa_names[i].name);
          if (!expanded)
            {
              fprintf (stderr, _("gnutrition: memory exhausted\n"));
              free (clean);
              return NULL;
            }
          free (clean);
          return expanded;
        }
    }

  return clean;
}

int
db_get_nutrients (sqlite3 *db, int food_code, struct nutrient_list *results)
{
  sqlite3_stmt *stmt;
  int rc, col, ncols;

  results->items = NULL;
  results->count = 0;
  results->capacity = 0;

  /* The nutrients table stores each nutrient as a separate column
     (wide format).  Columns 0-3 are metadata (Food code, description,
     WWEIA category number/description); nutrient data starts at
     column 4.  */
  rc = sqlite3_prepare_v2 (db,
    "SELECT * FROM nutrients WHERE \"Food code\" = ?1",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_int (stmt, 1, food_code);
  rc = sqlite3_step (stmt);

  if (rc == SQLITE_ROW)
    {
      ncols = sqlite3_column_count (stmt);
      for (col = 4; col < ncols; col++)
        {
          const char *raw_name = sqlite3_column_name (stmt, col);
          char *name = clean_column_name (raw_name ? raw_name : "");
          double value = sqlite3_column_double (stmt, col);
          if (!name
              || nutrient_list_append (results, name, value) < 0)
            {
              free (name);
              sqlite3_finalize (stmt);
              nutrient_list_free (results);
              return -1;
            }
          free (name);
        }
    }
  else if (rc != SQLITE_DONE)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return -1;
    }

  sqlite3_finalize (stmt);
  return 0;
}

int
db_get_fped (sqlite3 *db, int food_code, struct fped_entry *entry)
{
  sqlite3_stmt *stmt;
  int rc;

  memset (entry, 0, sizeof *entry);
  entry->food_code = food_code;

  /* The FPED table columns include aggregated food pattern categories.
     We sum the sub-categories into the six major groups.  */
  rc = sqlite3_prepare_v2 (db,
    "SELECT "
    "  COALESCE(\"V_TOTAL (cup eq)\", 0), "
    "  COALESCE(\"F_TOTAL (cup eq)\", 0), "
    "  COALESCE(\"G_TOTAL (oz eq)\", 0), "
    "  COALESCE(\"D_TOTAL (cup eq)\", 0), "
    "  COALESCE(\"PF_TOTAL (oz eq)\", 0), "
    "  COALESCE(\"OILS (grams)\", 0) "
    "FROM points "
    "WHERE FOODCODE = ?1 "
    "LIMIT 1",
    -1, &stmt, NULL);

  if (rc != SQLITE_OK)
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      return -1;
    }

  sqlite3_bind_int (stmt, 1, food_code);
  rc = sqlite3_step (stmt);

  if (rc == SQLITE_ROW)
    {
      entry->vegetables = sqlite3_column_double (stmt, 0);
      entry->fruits = sqlite3_column_double (stmt, 1);
      entry->grains = sqlite3_column_double (stmt, 2);
      entry->dairy = sqlite3_column_double (stmt, 3);
      entry->protein = sqlite3_column_double (stmt, 4);
      entry->oils = sqlite3_column_double (stmt, 5);
      sqlite3_finalize (stmt);
      return 0;
    }
  else if (rc == SQLITE_DONE)
    {
      sqlite3_finalize (stmt);
      return 1;  /* Not found.  */
    }
  else
    {
      fprintf (stderr, _("gnutrition: SQL error: %s\n"), sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return -1;
    }
}

void
food_list_free (struct food_list *list)
{
  size_t i;
  if (!list)
    return;
  for (i = 0; i < list->count; i++)
    free (list->items[i].description);
  free (list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

void
nutrient_list_free (struct nutrient_list *list)
{
  size_t i;
  if (!list)
    return;
  for (i = 0; i < list->count; i++)
    free (list->items[i].name);
  free (list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}
