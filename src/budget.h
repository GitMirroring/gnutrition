/* budget.h - USDA Food Pattern budget system for GNUtrition
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

#ifndef BUDGET_H
#define BUDGET_H

/* Daily budget for the USDA Healthy US-Style Eating Pattern.
   Units: cup-equivalents for vegetables, fruits, dairy;
          ounce-equivalents for grains, protein; grams for oils.  */
struct daily_budget
{
  int calories;        /* kcal level  */
  double vegetables;   /* cup-eq  */
  double fruits;       /* cup-eq  */
  double grains;       /* oz-eq   */
  double dairy;        /* cup-eq  */
  double protein;      /* oz-eq   */
  double oils;         /* grams   */
};

/* Physical activity level multiplier categories.  */
enum activity_level
{
  ACTIVITY_SEDENTARY,       /* little or no exercise: 1.2   */
  ACTIVITY_LIGHT,           /* light exercise 1-3 days/wk: 1.375  */
  ACTIVITY_MODERATE,        /* moderate exercise 3-5 days/wk: 1.55  */
  ACTIVITY_VERY_ACTIVE,     /* hard exercise 6-7 days/wk: 1.725  */
  ACTIVITY_EXTRA_ACTIVE     /* very hard exercise / physical job: 1.9  */
};

/* Estimate daily calorie needs using the Mifflin-St Jeor equation
   with a sex-neutral midpoint constant (-78).  This avoids asking
   for a binary sex/gender input; the user can always override with
   --calories if they want a more precise target.

   AGE_YEARS is the person's age.  HEIGHT_CM is height in centimeters.
   WEIGHT_KG is weight in kilograms.  ACTIVITY is the physical
   activity multiplier.  Returns estimated kcal/day, already rounded
   to the nearest USDA pattern level (200-kcal steps, 1000-3200).  */
int budget_estimate_calories (int age_years, double height_cm,
                              double weight_kg,
                              enum activity_level activity);

/* Round a raw calorie value to the nearest USDA Food Pattern level
   (200-kcal steps), clamped to the range 1000-3200.  */
int budget_round_to_pattern (int kcal_raw);

/* Return the daily budget for a given calorie level KCAL, using the
   USDA Healthy US-Style Eating Pattern table (Dietary Guidelines for
   Americans, 2020-2025, Appendix 3).  Supported levels are 1000 to
   3200 kcal.  Values between table entries are linearly
   interpolated; values outside the range are clamped.  */
struct daily_budget budget_for_calories (int kcal);

/* Return the standard 2,000 kcal daily budget.  */
struct daily_budget budget_get_default (void);

/* Print a budget summary to stdout, showing CONSUMED against BUDGET.  */
void budget_print (const struct daily_budget *budget,
                   const struct daily_budget *consumed);

#endif /* BUDGET_H */
