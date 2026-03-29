/* gui.c - GTK 3 user interface for GNUtrition
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

#include "gui.h"
#include "budget.h"
#include "db.h"
#include "dbus.h"
#include "log.h"
#include "i18n.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROGRAM_NAME "gnutrition"

/* G_APPLICATION_DEFAULT_FLAGS was added in GLib 2.74.  */
#if !GLIB_CHECK_VERSION(2, 74, 0)
#define G_APPLICATION_DEFAULT_FLAGS G_APPLICATION_FLAGS_NONE
#endif

/* Application state shared across callbacks.  */
struct gui_state
{
  sqlite3 *food_db;
  sqlite3 *log_db;
  int calories;

  /* Main window widgets.  */
  GtkWidget *window;
  GtkWidget *dashboard_box;
  GtkWidget *log_list_box;
  GtkWidget *budget_label;
  GtkWidget *log_frame;

  /* Currently displayed log date (ISO 8601).  */
  char log_date[11];

  /* Progress bars for budget dashboard.  */
  GtkWidget *pb_vegetables;
  GtkWidget *pb_fruits;
  GtkWidget *pb_grains;
  GtkWidget *pb_dairy;
  GtkWidget *pb_protein;
  GtkWidget *pb_oils;

  /* D-Bus service.  */
  struct dbus_context dbus_ctx;
  guint dbus_owner_id;
};

/* Return today's date as YYYY-MM-DD in a static buffer.  */
static const char *
today_date (void)
{
  static char buf[11];
  time_t now;
  struct tm *tm;

  now = time (NULL);
  tm = localtime (&now);
  strftime (buf, sizeof buf, "%Y-%m-%d", tm);
  return buf;
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

/* Update a single progress bar for a budget category.  */
static void
update_progress_bar (GtkWidget *pb, double budget, double consumed,
                     const char *label)
{
  double fraction;
  char text[128];

  if (budget > 0.0)
    fraction = consumed / budget;
  else
    fraction = 0.0;
  if (fraction > 1.0)
    fraction = 1.0;

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pb), fraction);
  snprintf (text, sizeof text, "%s: %.1f / %.1f", label, consumed, budget);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (pb), text);
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (pb), TRUE);
}

/* Refresh the budget dashboard progress bars for the currently
   displayed log date.  */
static void
refresh_dashboard (struct gui_state *state)
{
  struct daily_budget budget;
  struct daily_budget consumed;
  struct log_list entries;
  const char *date;

  budget = budget_for_calories (state->calories);
  memset (&consumed, 0, sizeof consumed);
  date = state->log_date;

  if (log_get_day (state->log_db, date, &entries) == 0)
    {
      size_t i;
      for (i = 0; i < entries.count; i++)
        {
          struct fped_entry fped;
          if (db_get_fped (state->food_db, entries.items[i].food_code,
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
      log_list_free (&entries);
    }

  update_progress_bar (state->pb_vegetables, budget.vegetables,
                       consumed.vegetables, _("Vegetables (cup-eq)"));
  update_progress_bar (state->pb_fruits, budget.fruits,
                       consumed.fruits, _("Fruits (cup-eq)"));
  update_progress_bar (state->pb_grains, budget.grains,
                       consumed.grains, _("Grains (oz-eq)"));
  update_progress_bar (state->pb_dairy, budget.dairy,
                       consumed.dairy, _("Dairy (cup-eq)"));
  update_progress_bar (state->pb_protein, budget.protein,
                       consumed.protein, _("Protein (oz-eq)"));
  update_progress_bar (state->pb_oils, budget.oils,
                       consumed.oils, _("Oils (g)"));

  /* Update the budget label to reflect the current calorie target
     and the date being displayed.  */
  if (state->budget_label)
    {
      char budget_text[192];
      snprintf (budget_text, sizeof budget_text,
                _("USDA Healthy US-Style Eating Pattern (%d kcal) - %s"),
                state->calories, format_date (state->log_date));
      gtk_label_set_text (GTK_LABEL (state->budget_label), budget_text);
    }
}

/* Forward declarations for mutual recursion.  */
static void refresh_log_list (struct gui_state *state);
static void refresh_all (struct gui_state *state);

/* Callback for the "Delete" button on a log entry row.  */
static void
on_log_delete_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  int entry_id;
  GtkWidget *confirm;
  int response;

  (void) button;

  entry_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                   "entry-id"));

  confirm = gtk_message_dialog_new (GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
    _("Delete this log entry?"));
  gtk_window_set_title (GTK_WINDOW (confirm), _("Confirm Delete"));
  response = gtk_dialog_run (GTK_DIALOG (confirm));
  gtk_widget_destroy (confirm);

  if (response == GTK_RESPONSE_YES)
    {
      log_delete (state->log_db, entry_id);
      refresh_all (state);
    }
}

