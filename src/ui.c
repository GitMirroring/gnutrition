/* ui.c - ncurses user interface for GNUtrition
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

#include "ui.h"
#include "budget.h"
#include "db.h"
#include "log.h"
#include "i18n.h"

#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Maximum length of the search input buffer.  This is a UI display
   limit, not a data limit.  */
#define SEARCH_BUF_SIZE 256

/* Get today's date as YYYY-MM-DD.  Returns a pointer to a static
   buffer; not reentrant.  */
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

/* Parse a locale-formatted date string back into an ISO 8601 date
   (YYYY-MM-DD).  Returns 0 on success, -1 on failure.  */
static int
parse_locale_date (const char *locale_date, char *iso_buf, int bufsz)
{
  struct tm tm;

  memset (&tm, 0, sizeof tm);
  if (strptime (locale_date, "%x", &tm) == NULL)
    return -1;
  if (strftime (iso_buf, (size_t) bufsz, "%Y-%m-%d", &tm) == 0)
    return -1;
  return 0;
}

/* Draw the title bar.  */
static void
draw_title (void)
{
  attron (A_REVERSE);
  mvhline (0, 0, ' ', COLS);
  mvprintw (0, 1, _("GNUtrition %s"), PACKAGE_VERSION);
  attroff (A_REVERSE);
}

/* Draw the status / help bar at the bottom.  */
static void
draw_status (const char *msg)
{
  attron (A_REVERSE);
  mvhline (LINES - 1, 0, ' ', COLS);
  mvprintw (LINES - 1, 1, "%s", msg);
  attroff (A_REVERSE);
}

/* Draw the daily budget summary for DATE.  Returns the number of
   lines used.  */
