# Copyright (C) 2010 Free Software Foundation, Inc.
#
# This file is part of GNUtrition.
# 
# GNUtrition is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNUtrition is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNUtrition.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import absolute_import
import unittest

import gtk
from . import gnutr_widgets

class TestGnutrComboBox(unittest.TestCase):
    def setUp(self):
        self.fruits = (('apple',), ('banana',), ('orange',))
    
    def test_gnutr_combobox_empty(self):
        cb = gnutr_widgets.GnutrComboBox()
        assert isinstance(cb, gtk.ComboBox), 'constructor did not create a gtk.ComboBox'
        model = cb.get_model()
        assert isinstance(model, gtk.TreeModel), 'combobox model is not a gtk.TreeModel'
        assert model.get_iter_first() == None, 'combobox model is not empty'

    def test_gnutr_combobox_rows(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        model = cb.get_model()
        it = model.get_iter_first()
        assert it != None, 'combobox model is empty'
        assert model.get_value(it, 0) == 'apple', 'combobox first item is not correct'
        it = model.iter_next(it)
        assert it != None, 'combobox model has too few rows'
        assert model.get_value(it, 0) == 'banana', 'combobox second item is not correct'
        it = model.iter_next(it)
        it = model.iter_next(it)
        assert it == None, 'combobox model has too many rows'

    def test_gnutr_combobox_set_rows(self):
        cb = gnutr_widgets.GnutrComboBox()
        cb.set_rows(self.fruits)
        assert cb.get_active() == -1, 'no row must be selected'
        model = cb.get_model()
        it = model.get_iter_first()
        assert it != None, 'combobox model is empty'
        assert model.get_value(it, 0) == 'apple', 'combobox first item is not correct'
        it = model.iter_next(it)
        assert it != None, 'combobox model has too few rows'
        assert model.get_value(it, 0) == 'banana', 'combobox second item is not correct'
        it = model.iter_next(it)
        it = model.iter_next(it)
        assert it == None, 'combobox model has too many rows'

    def test_gnutr_combobox_set_rows_active(self):
        cb = gnutr_widgets.GnutrComboBox()
        cb.set_rows(self.fruits, 1)
        assert cb.get_active() == 1, 'first row is not selected'
        
    def test_gnutr_combobox_clear(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        cb.clear_rows()
        model = cb.get_model()
        assert model.get_iter_first() == None, 'GnutrComboBox.clear_row did not remove data from the model'

    def test_gnutr_combobox_get_active_text(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        cb.set_active(1)
        assert cb.get_active_text() == 'banana', 'GnutrComboBox.get_active_text did not return the corret value'

    def test_gnutr_combobox_set_active_text(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        testfruit = 'banana'
        res = cb.set_active_text(testfruit)
        assert res, 'failed to set active text'
        assert cb.get_active_text() == testfruit, 'failed to set the active text'

        res = cb.set_active_text('steak')
        assert not res, 'set_active_text successful on invalid input'


class TestGnutrComboBoxEntry(unittest.TestCase):
    def setUp(self):
        self.fruits = (('apple',), ('banana',), ('avocado',), ('orange',))
    
    def test_gnutr_comboboxentry_empty(self):
        cb = gnutr_widgets.GnutrComboBoxEntry()
        assert isinstance(cb, gtk.ComboBox), 'constructor did not create a gtk.ComboBox'
        model = cb.get_model()
        assert isinstance(model, gtk.TreeModel), 'combobox model is not a gtk.TreeModel'
        assert model.get_iter_first() == None, 'combobox model is not empty'

    def test_gnutr_comboboxentry_rows(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        model = cb.get_model()
        it = model.get_iter_first()
        assert it != None, 'combobox model is empty'
        assert model.get_value(it, 0) == 'apple', 'combobox first item is not correct'
        it = model.iter_next(it)
        assert it != None, 'combobox model has too few rows'
        assert model.get_value(it, 0) == 'banana', 'combobox second item is not correct'
        it = model.iter_next(it)
        it = model.iter_next(it)
        it = model.iter_next(it)
        assert it == None, 'combobox model has too many rows'

    def test_gnutr_comboboxentry_set_rows(self):
        cb = gnutr_widgets.GnutrComboBox()
        cb.set_rows(self.fruits)
        assert cb.get_active() == -1, 'no row must be selected'
        model = cb.get_model()
        it = model.get_iter_first()
        assert it != None, 'combobox model is empty'
        assert model.get_value(it, 0) == 'apple', 'combobox first item is not correct'
        it = model.iter_next(it)
        assert it != None, 'combobox model has too few rows'
        assert model.get_value(it, 0) == 'banana', 'combobox second item is not correct'
        it = model.iter_next(it)
        it = model.iter_next(it)
        it = model.iter_next(it)
        assert it == None, 'combobox model has too many rows'

    def test_gnutr_comboboxentry_set_rows_active(self):
        cb = gnutr_widgets.GnutrComboBox()
        cb.set_rows(self.fruits, 1)
        assert cb.get_active() == 1, 'first row is not selected'
        
    def test_gnutr_comboboxentry_clear(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        cb.clear_rows()
        model = cb.get_model()
        assert model.get_iter_first() == None, 'GnutrComboBox.clear_row did not remove data from the model'

    def test_gnutr_comboboxentry_get_active_text(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        cb.set_active(1)
        assert cb.get_active_text() == 'banana', 'GnutrComboBox.get_active_text did not return the corret value'

    def test_gnutr_comboboxentry_set_active_text(self):
        cb = gnutr_widgets.GnutrComboBox(self.fruits)
        testfruit = 'banana'
        res = cb.set_active_text(testfruit)
        assert res, 'failed to set active text'
        assert cb.get_active_text() == testfruit, 'failed to set the active text'

        res = cb.set_active_text('steak')
        assert not res, 'set_active_text successful on invalid input'


if __name__ == '__main__':
    unittest.main()
