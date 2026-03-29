/* dbus.c - D-Bus interface for GNUtrition
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

#include "dbus.h"
#include "budget.h"
#include "db.h"
#include "log.h"
#include "i18n.h"

#include <string.h>

/* D-Bus introspection XML for org.gnu.gnutrition.Manager.  */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnu.gnutrition.Manager'>"
  "    <method name='SearchFoods'>"
  "      <arg type='s' name='query' direction='in'/>"
  "      <arg type='a(is)' name='results' direction='out'/>"
  "    </method>"
  "    <method name='GetNutrientInfo'>"
  "      <arg type='i' name='food_code' direction='in'/>"
  "      <arg type='a{sd}' name='nutrients' direction='out'/>"
  "    </method>"
  "    <method name='GetFPEDInfo'>"
  "      <arg type='i' name='food_code' direction='in'/>"
  "      <arg type='a{sd}' name='fped' direction='out'/>"
  "    </method>"
  "    <method name='LogFood'>"
  "      <arg type='i' name='food_code' direction='in'/>"
  "      <arg type='d' name='quantity' direction='in'/>"
  "      <arg type='s' name='date' direction='in'/>"
  "      <arg type='b' name='success' direction='out'/>"
  "    </method>"
  "    <method name='GetLogForDate'>"
  "      <arg type='s' name='date' direction='in'/>"
  "      <arg type='a(ids)' name='entries' direction='out'/>"
  "    </method>"
  "    <method name='GetBudgetStatus'>"
  "      <arg type='s' name='date' direction='in'/>"
  "      <arg type='a{s(dd)}' name='status' direction='out'/>"
  "    </method>"
  "    <method name='SetUserProfile'>"
  "      <arg type='i' name='age' direction='in'/>"
  "      <arg type='d' name='height' direction='in'/>"
  "      <arg type='d' name='weight' direction='in'/>"
  "      <arg type='i' name='activity' direction='in'/>"
  "      <arg type='i' name='calorie_target' direction='out'/>"
  "    </method>"
  "    <method name='GetUserProfile'>"
  "      <arg type='(iddii)' name='profile' direction='out'/>"
  "    </method>"
  "    <method name='SetCalorieTarget'>"
  "      <arg type='i' name='calories' direction='in'/>"
  "    </method>"
  "    <method name='DeleteLogEntry'>"
  "      <arg type='i' name='entry_id' direction='in'/>"
  "      <arg type='b' name='success' direction='out'/>"
  "    </method>"
  "    <method name='UpdateLogEntry'>"
  "      <arg type='i' name='entry_id' direction='in'/>"
  "      <arg type='d' name='quantity' direction='in'/>"
  "      <arg type='s' name='date' direction='in'/>"
  "      <arg type='b' name='success' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static GDBusNodeInfo *introspection_data = NULL;

/* Handle a SearchFoods call.  */
static void
handle_search_foods (struct dbus_context *ctx,
                     GVariant *parameters,
                     GDBusMethodInvocation *invocation)
{
  const gchar *query;
  struct food_list results;
  GVariantBuilder builder;
  size_t i;

  g_variant_get (parameters, "(&s)", &query);

  if (db_search_foods (ctx->food_db, query, &results) < 0)
    {
      g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
        "Database search failed");
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(is)"));
  for (i = 0; i < results.count; i++)
    g_variant_builder_add (&builder, "(is)",
                           results.items[i].food_code,
                           results.items[i].description);
  food_list_free (&results);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(a(is))", &builder));
}

/* Handle a GetNutrientInfo call.  */
static void
handle_get_nutrient_info (struct dbus_context *ctx,
                          GVariant *parameters,
                          GDBusMethodInvocation *invocation)
{
  gint food_code;
  struct nutrient_list nutrients;
  GVariantBuilder builder;
  size_t i;

  g_variant_get (parameters, "(i)", &food_code);

  if (db_get_nutrients (ctx->food_db, food_code, &nutrients) < 0)
    {
      g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
        "Nutrient lookup failed");
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sd}"));
  for (i = 0; i < nutrients.count; i++)
    g_variant_builder_add (&builder, "{sd}",
                           nutrients.items[i].name,
                           nutrients.items[i].value);
  nutrient_list_free (&nutrients);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(a{sd})", &builder));
}