static int
draw_budget (sqlite3 *food_db, sqlite3 *log_db, int start_row,
             int calories, const char *date)
{
  struct daily_budget budget;
  struct daily_budget consumed;
  struct log_list entries;
  size_t i;
  int row;

  budget = budget_for_calories (calories);
  memset (&consumed, 0, sizeof consumed);

  if (log_get_day (log_db, date, &entries) == 0)
    {
      for (i = 0; i < entries.count; i++)
        {
          struct fped_entry fped;
          if (db_get_fped (food_db, entries.items[i].food_code, &fped) == 0)
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

  row = start_row;
  attron (A_BOLD);
  mvprintw (row++, 2, _("Daily Budget (%s) - %d kcal USDA Pattern"),
            format_date (date), budget.calories);
  attroff (A_BOLD);
  row++;
  mvprintw (row++, 2, _("%-20s %10s %10s %10s"),
            _("Food Group"), _("Budget"), _("Consumed"), _("Remaining"));
  mvprintw (row++, 2, "%-20s %10s %10s %10s",
            "--------------------", "----------",
            "----------", "----------");
  mvprintw (row++, 2, _("%-20s %7.1f c  %7.1f c  %7.1f c"),
            _("Vegetables"), budget.vegetables,
            consumed.vegetables, budget.vegetables - consumed.vegetables);
  mvprintw (row++, 2, _("%-20s %7.1f c  %7.1f c  %7.1f c"),
            _("Fruits"), budget.fruits,
            consumed.fruits, budget.fruits - consumed.fruits);
  mvprintw (row++, 2, _("%-20s %7.1f oz %7.1f oz %7.1f oz"),
            _("Grains"), budget.grains,
            consumed.grains, budget.grains - consumed.grains);
  mvprintw (row++, 2, _("%-20s %7.1f c  %7.1f c  %7.1f c"),
            _("Dairy"), budget.dairy,
            consumed.dairy, budget.dairy - consumed.dairy);
  mvprintw (row++, 2, _("%-20s %7.1f oz %7.1f oz %7.1f oz"),
            _("Protein Foods"), budget.protein,
            consumed.protein, budget.protein - consumed.protein);
  mvprintw (row++, 2, _("%-20s %7.1f g  %7.1f g  %7.1f g"),
            _("Oils"), budget.oils,
            consumed.oils, budget.oils - consumed.oils);
  return row - start_row;
}

/* Read a string from the ncurses screen at ROW, COL.
   Edits BUF (of size BUFSZ) in-place.  Returns the key that
   ended input (Enter or Escape).  */
static int
read_field (int row, int col, char *buf, int bufsz)
{
  int len;
  int ch;

  len = (int) strlen (buf);
  for (;;)
    {
      mvprintw (row, col, "%-20s", buf);
      mvprintw (row, col + len, "_");
      clrtoeol ();
      refresh ();
      ch = getch ();

      if (ch == '\n' || ch == KEY_ENTER || ch == '\t')
        return '\n';
      if (ch == 27)
        return 27;
      if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && len > 0)
        buf[--len] = '\0';
      else if (ch >= 32 && ch < 127 && len < bufsz - 1)
        {
          buf[len++] = (char) ch;
          buf[len] = '\0';
        }
    }
}

/* Show food detail screen.  Display nutrient information and FPED
   data, and let the user log the food with a chosen quantity and
   date.  */
static void
food_detail_screen (sqlite3 *food_db, sqlite3 *log_db,
                    int food_code, const char *description)
{
  struct nutrient_list nutrients;
  struct fped_entry fped;
  int has_fped;
  char srv_buf[16];
  char date_buf[16];
  int scroll;
  int ch;
  int field;  /* 0 = browsing nutrients, 1 = servings, 2 = date  */

  if (db_get_nutrients (food_db, food_code, &nutrients) < 0)
    return;
  has_fped = (db_get_fped (food_db, food_code, &fped) == 0);

  snprintf (srv_buf, sizeof srv_buf, "1.0");
  strncpy (date_buf, today_date (), sizeof date_buf - 1);
  date_buf[sizeof date_buf - 1] = '\0';
  scroll = 0;
  field = 0;

  for (;;)
    {
      size_t i;
      int row;
      int visible;
      int total_lines;
      int srv_row;
      int date_row;
      double servings;

      {
        char *endp;
        errno = 0;
        servings = strtod (srv_buf, &endp);
        if (errno != 0 || endp == srv_buf || *endp != '\0'
            || servings <= 0.0)
          servings = 1.0;
      }

      clear ();
      draw_title ();
      if (field == 0)
        draw_status (_("Up/Down: scroll | Tab: edit fields | "
                     "l: log food | Esc: back"));
      else
        draw_status (_("Enter/Tab: next field | Esc: cancel edit"));

      attron (A_BOLD);
      mvprintw (2, 2, "%d - %-.*s", food_code, COLS - 14, description);
      attroff (A_BOLD);

      /* Count total display lines for scroll limit.  */
      total_lines = (int) nutrients.count + 1 + (has_fped ? 9 : 0);

      /* Visible area for scrollable nutrient list.  */
      visible = LINES - 9;
      if (visible < 1)
        visible = 1;

      if (scroll > total_lines - visible)
        scroll = total_lines - visible;
      if (scroll < 0)
        scroll = 0;

      row = 4;
      {
        int line_idx = 0;

        /* Nutrient header.  */
        if (nutrients.count > 0 && line_idx >= scroll
            && row < LINES - 5)
          {
            mvprintw (row++, 2, _("%-40s %10s"),
                      _("Nutrient"), _("Value"));
          }
        line_idx++;

        for (i = 0; i < nutrients.count; i++, line_idx++)
          {
            if (line_idx < scroll)
              continue;
            if (row >= LINES - 5)
              break;
            mvprintw (row++, 2, "%-40.40s %10.2f",
                      nutrients.items[i].name,
                      nutrients.items[i].value * servings);
          }

        /* FPED section.  */
        if (has_fped)
          {
            if (line_idx >= scroll && row < LINES - 5)
              {
                row++;
                attron (A_BOLD);
                if (servings != 1.0)
                  mvprintw (row++, 2,
                            _("Food Pattern Equivalents "
                              "(per 100g x %.1f):"), servings);
                else
                  mvprintw (row++, 2,
                            _("Food Pattern Equivalents (per 100g):"));
                attroff (A_BOLD);
              }
            line_idx += 2;

            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Vegetables: %.2f cup-eq"),
                        fped.vegetables * servings);
            line_idx++;
            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Fruits:     %.2f cup-eq"),
                        fped.fruits * servings);
            line_idx++;
            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Grains:     %.2f oz-eq"),
                        fped.grains * servings);
            line_idx++;
            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Dairy:      %.2f cup-eq"),
                        fped.dairy * servings);
            line_idx++;
            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Protein:    %.2f oz-eq"),
                        fped.protein * servings);
            line_idx++;
            if (line_idx >= scroll && row < LINES - 5)
              mvprintw (row++, 2,
                        _("  Oils:       %.2f g"),
                        fped.oils * servings);
            line_idx++;
          }
      }

      /* Bottom area: servings and date fields.  */
      srv_row = LINES - 4;
      date_row = LINES - 3;

      if (field == 1)
        attron (A_REVERSE);
      mvprintw (srv_row, 2, _("Servings: "));
      if (field == 1)
        attroff (A_REVERSE);
      mvprintw (srv_row, 12, "%-20s", srv_buf);

      if (field == 2)
        attron (A_REVERSE);
      mvprintw (date_row, 2, _("Date:     "));
      if (field == 2)
        attroff (A_REVERSE);
      mvprintw (date_row, 12, "%-20s", format_date (date_buf));

      refresh ();

      if (field == 1)
        {
          ch = read_field (srv_row, 12, srv_buf, (int) sizeof srv_buf);
          if (ch == 27)
            field = 0;
          else
            field = 2;
          continue;
        }
      else if (field == 2)
        {
          char disp_buf[64];
          strncpy (disp_buf, format_date (date_buf), sizeof disp_buf - 1);
          disp_buf[sizeof disp_buf - 1] = '\0';
          ch = read_field (date_row, 12, disp_buf, (int) sizeof disp_buf);
          if (ch != 27)
            {
              if (parse_locale_date (disp_buf, date_buf,
                                     (int) sizeof date_buf) < 0)
                {
                  draw_status (_("Invalid date.  Press any key..."));
                  refresh ();
                  getch ();
                }
            }
          field = 0;
          continue;
        }

      ch = getch ();

      switch (ch)
        {
        case 27:  /* Escape  */
          nutrient_list_free (&nutrients);
          return;

        case KEY_UP:
          if (scroll > 0)
            scroll--;
          break;

        case KEY_DOWN:
          scroll++;
          break;

        case KEY_PPAGE:
          scroll -= visible;
          break;

        case KEY_NPAGE:
          scroll += visible;
          break;

        case '\t':
          field = 1;
          break;

        case 'l':
        case 'L':
          {
            char *endp;
            double servings;
            errno = 0;
            servings = strtod (srv_buf, &endp);
            if (errno != 0 || endp == srv_buf || *endp != '\0'
                || servings <= 0.0)
              servings = 1.0;
            log_add (log_db, food_code, description,
                     date_buf, servings);
            draw_status (_("Logged! Press any key..."));
            refresh ();
            getch ();
          }
          break;

        default:
          break;
        }
    }
}

