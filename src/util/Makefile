#  GNUtrition - a nutrition and diet analysis program.
#  Copyright (C) 2001-2002 Edgar Denny (edenny@skyweb.net)
#  Copyright (C) 2010 Free Software Foundation, Inc.
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

prefix      = /usr/local
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/bin
datadir     = ${datarootdir}
datarootdir = ${prefix}/share
INSTALL     = /usr/bin/install -c
srcdir      = ../..
pycomp      = ${srcdir}/py-compile



all:
	chmod +x ${pycomp}
	${pycomp} *.py

tests:
	@TESTS='exception intrange log_levels validate_file';\
	for test in $${TESTS} ; do \
		cp test/$${test}.py ./$${test} ;\
		echo; echo " ***-- Running test: $$test --***" ;\
		./$${test};\
		rm $${test};\
	done

install:
	mkdir -p ${datadir}/gnutrition/src/util
	${INSTALL} -m 644 -c *.py *.py[co] ${datadir}/gnutrition/src/util

clean:
	rm -f *.py[co] test/*.py[co]

uninstall:
	rm -f ${datadir}/gnutrition/src/*.py ${datadir}/gnutrition/src/*.pyo
	rm -rf ${datadir}/gnutrition/src
