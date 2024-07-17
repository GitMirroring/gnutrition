# Copyright (C) 2000, 2001, 2002 Edgar Denny
# Copyright (C) 2010, 2012 Free Software Foundation, Inc.
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

dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/run_app.py)

PACKAGE=gnutrition
AC_SUBST(PACKAGE)

MAJOR_VERSION=0
MINOR_VERSION=32
REVISION=2
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(REVISION)

dnl Get the default Python
AC_PATH_PROG(PYTHON, python)
AC_SUBST(PYTHON)

dnl Checking for Python >= 2.6
AC_MSG_CHECKING(for python >= 2.6)

prog1="
import sys
if sys.hexversion < 33685744:
    sys.exit(1)
sys.exit(0)"

prog2="
import sys
"

if $PYTHON -c "$prog1" 1>&AC_FD_CC 2>&AC_FD_CC
then  
    dnl The default Python path is 2.6 or higher.
    AC_MSG_RESULT(yes)
else
    dnl The default Python path is lower that 2.2. Can we find
    dnl any other versions of Python?
    AC_MSG_ERROR(no)
fi

changequote(, )
PYTHON_VERSION=`$PYTHON -c "import sys; print sys.version[:3]"`
PYTHON_PREFIX=`$PYTHON -c "import sys; print sys.prefix"`
PYTHON_SITE_DIR=$PYTHON_PREFIX/lib/python$PYTHON_VERSION/site-packages
changequote([, ])
AC_SUBST(PYTHON_VERSION)
AC_SUBST(PYTHON_PREFIX)
AC_SUBST(PYTHON_SITE_DIR)

AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl Check if python bindings for gtk are installed
AC_MSG_CHECKING(for pygtk)
prog3="
import sys
try:
    import gtk
except ImportError:
    sys.exit(1)
sys.exit(0)"
if $PYTHON -c "$prog3" 1>&AC_FD_CC 2>&AC_FD_CC
then  
    AC_MSG_RESULT(yes)
else
    AC_MSG_ERROR([
**** The python interpreter can't find the python bindings for gtk.])
fi

AC_OUTPUT( src/install.py\
	Makefile\
	src/Makefile\
	src/util/Makefile\
	data/Makefile\
	pixmaps/Makefile\
	doc/Makefile\
	rpm/Makefile\
	rpm/gnutrition.spec\
	debian/Makefile)
