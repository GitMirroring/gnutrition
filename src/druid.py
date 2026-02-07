# GNUtrition - a nutrition and diet analysis program.
# Copyright(C) 2000-2002 Edgar Denny (edenny@skyweb.net)
# Copyright (C) 2010, 2012, 2013, 2026 Free Software Foundation, Inc.
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
from time import sleep
import wx
from . import druid_ui
from . import config
from . import person
import os
from . import calc_rdi
from . import database
from . import nutr_goal_dlg
from .gnutr import Dialog

class Druid:
    def __init__(self, app):
        self.app = app
        self.ui = druid_ui.DruidUI()
        self.connect_signals()

    def connect_signals(self):
        self.ui.cancel_button.connect('clicked', self.on_cancel)
        self.ui.next_button.connect('clicked', self.on_next)
        self.ui.back_button.connect('clicked', self.on_back)
        self.ui.dialog.connect('destroy', self.on_cancel)

    def show(self):
        self.ui.dialog.show_all()

    def on_cancel(self, w, d=None):
        self.ui.dialog.hide()
        gtk.main_quit()
        for user_file in os.listdir(self.app.user_dir):
            os.unlink(os.path.join(self.app.user_dir, user_file))
        return 0

    def on_next(self, w, d=None):
        if self.ui.page_num == 0:
            self.ui.set_page(1)

        # Database Create
        elif self.ui.page_num == 1:
            try:
                self.sqlite = database.Database()
            except Exception as ex:
                self.ui.set_page(2)
                return

            self.sqlite.init_USDA_data()
            self.sqlite.init_user()

            # See if this user has GNUtrition data from older version
            # which used MySQL. That data should be migrated to newer SQLite
            # storage first.
            db_uname = config.get_value('Username')
            db_pword = config.get_value('Password')

            if (db_uname and db_pword):
                dialog = Dialog('question',
                    "Would you like to try to import recipies and other\n" +
                    "data from MySQL (from older version of gnutrition)?")
                reply = dialog.run()
                dialog.destroy()
                if reply == gtk.RESPONSE_YES:
                    self.migration = True
                    from . import mysql
                    self.mysql = mysql.open_mysqldb(db_uname, db_pword)
                    if self.mysql:
                        database.migrate(self.mysql)
            # no error, so skip over page_db_error
            self.ui.set_page(3)
            return

        # Personal details
        elif self.ui.page_num == 3:
            self.person = person.Person()
            # Does the user have an entry in the person table? They will if
            # a previous version of gnutrition was installed and the config
            # file from that installation still remains in user's home directory.
            # Note: sqlite.user is basename $HOME 
            #      gnutrition MySQL setup asks for MySQL username and that
            #      may be what is in the 'person' table for 'user_name'
            db_uname = config.get_value('Username')
            if db_uname:
                db_name = db_uname
            else:
                db_name = self.sqlite.user
            # name1 may or not be 'old' name
            # person.get_name() returns a name from SQLite database associated
            # with user_name = db_name
            name1 = self.person.get_name(db_name)
            # This is the only place in the sources where 'Username' is set
            # in ~/.gnutrition/config
            # Note that this may simply be overwriting with same name if
            # get_value('Username') above returned a name.
            config.set_key_value('Username', db_name)
            new_name = self.ui.page_list[3].name_entry.get_text()
            age = self.ui.page_list[3].age_entry.get_text()
            weight_txt = self.ui.page_list[3].weight_entry.get_text()
            if (not new_name) or (not age) or (not weight_txt):
                return
            weight = float(weight_txt)
            config.set_key_value('Age', age)
            config.set_key_value('Weight', weight)
            if name1 and name1 != new_name:
#  If person changes 'Name' from previously used one to different one they
#  should be asked (presented a dialog) about associating new name with
#  imported (old) recipies. If yes, person table needs to be updated.
                self.person.update_name(name1, new_name)
            elif not name1:
                self.person.add_name(new_name)
            name2 = config.get_value('Name')
            # 'Name' is set in two places: here and in database.py in migrate()
            if name2 != new_name:
                config.set_key_value('Name', new_name)
            self.person.setup()
            if self.ui.page_list[3].weight_combo.get_active() == 0:
                # convert from pounds to kilos
                weight = weight * 0.4536
            female = self.ui.page_list[3].female_button.get_active()
            if female == 1:
                pregnant = self.ui.page_list[3].preg_button.get_active()
                lactating = self.ui.page_list[3].lac_button.get_active()
            else:
                pregnant = 0
                lactating = 0

            data = calc_rdi.compute(age, weight, female, pregnant, lactating)
            self.nutr_goal_dlg = nutr_goal_dlg.NutrGoalDlg()
            self.nutr_goal_dlg.save_goal(data)

            self.ui.set_page(5)
            return

        # Finish
        elif self.ui.page_num == 5:
            self.ui.dialog.hide()
            self.app.startup()
           
    def on_back(self, w, d=None):
        # skip back over page_db_error
        if self.ui.page_num == 3:
            self.ui.set_page(1)
        elif self.ui.page_num == 5:
            self.ui.set_page(3)
        else:
            self.ui.set_page(self.ui.page_num - 1)
