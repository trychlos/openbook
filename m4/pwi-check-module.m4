# pwi-check-module.m4
#
# Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

# serial 3 missing is fatal if CFLAGS or LIBS are set
#
# PWI_CHECK_MODULE:
# $1: mnemo
# $2: pkg-config library name
# $3: minimal required version (opt)
# $4: missing is fatal (yes|no, default to yes)

dnl usage: PWI_CHECK_MODULE(var,module[,version[,fatal]])
dnl if 'fatal' != 'no', then a missing library will emit a warning and
dnl terminate the process.

# translit($1, 'a-z', 'A-Z'),

AC_DEFUN([PWI_CHECK_MODULE],[
	_ac_cond="$2"
	if test "$3" != ""; then _ac_cond="$2 >= $3"; fi
	_ac_fatal="yes"
	if test "$4" = "no"; then _ac_fatal="no"; fi

	# if CFLAGS or LIBS are set, then missing library is fatal
	#echo "$1_CFLAGS='${$1_CFLAGS}'"
	#echo "$1_LIBS='${$1_LIBS}'"
	if ! test -z "${$1_CFLAGS}${$1_LIBS}"; then
		_ac_fatal="yes"
	fi

	PKG_CHECK_MODULES([$1],[${_ac_cond}],[have_$1="yes"],[have_$1="no"])

	#echo "have_gtk2=$have_gtk2 have_gtk3=$have_gtk3"
	#echo "$1_CFLAGS='${$1_CFLAGS}'"
	#echo "$1_LIBS='${$1_LIBS}'"
	#echo "against Gtk2: $(echo ${$1_LIBS} | grep -E 'gtk-@<:@^-@:>@+-2\.0')"
	#echo "against Gtk3: $(echo ${$1_LIBS} | grep -E 'gtk-@<:@^-@:>@+-3\.0')"

	if test "${have_$1}" = "yes"; then
		$1_msg_version=$(pkg-config --modversion $2)
		echo "Adding '${$1_CFLAGS}' to PWI_CFLAGS"
		PWI_CFLAGS="${PWI_CFLAGS} ${$1_CFLAGS}"
		echo "Adding '${$1_LIBS}' to PWI_LIBS"
		PWI_LIBS="${PWI_LIBS} ${$1_LIBS}"
	else
		_PWI_CHECK_MODULE_MSG([${_ac_fatal}],[$1: condition ${_ac_cond} not satisfied])
	fi

	AM_CONDITIONAL([HAVE_$1], [test "${have_$1}" = "yes"])
])

AC_DEFUN([_PWI_CHECK_MODULE_MSG],[
	if test "$1" = "no"; then
		AC_MSG_RESULT([warning: $2])
	else
		let pwi_fatal_count+=1
		AC_MSG_WARN([$2 (fatal_count=${pwi_fatal_count})])
		if test "${pwi_fatal_list}" != ""; then pwi_fatal_list="${pwi_fatal_list}
"; fi
		pwi_fatal_list="${pwi_fatal_list}$2"
	fi
])
