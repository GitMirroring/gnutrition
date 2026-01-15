# GNUtrition - a nutrition and diet analysis program.
# Copyright (C) 2000-2002 Edgar Denny (edenny@skyweb.net)
# Copyright (C) 2012 Free Software Foundation, Inc.
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
from . import database
from . import config

class Person:
    _shared_state = {}
    def __init__(self):
        self.__dict__ = self._shared_state
        if self._shared_state:
            return
        self.db = database.Database()

    def get_name(self, user):
        self.db.query("SELECT person_name FROM person WHERE user_name = '%s'" 
            % (user))
        return self.db.get_single_result()

    def add_name(self, person_name):
        user = self.get_user()
        self.db.query("SELECT person_name FROM person")
        result = self.db.get_result()

        if not result:
            # first name to be added to the table
            person_num = 10001
            self.db.query("INSERT INTO person VALUES (%d, '%s', '%s')" 
                % (person_num, person_name, user))
        else:
            match = 0
            for name in result:
                if name[0] == person_name:
                    match = 1
                    break
            if match == 0:
                self.db.query("INSERT INTO person VALUES (NULL, '%s', '%s')" 
                    % (person_name, user))

    def update_name(self, old_name, new_name):
        user = self.get_user()
        sql = "UPDATE  person SET person_name = '{0:s}' WHERE user_name = '{1:s}'"
        self.db.query(sql.format(new_name, user))

    def setup(self):
        person_num = self.get_person_num()

        # drop any existing temporary tables
        self.db.query("DROP TABLE IF EXISTS food_plan_temp")
        self.db.query("DROP TABLE IF EXISTS recipe_plan_temp")

        # create a series of temporary tables
        self.db.query("CREATE TEMPORARY TABLE food_plan_temp " + 
            "(person_no INTEGER NOT NULL, " + 
            "date TEXT NOT NULL, " +
            "time TEXT NOT NULL, " + 
            "amount REAL NOT NULL, " +
            "Msre_Desc TEXT NOT NULL, " +
            "NDB_No TEXT NOT NULL, " +
            "PRIMARY KEY (date, time, NDB_No))")

        #self.db.query("CREATE TEMPORARY TABLE recipe_plan_temp " +
        self.db.query("CREATE TABLE recipe_plan_temp " +
            "(person_no INTEGER NOT NULL, " +
            "date TEXT NOT NULL, " +
            "time TEXT NOT NULL, " +
            "no_portions REAL NOT NULL, " +
            "recipe_no INTEGER NOT NULL, " +
            "PRIMARY KEY (date, recipe_no, time) )")

        # copy any data from stored tables to temporary ones
        self.db.query("SELECT * FROM food_plan WHERE person_no = %d" 
            % (person_num))
        result = self.db.get_result()

        if result and len(result) != 0:
            for person_no, date, time, amount, msre_desc, ndb_no in result:
                self.db.query("INSERT INTO food_plan_temp VALUES" +
                    "(%d, '%s', '%s', %f, '%s', '%s' )"
                    %(person_no, str(date), str(time), amount, msre_desc, ndb_no),
                        caller='Person.setup')

        self.db.query("SELECT * FROM recipe_plan WHERE person_no = %d" 
            % (person_num))
        result = self.db.get_result()
        if result and len(result) != 0:
            for person_num, date, time, num_portions, recipe_num in result:
                self.db.query("INSERT INTO recipe_plan_temp VALUES" +
                    " (%d, '%s', '%s', %f, %d )" 
                    % (person_num, date, time, num_portions, 
                        recipe_num), caller='Person.setup')

    # self.db.user is basename($HOME)
    # 'Username' will be:
    #   - MySQL username
    #       If user had older version of app that used MySQL and chose
    #       to import old MySQL data during installation.
    #   - basename($HOME)
    #       If during setup config.get_value('Username') is not set. It
    #       subsequently will be set to name of user's home directory.
    #
    def get_user(self):
        #return self.db.user
        return config.get_value('Username')

    def get_person_num(self):
        user_name = self.get_user()
        self.db.query("SELECT person_no FROM person WHERE user_name = '%s'" 
            % (user_name))
        return self.db.get_single_result()