/* Callback for the Edit dialog "Save" button.  */
static void
on_edit_save_clicked (GtkDialog *dialog, gint response_id,
                      gpointer user_data)
{
  struct gui_state *state = user_data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkWidget *spin;
      GtkWidget *calendar;
      int entry_id;
      double quantity;
      char date[11];
      guint year, month, day;

      spin = g_object_get_data (G_OBJECT (dialog), "spin-quantity");
      calendar = g_object_get_data (G_OBJECT (dialog), "calendar");
      entry_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog),
                                                      "entry-id"));
      quantity = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

      gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);
      snprintf (date, sizeof date, "%04u-%02u-%02u", year, month + 1, day);

      log_update (state->log_db, entry_id, date, quantity);
      refresh_all (state);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* Callback for the "Edit" button on a log entry row.  */
static void
on_log_edit_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  int entry_id;
  double servings;
  const char *entry_date;
  const char *description;
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *spin;
  GtkWidget *calendar;
  GtkWidget *lbl;
  struct tm tm;

  entry_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                   "entry-id"));
  {
    double *srv_ptr = g_object_get_data (G_OBJECT (button),
                                          "entry-servings");
    servings = srv_ptr ? *srv_ptr : 1.0;
  }
  entry_date = g_object_get_data (G_OBJECT (button), "entry-date");
  description = g_object_get_data (G_OBJECT (button), "entry-desc");

  dialog = gtk_dialog_new_with_buttons (
    description ? description : _("Edit Log Entry"),
    GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    _("Save"), GTK_RESPONSE_OK,
    _("Cancel"), GTK_RESPONSE_CANCEL,
    NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 350);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  /* Quantity spinner.  */
  {
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    lbl = gtk_label_new (_("Servings:"));
    spin = gtk_spin_button_new_with_range (0.1, 100.0, 0.5);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), servings);
    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (content), hbox, FALSE, FALSE, 6);
  }

  /* Date selector.  */
  {
    GtkWidget *date_frame = gtk_frame_new (_("Date"));
    calendar = gtk_calendar_new ();

    /* Set the calendar to the entry's date.  */
    memset (&tm, 0, sizeof tm);
    if (entry_date
        && sscanf (entry_date, "%d-%d-%d",
                   &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3)
      {
        gtk_calendar_select_month (GTK_CALENDAR (calendar),
                                   (guint) (tm.tm_mon - 1),
                                   (guint) tm.tm_year);
        gtk_calendar_select_day (GTK_CALENDAR (calendar),
                                 (guint) tm.tm_mday);
      }

    gtk_container_add (GTK_CONTAINER (date_frame), calendar);
    gtk_box_pack_start (GTK_BOX (content), date_frame, FALSE, FALSE, 6);
  }

  g_object_set_data (G_OBJECT (dialog), "spin-quantity", spin);
  g_object_set_data (G_OBJECT (dialog), "calendar", calendar);
  g_object_set_data (G_OBJECT (dialog), "entry-id",
                     GINT_TO_POINTER (entry_id));

  g_signal_connect (dialog, "response",
                    G_CALLBACK (on_edit_save_clicked), state);
  gtk_widget_show_all (dialog);
}

