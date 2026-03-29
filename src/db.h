/* db.h - Database access layer for GNUtrition
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

#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stddef.h>

/* A single food item from the FNDDS foods table.  */
struct food_item
{
  int food_code;
  char *description;
};

/* A dynamically-sized list of food items.  */
struct food_list
{
  struct food_item *items;
  size_t count;
  size_t capacity;
};

/* A single nutrient value for a food.  */
struct nutrient_info
{
  char *name;
  double value;
};

/* A dynamically-sized list of nutrient values.  */
struct nutrient_list
{
  struct nutrient_info *items;
  size_t count;
  size_t capacity;
};

/* FPED food pattern equivalents for a food (per 100g).  */
struct fped_entry
{
  int food_code;
  double vegetables;
  double fruits;
  double grains;
  double dairy;
  double protein;
  double oils;
};

/* Open the food database at PATH.  Returns NULL on failure, printing
   an error message to stderr.  */
sqlite3 *db_open (const char *path);

/* Close the database DB.  */
void db_close (sqlite3 *db);

/* Search for foods matching QUERY (case-insensitive substring match).
   Returns 0 on success, -1 on error.  The caller must free the result
   with food_list_free.  */
int db_search_foods (sqlite3 *db, const char *query,
                     struct food_list *results);

/* Look up nutrient information for FOOD_CODE.  Returns 0 on success,
   -1 on error.  The caller must free the result with
   nutrient_list_free.  */
int db_get_nutrients (sqlite3 *db, int food_code,
                      struct nutrient_list *results);

/* Look up FPED food pattern equivalents for FOOD_CODE.  Returns 0 on
   success, -1 on error, 1 if not found.  */
int db_get_fped (sqlite3 *db, int food_code, struct fped_entry *entry);

/* Free resources held by a food_list.  */
void food_list_free (struct food_list *list);

/* Free resources held by a nutrient_list.  */
void nutrient_list_free (struct nutrient_list *list);

#endif /* DB_H */
