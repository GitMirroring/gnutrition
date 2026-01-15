# GNUtrition - a nutrition and diet analysis program.
# Copyright (C) 2000-2002 Edgar Denny (edenny@skyweb.net)
# Copyright (C) 2010 2012 Free Software Foundation, Inc.
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
import MySQLdb
from .util.exception import AppException
from .util.log import LOG as log
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

class MySQLError(AppException): pass

class Database:
    _shared_state = {}
    def __init__(self, uname=None, pword=None):
        self.__dict__ = self._shared_state
        if self._shared_state:
            return
        self.db = MySQLdb.Connect(user=uname, passwd=pword)
        self.cursor = self.db.cursor()
        self.user = uname
        self.rows = 0
        self.result = None
 
    def initialize(self):
        self.query('SHOW DATABASES')
        db_list = self.get_result()
        for b in db_list:
            if b[0] == 'gnutr_db':
                self.query('USE gnutr_db')
                return True
        return False

    def query(self, query, caller=None):
        try:
            self.cursor.execute(query)
        except MySQLdb.Error as sqlerr:
            excp = MySQLError('{0:s}\nquery: {1:s}'.format(sqlerr,query))
            error(excp)
            self.cursor.execute('SHOW ERRORS');
            import traceback
            import sys
            traceback.print_exc()
            if caller: excp += 'Caller: '.format(caller)
            raise excp
        self.result = self.cursor.fetchall()
        self.rows = self.db.affected_rows()
        self.db.commit()

    def get_result(self):
        result = self.result
        self.result = None
        if not result:
            debug('No result')
        return result

    def get_row_result(self):
        result = self.result
        self.result = None
        if not result:
            debug('No result')
            return None
        if len(result) == 1:
            return result[0]
        error('not a single row')
        return None

    def get_single_result(self):
        result = self.result
        self.result = None
        if not result:
            debug('No result')
            return None
        if len(result) == 1:
            if len(result[0] ) == 1:
                return result[0][0]
        error('not a single value')
        return None

    def delete_db(self):
        self.query("DROP DATABASE gnutr_db")

def open_mysqldb(user=None, passwd=None):
    from .gnutr import Dialog
    db_uname, db_pword = None, None
    if user and passwd:
        db_uname, db_pword = user, passwd
    else:
        from . import config
        # Username and Password would be left over from MySQL versions
        # of GNUtrition.
        db_uname = config.get_value('Username')
        db_pword = config.get_value('Password')
    if (db_uname and db_pword):
        try:
            db_instance = Database(db_uname, db_pword)
        except Exception:
            Dialog('error',"Unable to connect to MySQL's GNUtrition database.")
            return False
        else:
            if not db_instance.initialize():
                Dialog('error', "MySQL GNUtrition database no longer exists!")
                return False
    else:
        return False
    return db_instance

if __name__ == '__main__':
    import sys
    if open_mysqldb():
        print('Successful MySQL database test.')
    else:
        print('MySQL database test failed.', db)
        sys.exit(1)
    sys.exit(0)