/* Show the food search screen.  Let the user type a query, display
   results, and view food details on selection.  */
static void
search_screen (sqlite3 *food_db, sqlite3 *log_db)
{
  char query[SEARCH_BUF_SIZE];
  int qlen;
  struct food_list results;
  int selected;
  int scroll;
  int ch;
  int running;

  memset (query, 0, sizeof query);
  qlen = 0;
  results.items = NULL;
  results.count = 0;
  results.capacity = 0;
  selected = 0;
  scroll = 0;
  running = 1;

  while (running)
    {
      size_t i;
      int row;
      int visible;

      clear ();
      draw_title ();
      draw_status (_("Type to search | Enter: view details | "
                   "Up/Down: select | Esc: back"));

      mvprintw (2, 2, _("Search: %s_"), query);
      row = 4;

      if (results.count > 0)
        {
          visible = LINES - 6;
          if (visible < 1)
            visible = 1;

          /* Keep selected item visible by adjusting scroll.  */
          if (selected < scroll)
            scroll = selected;
          if (selected >= scroll + visible)
            scroll = selected - visible + 1;

          for (i = (size_t) scroll;
               i < results.count && row < LINES - 2; i++)
            {
              if ((int) i == selected)
                attron (A_REVERSE);
              mvprintw (row, 2, " %-8d %-.*s",
                        results.items[i].food_code,
                        COLS - 14,
                        results.items[i].description);
              if ((int) i == selected)
                attroff (A_REVERSE);
              row++;
            }
        }
      else if (qlen > 0)
        {
          mvprintw (row, 2, _("(no results)"));
        }

      refresh ();
      ch = getch ();

      switch (ch)
        {
        case 27:  /* Escape */
          running = 0;
          break;

        case KEY_UP:
          if (selected > 0)
            selected--;
          break;

        case KEY_DOWN:
          if (selected < (int) results.count - 1)
            selected++;
          break;

        case KEY_BACKSPACE:
        case 127:
        case 8:
          if (qlen > 0)
            {
              query[--qlen] = '\0';
              food_list_free (&results);
              selected = 0;
              scroll = 0;
              if (qlen > 0)
                db_search_foods (food_db, query, &results);
            }
          break;

        case '\n':
        case KEY_ENTER:
          if (results.count > 0 && selected < (int) results.count)
            {
              food_detail_screen (food_db, log_db,
                                  results.items[selected].food_code,
                                  results.items[selected].description);
            }
          break;

        default:
          if (ch >= 32 && ch < 127
              && qlen < (int) sizeof query - 1)
            {
              query[qlen++] = (char) ch;
              query[qlen] = '\0';
              food_list_free (&results);
              selected = 0;
              scroll = 0;
              db_search_foods (food_db, query, &results);
            }
          break;
        }
    }

  food_list_free (&results);
}