/* Refresh the food log list for the currently selected date.  */
static void
refresh_log_list (struct gui_state *state)
{
  struct log_list entries;
  const char *date;
  GList *children;
  GList *iter;
  char frame_title[128];

  /* Remove existing rows.  */
  children = gtk_container_get_children (GTK_CONTAINER (state->log_list_box));
  for (iter = children; iter; iter = iter->next)
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  g_list_free (children);

  date = state->log_date;

  /* Update the log frame title to show the selected date.  */
  if (state->log_frame)
    {
      if (strcmp (date, today_date ()) == 0)
        snprintf (frame_title, sizeof frame_title,
                  _("Food Log - %s (Today)"), format_date (date));
      else
        snprintf (frame_title, sizeof frame_title,
                  _("Food Log - %s"), format_date (date));
      gtk_frame_set_label (GTK_FRAME (state->log_frame), frame_title);
    }

  if (log_get_day (state->log_db, date, &entries) == 0)
    {
      size_t i;
      if (entries.count == 0)
        {
          GtkWidget *label = gtk_label_new (_("No entries for this date."));
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_container_add (GTK_CONTAINER (state->log_list_box), label);
        }
      else
        {
          for (i = 0; i < entries.count; i++)
            {
              char row_text[512];
              GtkWidget *hbox;
              GtkWidget *label;
              GtkWidget *edit_btn;
              GtkWidget *del_btn;
              double *srv_copy;

              hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

              snprintf (row_text, sizeof row_text,
                        "%d  -  %s  (%.1f %s)",
                        entries.items[i].food_code,
                        entries.items[i].description,
                        entries.items[i].servings,
                        _("servings"));
              label = gtk_label_new (row_text);
              gtk_widget_set_halign (label, GTK_ALIGN_START);
              gtk_label_set_xalign (GTK_LABEL (label), 0.0);
              gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

              edit_btn = gtk_button_new_with_label (_("Edit"));
              del_btn = gtk_button_new_with_label (_("Delete"));

              /* Store entry metadata on the buttons.  */
              g_object_set_data (G_OBJECT (edit_btn), "entry-id",
                                 GINT_TO_POINTER (entries.items[i].id));
              srv_copy = g_new (double, 1);
              *srv_copy = entries.items[i].servings;
              g_object_set_data_full (G_OBJECT (edit_btn),
                                      "entry-servings",
                                      srv_copy, g_free);
              g_object_set_data_full (G_OBJECT (edit_btn), "entry-date",
                                      g_strdup (entries.items[i].date),
                                      g_free);
              g_object_set_data_full (G_OBJECT (edit_btn), "entry-desc",
                                      g_strdup (entries.items[i].description),
                                      g_free);

              g_object_set_data (G_OBJECT (del_btn), "entry-id",
                                 GINT_TO_POINTER (entries.items[i].id));

              g_signal_connect (edit_btn, "clicked",
                                G_CALLBACK (on_log_edit_clicked), state);
              g_signal_connect (del_btn, "clicked",
                                G_CALLBACK (on_log_delete_clicked), state);

              gtk_box_pack_end (GTK_BOX (hbox), del_btn, FALSE, FALSE, 0);
              gtk_box_pack_end (GTK_BOX (hbox), edit_btn, FALSE, FALSE, 0);

              gtk_container_add (GTK_CONTAINER (state->log_list_box), hbox);
            }
        }
      log_list_free (&entries);
    }

  gtk_widget_show_all (state->log_list_box);
}

/* Refresh both the dashboard and log list.  */
static void
refresh_all (struct gui_state *state)
{
  refresh_dashboard (state);
  refresh_log_list (state);
}

/* --- Search Dialog --- */

enum
{
  COL_FOOD_CODE,
  COL_DESCRIPTION,
  NUM_COLS
};

/* Callback for food detail dialog "Add" button.  */
static void
on_food_add_clicked (GtkDialog *dialog, gint response_id,
                     gpointer user_data)
{
  struct gui_state *state = user_data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkWidget *content;
      GList *children;
      GtkWidget *spin;
      GtkWidget *calendar;
      int food_code;
      double quantity;
      const char *description;
      char date[11];
      guint year, month, day;

      content = gtk_dialog_get_content_area (dialog);
      children = gtk_container_get_children (GTK_CONTAINER (content));

      /* The spin button is attached as object data on the dialog.  */
      spin = g_object_get_data (G_OBJECT (dialog), "spin-quantity");
      calendar = g_object_get_data (G_OBJECT (dialog), "calendar");
      food_code = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog),
                                                       "food-code"));
      description = g_object_get_data (G_OBJECT (dialog), "food-desc");
      quantity = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

      gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);
      snprintf (date, sizeof date, "%04u-%02u-%02u", year, month + 1, day);

      log_add (state->log_db, food_code, description, date, quantity);
      refresh_all (state);

      g_list_free (children);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* Populate the nutrient grid with values scaled by SERVINGS.  */
