# Makefile.am for GNUtrition
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

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = gnutrition

# -----------------------------------------------------------------------------
# CLI binary (ncurses)
# -----------------------------------------------------------------------------

gnutrition_SOURCES = \
	src/main.c \
	src/ui.c \
	src/db.c \
	src/budget.c \
	src/log.c

gnutrition_CFLAGS = -Wall -Wextra -pedantic $(SQLITE3_CFLAGS) $(NCURSES_CFLAGS)
gnutrition_CPPFLAGS = -I$(srcdir)/src -DLOCALEDIR=\"$(localedir)\"
gnutrition_LDADD = $(SQLITE3_LIBS) $(NCURSES_LIBS) -lm

# -----------------------------------------------------------------------------
# GTK GUI binary (optional, built when GTK 3 and GIO are available)
# -----------------------------------------------------------------------------

if BUILD_GTK
bin_PROGRAMS += gnutrition-gtk

gnutrition_gtk_SOURCES = \
	src/gui_main.c \
	src/gui.c \
	src/dbus.c \
	src/db.c \
	src/budget.c \
	src/log.c

gnutrition_gtk_CFLAGS = -Wall -Wextra -pedantic $(SQLITE3_CFLAGS) $(GTK_CFLAGS) $(GIO_CFLAGS)
gnutrition_gtk_CPPFLAGS = -I$(srcdir)/src -DLOCALEDIR=\"$(localedir)\"
gnutrition_gtk_LDADD = $(SQLITE3_LIBS) $(GTK_LIBS) $(GIO_LIBS) -lm
endif

# Include header files and support scripts in the distribution.
EXTRA_DIST = \
	src/budget.h \
	src/db.h \
	src/dbus.h \
	src/gui.h \
	src/i18n.h \
	src/log.h \
	src/ui.h \
	version.sh \
	build_db.sh

# Man page.
dist_man_MANS = man/gnutrition.1

# -----------------------------------------------------------------------------
# Distribution support
# The dist-hook bakes the version into .tarball-version so release tarballs
# can be built without git.
# -----------------------------------------------------------------------------

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
