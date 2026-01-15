#  GNUtrition - a nutrition and diet analysis program.
#  Copyright(C) 2000 - 2002 Edgar Denny (edenny@skyweb.net)
#  Copuright (C) 2010 2012 Free Software Foundation, Inc.
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
import gtk

from . import config
from . import base_win_ui
from . import plan_win
from . import recipe_win
from . import food_win

class BaseWin:
    def __init__(self, app):
        self.app = app
        self.ui = base_win_ui.BaseWinUI()

        self.plan = plan_win.PlanWin(app, self.ui.win)
        self.recipe = recipe_win.RecipeWin(app, self.ui.win)
        self.food = food_win.FoodWin(app, self.ui.win)

        self.pack_view(self.plan.ui)
        self.pack_view(self.recipe.ui)
        self.pack_view(self.food.ui)

        self.connect_signals()

    def pack_view(self, view):
        self.ui.pane_box.pack_start(view.pane, True, True, 0)
        self.ui.menubar_box.pack_start(view.menubar_box,
            False, True, 0)
        self.ui.toolbar_box.pack_start(view.toolbar_box,
            False, True, 0)

    def connect_signals(self):
        self.ui.win.connect('delete_event', self.on_delete_event)
        self.ui.win.connect('destroy', self.on_destroy)
        self.ui.plan_button.connect('clicked', self.on_plan_button_released)
        self.ui.recipe_button.connect('clicked', 
            self.on_recipe_button_released)
        self.ui.food_button.connect('clicked', self.on_food_button_released)

    def show(self):
        page = config.get_value('Page')
        if page == 'Plan':
            self.on_plan_button_released(None)
        elif page == 'Food':
            self.on_food_button_released(None)
        else:
            self.on_recipe_button_released(None)

    def on_plan_button_released(self, w, d=None):
        self.hide_view(self.food.ui)
        self.hide_view(self.recipe.ui)
        self.show_view(self.plan.ui)
        config.set_key_value('Page', 'Plan')

    def on_recipe_button_released(self, w, d=None):
        self.hide_view(self.plan.ui)
        self.hide_view(self.food.ui)
        self.show_view(self.recipe.ui)
        config.set_key_value('Page', 'Recipe')

    def on_food_button_released(self, w, d=None):
        self.hide_view(self.plan.ui)
        self.hide_view(self.recipe.ui)
        self.show_view(self.food.ui)
        config.set_key_value('Page', 'Food')

    def on_destroy(self, w, d=None):
        gtk.main_quit()

    def on_delete_event(self, w, e, d=None):
        return False

    def hide_view(self, view):
        view.menubar_box.hide()
        view.toolbar_box.hide()
        view.pane.hide()

    def show_view(self, view):
        view.menubar_box.show_all()
        view.toolbar_box.show_all()
        view.pane.show_all()