static void
populate_nutrient_grid (GtkWidget *grid, sqlite3 *food_db,
                        int food_code, double servings)
{
  GtkWidget *lbl;
  struct nutrient_list nutrients;
  struct fped_entry fped;
  int row;
  size_t i;
  GList *children, *iter;

  /* Clear existing grid content.  */
  children = gtk_container_get_children (GTK_CONTAINER (grid));
  for (iter = children; iter; iter = iter->next)
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  g_list_free (children);

  row = 0;
  if (db_get_nutrients (food_db, food_code, &nutrients) == 0)
    {
      lbl = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (lbl), _("<b>Nutrient</b>"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      lbl = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (lbl), _("<b>Value</b>"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      for (i = 0; i < nutrients.count; i++)
        {
          char val_buf[32];
          lbl = gtk_label_new (nutrients.items[i].name);
          gtk_widget_set_halign (lbl, GTK_ALIGN_START);
          gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
          snprintf (val_buf, sizeof val_buf, "%.2f",
                    nutrients.items[i].value * servings);
          lbl = gtk_label_new (val_buf);
          gtk_widget_set_halign (lbl, GTK_ALIGN_END);
          gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
          row++;
        }
      nutrient_list_free (&nutrients);
    }

  /* FPED info.  */
  if (db_get_fped (food_db, food_code, &fped) == 0)
    {
      char val_buf[32];
      row++;
      lbl = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (lbl),
                            _("<b>Food Pattern Equivalents</b>"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 2, 1);
      row++;

      lbl = gtk_label_new (_("Vegetables (cup-eq)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f",
                fped.vegetables * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      lbl = gtk_label_new (_("Fruits (cup-eq)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f", fped.fruits * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      lbl = gtk_label_new (_("Grains (oz-eq)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f", fped.grains * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      lbl = gtk_label_new (_("Dairy (cup-eq)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f", fped.dairy * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      lbl = gtk_label_new (_("Protein (oz-eq)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f", fped.protein * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
      row++;

      lbl = gtk_label_new (_("Oils (g)"));
      gtk_widget_set_halign (lbl, GTK_ALIGN_START);
      gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
      snprintf (val_buf, sizeof val_buf, "%.2f", fped.oils * servings);
      lbl = gtk_label_new (val_buf);
      gtk_widget_set_halign (lbl, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (grid), lbl, 1, row, 1, 1);
    }

  gtk_widget_show_all (grid);
}

/* Callback when the servings spin button changes in the food detail
   dialog.  Rebuilds the nutrient grid to reflect the new quantity.  */
static void
on_detail_quantity_changed (GtkSpinButton *spin, gpointer user_data)
{
  GtkWidget *dialog = GTK_WIDGET (user_data);
  GtkWidget *grid;
  sqlite3 *food_db;
  int food_code;
  double servings;

  grid = g_object_get_data (G_OBJECT (dialog), "nutrient-grid");
  food_db = g_object_get_data (G_OBJECT (dialog), "food-db");
  food_code = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog),
                                                    "food-code"));
  servings = gtk_spin_button_get_value (spin);

  populate_nutrient_grid (grid, food_db, food_code, servings);
}

/* Show a food detail dialog with nutrient info and an Add button.  */
static void
show_food_detail (struct gui_state *state, int food_code,
                  const char *description)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *scroll;
  GtkWidget *grid;
  GtkWidget *spin;
  GtkWidget *calendar;
  GtkWidget *lbl;

  dialog = gtk_dialog_new_with_buttons (description,
    GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    _("Add to Log"), GTK_RESPONSE_OK,
    _("Cancel"), GTK_RESPONSE_CANCEL,
    NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 500);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  /* Quantity spinner and date picker.  */
  {
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    lbl = gtk_label_new (_("Servings:"));
    spin = gtk_spin_button_new_with_range (0.1, 100.0, 0.5);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 1.0);
    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (content), hbox, FALSE, FALSE, 6);
  }

  /* Date selector.  */
  {
    GtkWidget *date_frame = gtk_frame_new (_("Log Date"));
    calendar = gtk_calendar_new ();
    gtk_container_add (GTK_CONTAINER (date_frame), calendar);
    gtk_box_pack_start (GTK_BOX (content), date_frame, FALSE, FALSE, 6);
  }

  g_object_set_data (G_OBJECT (dialog), "spin-quantity", spin);
  g_object_set_data (G_OBJECT (dialog), "calendar", calendar);
  g_object_set_data (G_OBJECT (dialog), "food-code",
                     GINT_TO_POINTER (food_code));
  g_object_set_data_full (G_OBJECT (dialog), "food-desc",
                          g_strdup (description), g_free);
  g_object_set_data (G_OBJECT (dialog), "food-db", state->food_db);

  /* Nutrient info in a scrolled grid.  */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 2);

  g_object_set_data (G_OBJECT (dialog), "nutrient-grid", grid);

  populate_nutrient_grid (grid, state->food_db, food_code, 1.0);

  gtk_container_add (GTK_CONTAINER (scroll), grid);
  gtk_box_pack_start (GTK_BOX (content), scroll, TRUE, TRUE, 0);

  g_signal_connect (spin, "value-changed",
                    G_CALLBACK (on_detail_quantity_changed), dialog);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (on_food_add_clicked), state);
  gtk_widget_show_all (dialog);
}

/* Callback when a row in the search results tree view is activated.  */
static void
on_search_row_activated (GtkTreeView *tree_view, GtkTreePath *path,
                         GtkTreeViewColumn *column, gpointer user_data)
{
  struct gui_state *state = user_data;
  GtkTreeModel *model;
  GtkTreeIter iter;

  (void) column;

  model = gtk_tree_view_get_model (tree_view);
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gint food_code;
      gchar *description;
      gtk_tree_model_get (model, &iter,
                          COL_FOOD_CODE, &food_code,
                          COL_DESCRIPTION, &description,
                          -1);
      show_food_detail (state, food_code, description);
      g_free (description);
    }
}

/* Callback for search entry text changes (live search).  */
static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
  GtkListStore *store = user_data;
  struct gui_state *state;
  const gchar *query;
  struct food_list results;
  size_t i;

  state = g_object_get_data (G_OBJECT (entry), "gui-state");
  query = gtk_entry_get_text (GTK_ENTRY (entry));

  gtk_list_store_clear (store);

  if (query[0] == '\0')
    return;

  if (db_search_foods (state->food_db, query, &results) == 0)
    {
      for (i = 0; i < results.count; i++)
        {
          GtkTreeIter iter;
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              COL_FOOD_CODE,
                              results.items[i].food_code,
                              COL_DESCRIPTION,
                              results.items[i].description,
                              -1);
        }
      food_list_free (&results);
    }
}

/* Show the Search dialog.  */
static void
on_search_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *search_entry;
  GtkWidget *scroll;
  GtkWidget *tree;
  GtkListStore *store;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *col;

  (void) button;

  dialog = gtk_dialog_new_with_buttons (_("Search Foods"),
    GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    _("Close"), GTK_RESPONSE_CLOSE,
    NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 450);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  /* Search entry.  */
  search_entry = gtk_search_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (search_entry),
                                  _("Type to search foods..."));
  gtk_box_pack_start (GTK_BOX (content), search_entry, FALSE, FALSE, 6);

  /* Results list.  */
  store = gtk_list_store_new (NUM_COLS, G_TYPE_INT, G_TYPE_STRING);
  tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Code"), renderer,
                                                   "text", COL_FOOD_CODE,
                                                   NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Description"),
                                                   renderer,
                                                   "text", COL_DESCRIPTION,
                                                   NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), col);

  g_object_set_data (G_OBJECT (search_entry), "gui-state", state);
  g_signal_connect (search_entry, "search-changed",
                    G_CALLBACK (on_search_changed), store);
  g_signal_connect (tree, "row-activated",
                    G_CALLBACK (on_search_row_activated), state);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scroll), tree);
  gtk_box_pack_start (GTK_BOX (content), scroll, TRUE, TRUE, 0);

  gtk_widget_show_all (dialog);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
}