/* Handle a GetFPEDInfo call.  */
static void
handle_get_fped_info (struct dbus_context *ctx,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation)
{
  gint food_code;
  struct fped_entry fped;
  GVariantBuilder builder;
  int rc;

  g_variant_get (parameters, "(i)", &food_code);

  rc = db_get_fped (ctx->food_db, food_code, &fped);
  if (rc < 0)
    {
      g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
        "FPED lookup failed");
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sd}"));
  if (rc == 0)
    {
      g_variant_builder_add (&builder, "{sd}", "Vegetables",
                             fped.vegetables);
      g_variant_builder_add (&builder, "{sd}", "Fruits", fped.fruits);
      g_variant_builder_add (&builder, "{sd}", "Grains", fped.grains);
      g_variant_builder_add (&builder, "{sd}", "Dairy", fped.dairy);
      g_variant_builder_add (&builder, "{sd}", "Protein", fped.protein);
      g_variant_builder_add (&builder, "{sd}", "Oils", fped.oils);
    }

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(a{sd})", &builder));
}

/* Handle a LogFood call.  */
static void
handle_log_food (struct dbus_context *ctx,
                 GVariant *parameters,
                 GDBusMethodInvocation *invocation)
{
  gint food_code;
  gdouble quantity;
  const gchar *date;
  struct food_list results;
  const char *desc;
  size_t j;
  gboolean success;

  g_variant_get (parameters, "(ids)", &food_code, &quantity, &date);

  /* Look up food description.  */
  desc = _("Unknown food");
  if (db_search_foods (ctx->food_db, "", &results) == 0)
    {
      for (j = 0; j < results.count; j++)
        {
          if (results.items[j].food_code == food_code)
            {
              desc = results.items[j].description;
              break;
            }
        }
    }

  success = (log_add (ctx->log_db, food_code, desc, date, quantity) == 0);
  food_list_free (&results);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(b)", success));
}

/* Handle a GetLogForDate call.  */
static void
handle_get_log_for_date (struct dbus_context *ctx,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  const gchar *date;
  struct log_list entries;
  GVariantBuilder builder;
  size_t i;

  g_variant_get (parameters, "(&s)", &date);

  if (log_get_day (ctx->log_db, date, &entries) < 0)
    {
      g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
        "Log retrieval failed");
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ids)"));
  for (i = 0; i < entries.count; i++)
    g_variant_builder_add (&builder, "(ids)",
                           entries.items[i].food_code,
                           entries.items[i].servings,
                           entries.items[i].description);
  log_list_free (&entries);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(a(ids))", &builder));
}

