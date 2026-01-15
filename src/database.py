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
import sqlite3 as dbms
import datetime, time
import re
from . import config
from .util.utility import stdout, stderr, func
from .util.exception import AppException, AppFileReadError
from .util.log import LOG as log
debug = log.debug
info = log.info
warn = log.warn
error = log.error
critical = log.critical

class SQLiteQueryError(AppException): pass

def ticks():
    return time.time()

def curtime():
    """Return current local time as hh:mm"""
    return str(dbms.TimeFromTicks(ticks()))[:5]

def curdate():
    """Return todays date as yyyy-mm-dd"""
    return str(dbms.DateFromTicks(ticks()))

def leap_year(year):
    """
    """
    if year % 400 == 0:
        return True
    if year % 100 == 0:
        return False
    if year % 4 == 0:
        return True
    return False

def days_since(start_year, datestr):
    """
    """
    Y = int(start_year)
    # check for >= 1900 && < 2100
    c = Y/100
    if not c in [19,20]:
        raise Exception("Start year must be 1900 <= y < 2100")
    if not datestr:
        raise Exception("Date string required.")
    #          J  F  M  A  M  J  J  A  S  O  N  D
    days_in_month =  [31,28,31,20,31,30,31,31,30,31,30,31]
    ymd = datestr.split('-')
    if len(ymd) != 3:
        raise Exception("Date string must be in YYYY-MM-DD format.")
    Y,M,D = int(ymd[0]), int(ymd[1]), int(ymd[2])
    days = 0
    # Accumulate days since start_year, four years at a time, and adjust for leap year
    for y in range(start_year, Y):
        days = days + 365
        if leap_year(y):
            days = days + 1
    if leap_year(Y):
        days_in_month[1] = 29
    for m in range(0, M-1):
        days = days + days_in_month[m]
    days = days + D
    return days - 1  # Not inclusive- days_since(2012, 2012-01-01) is zero

def to_days(datestr=None):
    # Replacement for MySQLs TO_DAYS() function:
    # The app only uses this function for a difference calculation in calandar
    # days. The arbitrary starting point is January 1, 1900.
    return days_since(1900, datestr)

dbms.register_adapter(datetime.datetime, curtime)
dbms.register_adapter(datetime.datetime, curdate)

def regexp(exp, text):
    """Define a function to be called when sqlite3 module sees 'REGEXP'"""
    return text is not None and re.search(exp, text) is not None