/* Profile dialog response handler.  */
static void
on_profile_response (GtkDialog *dialog, gint response_id,
                     gpointer user_data)
{
  struct gui_state *state = user_data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkWidget *age_spin, *height_spin, *weight_spin, *activity_combo;
      struct user_profile prof;

      age_spin = g_object_get_data (G_OBJECT (dialog), "age-spin");
      height_spin = g_object_get_data (G_OBJECT (dialog), "height-spin");
      weight_spin = g_object_get_data (G_OBJECT (dialog), "weight-spin");
      activity_combo = g_object_get_data (G_OBJECT (dialog),
                                           "activity-combo");

      prof.age_years = gtk_spin_button_get_value_as_int (
                         GTK_SPIN_BUTTON (age_spin));
      prof.height_cm = gtk_spin_button_get_value (
                         GTK_SPIN_BUTTON (height_spin));
      prof.weight_kg = gtk_spin_button_get_value (
                         GTK_SPIN_BUTTON (weight_spin));
      prof.activity_level = gtk_combo_box_get_active (
                              GTK_COMBO_BOX (activity_combo));
      prof.calorie_target = budget_estimate_calories (prof.age_years,
                              prof.height_cm, prof.weight_kg,
                              prof.activity_level);

      if (log_save_profile (state->log_db, &prof) == 0)
        {
          state->calories = prof.calorie_target;
          state->dbus_ctx.calories = prof.calorie_target;
          refresh_all (state);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* Show the Profile dialog.  */
static void
on_profile_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *grid;
  GtkWidget *age_spin, *height_spin, *weight_spin, *activity_combo;
  GtkWidget *lbl;
  struct user_profile prof;
  int rc;
  int row;

  (void) button;

  dialog = gtk_dialog_new_with_buttons (_("Profile"),
    GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    _("Save"), GTK_RESPONSE_OK,
    _("Cancel"), GTK_RESPONSE_CANCEL,
    NULL);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_start (grid, 12);
  gtk_widget_set_margin_end (grid, 12);
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);

  row = 0;

  /* Age.  */
  lbl = gtk_label_new (_("Age (years):"));
  gtk_widget_set_halign (lbl, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
  age_spin = gtk_spin_button_new_with_range (1, 120, 1);
  gtk_grid_attach (GTK_GRID (grid), age_spin, 1, row, 1, 1);
  row++;

  /* Height.  */
  lbl = gtk_label_new (_("Height (cm):"));
  gtk_widget_set_halign (lbl, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
  height_spin = gtk_spin_button_new_with_range (30.0, 300.0, 0.5);
  gtk_grid_attach (GTK_GRID (grid), height_spin, 1, row, 1, 1);
  row++;

  /* Weight.  */
  lbl = gtk_label_new (_("Weight (kg):"));
  gtk_widget_set_halign (lbl, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
  weight_spin = gtk_spin_button_new_with_range (1.0, 500.0, 0.5);
  gtk_grid_attach (GTK_GRID (grid), weight_spin, 1, row, 1, 1);
  row++;

  /* Activity level.  */
  lbl = gtk_label_new (_("Activity level:"));
  gtk_widget_set_halign (lbl, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), lbl, 0, row, 1, 1);
  activity_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (activity_combo),
                                  _("Sedentary"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (activity_combo),
                                  _("Light"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (activity_combo),
                                  _("Moderate"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (activity_combo),
                                  _("Very active"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (activity_combo),
                                  _("Extra active"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (activity_combo),
                            ACTIVITY_SEDENTARY);
  gtk_grid_attach (GTK_GRID (grid), activity_combo, 1, row, 1, 1);

  /* Load existing profile.  */
  rc = log_get_profile (state->log_db, &prof);
  if (rc == 0)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (age_spin),
                                 prof.age_years);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (height_spin),
                                 prof.height_cm);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (weight_spin),
                                 prof.weight_kg);
      gtk_combo_box_set_active (GTK_COMBO_BOX (activity_combo),
                                prof.activity_level);
    }

  g_object_set_data (G_OBJECT (dialog), "age-spin", age_spin);
  g_object_set_data (G_OBJECT (dialog), "height-spin", height_spin);
  g_object_set_data (G_OBJECT (dialog), "weight-spin", weight_spin);
  g_object_set_data (G_OBJECT (dialog), "activity-combo", activity_combo);

  gtk_box_pack_start (GTK_BOX (content), grid, TRUE, TRUE, 0);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (on_profile_response), state);
  gtk_widget_show_all (dialog);
}

/* Navigate the food log to the previous date.  */
static void
on_log_prev_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  struct date_list dates;
  size_t i;

  (void) button;

  if (log_get_dates (state->log_db, &dates) != 0 || dates.count == 0)
    return;

  /* Find the current date in the list and go to the previous one.  */
  for (i = 0; i < dates.count; i++)
    {
      if (strcmp (dates.dates[i], state->log_date) == 0)
        break;
    }

  if (i > 0 && i <= dates.count)
    {
      /* Move to the previous date.  If current date was not found
         (i == dates.count), pick the last date before it.  */
      size_t target = (i < dates.count) ? i - 1 : dates.count - 1;
      strncpy (state->log_date, dates.dates[target],
               sizeof state->log_date - 1);
      state->log_date[sizeof state->log_date - 1] = '\0';
      refresh_all (state);
    }

  date_list_free (&dates);
}

