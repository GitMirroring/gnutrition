# GNUtrition - a nutrition and diet analysis program.
# Copyright(C) 2000-2002 Edgar Denny (edenny@skyweb.net)
# Copyright (C) 2010, 2012, 2026 Free Software Foundation, Inc.
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
import gobject
import wx

class Recipe(gobject.GObject):
    def __init__(self):
        self.__gobject_init__()

gobject.type_register(Recipe)

class Ingredient(gobject.GObject):
    def __init__(self):
        self.__gobject_init__()

gobject.type_register(Ingredient)

class WarnDialog(gtk.MessageDialog):
    def __init__(self, msg, parent=None):
        gtk.MessageDialog.__init__(self, parent, 
            gtk.DIALOG_DESTROY_WITH_PARENT, gtk.MESSAGE_WARNING, 
            gtk.BUTTONS_CLOSE, msg)
        self.set_resizable(False)
        self.set_default_response(gtk.RESPONSE_CLOSE)

class QuestionDialog(gtk.MessageDialog):
    def __init__(self, msg, parent=None):
        gtk.MessageDialog.__init__(self, parent, 
            gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_WARNING, gtk.BUTTONS_YES_NO, msg)
        self.set_resizable(False)

class ErrorDialog(gtk.MessageDialog):
    def __init__(self, msg, parent=None):
        gtk.MessageDialog.__init__(self, parent, 
            gtk.DIALOG_DESTROY_WITH_PARENT,
            gtk.MESSAGE_ERROR, gtk.BUTTONS_CLOSE, msg)
        self.set_resizable(False)

class NotifyDialog(gtk.MessageDialog):
    def __init__(self, msg, parent=None):
        gtk.MessageDialog.__init__(self, parent, 
            gtk.DIALOG_DESTROY_WITH_PARENT, gtk.MESSAGE_WARNING, 
            gtk.BUTTONS_CLOSE, msg)
        self.set_resizable(False)
        self.set_default_response(gtk.RESPONSE_CLOSE)

dlg_dict = { 'warn': WarnDialog, 'error': ErrorDialog, 
    'question':QuestionDialog, 'notify': NotifyDialog}

def Dialog(type, msg, parent=None):
    dlg = dlg_dict[ type](msg, parent)
    if type == 'question':
        return dlg
    dlg.run()
    dlg.destroy()