/* Edit a log entry.  Let the user adjust the servings and date for
   the selected entry.  Returns 1 if the entry was modified.  */
static int
edit_log_entry (sqlite3 *log_db, struct log_entry *entry)
{
  char srv_buf[16];
  char date_buf[16];
  int field;  /* -1 = idle, 0 = editing servings, 1 = editing date  */
  int ch;

  snprintf (srv_buf, sizeof srv_buf, "%.1f", entry->servings);
  strncpy (date_buf, entry->date, sizeof date_buf - 1);
  date_buf[sizeof date_buf - 1] = '\0';
  field = -1;

  for (;;)
    {
      int srv_row;
      int date_row;

      clear ();
      draw_title ();
      if (field < 0)
        draw_status (_("Tab: edit fields | s: save changes | "
                     "Esc: cancel"));
      else
        draw_status (_("Enter/Tab: next field | Esc: cancel edit"));

      attron (A_BOLD);
      mvprintw (2, 2, _("Edit Log Entry: %s"), entry->description);
      attroff (A_BOLD);

      srv_row = 4;
      date_row = 5;

      if (field == 0)
        attron (A_REVERSE);
      mvprintw (srv_row, 2, _("Servings: "));
      if (field == 0)
        attroff (A_REVERSE);
      mvprintw (srv_row, 12, "%-20s", srv_buf);

      if (field == 1)
        attron (A_REVERSE);
      mvprintw (date_row, 2, _("Date:     "));
      if (field == 1)
        attroff (A_REVERSE);
      mvprintw (date_row, 12, "%-20s", format_date (date_buf));

      refresh ();

      if (field == 0)
        {
          ch = read_field (srv_row, 12, srv_buf, (int) sizeof srv_buf);
          if (ch == 27)
            field = -1;
          else
            field = 1;
          continue;
        }
      else if (field == 1)
        {
          char disp_buf[64];
          strncpy (disp_buf, format_date (date_buf), sizeof disp_buf - 1);
          disp_buf[sizeof disp_buf - 1] = '\0';
          ch = read_field (date_row, 12, disp_buf, (int) sizeof disp_buf);
          if (ch != 27)
            {
              if (parse_locale_date (disp_buf, date_buf,
                                     (int) sizeof date_buf) < 0)
                {
                  draw_status (_("Invalid date.  Press any key..."));
                  refresh ();
                  getch ();
                }
            }
          field = -1;
          continue;
        }

      ch = getch ();

      switch (ch)
        {
        case 27:
          return 0;

        case '\t':
          field = 0;
          break;

        case 's':
        case 'S':
          {
            char *endp;
            double servings;
            errno = 0;
            servings = strtod (srv_buf, &endp);
            if (errno != 0 || endp == srv_buf || *endp != '\0'
                || servings <= 0.0)
              {
                draw_status (_("Invalid servings.  Press any key..."));
                refresh ();
                getch ();
                break;
              }
            if (log_update (log_db, entry->id, date_buf, servings) == 0)
              {
                draw_status (_("Updated! Press any key..."));
                refresh ();
                getch ();
                return 1;
              }
            else
              {
                draw_status (_("Error updating! Press any key..."));
                refresh ();
                getch ();
              }
          }
          break;

        default:
          break;
        }
    }
}