/* Navigate the food log to the next date.  */
static void
on_log_next_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  struct date_list dates;
  size_t i;

  (void) button;

  if (log_get_dates (state->log_db, &dates) != 0 || dates.count == 0)
    return;

  /* Find the current date in the list and go to the next one.  */
  for (i = 0; i < dates.count; i++)
    {
      if (strcmp (dates.dates[i], state->log_date) == 0)
        break;
    }

  if (i < dates.count && i + 1 < dates.count)
    {
      strncpy (state->log_date, dates.dates[i + 1],
               sizeof state->log_date - 1);
      state->log_date[sizeof state->log_date - 1] = '\0';
      refresh_all (state);
    }

  date_list_free (&dates);
}

/* Navigate the food log back to today's date.  */
static void
on_log_today_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;

  (void) button;

  strncpy (state->log_date, today_date (), sizeof state->log_date - 1);
  state->log_date[sizeof state->log_date - 1] = '\0';
  refresh_all (state);
}

/* Show the About dialog.  */
static void
on_about_clicked (GtkButton *button, gpointer user_data)
{
  struct gui_state *state = user_data;
  GtkWidget *dialog;
  char version_text[512];

  (void) button;

  snprintf (version_text, sizeof version_text,
            _("GNUtrition %s\n"
              "Copyright (C) 2026 Free Software Foundation, Inc.\n"
              "License GPLv3+: GNU GPL version 3 or later "
              "<https://gnu.org/licenses/gpl.html>\n"
              "This is free software: you are free to change "
              "and redistribute it.\n"
              "There is NO WARRANTY, to the extent permitted by law."),
            PACKAGE_VERSION);

  dialog = gtk_message_dialog_new (GTK_WINDOW (state->window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
    "%s", version_text);
  gtk_window_set_title (GTK_WINDOW (dialog), _("About GNUtrition"));
  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_widget_show_all (dialog);
}

/* Create a single GtkProgressBar for a budget category.  */
static GtkWidget *
make_progress_bar (void)
{
  GtkWidget *pb;

  pb = gtk_progress_bar_new ();
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (pb), TRUE);
  gtk_widget_set_hexpand (pb, TRUE);
  return pb;
}

