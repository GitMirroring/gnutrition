#  GNUtrition - a nutrition and diet analysis program.
#  Copyright(C) 2000 - 2002 Edgar Denny (edenny@skyweb.net)
#  Copyright (C) 2010 2012 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from __future__ import absolute_import
import gtk
import string
from . import recipe_win_ui
from . import gnutr
from . import gnutr_consts
from . import store
from . import database
from . import help
from .util.log import LOG as log
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

class RecipeWin:
    def __init__(self, app, parent):
        self.ui = recipe_win_ui.RecipeWinUI()
        self.connect_signals()
        self.app = app
        self.db = database.Database()
        self.num_ingr = 0
        self.parent = parent
        self.store = store.Store()
        # Dirty flag. Set True if user edits a recipe. If not set do not ask
        # 'Do you wish to save' the recipe.
        self.dirty = False
        self.ui.category_combo.set_rows(self.store.cat_desc_tuple)

    def connect_signals(self):
        self.ui.add_button.connect('clicked', self.on_add_released)
        self.ui.edit_button.connect('clicked', self.on_edit_released)
        self.ui.delete_button.connect('clicked', self.on_delete_released)
        self.ui.open_button.connect('clicked', self.on_open_released)
        self.ui.nutr_button.connect('clicked', self.on_nutr_released)
        self.ui.goal_button.connect('clicked', self.on_goal_released)
        self.ui.clear_button.connect('clicked', self.on_clear_released)
        self.ui.save_button.connect('clicked', self.on_save_released)

        self.ui.exit_item.connect('activate', self.on_exit_activate)
        self.ui.about_item.connect('activate', self.on_about_activate)
        self.ui.plan_item.connect('activate', self.on_plan_view_activate)
        self.ui.food_item.connect('activate', self.on_food_view_activate)

        self.ui.manual_item.connect('activate', self.on_manual_activate)
        self.ui.nutrient_goal_item.connect('activate', self.on_goal_released)
        self.ui.add_food_item.connect('activate', self.on_add_released)
        self.ui.edit_food_item.connect('activate', self.on_edit_released)
        self.ui.delete_food_item.connect('activate', self.on_delete_released)
        self.ui.clear_item.connect('activate', self.on_clear_released)
        self.ui.open_item.connect('activate', self.on_open_released)
        self.ui.save_item.connect('activate', self.on_save_released)
        self.ui.save_as_item.connect('activate', self.on_save_as_activate)

        self.ui.hide_instr_item.connect('toggled', self.on_instr_toggled)

    def on_instr_toggled(self, w, d=None):
        if w.active:
            self.ui.vbox2.hide()
        else:
            self.ui.vbox2.show()

    def on_manual_activate(self, w, d=None):
        help.open('')

    def on_save_as_activate(self, w, d=None):
        recipe = self.get_recipe()
        if not hasattr(self, 'file_select_dlg'):
            from . import file_select_dlg
            self.file_select_dlg = file_select_dlg.FileSelectDlg()
        if recipe:
            if not hasattr(self, 'nutr_composition_dlg'):
                from . import nutr_composition_dlg
                self.nutr_composition_dlg = \
                    nutr_composition_dlg.NutrCompositionDlg()
            nutr_list = self.nutr_composition_dlg.compute_nutr_total(recipe)
            pcnt_cal = self.nutr_composition_dlg.compute_pcnt_calories()
            self.file_select_dlg.show(recipe, nutr_list, pcnt_cal)

    def on_add_released(self, w, d=None):
        if not hasattr(self, 'food_srch_dlg'):
            from . import food_srch_dlg
            self.food_srch_dlg = food_srch_dlg.FoodSrchDlg(self.app)
        self.food_srch_dlg.show(gnutr_consts.RECIPE)

    def on_edit_released(self, w, d=None):
        (treemodel, iter) = self.ui.selection.get_selected()
        if not iter:
            gnutr.Dialog('warn', 'No ingredient is selected.', self.parent)
        else:
            ingr = treemodel.get_value(iter, 3)
            if not hasattr(self, 'food_edit_dlg'):
                from . import food_edit_dlg
                self.food_edit_dlg = food_edit_dlg.FoodEditDlg(self.app)
            self.food_edit_dlg.show(ingr, gnutr_consts.RECIPE)

    def on_exit_activate(self, w, d=None):
        if len(self.get_ingredient_list()) > 0 or self.prep_description():
            if self.is_dirty():
                self.ask_save("Save your work?")
        gtk.main_quit()
    
    def on_about_activate(self, w, d=None):
        r = self.app.base_win.ui.about_dlg.run()
        if r == gtk.RESPONSE_CANCEL or r == gtk.RESPONSE_DELETE_EVENT:
            self.app.base_win.ui.about_dlg.hide()

    def on_open_released(self, w, d=None):
        if not hasattr(self, 'recipe_srch_dlg'):
            from . import recipe_srch_dlg
            self.recipe_srch_dlg = recipe_srch_dlg.RecipeSrchDlg(self.app)
        self.recipe_srch_dlg.show()

    def on_delete_released(self, w, d=None):
        (treemodel, iter) = self.ui.selection.get_selected()
        if not iter:
            gnutr.Dialog('warn', 'No ingredient is selected.', self.parent)
        else:
            self.ui.treemodel.remove(iter)
            self.num_ingr = self.num_ingr - 1
            self.dirty = True

    def on_nutr_released(self, w, d=None):
        if self.num_ingr == 0:
            gnutr.Dialog('warn', 'There are no ingredients.', self.parent)
            return
        num_serv = self.ui.num_serv_entry.get_text()
        try:
            num = int(num_serv)
        except ValueError:
            gnutr.Dialog('warn', 
"""The number of servings in not specified +
or is not a number.""", self.parent)
            return
        r = self.get_recipe()
        if not r:
            return
        if not hasattr(self, 'nutr_composition_dlg'):
            from . import nutr_composition_dlg
            self.nutr_composition_dlg = \
                nutr_composition_dlg.NutrCompositionDlg()
        self.nutr_composition_dlg.show(recipe=r)

    def replace_ingredient(self, ingr):
        (treemodel, iter) = self.ui.selection.get_selected()
        self.ui.treemodel.set_value(iter, 0, ingr.amount)
        self.ui.treemodel.set_value(iter, 1, ingr.msre_desc)
        self.ui.treemodel.set_value(iter, 3, ingr)
        self.dirty = True

    def on_goal_released(self, w, d=None):
        if not hasattr(self, 'nutr_goal_dlg'):
            from . import nutr_goal_dlg
            self.nutr_goal_dlg = nutr_goal_dlg.NutrGoalDlg()
        self.nutr_goal_dlg.show()

    def ask_save(self, mesg):
            dlg = gnutr.Dialog('question', mesg, self.parent)
            reply = dlg.run()
            if reply == gtk.RESPONSE_YES:
                dlg.destroy()
                self.on_save_released(None)
            else:
                info('Not saving changes.')
                dlg.destroy()

    def on_clear_released(self, w, d=None):
        if self.num_ingr != 0 and self.is_dirty():
            self.ask_save("""You are about to clear the recipe. 
Do you wish to save it first?""")
        self.ui.recipe_entry.set_text('')
        self.ui.num_serv_entry.set_text('')
        self.ui.treemodel.clear()
        self.num_ingr = 0
        self.ui.text_buffer.set_text('')
        self.dirty = False

    def check_recipe_exists(self, recipe_name):
        self.db.query("""SELECT recipe_no FROM recipe WHERE
            recipe_name = '%s'""" % (recipe_name))
        return self.db.get_single_result()

    def save_recipe(self, recipe):
        info('Saving recipe: {0:s}'.format(recipe.desc))
        recipe_no = self.db.next_row('recipe_no', 'recipe')
        debug("*** TYPES ***")
        debug('recipe_no'.format(type(recipe_no)))
        debug('recipe.desc'.format(type(recipe.desc)))
        debug('recipe.num_serv'.format(type(recipe.num_serv)))
        debug('self.num_ingr'.format(type(self.num_ingr)))
        debug('recipe.cat_num'.format(type(recipe.cat_num)))
        debug("*** END TYPES ***")
        self.db.query("INSERT INTO recipe VALUES" +
            "(%d, '%s', %d, %d, %d)" % (recipe_no, recipe.desc,
            recipe.num_serv, self.num_ingr, recipe.cat_num),
            caller='RecipeWin.save_recipe')

        for ingr in recipe.ingr_list:
            self.db.query("INSERT INTO ingredient VALUES" +
                "({0:d}, {1:f}, '{2:s}', '{3:s}')".format(recipe_no,
                ingr.amount, ingr.msre_desc, ingr.food_num),
                caller='RecipeWin.save_recipe')

        self.db.query("INSERT INTO preparation VALUES (%d, 0.0, '%s')"
            % (recipe_no, recipe.prep_desc), caller='RecipeWin.save_recipe')
        self.dirty = False

    def delete_recipe(self, recipe_name):
        self.db.query("""SELECT recipe_no FROM recipe
            WHERE recipe_name = '%s'""" %(recipe_name))
        recipe_num = str(self.db.get_single_result())
        self.db.query("DELETE FROM recipe WHERE recipe_no = %d" 
            % (recipe_num))
        self.db.query("DELETE FROM ingredient WHERE recipe_no = %d" 
            % (recipe_num))
        self.db.query("DELETE FROM recipe_plan WHERE recipe_no = %d" 
            % (recipe_num))
        self.db.query("DELETE FROM preparation WHERE recipe_no = %d"
            % (recipe_num))

    def prep_description(self):
        start = self.ui.text_buffer.get_start_iter();
        end = self.ui.text_buffer.get_end_iter();
        return self.ui.text_buffer.get_text(start, end, True)

    def grab_window(self):
        desc = self.ui.recipe_entry.get_text()
        num_serv = self.ui.num_serv_entry.get_text()
        cat_desc = self.ui.category_combo.get_active_text()
        prep_desc = self.prep_description()
        return (desc,int(num_serv),cat_desc,prep_desc)

    def get_recipe(self):
        r = gnutr.Recipe()
        (r.desc,r.num_serv,r.cat_desc,r.prep_desc) = self.grab_window()

        if not r.desc:
            gnutr.Dialog('error', 'No recipe name is defined.', self.parent)
            return None

        if not r.num_serv:
            gnutr.Dialog('error', 
"""The number of servings is not defined,
or is not a number.""", self.parent)
            return None

        r.cat_num = self.store.cat_desc2num[r.cat_desc]
        r.ingr_list = self.get_ingredient_list()
        return r

    def empty_window(self):
        (desc, num_serv, cat_desc, prep_desc) = self.grab_window()
        if desc or num_serv or cat_desc or prep_desc: return False
        return True
       
    def is_dirty(self):
        if self.dirty:
            debug('Dirty flag is set.')
            return True
        if self.empty_window():
            debug('No recipe displayed.')
            return False
        showing = self.get_recipe()
        if not showing: return False
        if not self.check_recipe_exists(showing.desc):
            debug('Recipe name has changed.')
            return True

        self.db.query("""SELECT recipe_no, no_serv, category_no FROM recipe
            WHERE recipe_name = '%s'""" % (showing.desc))
        (recipe_no, no_serv, category_no) = self.db.get_row_result()
        debug('recipe_no: {0:d}'.format(recipe_no))
        debug('no_serv: {0:d}'.format(no_serv))
        debug('category_no: {0:d}'.format(category_no))
        if int(no_serv) != int(showing.num_serv):
            debug('Number of servings has changed from {0:d} to {1:d}.'.format(
                    int(no_serv), int(showing.num_serv)))
            return True

        if category_no != showing.cat_num:
            debug('Category has changed.')
            return True

        self.db.query("""SELECT prep_desc FROM preparation
                        WHERE recipe_no = {0:d}""".format(recipe_no))
        prep_desc = self.db.get_single_result()

        start = self.ui.text_buffer.get_start_iter();
        end = self.ui.text_buffer.get_end_iter();
        curr_prep_desc = self.prep_description()
        if prep_desc != curr_prep_desc:
            debug('Recipe Instructions have changed.')
            return True
        return False

    def on_save_released(self, w, d=None):
        recipe = self.get_recipe()
        if not recipe:
            debug('No recipe to save.')
            return
        recipe.num = self.check_recipe_exists(recipe.desc)
        if recipe.num:
            dlg = gnutr.Dialog('question', 
"""A recipe with the same name exists
in the database. Do you want to overwrite it?""", self.parent)
            reply = dlg.run()
            if reply == gtk.RESPONSE_YES:
                dlg.destroy()
                self.delete_recipe(recipe.desc)
                debug('Saving changes to recipe.')
                self.save_recipe(recipe)
            else:
                dlg.destroy()
        else:
            debug('Saving new recipe.')
            self.save_recipe(recipe)

    def add_ingredient(self, ingr):
        match = 0
        ret = True
        iter1 = self.ui.treemodel.get_iter_root()
        if self.num_ingr > 0:
            while ret:
                try:
                    ingr_in_recipe = self.ui.treemodel.get_value(iter1, 3)
                except TypeError:
                    break
                if ingr_in_recipe.food_num == ingr.food_num:
                    match = 1
                    break
                ret = self.ui.treemodel.iter_next(iter1)

        if match == 0:
            iter2 = self.ui.treemodel.append()
            self.ui.treemodel.set_value(iter2, 0, ingr.amount)
            self.ui.treemodel.set_value(iter2, 1, ingr.msre_desc)
            self.ui.treemodel.set_value(iter2, 2, ingr.food_desc)
            self.ui.treemodel.set_value(iter2, 3, ingr)
        else:
            iter3 = self.ui.treemodel.insert_after(iter1) 
            self.ui.treemodel.set_value(iter3, 0, ingr.amount)
            self.ui.treemodel.set_value(iter3, 1, ingr.msre_desc)
            self.ui.treemodel.set_value(iter3, 2, ingr.food_desc)
            self.ui.treemodel.set_value(iter3, 3, ingr)
            self.ui.treemodel.remove(iter1)
        self.num_ingr = self.num_ingr + 1
        self.dirty = True

    def update(self, recipe):
        self.num_ingr = 0
        self.ui.treemodel.clear()

        for ingr in recipe.ingr_list:
            iter = self.ui.treemodel.append()
            self.ui.treemodel.set_value(iter, 0, ingr.amount)
            self.ui.treemodel.set_value(iter, 1, ingr.msre_desc)
            self.ui.treemodel.set_value(iter, 2, ingr.food_desc)
            self.ui.treemodel.set_value(iter, 3, ingr)
            self.num_ingr = self.num_ingr + 1

        self.ui.recipe_entry.set_text(recipe.desc)
        self.ui.num_serv_entry.set_text(str(recipe.num_serv))
        self.ui.category_combo.set_active_text(recipe.cat_desc)
        if recipe.prep_desc:
            self.ui.text_buffer.set_text(recipe.prep_desc) 

    def get_ingredient_list(self):
        ingr_list = []
        # HERE: This was misbehaving.
        #ret = True
        #iter = self.ui.treemodel.get_iter_root()
        #while ret:
        #    ingr = self.ui.treemodel.get_value(iter, 3)
            #print 'ingredient', ingr
        #    ingr_list.append(ingr)
        #    ret = self.ui.treemodel.iter_next(iter)
        for row in self.ui.treemodel:
            ingr_list.append(row[3])
        return ingr_list

    def on_plan_view_activate(self, w, d=None):
        self.app.base_win.on_plan_button_released(None)

    def on_food_view_activate(self, w, d=None):
        self.app.base_win.on_food_button_released(None)