/* Show the food log with date navigation.  */
static void
log_screen (sqlite3 *food_db, sqlite3 *log_db, int calories)
{
  struct log_list entries;
  struct date_list dates;
  char date[16];
  int date_idx;
  int selected;
  int ch;
  size_t j;

  strncpy (date, today_date (), sizeof date - 1);
  date[sizeof date - 1] = '\0';

  /* Load all dates that have entries.  */
  if (log_get_dates (log_db, &dates) < 0)
    dates.count = 0;

  /* Find today's position in the date list (or -1).  */
  date_idx = -1;
  for (j = 0; j < dates.count; j++)
    {
      if (strcmp (dates.dates[j], date) == 0)
        {
          date_idx = (int) j;
          break;
        }
    }

  selected = 0;

  while (1)
    {
      size_t i;
      int row;
      int budget_lines;
      int log_start;

      clear ();
      draw_title ();
      draw_status (_("Left/Right: change date | Up/Down: select | "
                   "d: delete | e: edit | Esc: back"));

      /* Show budget for the currently displayed date.  */
      budget_lines = draw_budget (food_db, log_db, 2, calories, date);
      log_start = 2 + budget_lines + 1;

      attron (A_BOLD);
      if (dates.count > 1)
        {
          mvprintw (log_start, 2, _("Food Log for %s (%d/%d)"),
                    format_date (date),
                    date_idx >= 0 ? date_idx + 1 : 0,
                    (int) dates.count);
        }
      else
        {
          mvprintw (log_start, 2, _("Food Log for %s"),
                    format_date (date));
        }
      attroff (A_BOLD);

      row = log_start + 2;
      if (log_get_day (log_db, date, &entries) == 0)
        {
          if (entries.count == 0)
            {
              mvprintw (row, 2, _("(no entries yet)"));
              selected = 0;
            }
          else
            {
              if (selected >= (int) entries.count)
                selected = (int) entries.count - 1;
              if (selected < 0)
                selected = 0;
              mvprintw (row++, 2, _("%-5s %-8s %-6s %s"),
                        _("ID"), _("Code"), _("Srv"), _("Description"));
              mvprintw (row++, 2, "%-5s %-8s %-6s %s",
                        "-----", "--------", "------",
                        "------------------------------------");
              for (i = 0; i < entries.count; i++)
                {
                  if ((int) i == selected)
                    attron (A_REVERSE);
                  mvprintw (row, 2, "%-5d %-8d %5.1f  %-.*s",
                            entries.items[i].id,
                            entries.items[i].food_code,
                            entries.items[i].servings,
                            COLS - 26,
                            entries.items[i].description);
                  if ((int) i == selected)
                    attroff (A_REVERSE);
                  row++;
                  if (row >= LINES - 2)
                    break;
                }
            }
          log_list_free (&entries);
        }

      refresh ();
      ch = getch ();

      switch (ch)
        {
        case 27:  /* Escape  */
          date_list_free (&dates);
          return;

        case KEY_UP:
          if (selected > 0)
            selected--;
          break;

        case KEY_DOWN:
          selected++;
          break;

        case KEY_LEFT:
          if (dates.count > 0)
            {
              if (date_idx > 0)
                date_idx--;
              else if (date_idx < 0 && dates.count > 0)
                date_idx = (int) dates.count - 1;
              if (date_idx >= 0
                  && date_idx < (int) dates.count)
                {
                  strncpy (date, dates.dates[date_idx],
                           sizeof date - 1);
                  date[sizeof date - 1] = '\0';
                }
              selected = 0;
            }
          break;

        case KEY_RIGHT:
          if (dates.count > 0)
            {
              if (date_idx < 0)
                date_idx = 0;
              else if (date_idx < (int) dates.count - 1)
                date_idx++;
              if (date_idx >= 0
                  && date_idx < (int) dates.count)
                {
                  strncpy (date, dates.dates[date_idx],
                           sizeof date - 1);
                  date[sizeof date - 1] = '\0';
                }
              selected = 0;
            }
          break;

        case 'd':
        case 'D':
          {
            /* Delete the selected entry with confirmation.  */
            struct log_list del_entries;
            if (log_get_day (log_db, date, &del_entries) == 0
                && del_entries.count > 0
                && selected < (int) del_entries.count)
              {
                draw_status (_("Delete this entry? (y/n)"));
                refresh ();
                ch = getch ();
                if (ch == 'y' || ch == 'Y')
                  {
                    log_delete (log_db, del_entries.items[selected].id);
                    /* Reload dates.  */
                    date_list_free (&dates);
                    if (log_get_dates (log_db, &dates) < 0)
                      dates.count = 0;
                    /* Re-find date index.  */
                    date_idx = -1;
                    for (j = 0; j < dates.count; j++)
                      {
                        if (strcmp (dates.dates[j], date) == 0)
                          {
                            date_idx = (int) j;
                            break;
                          }
                      }
                    if (selected > 0)
                      selected--;
                  }
                log_list_free (&del_entries);
              }
          }
          break;

        case 'e':
        case 'E':
          {
            /* Edit the selected entry.  */
            struct log_list ed_entries;
            if (log_get_day (log_db, date, &ed_entries) == 0
                && ed_entries.count > 0
                && selected < (int) ed_entries.count)
              {
                if (edit_log_entry (log_db, &ed_entries.items[selected]))
                  {
                    /* Reload dates since date might have changed.  */
                    date_list_free (&dates);
                    if (log_get_dates (log_db, &dates) < 0)
                      dates.count = 0;
                    date_idx = -1;
                    for (j = 0; j < dates.count; j++)
                      {
                        if (strcmp (dates.dates[j], date) == 0)
                          {
                            date_idx = (int) j;
                            break;
                          }
                      }
                  }
                log_list_free (&ed_entries);
              }
          }
          break;

        default:
          break;
        }
    }
}

