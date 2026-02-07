# GNUtrition - a nutrition and diet analysis program.
# Copyright (C) 2000-2002 Edgar Denny (edenny@skyweb.net)
# Copyright (C) 2012, 2026 Free Software Foundation, Inc.
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

from __future__ import absolute_import
from __future__ import print_function
import wx
from . import nutr_composition_dlg_ui
from . import store
from . import database
from . import help
from .util.log import LOG as log
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

class NutrCompositionDlg:
    def __init__(self):
        self.ui = nutr_composition_dlg_ui.NutrCompositionDlgUI()
        self.db = database.Database()
        self.store = store.Store()
        self.list_nutr_tot = []

        self.ui.dialog.connect('response', self.on_response)
        
    def show(self, recipe=None, nutr_list=None):
        if recipe:
            self.compute_recipe(recipe)
        elif nutr_list:
            self.compute_total(nutr_list)

        self.update()
        self.ui.dialog.vbox.show_all()
        self.ui.dialog.run()

    def on_response(self, w, r, d=None):
        if r == gtk.RESPONSE_HELP:
            help.open('')
        if r == gtk.RESPONSE_CANCEL or r == gtk.RESPONSE_DELETE_EVENT:
            self.ui.dialog.hide()

    def compute_recipe(self, recipe):
        self.compute_nutr_total(recipe)
        self.list_pcnt_goal = self.compute_pcnt_nutr_goal()

    def compute_total(self, nutr_list):
        self.list_nutr_tot = nutr_list
        self.list_pcnt_goal = self.compute_pcnt_nutr_goal()

    def compute_food(self, amount, msre_desc, food_num):
        nutr_num_list = self.store.nutr_num_list
        self.list_nutr_tot[:] = []

        for nutr_num in nutr_num_list:
            self.list_nutr_tot.append((nutr_num, 0.000))

        self.add_food_to_nutr_total(amount, msre_desc, food_num)
        self.list_pcnt_goal = self.compute_pcnt_nutr_goal()
        self.update()

    def update(self):
        self.reset()
        nutr_num2val_dict = {}
        for num, val in self.list_nutr_tot:
            nutr_num2val_dict[num] = val
        
        for n in self.ui.nutr_list:
            if str(n.num) in nutr_num2val_dict.keys():
                n.entry_amount.set_text('%.3f' %(nutr_num2val_dict[str(n.num)]))
                
        pcnt_num2val_dict = {}
        for num, val in self.list_pcnt_goal:
            pcnt_num2val_dict[num] = val
        
        for n in self.ui.nutr_list:
            if str(n.num) in pcnt_num2val_dict.keys():
                n.entry_pcnt.set_text('%.3f' %(pcnt_num2val_dict[str(n.num)]))
        
        protein, fat, carbs = self.compute_pcnt_calories()
        self.ui.protein_entry.set_text('%.3f' %(protein))
        self.ui.fat_entry.set_text('%.3f' %(fat))
        self.ui.carb_entry.set_text('%.3f' %(carbs))

    def add_food_to_nutr_total(self, amount, msre_desc, food_num):

        self.db.query("SELECT Nutr_No, Nutr_Val FROM nut_data " +
            "WHERE NDB_No = '%s'" % (food_num))
        list_food_nutr = self.db.get_result()

        self.db.query("SELECT Gm_wgt FROM weight " +
            "WHERE NDB_No = '{0:s}'  AND Msre_Desc ='{1:s}'".format(
                    food_num, msre_desc))
        gm_per_msre = self.db.get_single_result()

        for i in range(len(self.list_nutr_tot)):
            tot_nutr_num, tot_nutr_val = self.list_nutr_tot[i]
            for j in range(len(list_food_nutr)):
                fd_nutr_num, fd_nutr_val = list_food_nutr[j]
                if tot_nutr_num == fd_nutr_num:
                    total = tot_nutr_val
                    total = total + (float(amount) * gm_per_msre
                        * fd_nutr_val / 100.0)
                    self.list_nutr_tot[i] = (tot_nutr_num, total)

    def compute_pcnt_calories(self):
        #dict = self.store.nutr_desc2num #wtf?
        c,cals_protein,cals_fat,cals_carb = 0,0,0,0
        for nutr_num, nutr_val in self.list_nutr_tot:
            if nutr_num == '203':
                cals_protein = nutr_val * 4.0
                c = c + 1
            elif nutr_num == '204':
                cals_fat = nutr_val * 9.0
                c = c + 1
            elif nutr_num == '205':
                cals_carb = nutr_val * 4.0
                c = c + 1
            if c == 3:
                break
        tot = cals_protein + cals_fat + cals_carb
        if tot == 0.0:
            return (0.0, 0.0, 0.0)
        else:
            return (cals_protein * 100.0/tot, cals_fat * 100.0/tot,
                cals_carb * 100.0/tot)

    def compute_nutr_total(self, recipe):
        list_nutr_num = self.store.nutr_num_list

        # initialize to zero
        self.list_nutr_tot[:] = []
        for nutr_num in list_nutr_num:
            self.list_nutr_tot.append((nutr_num, 0.000))

        info('compute_nutr_total(recipe):')
        # iterate over ingredients
        for ingr in recipe.ingr_list:
            info('  amount: {0:d} msre_desc: {1:s} food_num: {2:s}'.format(
                                ingr.amount, ingr.msre_desc, ingr.food_num))
            self.add_food_to_nutr_total(ingr.amount, ingr.msre_desc, ingr.food_num)

        # divide by the number of servings
        for i in range(len(self.list_nutr_tot)):
            nutr_num, nutr_val = self.list_nutr_tot[i]
            total = nutr_val/float(recipe.num_serv)
            self.list_nutr_tot[i] = (nutr_num, total)
        return self.list_nutr_tot

    def compute_pcnt_nutr_goal(self):
        if not hasattr(self, 'person'):
            from . import person
            self.person = person.Person()

        person_num = self.person.get_person_num()

        self.db.query("SELECT Nutr_No, goal_val FROM nutr_goal " +
            "WHERE person_no = %d " % (person_num))
        list_nutr_goal = self.db.get_result()

        dict = {}
        #print 'list_nutr_tot (nutr_composition_dlg.py):'
        for num, val in self.list_nutr_tot:
            #print 'num:', num, 'val:', val
            dict[num] = val

        list_pcnt_goal = []
        print('list_nutr_goal (nutr_composition_dlg.py):')
        for num, val in list_nutr_goal:
            #print 'num:', num, 'val:', val
            if val == 0.0:
                pcnt = 0.0
            else:
                try:
                    pcnt = dict[num] * 100.0 / val
                except KeyError:
                    #DEBUG FIXME: why this error?
                    print('nutr_composition_dlg: line 186, key error', num)  
                    continue
            list_pcnt_goal.append((num, pcnt))
        return list_pcnt_goal

    def reset(self):
        for n in self.ui.nutr_list:
            n.entry_amount.set_text('0.000')
            n.entry_pcnt.set_text('0.000')

        self.ui.protein_entry.set_text('0.000')
        self.ui.fat_entry.set_text('0.000')
        self.ui.carb_entry.set_text('0.000')
