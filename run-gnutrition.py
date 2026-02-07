#!/usr/bin/env python3
#
#  GNUtrition - a nutrition and diet analysis program.
#  Copyright (C) 2001-2002 Edgar Denny (edenny@skyweb.net)
#  Copyright (C) 2010, 2026 Free Software Foundation, Inc.
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

# This file is installed as $(prefix)/bin/gnutrition

from __future__ import absolute_import
import sys, pathlib, os

os.chdir(pathlib.Path(__file__).resolve().parent.parent / 'share' / 'gnutrition')
sys.path.append(os.getcwd())

import src.run_app
