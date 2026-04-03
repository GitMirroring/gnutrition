/* budget.c - USDA Food Pattern budget system for GNUtrition
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

#include "budget.h"
#include "i18n.h"

#include <math.h>
#include <stdio.h>

/* USDA Healthy US-Style Eating Pattern table.
   Source: Dietary Guidelines for Americans, 2020-2025, Appendix 3.
   Columns: kcal, vegetables (cup-eq), fruits (cup-eq),
            grains (oz-eq), dairy (cup-eq), protein (oz-eq),
            oils (grams).  */
static const struct daily_budget usda_table[] =
{
  { 1000, 1.0, 1.0, 3.0, 2.0, 2.0, 13.0 },
  { 1200, 1.5, 1.0, 4.0, 2.0, 2.0, 17.0 },
  { 1400, 1.5, 1.5, 5.0, 2.5, 4.0, 17.0 },
  { 1600, 2.0, 1.5, 5.0, 2.5, 5.0, 22.0 },
  { 1800, 2.5, 1.5, 6.0, 3.0, 5.0, 24.0 },
  { 2000, 2.5, 2.0, 6.0, 3.0, 5.5, 27.0 },
  { 2200, 3.0, 2.0, 7.0, 3.0, 6.0, 29.0 },
  { 2400, 3.0, 2.0, 8.0, 3.0, 6.5, 32.0 },
  { 2600, 3.5, 2.0, 9.0, 3.0, 6.5, 34.0 },
  { 2800, 3.5, 2.5, 10.0, 3.0, 7.0, 36.0 },
  { 3000, 4.0, 2.5, 10.0, 3.0, 7.0, 40.0 },
  { 3200, 4.0, 2.5, 10.0, 3.0, 7.0, 44.0 },
};

#define TABLE_SIZE (sizeof usda_table / sizeof usda_table[0])

/* Activity-factor multipliers (Mifflin-St Jeor convention).  */
static const double activity_factors[] =
{
  1.2,     /* ACTIVITY_SEDENTARY    */
  1.375,   /* ACTIVITY_LIGHT        */
  1.55,    /* ACTIVITY_MODERATE     */
  1.725,   /* ACTIVITY_VERY_ACTIVE  */
  1.9      /* ACTIVITY_EXTRA_ACTIVE */
};

/* Linearly interpolate between A and B by fraction T (0.0 to 1.0).  */
static double
lerp (double a, double b, double t)
{
  return a + (b - a) * t;
}

int
budget_round_to_pattern (int kcal_raw)
{
  int rounded;

  rounded = ((kcal_raw + 100) / 200) * 200;
  if (rounded < 1000)
    rounded = 1000;
  if (rounded > 3200)
    rounded = 3200;
  return rounded;
}

int
budget_estimate_calories (int age_years, double height_cm,
                          double weight_kg,
                          enum activity_level activity)
{
  double bmr;
  double tdee;
  double af;

  /* Mifflin-St Jeor with sex-neutral midpoint constant.
     Male constant is +5, female is -161; midpoint is -78.
     BMR = 10 * weight_kg + 6.25 * height_cm - 5 * age - 78  */
  bmr = 10.0 * weight_kg + 6.25 * height_cm - 5.0 * age_years - 78.0;

  if ((int) activity >= 0
      && (int) activity < (int) (sizeof activity_factors
                                 / sizeof activity_factors[0]))
    af = activity_factors[activity];
  else
    af = activity_factors[ACTIVITY_SEDENTARY];

  tdee = bmr * af;

  return budget_round_to_pattern ((int) round (tdee));
}

struct daily_budget
budget_for_calories (int kcal)
{
  struct daily_budget b;
  size_t i;
  double t;

  /* Clamp to table range.  */
  if (kcal <= usda_table[0].calories)
    return usda_table[0];

  if (kcal >= usda_table[TABLE_SIZE - 1].calories)
    return usda_table[TABLE_SIZE - 1];

  /* Find the bracketing entries and interpolate.  */
  for (i = 0; i < TABLE_SIZE - 1; i++)
    {
      if (kcal >= usda_table[i].calories
          && kcal <= usda_table[i + 1].calories)
        {
          t = (double) (kcal - usda_table[i].calories)
              / (double) (usda_table[i + 1].calories
                          - usda_table[i].calories);
          b.calories = kcal;
          b.vegetables = lerp (usda_table[i].vegetables,
                               usda_table[i + 1].vegetables, t);
          b.fruits = lerp (usda_table[i].fruits,
                           usda_table[i + 1].fruits, t);
          b.grains = lerp (usda_table[i].grains,
                           usda_table[i + 1].grains, t);
          b.dairy = lerp (usda_table[i].dairy,
                          usda_table[i + 1].dairy, t);
          b.protein = lerp (usda_table[i].protein,
                            usda_table[i + 1].protein, t);
          b.oils = lerp (usda_table[i].oils,
                         usda_table[i + 1].oils, t);
          return b;
        }
    }

  /* Should not reach here; the loop above covers all cases.  */
  return usda_table[TABLE_SIZE - 1];
}

struct daily_budget
budget_get_default (void)
{
  return budget_for_calories (2000);
}

void
budget_print (const struct daily_budget *budget,
              const struct daily_budget *consumed)
{
  printf (_("Daily budget (%d kcal USDA Healthy US-Style Eating Pattern):\n\n"),
          budget->calories);
  printf (_("%-20s %10s %10s %10s\n"),
          _("Food Group"), _("Budget"), _("Consumed"), _("Remaining"));
  printf ("%-20s %10s %10s %10s\n",
          "--------------------", "----------",
          "----------", "----------");
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Vegetables"),
          budget->vegetables, _("c "),
          consumed->vegetables, _("c "),
          budget->vegetables - consumed->vegetables, _("c "));
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Fruits"),
          budget->fruits, _("c "),
          consumed->fruits, _("c "),
          budget->fruits - consumed->fruits, _("c "));
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Grains"),
          budget->grains, _("oz"),
          consumed->grains, _("oz"),
          budget->grains - consumed->grains, _("oz"));
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Dairy"),
          budget->dairy, _("c "),
          consumed->dairy, _("c "),
          budget->dairy - consumed->dairy, _("c "));
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Protein Foods"),
          budget->protein, _("oz"),
          consumed->protein, _("oz"),
          budget->protein - consumed->protein, _("oz"));
  printf (_("%-20s %8.1f %s %8.1f %s %8.1f %s\n"),
          _("Oils"),
          budget->oils, _("g "),
          consumed->oils, _("g "),
          budget->oils - consumed->oils, _("g "));
}