/* Build and show the main window.  */
static void
on_activate (GtkApplication *app, gpointer user_data)
{
  struct gui_state *state = user_data;
  GtkWidget *header;
  GtkWidget *search_btn, *profile_btn, *about_btn;
  GtkWidget *main_box;
  GtkWidget *dash_frame, *log_frame;
  GtkWidget *dash_box;
  GtkWidget *log_scroll;
  GtkWidget *log_vbox;
  GtkWidget *log_nav_box;
  GtkWidget *prev_btn, *today_btn, *next_btn;
  char budget_text[128];

  /* Initialize the log date to today.  */
  strncpy (state->log_date, today_date (), sizeof state->log_date - 1);
  state->log_date[sizeof state->log_date - 1] = '\0';

  /* Main window.  */
  state->window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (state->window), _("GNUtrition"));
  gtk_window_set_default_size (GTK_WINDOW (state->window), 700, 550);

  /* Header bar.  */
  header = gtk_header_bar_new ();
  gtk_header_bar_set_title (GTK_HEADER_BAR (header), _("GNUtrition"));
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);

  search_btn = gtk_button_new_with_label (_("Search"));
  profile_btn = gtk_button_new_with_label (_("Profile"));
  about_btn = gtk_button_new_with_label (_("About"));

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), search_btn);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), about_btn);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), profile_btn);

  gtk_window_set_titlebar (GTK_WINDOW (state->window), header);

  g_signal_connect (search_btn, "clicked",
                    G_CALLBACK (on_search_clicked), state);
  g_signal_connect (profile_btn, "clicked",
                    G_CALLBACK (on_profile_clicked), state);
  g_signal_connect (about_btn, "clicked",
                    G_CALLBACK (on_about_clicked), state);

  /* Main layout: vertical box with dashboard on top and log on bottom.  */
  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_start (main_box, 12);
  gtk_widget_set_margin_end (main_box, 12);
  gtk_widget_set_margin_top (main_box, 12);
  gtk_widget_set_margin_bottom (main_box, 12);

  /* Dashboard frame.  */
  dash_frame = gtk_frame_new (_("Daily Budget"));
  dash_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_start (dash_box, 8);
  gtk_widget_set_margin_end (dash_box, 8);
  gtk_widget_set_margin_top (dash_box, 4);
  gtk_widget_set_margin_bottom (dash_box, 8);

  snprintf (budget_text, sizeof budget_text,
            _("USDA Healthy US-Style Eating Pattern (%d kcal)"),
            state->calories);
  state->budget_label = gtk_label_new (budget_text);
  gtk_widget_set_halign (state->budget_label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (dash_box), state->budget_label,
                      FALSE, FALSE, 2);

  state->pb_vegetables = make_progress_bar ();
  state->pb_fruits = make_progress_bar ();
  state->pb_grains = make_progress_bar ();
  state->pb_dairy = make_progress_bar ();
  state->pb_protein = make_progress_bar ();
  state->pb_oils = make_progress_bar ();

  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_vegetables,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_fruits,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_grains,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_dairy,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_protein,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (dash_box), state->pb_oils,
                      FALSE, FALSE, 2);

  gtk_container_add (GTK_CONTAINER (dash_frame), dash_box);
  state->dashboard_box = dash_box;

  /* Log frame with date navigation.  */
  log_frame = gtk_frame_new (_("Food Log"));
  state->log_frame = log_frame;

  log_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  /* Date navigation bar.  */
  log_nav_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_margin_start (log_nav_box, 8);
  gtk_widget_set_margin_end (log_nav_box, 8);
  gtk_widget_set_margin_top (log_nav_box, 4);

  prev_btn = gtk_button_new_with_label ("\342\227\200");   /* U+25C0 â */
  today_btn = gtk_button_new_with_label (_("Today"));
  next_btn = gtk_button_new_with_label ("\342\226\266");   /* U+25B6 â¶ */

  gtk_box_pack_start (GTK_BOX (log_nav_box), prev_btn, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (log_nav_box), today_btn, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (log_nav_box), next_btn, FALSE, FALSE, 0);

  g_signal_connect (prev_btn, "clicked",
                    G_CALLBACK (on_log_prev_clicked), state);
  g_signal_connect (today_btn, "clicked",
                    G_CALLBACK (on_log_today_clicked), state);
  g_signal_connect (next_btn, "clicked",
                    G_CALLBACK (on_log_next_clicked), state);

  gtk_box_pack_start (GTK_BOX (log_vbox), log_nav_box, FALSE, FALSE, 0);

  log_scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (log_scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  state->log_list_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_start (state->log_list_box, 8);
  gtk_widget_set_margin_end (state->log_list_box, 8);
  gtk_widget_set_margin_top (state->log_list_box, 4);
  gtk_widget_set_margin_bottom (state->log_list_box, 4);
  gtk_container_add (GTK_CONTAINER (log_scroll), state->log_list_box);
  gtk_box_pack_start (GTK_BOX (log_vbox), log_scroll, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (log_frame), log_vbox);

  gtk_box_pack_start (GTK_BOX (main_box), dash_frame, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_box), log_frame, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (state->window), main_box);

  /* Populate data.  */
  refresh_all (state);

  gtk_widget_show_all (state->window);
}

int
gui_run (sqlite3 *food_db, sqlite3 *log_db, int calories,
         int argc, char **argv)
{
  GtkApplication *app;
  struct gui_state state;
  int status;

  memset (&state, 0, sizeof state);
  state.food_db = food_db;
  state.log_db = log_db;
  state.calories = calories;

  /* Start D-Bus service.  */
  state.dbus_ctx.food_db = food_db;
  state.dbus_ctx.log_db = log_db;
  state.dbus_ctx.calories = calories;
  state.dbus_owner_id = dbus_service_start (&state.dbus_ctx);

  app = gtk_application_new ("org.gnu.gnutrition",
                              G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), &state);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  /* Stop D-Bus service.  */
  dbus_service_stop (state.dbus_owner_id);

  return status;
}