/* Activity level names for display.  */
static const char *activity_names[] =
{
  N_("Sedentary"),
  N_("Light"),
  N_("Moderate"),
  N_("Very active"),
  N_("Extra active")
};

#define NUM_ACTIVITIES (sizeof (activity_names) / sizeof (activity_names[0]))

/* Profile setup screen.  Returns the new calorie target, or the
   original CALORIES if the user cancels.  */
static int
profile_screen (sqlite3 *log_db, int calories)
{
  struct user_profile prof;
  char age_buf[16];
  char height_buf[16];
  char weight_buf[16];
  int activity_sel;
  int field;  /* 0=age, 1=height, 2=weight, 3=activity  */
  int ch;
  int rc;

  memset (&prof, 0, sizeof prof);
  memset (age_buf, 0, sizeof age_buf);
  memset (height_buf, 0, sizeof height_buf);
  memset (weight_buf, 0, sizeof weight_buf);
  activity_sel = ACTIVITY_SEDENTARY;

  /* Load existing profile if any.  */
  rc = log_get_profile (log_db, &prof);
  if (rc == 0)
    {
      snprintf (age_buf, sizeof age_buf, "%d", prof.age_years);
      snprintf (height_buf, sizeof height_buf, "%.1f", prof.height_cm);
      snprintf (weight_buf, sizeof weight_buf, "%.1f", prof.weight_kg);
      activity_sel = prof.activity_level;
    }

  field = 0;

  for (;;)
    {
      int row;
      int field_rows[4];

      clear ();
      draw_title ();
      draw_status (_("Tab/Enter: next field | Up/Down: activity | "
                   "Esc: cancel | s: save"));

      row = 2;
      attron (A_BOLD);
      mvprintw (row++, 2, _("Profile Setup"));
      attroff (A_BOLD);
      row++;
      mvprintw (row++, 2,
                _("Enter your details to estimate a daily calorie target."));
      mvprintw (row++, 2,
                _("Uses the Mifflin-St Jeor equation (sex-neutral)."));
      row++;

      /* Age field.  */
      field_rows[0] = row;
      if (field == 0)
        attron (A_REVERSE);
      mvprintw (row, 2, _("Age (years):     "));
      if (field == 0)
        attroff (A_REVERSE);
      mvprintw (row++, 20, "%-20s", age_buf);

      /* Height field.  */
      field_rows[1] = row;
      if (field == 1)
        attron (A_REVERSE);
      mvprintw (row, 2, _("Height (cm):     "));
      if (field == 1)
        attroff (A_REVERSE);
      mvprintw (row++, 20, "%-20s", height_buf);

      /* Weight field.  */
      field_rows[2] = row;
      if (field == 2)
        attron (A_REVERSE);
      mvprintw (row, 2, _("Weight (kg):     "));
      if (field == 2)
        attroff (A_REVERSE);
      mvprintw (row++, 20, "%-20s", weight_buf);

      /* Activity field.  */
      field_rows[3] = row;
      if (field == 3)
        attron (A_REVERSE);
      mvprintw (row, 2, _("Activity level:  "));
      if (field == 3)
        attroff (A_REVERSE);
      if (activity_sel >= 0 && activity_sel < (int) NUM_ACTIVITIES)
        mvprintw (row++, 20, "%-20s", _(activity_names[activity_sel]));
      else
        mvprintw (row++, 20, "%-20s", _("Sedentary"));

      row++;
      if (prof.calorie_target > 0)
        mvprintw (row++, 2, _("Current saved target: %d kcal/day"),
                  prof.calorie_target);

      /* If all fields have values, show preview.  */
      if (age_buf[0] && height_buf[0] && weight_buf[0])
        {
          char *endp;
          double h, w;
          errno = 0;
          h = strtod (height_buf, &endp);
          if (errno != 0 || endp == height_buf || *endp != '\0')
            h = 0.0;
          errno = 0;
          w = strtod (weight_buf, &endp);
          if (errno != 0 || endp == weight_buf || *endp != '\0')
            w = 0.0;
          if (h > 0.0 && w > 0.0)
            {
              int est = budget_estimate_calories (atoi (age_buf),
                                                   h, w,
                                                   activity_sel);
              mvprintw (row++, 2,
                        _("Estimated target:     %d kcal/day"), est);
            }
        }

      refresh ();

      if (field < 3)
        {
          char *buf;
          int bufsz;

          if (field == 0)
            { buf = age_buf; bufsz = (int) sizeof age_buf; }
          else if (field == 1)
            { buf = height_buf; bufsz = (int) sizeof height_buf; }
          else
            { buf = weight_buf; bufsz = (int) sizeof weight_buf; }

          ch = read_field (field_rows[field], 20, buf, bufsz);
          if (ch == 27)
            return calories;
          field++;
        }
      else
        {
          /* Activity selector.  */
          ch = getch ();
          if (ch == 27)
            return calories;
          if (ch == KEY_UP && activity_sel > 0)
            activity_sel--;
          else if (ch == KEY_DOWN
                   && activity_sel < (int) NUM_ACTIVITIES - 1)
            activity_sel++;
          else if (ch == 's' || ch == 'S')
            {
              /* Save profile.  */
              if (!age_buf[0] || !height_buf[0] || !weight_buf[0])
                {
                  draw_status (_("All fields required! Press any key..."));
                  refresh ();
                  getch ();
                }
              else
                {
                  int est;
                  char *endp;
                  prof.age_years = atoi (age_buf);
                  errno = 0;
                  prof.height_cm = strtod (height_buf, &endp);
                  if (errno != 0 || endp == height_buf
                      || *endp != '\0' || prof.height_cm <= 0.0)
                    {
                      draw_status (_("Invalid height! Press any key..."));
                      refresh ();
                      getch ();
                      continue;
                    }
                  errno = 0;
                  prof.weight_kg = strtod (weight_buf, &endp);
                  if (errno != 0 || endp == weight_buf
                      || *endp != '\0' || prof.weight_kg <= 0.0)
                    {
                      draw_status (_("Invalid weight! Press any key..."));
                      refresh ();
                      getch ();
                      continue;
                    }
                  prof.activity_level = activity_sel;
                  est = budget_estimate_calories (prof.age_years,
                                                  prof.height_cm,
                                                  prof.weight_kg,
                                                  prof.activity_level);
                  prof.calorie_target = est;

                  if (log_save_profile (log_db, &prof) < 0)
                    {
                      draw_status (_("Error saving! Press any key..."));
                      refresh ();
                      getch ();
                    }
                  else
                    {
                      draw_status (_("Profile saved! Press any key..."));
                      refresh ();
                      getch ();
                      return est;
                    }
                }
            }
          else if (ch == '\n' || ch == KEY_ENTER || ch == '\t')
            field = 0;  /* cycle back to first field  */
        }
    }
}

int
ui_run (sqlite3 *food_db, sqlite3 *log_db, int calories)
{
  int ch;
  int running;

  initscr ();
  cbreak ();
  noecho ();
  keypad (stdscr, TRUE);
  curs_set (0);

  running = 1;
  while (running)
    {
      clear ();
      draw_title ();
      draw_status (_("s: Search foods | l: View log | "
                   "p: Profile | q: Quit"));

      draw_budget (food_db, log_db, 2, calories, today_date ());

      mvprintw (LINES - 3, 2,
                _("[s] Search   [l] Log   [p] Profile   [q] Quit"));

      refresh ();
      ch = getch ();

      switch (ch)
        {
        case 's':
        case 'S':
          search_screen (food_db, log_db);
          break;

        case 'l':
        case 'L':
          log_screen (food_db, log_db, calories);
          break;

        case 'p':
        case 'P':
          calories = profile_screen (log_db, calories);
          break;

        case 'q':
        case 'Q':
          running = 0;
          break;

        default:
          break;
        }
    }

  endwin ();
  return 0;
}
