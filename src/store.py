#  GNUtrition - a nutrition and diet analysis program.
#  Copyright (C) 2000-2002 Edgar Denny (edenny@skyweb.net)
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
from . import database

class Store:
    _shared_state = {}
    def __init__(self):
        self.__dict__ = self._shared_state
        if self._shared_state:
            return
        self.cat_desc2num = {}
        self.cat_num2desc = {}
        self.cat_desc_list = []
        self.nutr_num_list = []
        self.nutr_desc_list = []
        self.fg_desc_list = []
        self.nutr_desc2num = {} 
        self.nutr_num2desc = {} 
        self.fg_desc2num = {} 
        self.fd_desc2num = {} 
        self.fd_num2desc = {} 
        self.db = database.Database()
        self.create_nutr_num_list()
        self.create_nutr_desc_list()
        self.create_cat_desc_list()
        self.create_fd_group_desc_list()
        self.create_nutr_desc_nutr_no_dict()
        self.create_fd_gp_desc_fd_gp_no_dict()
        self.create_fd_desc_fd_no_dict()
        self.create_cat_desc_cat_no_dict()

    def create_cat_desc_cat_no_dict(self):
        self.cat_desc2num['All'] = 0
        self.cat_num2desc[0] = 'All'
        self.db.query("SELECT category_no, category_desc FROM " +
            "category")
        result = self.db.get_result()
        for num, desc in result:
            self.cat_desc2num[desc] = num
            self.cat_num2desc[num] = desc

    def create_cat_desc_list(self):
        self.db.query("SELECT category_desc FROM category")
        result = self.db.get_result()
        self.cat_desc_tuple = (('All',),) + result
        self.cat_desc_list.append('All')
        for desc in result:
            self.cat_desc_list.append(desc[0])

    def create_nutr_num_list(self):
        self.db.query("SELECT Nutr_No FROM nutr_def")
        result = self.db.get_result()
        for num_tuple in result:
            self.nutr_num_list.append(num_tuple[0])
    
    def create_nutr_desc_list(self):
        self.db.query("SELECT NutrDesc FROM nutr_def")
        result = self.db.get_result()
        self.nutr_desc_tuples = result
        for desc in result:
            self.nutr_desc_list.append(desc[0])

    def create_fd_group_desc_list(self):
        self.db.query("SELECT FdGrp_Desc FROM fd_group")
        result = self.db.get_result()
        self.fg_desc_tuple = (('All Foods',),) + result
        for desc_tuple in result:
            self.fg_desc_list.append(desc_tuple[0])
        self.fg_desc_list.insert(0, 'All Foods')

    def create_nutr_desc_nutr_no_dict(self):
        self.db.query("SELECT Nutr_No, NutrDesc FROM nutr_def")
        result = self.db.get_result()
        for num, desc in result:
            self.nutr_desc2num[desc] = num
            self.nutr_num2desc[num] = desc

    def create_fd_gp_desc_fd_gp_no_dict(self):
        self.db.query("SELECT FdGrp_Cd, FdGrp_Desc FROM fd_group")
        result = self.db.get_result()
        for num, desc in result:
            self.fg_desc2num[desc] = num

    def create_fd_desc_fd_no_dict(self):
        self.db.query("SELECT NDB_No, Long_Desc FROM food_des")
        result = self.db.get_result()
        for num, desc in result:
            #despite of description, num is a string, always 5 digits long. If < 10000, then begins with 0's
            self.fd_desc2num[desc] = num
            self.fd_num2desc[num] = desc

    def get_msre_desc_tuples(self, fd_num):
        self.db.query("SELECT Msre_Desc FROM weight WHERE " +
            "NDB_No = '{0:s}'".format(fd_num))
        result = self.db.get_result()
        return result