/* Handle a GetBudgetStatus call.  */
static void
handle_get_budget_status (struct dbus_context *ctx,
                          GVariant *parameters,
                          GDBusMethodInvocation *invocation)
{
  const gchar *date;
  struct daily_budget budget;
  struct daily_budget consumed;
  struct log_list entries;
  GVariantBuilder builder;
  size_t i;

  g_variant_get (parameters, "(&s)", &date);

  budget = budget_for_calories (ctx->calories);
  memset (&consumed, 0, sizeof consumed);

  if (log_get_day (ctx->log_db, date, &entries) == 0)
    {
      for (i = 0; i < entries.count; i++)
        {
          struct fped_entry fped;
          if (db_get_fped (ctx->food_db, entries.items[i].food_code,
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

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{s(dd)}"));
  g_variant_builder_add (&builder, "{s(dd)}", "Vegetables",
                         budget.vegetables, consumed.vegetables);
  g_variant_builder_add (&builder, "{s(dd)}", "Fruits",
                         budget.fruits, consumed.fruits);
  g_variant_builder_add (&builder, "{s(dd)}", "Grains",
                         budget.grains, consumed.grains);
  g_variant_builder_add (&builder, "{s(dd)}", "Dairy",
                         budget.dairy, consumed.dairy);
  g_variant_builder_add (&builder, "{s(dd)}", "Protein",
                         budget.protein, consumed.protein);
  g_variant_builder_add (&builder, "{s(dd)}", "Oils",
                         budget.oils, consumed.oils);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(a{s(dd)})", &builder));
}

/* Handle a SetUserProfile call.  */
static void
handle_set_user_profile (struct dbus_context *ctx,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  gint age, activity;
  gdouble height, weight;
  struct user_profile prof;

  g_variant_get (parameters, "(iddi)", &age, &height, &weight, &activity);

  prof.age_years = age;
  prof.height_cm = height;
  prof.weight_kg = weight;
  prof.activity_level = activity;
  prof.calorie_target = budget_estimate_calories (age, height, weight,
                                                   activity);

  if (log_save_profile (ctx->log_db, &prof) < 0)
    {
      g_dbus_method_invocation_return_error (invocation,
        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
        "Failed to save profile");
      return;
    }

  ctx->calories = prof.calorie_target;

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(i)", prof.calorie_target));
}

/* Handle a GetUserProfile call.  */
static void
handle_get_user_profile (struct dbus_context *ctx,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  struct user_profile prof;
  int rc;

  (void) parameters;

  rc = log_get_profile (ctx->log_db, &prof);
  if (rc != 0)
    {
      /* No profile saved yet; return zeros.  */
      memset (&prof, 0, sizeof prof);
    }

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("((iddii))",
                   prof.age_years,
                   prof.height_cm,
                   prof.weight_kg,
                   prof.activity_level,
                   prof.calorie_target));
}

/* Handle a SetCalorieTarget call.  */
static void
handle_set_calorie_target (struct dbus_context *ctx,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation)
{
  gint cal;

  g_variant_get (parameters, "(i)", &cal);
  ctx->calories = budget_round_to_pattern (cal);

  g_dbus_method_invocation_return_value (invocation, NULL);
}

/* Handle a DeleteLogEntry call.  */
static void
handle_delete_log_entry (struct dbus_context *ctx,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  gint entry_id;
  gboolean success;

  g_variant_get (parameters, "(i)", &entry_id);
  success = (log_delete (ctx->log_db, entry_id) == 0);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(b)", success));
}

/* Handle an UpdateLogEntry call.  */
static void
handle_update_log_entry (struct dbus_context *ctx,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
  gint entry_id;
  gdouble quantity;
  const gchar *date;
  gboolean success;

  g_variant_get (parameters, "(ids)", &entry_id, &quantity, &date);
  success = (log_update (ctx->log_db, entry_id, date, quantity) == 0);

  g_dbus_method_invocation_return_value (invocation,
    g_variant_new ("(b)", success));
}

/* Dispatch incoming D-Bus method calls.  */
static void
handle_method_call (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
  struct dbus_context *ctx = user_data;

  (void) connection;
  (void) sender;
  (void) object_path;
  (void) interface_name;

  if (g_strcmp0 (method_name, "SearchFoods") == 0)
    handle_search_foods (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "GetNutrientInfo") == 0)
    handle_get_nutrient_info (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "GetFPEDInfo") == 0)
    handle_get_fped_info (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "LogFood") == 0)
    handle_log_food (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "GetLogForDate") == 0)
    handle_get_log_for_date (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "GetBudgetStatus") == 0)
    handle_get_budget_status (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "SetUserProfile") == 0)
    handle_set_user_profile (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "GetUserProfile") == 0)
    handle_get_user_profile (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "SetCalorieTarget") == 0)
    handle_set_calorie_target (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "DeleteLogEntry") == 0)
    handle_delete_log_entry (ctx, parameters, invocation);
  else if (g_strcmp0 (method_name, "UpdateLogEntry") == 0)
    handle_update_log_entry (ctx, parameters, invocation);
  else
    g_dbus_method_invocation_return_error (invocation,
      G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
      "Unknown method: %s", method_name);
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,  /* get_property  */
  NULL,  /* set_property  */
  { NULL }
};

/* Called when the bus name is acquired.  Register our object.  */
static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
  GError *error = NULL;

  (void) name;

  g_dbus_connection_register_object (connection,
    "/org/gnu/gnutrition/Manager",
    introspection_data->interfaces[0],
    &interface_vtable,
    user_data, NULL, &error);

  if (error)
    {
      g_printerr ("gnutrition: D-Bus register error: %s\n", error->message);
      g_error_free (error);
    }
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
  (void) connection;
  (void) name;
  (void) user_data;
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
  (void) connection;
  (void) user_data;
  g_printerr ("gnutrition: lost D-Bus name '%s'\n", name);
}

guint
dbus_service_start (struct dbus_context *ctx)
{
  guint owner_id;

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml,
                                                      NULL);
  if (!introspection_data)
    {
      g_printerr ("gnutrition: failed to parse D-Bus introspection XML\n");
      return 0;
    }

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                              "org.gnu.gnutrition",
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              ctx, NULL);
  return owner_id;
}

void
dbus_service_stop (guint owner_id)
{
  if (owner_id > 0)
    g_bus_unown_name (owner_id);
  if (introspection_data)
    {
      g_dbus_node_info_unref (introspection_data);
      introspection_data = NULL;
    }
}
