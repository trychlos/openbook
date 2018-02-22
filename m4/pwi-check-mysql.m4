# pwi-check-mysql.m4
#
# Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at 
# your option) any later version.
#
# This code is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this code; see the file COPYING. If not, see
# <http://www.gnu.org/licenses/>.
#
# Authors:
#   Pierre Wieser <pwieser@trychlos.org>

# serial 1 creation
#
# PWI_CHECK_MYSQL:
# $1: minimal required version (opt)
# $2: missing is fatal (yes|no, default to yes)
#
# pwi 2015- 3- 8: this is a hack from MYSQL_CLIENT standard macro
# in order to survive to a missing library. 
# As of 5.5.40, MySQL doesn't provide a pkg-config file.

dnl usage: PWI_CHECK_MYSQL([version[,fatal]])
dnl if 'fatal' != 'no', then a missing library will emit a warning and
dnl terminate the process.

# translit($1, 'a-z', 'A-Z'),

AC_DEFUN([PWI_CHECK_MYSQL],[
	_ac_cond="MySQL-client"
	if test "$1" != ""; then _ac_cond="MySQL-client >= $1"; fi
	_ac_fatal="yes"
	if test "$2" = "no"; then _ac_fatal="no"; fi

	AC_REQUIRE([PWI_MYSQL_CONFIG])
	AC_MSG_CHECKING([for MySQL])

	MYSQL_msg_version=`$mysql_config --version 2>/dev/null`
	have_MYSQL="yes"
	if test -z "${MYSQL_msg_version}" ; then
		have_MYSQL="no"
		MYSQL_msg_version="no"
	else
		_ac_includedir=`$mysql_config --variable=pkgincludedir 2>/dev/null`
		if test -z "${_ac_includedir}" || test ! -d "${_ac_includedir}"; then
			have_MYSQL="no"
			MYSQL_msg_version="no"
		fi
	fi

	AC_MSG_RESULT([${have_MYSQL}])

	if test "${have_MYSQL}" = "yes"; then
		PWI_CFLAGS="${PWI_CFLAGS} `$mysql_config --cflags`"
		PWI_LIBS="${PWI_LIBS} `$mysql_config --libs`"
	else
		_PWI_CHECK_MODULE_MSG([${_ac_fatal}],[MySQL-client: condition ${_ac_cond} not satisfied])
	fi

	AM_CONDITIONAL([HAVE_MYSQL], [test "${have_MYSQL}" = "yes"])
])

AC_DEFUN([PWI_MYSQL_CONFIG],[
	AC_ARG_WITH([mysql-config],
		AS_HELP_STRING(
			[--with-mysql-config=PATH],
			[A path to mysql_config script]),
            [mysql_config="$withval"], [mysql_config=mysql_config])
])
