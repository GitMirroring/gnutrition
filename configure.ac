# configure.ac for GNUtrition
# Copyright (C) 2026 Free Software Foundation, Inc.
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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

AC_INIT([gnutrition], m4_esyscmd_s([./version.sh]), [bug-gnutrition@gnu.org], [gnutrition], [https://www.gnu.org/software/gnutrition/])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

# Checks for programs.
AC_PROG_CC

# Use C99.
CFLAGS="${CFLAGS:-} -D_POSIX_C_SOURCE=200809L -std=c99"

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h ctype.h errno.h getopt.h libgen.h locale.h])

# Checks for library functions.
AC_CHECK_FUNCS([strdup strerror getopt_long setlocale])

# Check for math library.
AC_CHECK_LIB([m], [fmod])

# Check for SQLite3.
PKG_CHECK_MODULES([SQLITE3], [sqlite3])

# Check for ncurses.
PKG_CHECK_MODULES([NCURSES], [ncurses])

# Optional: GTK 3 for the GUI binary.
PKG_CHECK_MODULES([GTK], [gtk+-3.0], [have_gtk=yes], [have_gtk=no])

# Optional: GIO 2 for D-Bus support in the GUI binary.
PKG_CHECK_MODULES([GIO], [gio-2.0], [have_gio=yes], [have_gio=no])

AM_CONDITIONAL([BUILD_GTK], [test "x$have_gtk" = xyes -a "x$have_gio" = xyes])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
