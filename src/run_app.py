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
from . import config
import wx
from .util.log import init_logging

class RunApp:
    def __init__(self):
        from os import path
        logfile = path.join(config.udir, 'log')
        init_logging(logfile, logto='both', level='info')
        self.first_run = not config.get_value('sqlite3')
        if self.first_run:
            # First run, program default values can be added here
            from . import druid
            # Set default version check information
            from . import gnutr_consts
            config.set_key_value('sqlite3', 'Yes')
            config.set_key_value('check_disabled', gnutr_consts.CHECK_DISABLED)
            config.set_key_value('check_version', gnutr_consts.CHECK_VERSION)
            config.set_key_value('check_interval', gnutr_consts.CHECK_INTERVAL)
            config.set_key_value('last_check', 0)
            self.user_dir = config.udir
            self.druid = druid.Druid(self)
            self.druid.show()
        else:
            self.startup()

    def startup(self):
        from . import version
        version.check_version()

        from . import database 
        self.db = database.Database()

        from . import store
        self.store = store.Store()

        from . import person
        self.person = person.Person()
        self.person.setup()

        from . import base_win
        self.base_win = base_win.BaseWin(self)
        self.base_win.show()

    def shutdown(self):
        if not self.first_run:          #otherwise, after first run empty db would be created. Smells like program crash in future
            from . import database 
            db = database.Database()    #foo-script: Do we really need it at all?
            db.close()
        
def run_app():
    app = RunApp()
    gtk.main()
    app.shutdown()

run_app()