class Database:
    _shared_state = {}
    def __init__(self):
        self.__dict__ = self._shared_state
        if self._shared_state:
            return
        self.Error = dbms.Error
        from os import path
        self.user = config.user
        dbfile =  path.join(config.udir, 'gnutr_db.lt3')
        try:
            con = dbms.connect(dbfile)
            # text_factory must be set to 'str' due to current limitations
            # in csv.reader()
            con.text_factory = str
            con.create_function('REGEXP', 2, regexp)
            con.create_function('TO_DAYS', 1, to_days)
            cur = con.cursor()
        except self.Error as e:
            "Error {0:s}:".format(e.args[0])
            raise self.Error
        self.con = con
        self.cur = cur

    def close(self): 
        if self.con:
            self.con.close()

    def init_USDA_data(self):
        """Load the USDA Standard Reference release data."""
        # Create Food Description (food_des) table.
        # Data file FOOD_DES.
        self.create_table_food_des()
        self.create_table_fd_group()
        self.create_table_nut_data()
        self.create_table_nutr_def()
        self.create_table_weight()

    def create_table_food_des(self):
        self.query("DROP TABLE IF EXISTS food_des")
        self.create_load_table("CREATE TABLE food_des" +
            "(NDB_No TEXT NOT NULL, " + 
            "FdGrp_Cd TEXT NOT NULL, " + 
            "Long_Desc TEXT NOT NULL, " + 
            "Shrt_Desc TEXT NOT NULL, " + 
            # Three new fields for sr24
            "ComName TEXT, " + 
            "ManufacName TEXT, " +
            "Survey TEXT, " +
            # end new
            "Ref_desc TEXT, " + 
            "Refuse INTEGER, " + 
            "SciName TEXT, " + 
            "N_Factor REAL, " +
            "Pro_Factor REAL, " + 
            "Fat_Factor REAL, " + 
            "CHO_Factor REAL, " +
            "PRIMARY KEY(NDB_No, FdGrp_Cd))",
            ### Insert statement
            "INSERT INTO 'food_des' VALUES " +
            "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            'food_des')

    def create_table_fd_group(self):
        # Create Food Group Description (fd_group) table.
        # Data file FD_GROUP.
        self.query("DROP TABLE IF EXISTS fd_group")
        self.create_load_table("CREATE TABLE fd_group " + 
            "(FdGrp_Cd TEXT PRIMARY KEY NOT NULL, " + 
            "FdGrp_Desc TEXT NOT NULL)",
            ### Insert statement for one row
            "INSERT INTO 'fd_group' VALUES (?, ?)",
            'fd_group')

    def create_table_nut_data(self):
        # Create Nutrient Data (nut_data) table.
        # Data file NUT_DATA
        self.query("DROP TABLE IF EXISTS nut_data")
        self.create_load_table("CREATE TABLE nut_data " + 
            "(NDB_No TEXT NOT NULL, " + 
            "Nutr_No TEXT NOT NULL, " + 
            "Nutr_Val REAL NOT NULL, " + 
            "Num_Data_Pts REAL NOT NULL, " + 
            "Std_Error REAL, " + 
            "Src_Cd TEXT NOT NULL, " +
            "Deriv_Cd TEXT, " +
            "Ref_NDB_No TEXT, " +
            "Add_Nutr_Mark TEXT, " +
            "Num_Studies INTEGER, " +
            "Min REAL, " +
            "Max REAL, " +
            "DF INTEGER, " +
            "Low_EB REAL, " +
            "Up_EB REAL, " +
            "Stat_cmt TEXT, " +
            "AddMod_Date TEXT, " +
            "CC TEXT, " +
            "PRIMARY KEY(NDB_No, Nutr_No))",
            ### Insert statement for one row
            "INSERT INTO 'nut_data' VALUES " +
            "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            'nut_data') 

    def create_table_nutr_def(self):
        # Create Nutrient Definition (nutr_def table.
        # Data file NUTR_DEF
        self.query("DROP TABLE IF EXISTS nutr_def")
        self.create_load_table("CREATE TABLE nutr_def " + 
            "(Nutr_No TEXT PRIMARY KEY NOT NULL, " + 
            "Units TEXT NOT NULL, " +
            "Tagname TEXT, " +
            "NutrDesc TEXT NOT NULL, " +
            # Two new in sr24
            "Num_Dec INTEGER NOT NULL, " +
            "SR_Order INTEGER NOT NULL)",
            ### Insert statement for one row
            "INSERT INTO 'nutr_def' VALUES " +
            "(?, ?, ?, ?, ?, ?)",
            'nutr_def')

    def create_table_weight(self):
        # Create temporary weight table.
        # Data file WEIGHT.
        self.query("DROP TABLE IF EXISTS weight")
        self.create_load_table("CREATE TABLE weight" +
            "(NDB_No TEXT NOT NULL, " +
            # Seq == Sequence number for measure description (Msre_Desc)
            # The NDB_No for a food item will appear once for each measure
            # description. Measure descriptions are sequenced. For example:
            # NDB_No Seq Amount Msre_Desc                Gm_wgt
            # 01001   1    1     cup                       227
            # 01001   2    1     tbsp                       14.2
            # 01001   3    1     pat (1" sq, 1/3" high)      5.0
            # 01001   4    1     stick                     113
            "Seq INTEGER NOT NULL, " +
            # Amount == Unit modifier (for example, 1 in "1 cup").
            "Amount REAL NOT NULL, " +
            "Msre_Desc TEXT NOT NULL, " +
            "Gm_wgt REAL NOT NULL, " +
            "Num_Data_Pts INTEGER, " +
            "Std_Dev REAL, " +
            "PRIMARY KEY(NDB_No, Seq))",
            ### Insert statement for one row
            "INSERT INTO 'weight' VALUES " +
            "(?, ?, ?, ?, ?, ?, ?)",
            'weight')

    def init_user(self):
        # May have user data from previous install that we don't want to lose
        # so IF NOT EXISTS is used

        # create recipe table
        # Note: Want index on recipe_name, category_no?
        self.create_table("CREATE TABLE IF NOT EXISTS recipe" +
            "(recipe_no INTEGER PRIMARY KEY AUTOINCREMENT, " +
            "recipe_name TEXT NOT NULL, " +
            "no_serv INTEGER NOT NULL, " +
            "no_ingr INTEGER NOT NULL, " +
            "category_no INTEGER NOT NULL)", 'recipe') 

        # create ingredient table
        self.create_table("CREATE TABLE IF NOT EXISTS ingredient" + 
            "(recipe_no INTEGER NOT NULL, " + 
            "amount REAL NOT NULL, " +
            "Msre_Desc TEXT NOT NULL, " +
            "NDB_No TEXT NOT NULL)", 'ingredient')

        # create recipe category table
        self.create_load_table("CREATE TABLE IF NOT EXISTS category" +
            "(category_no INTEGER PRIMARY KEY NOT NULL, " +
            "category_desc TEXT NOT NULL)",
            ### Insert statement
            "INSERT INTO 'category' VALUES (?, ?)",
            'category')

        # create recipe preparation table
        self.create_table("CREATE TABLE IF NOT EXISTS preparation" +
            "(recipe_no INTEGER PRIMARY KEY NOT NULL, " +
            "prep_time TEXT, " +
            "prep_desc TEXT)", 'preparation')

        # create person table
        self.create_table("CREATE TABLE IF NOT EXISTS person" +
            "(person_no INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," +
            "person_name TEXT, " +
            "user_name TEXT)", 'person')

        # create food_plan table
        self.create_table("CREATE TABLE IF NOT EXISTS food_plan" +
            "(person_no INTEGER NOT NULL, " +
            "date TEXT NOT NULL, " +
            "time TEXT NOT NULL, " +
            "amount REAL NOT NULL, " +
            "Msre_Desc TEXT NOT NULL, " +
            "NDB_No TEXT NOT NULL)", 'food_plan')

        # create recipe_plan table
        self.create_table("CREATE TABLE IF NOT EXISTS recipe_plan" +
            "(person_no INTEGER NOT NULL, " +
            "date TEXT NOT NULL, " +
            "time TEXT NOT NULL, " +
            "no_portions REAL NOT NULL, " +
            "recipe_no INTEGER NOT NULL)", 'recipe_plan')

        # create nutr_goal table
        self.create_table("CREATE TABLE IF NOT EXISTS nutr_goal" +
            "(person_no INTEGER NOT NULL, " +
            "Nutr_No TEXT NOT NULL, " +
            "goal_val REAL NOT NULL)", 'nutr_goal')

    def curtime(self):
        return curtime()

    def curdate(self):
        return curdate()

    def show_query(self, sql, sql_params, caller=None, log=False, log_only=False):
        if not caller: return
        s = ''
        if caller: s = '{0:s}(): '.format(caller)
        s = s + '{0:s}\n'.format(sql)
        if sql_params:
            s = s + '\tparams:'
            if log:
                info("{0:s} {1!r}".format(s, sql_params))
            if not log_only:
                stdout("{0:s} {1!r}\n".format(s, sql_params))
        else:
            if log:
                info(s)
            if not log_only:
                stdout(s)

    def query(self, sql, many=False, sql_params=None, caller=None):
        """Execute the SQL statement with given SQL parameters."""
        try:
            if sql_params:
                if many:
                    self.cur.executemany(sql, sql_params)
                else:
                    self.cur.execute(sql, sql_params)
            elif many:
                self.cur.executemany(sql)
            else:
                self.cur.execute(sql)
            self.con.commit()
            result = self.cur.fetchall()
        except self.Error as sqlerr:
            self.con.rollback()
            excp = SQLiteQueryError("{0:s}\n\tquery: {1:s}".format(sqlerr, sql))
            if caller:
                excp += '  Caller: {0:s}'.format(caller)
            error(excp)
            raise excp
        # Convert to tuple as GNUtrition code expects MySQLdb tuple return
        self.result = tuple(result)
        self.last_query = sql
        self.last_query_params = sql_params
        # Added for debugging
        self.show_query(sql, sql_params, caller)

    def get_result(self):
        """Return full result, fetchall() from cursor.execute()"""
        result = self.result
        self.result = None
        if not result:
            s = 'No result from:\n  {0:s}'.format(self.last_query)
            if self.last_query_params:
                s = s + '\n  sql params: {0!r}'.format(self.last_query_params)
            info(s)
        return result

    def get_row_result(self):
        """Return a single row from query."""
        result = self.result
        self.result = None
        if not result:
            s = 'No result from:\n  {0:s}'.format(self.last_query)
            if self.last_query_params:
                s = s + '\n  sql params: {0!r}'.format(self.last_query_params)
            info(s)
            return result
        if len(result) == 1:
            return result[0]
        error('not a single row')
        return None

    def get_single_result(self):
        """Return a single result from a query."""
        result = self.result
        self.result = None
        if not result:
            s = 'No result from:\n  {0:s}'.format(self.last_query)
            if self.last_query_params:
                s = s + '\n  sql params: {0!r}'.format(self.last_query_params)
            info(s)
            return None
        if len(result) == 1:
            if len(result[0] ) == 1:
                return result[0][0]
        error('not a single value')
        return None

    def create_table(self, sql, tablename):
        self.query(sql)
        info("created table '{0:s}'".format(tablename))

    def load_table(self, sql, data_fn):
        """Load a table from disk file."""
        import csv
        try:
            data = csv.reader(open(data_fn,'r'), delimiter='^', quotechar="'")
        except Exception as e:
            e = AppFileReadError(e)
            e = e + "Failed to read data file '{0:s}'".format(data_fn)
            raise e
        self.query(sql, many=True, sql_params=data)
        return True

    def create_load_table(self, create_sql, insert_sql, table_name):
        """Create and load table from file.
        'create_sql' is the SQL statement for table creation.
        'insert_sql' is the SQL statement given to executemany.
        'table_name' serves as both the database table name and the data file name.
        """
        import install
        from os import path
        self.create_table(create_sql, table_name)
        data_file = path.join(install.idir, 'data', table_name.upper() + '.txt')
        if self.load_table(insert_sql, data_file):
            info("loaded table '{0:s}'".format(table_name))

    def delete_db(self):
        self.query("DROP DATABASE gnutr_db")

    def next_row(self, col, table):
        """Return a number one greater than maximum row of table."""
        self.query("SELECT MAX({0:s}) from {1:s}".format(col, table))
        m = self.get_single_result()
        if not m:
            m = 1
        else:
            m += 1
        return m

def table_exists(table):
    """Return True or False for existence of table.

    This is for a quick check for 'old_recipes' which is created only if
    migrating (importing) from MySQL database.
    """
    db = Database()
    sql = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='{0:s}'"
    db.query(sql.format(table))
    if db.get_single_result():
        return True
    return False

def num2str(number, n):
    """Convert integer number to text of length n. Add leading zeros if needed.

    The MySQL database stored food number as integer, not string. The correct
    way to store as per SR documentation is text. Leading zeros from data
    file need to be retained.
    """
    s = str(number)
    for z in range(n - len(s)):
        s = '0' + s
    return s
 
def good_NDB_No(NDB_No):
    """Verify NDB_No is valid for current data set."""
    db = Database()
    sql = "SELECT NDB_No FROM food_des WHERE NDB_No = '{0:s}'".format(NDB_No)
    db.query(sql)
    return db.get_single_result()

# These next two are only valid for MySQL database 
def msre_desc_from_msre_no(mysql=None, msre_no=None):
    """Use msre_no (measure number) to retrieve corresponding measure description."""
    if not (mysql and msre_no):
        raise Exception("Parameters for MySQL instance and/or msre_no missing.")
    sql = "SELECT msre_desc FROM measure WHERE msre_no = {0:d}"
    mysql.query(sql.format(msre_no))
    return mysql.get_single_result()

# Need to be careful here; MySQL db uses int fields in places where SQLite
# does not (fd_no, nutr_no) == (NDB_No, Nutr_No).
def gm_wgt_from_fd_and_msre(mysql, NDB_No, Msre_No):
    """Retrieve gram weight given NDB_No (food number) and Msre_No (measure number)."""
    sql = "SELECT wgt_val FROM weight WHERE fd_no = {0:d}" +\
          " AND msre_no = {1:d}"
    mysql.query(sql.format(int(NDB_No), int(Msre_No)))
    return mysql.get_single_result()

# This is only valid for initialized database.Database() class
def latest_Msre_Desc_for_NDB_No(sqlite, NDB_No):
    """Retrieve current measure descriptions for given NDB_No (food number)"""
    sql = "SELECT Msre_Desc, Gm_wgt FROM weight WHERE NDB_No = '{0:s}'"
    sqlite.query(sql.format(NDB_No))
    desc_list = sqlite.get_result()
    if not desc_list:
        return ([],[]) # Nothing we can do about this...
    all_desc = []
    all_weights = []
    for choice in desc_list: 
        all_desc.append(choice[0])
        all_weights.append(choice[1])
    iam = func()
    debug('{0:s}({1:s}): {2!r}'.format(iam, NDB_No, all_desc))
    debug('{0:s}({1:s}): {2!r}'.format(iam, NDB_No, all_weights))
    return (all_desc, all_weights)
    
    def find_closest(desc_list, desc): """Attempt to find a close match for Msre_Desc.
    Parameter desc_list is a current list of all Msre_Desc for a particular NDB_No.
    Parameter desc is a specific Msre_Desc from old data.
    """
    import re
    # Some differences in measure descriptions are just white space. 
    found = None
    s2 = desc.replace(' ','')
    for item in desc_list:
        s1 = item.replace(' ', '')
        if s1 == s2:
            found = item
            break
    info('find_closest: {0!r} {1!r}'.format( desc_list, desc))
    info('  result: {0!r}'.format(found))
    return found
    
def to_Msre_Desc(sqlite=None, mysql=None,
                            NDB_No=None, Msre_Desc=None, Msre_No=None):
    """Return a measurement deescription from current database.

    Given Nutrient Database Number NDB_No and one of measure description
    or measure number, find and return the appropriate MsreDesc from the
    current USDA Standard Reference data.

    Parameter sqlite must be an initialized instance of database.Database()
    class.
    Parameter mysql is needed if parameter Msre_No is given, and must
    be an instance of mysql.Database() class.
    """
    if not sqlite:
        raise Exception("sqlite parameter for SQLite3 instance missing.")
    if Msre_No and not mysql:
        raise Exception("mysql parameter for MySQL instance missing.")
    if not NDB_No:
        raise Exception("NDB_No parameter missing.")
    if not (Msre_Desc or Msre_No) and not (Msre_Desc and Msre_No):
        raise Exception("Either Msre_Desc or Msre_No must be given.")
    if Msre_No:
        desc = msre_desc_from_msre_no(mysql, Msre_No)
    else:
        desc = Msre_Desc

    # First see if we have exact match
    (desc_list, new_gwt_list) = latest_Msre_Desc_for_NDB_No(sqlite, NDB_No)
    for d in desc_list:
        if d == desc:
            return d  # Great

    # Next see if we can match gram weights. If we can chances are excellent
    # we then derive the correct Msre_Desc.
    if Msre_No:
        old_gwt = gm_wgt_from_fd_and_msre(mysql, NDB_No, Msre_No)
        for wt in range(len(new_gwt_list)):
            if new_gwt_list[wt] == old_gwt:
                return desc_list[wt]
    description = find_closest(desc_list, desc)
    if not description:
        debug('No Msre_Desc found for desc {0:s} and NDB_No {1:s}'.format(
                desc, NDB_No))
        debug('All Msre_Desc for NDB_No: {0!r}'.format(desc_list))
    return description

def migrate(mysql):
    """Retrieve gnutrition table data from MySQL database.
    Parameters uname and pword are the MySQL username and password used with
    the older version of GNUtritin."""
    from .gnutr import Dialog
    lite = Database()

    # Need to check for tables: recipe, ingredient, preparation
    # person, food_plan, recipe_plan, nutr_goal 
    tables = ['recipe', 'ingredient', 'preparation','person',
              'food_plan', 'recipe_plan']
    mysql.query('SHOW TABLES')
    old_tables = mysql.get_result()
    # At some point use of a 'measure' table was discontinued
    use_msre_no = False
    found = []
    for t in old_tables:
        if t[0] in tables:
            found.append(t[0])
        if t[0] == 'measure':
            use_msre_no = True

    # Quirks:
    # 0.31 does not save recipe or food_plan (nothing to migrate except 
    #      person data.
    # 0.31.1 uses fd_no (for NDB_No) and msre_no to index measure table
    # 0.32 onward uses NDB_No and Msre_Desc (no measure table)

    # Any version - food numbers may have disappeared from earlier SR data
    #               to later SR data. NDB_No inserted in SQLite tables must
    #               exist in current SR data.
    #               

    # Create a list of for obsolete NDB_No (food number) and recipe_no 
    # (recipe number) for recipes which will fail to import properly due to
    # the obsolete NDB_No.
    recipe_failures = []
    obsolete_NDB_No = []

    # ingredient table
    if 'ingredient' in found:
        sql1 = "SELECT recipe_no, amount, msre_no, fd_no FROM ingredient"
        sql2 = "SELECT recipe_no, amount, Msre_Desc, NDB_No FROM ingredient"
        if use_msre_no:
            mysql.query(sql1)
        else:
            mysql.query(sql2)
        result = mysql.get_result()
        if result:
            debug('found {0:d} ingredients'.format(len(result)))
            debug("{0!r}".format(result))
            for i in range(len(result)):
                NDB_No = num2str(result[i][3],5)
                if not good_NDB_No(NDB_No):
                    recipe_failures.append(recipe_no)
                    obsolete_NDB_No.append(NDB_No)
                    debug('NDB_No {0:d} is obsolete.'.format(NDB_No))
                    continue   
                Msre_No, Msre_Desc = None, None
                recipe_no = result[i][0]
                amount = result[i][1]
                if use_msre_no:
                    Msre_No = result[i][2]
                else:
                    Msre_Desc = result[i][2]
                Msre_Desc = to_Msre_Desc(sqlite=lite, mysql=mysql, NDB_No=NDB_No,
                                            Msre_Desc=Msre_Desc, Msre_No=Msre_No)
                assert Msre_Desc
                debug('Found Msre_Desc {0:s}'.format(Msre_Desc))
                params = (recipe_no, amount, Msre_Desc, NDB_No)
                lite.query("INSERT INTO 'ingredient' VALUES (?,?,?,?)",
                           many=False, sql_params=params, caller='migrate')

    # recipie table
    if 'recipe' in found:
        mysql.query("SELECT recipe_no, recipe_name, no_serv, no_ingr, " +
                    "category_no FROM recipe")
        result = mysql.get_result()
        if result:
            debug('found {0:d} recipies'.format(len(result)))
            debug('{0!r}'.format(result))
            for i in range(len(result)):
                if result[i][0] in recipe_failures:
                    debug('Recipe {0:s} will not import'.format(result[i][1]))
                    continue
                lite.query("INSERT INTO 'recipe' VALUES (?,?,?,?,?)",
                           many=False, sql_params=result[i], caller='migrate')
    # preparation table
    if 'preparation' in found:
        mysql.query("SELECT recipe_no, prep_time, prep_desc FROM preparation")
        result = mysql.get_result()
        if result:
            debug('found {0:d} entries in preparation table'.format(len(result)))
            debug('{0!r}'.format(result))
            for i in range(len(result)):
                if result[i][0] in recipe_failures:
                    continue
                lite.query("INSERT INTO 'preparation' VALUES (?,?,?)",
                           many=False, sql_params=result[i], caller='migrate')

    # person table
    if 'person' in found:
        mysql.query("SELECT person_no, person_name, user_name FROM person")
        result = mysql.get_result()
        if result and len(result) > 0:
            persons = len(result)
            debug('found {0:d} entries in person table'.format(persons))
            debug('{0!r}'.format(result))
            lite.query("INSERT INTO 'person' VALUES (?,?,?)",
                           many=True, sql_params=result, caller='migrate')
            # This for filling in 'Name' in User Setup Personal Details
            # User can change but show them what they used before.
            if persons == 1 and not config.get_value('Name'):
                config.set_key_value('Name', person_name)

    # food_plan table
    if 'food_plan' in found:
        sql1 = "SELECT person_no, date, time, amount, msre_no, fd_no FROM food_plan"
        sql2 = "SELECT person_no, date, time, amount, Msre_Desc, NDB_No FROM food_plan"
        if use_msre_no:
            mysql.query(sql1)
        else:
            mysql.query(sql2)
        result = mysql.get_result()
        if result:
            debug('found {0:d} entries in food_plan table'.format(len(result)))
            for i in range(len(result)):
                NDB_No = num2str(result[i][5],5)
                date = str(result[i][1])
                time = str(result[i][2])
                if not good_NDB_No(result[i][5]):
                    s = 'Food plan for {0:s} {1:s} contains obsolete NDB_No.'
                    debug(s.format(date, time))
                    continue
                person_no = result[i][0]
                amount = result[i][3]
                if use_msre_no:
                    Msre_Desc = msre_desc_from_msre_no(result[i][4])
                else:
                    Msre_Desc = result[i][4]
                Msre_Desc = to_Msre_Desc(sqlite=lite, mysql=mysql, NDB_No=NDB_No,
                                            Msre_Desc=Msre_Desc, Msre_No=Msre_No)
                params = (person_no, date, time[:-3], amount, Msre_Desc, NDB_No)
                lite.query("INSERT INTO 'food_plan' VALUES (?,?,?,?,?,?)",
                           many=False, sql_params=params, caller='migrate')
    # recipe_plan table
    if 'recipe_plan' in found:
        # Need to convert datetime.date and datetime.timedelta MySQL types to
        # strings before inserting into SQLite table.
        mysql.query("SELECT person_no, date, time, no_portions, " +
                    "recipe_no FROM recipe_plan")
        result = mysql.get_result()
        debug('found {0:d} entries in recipe_plan table'.format(len(result)))
        if result:
            for r in range(len(result)):
                recipe_no = result[r][4]
                date = str(result[r][1])
                time = str(result[r][2])
                if recipe_no in recipe_failures:
                    debug('recipe plan for {0:s} {1:s} will not import'.format(date, time))
                    continue
                person_no = result[r][0]
                no_portions = result[r][3]
                params = (person_no, date, time[:-3], no_portions, recipe_no)
                debug("{0!r}".format(params))
                lite.query("INSERT INTO 'recipe_plan' VALUES (?,?,?,?,?)",
                           many=False, sql_params=params, caller='migrate')
    # nutr_goal table needs to be recalculated
    return True
#---------------------------------------------------------------------------
if __name__ == '__main__':
    from .util.log import init_logging
    init_logging('/dev/null', logto='console', level='info')
    info('curdate: {0:s}'.format(curdate()))
    info('curtime: {0:s}'.format(curtime()))
