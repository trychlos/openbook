# Open Freelance Accounting
# A double-entry accounting application for freelances.
#
# Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
#
# Open Freelance Accounting is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# Open Freelance Accounting is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Open Freelance Accounting; see the file COPYING. If not,
# see <http://www.gnu.org/licenses/>.
#
# Authors:
#   Pierre Wieser <pwieser@trychlos.org>
#
# $Id$

# serial 1 creation
#          OFA_CHECK_MODULE

dnl usage: OFA_CHECK_MODULE(var,condition[,error])
dnl if 'error' != 'no', then displays an error message if condition is
dnl not met.
# translit($1, 'a-z', 'A-Z'),

AC_DEFUN([OFA_CHECK_MODULE],[
	PKG_CHECK_MODULES([$1],[$2],[have_$1="yes"],[have_$1="no"])

	#echo "have_gtk2=$have_gtk2 have_gtk3=$have_gtk3"
	#echo "$1_CFLAGS='${$1_CFLAGS}'"
	#echo "$1_LIBS='${$1_LIBS}'"
	#echo "against Gtk2: $(echo ${$1_LIBS} | grep -E 'gtk-@<:@^-@:>@+-2\.0')"
	#echo "against Gtk3: $(echo ${$1_LIBS} | grep -E 'gtk-@<:@^-@:>@+-3\.0')"

	if test "${have_$1}" = "yes"; then
		if test "${have_gtk3}" = "yes"; then
			if echo ${$1_LIBS} | grep -qE 'gtk-@<:@^-@:>@+-2\.0'; then
				_OFA_CHECK_MODULE_MSG([$3],[$1: compiling with Gtk+-3 but adresses Gtk+-2 libraries])
				have_$1="no"
			fi
		fi
	else
		_OFA_CHECK_MODULE_MSG([$3],[$1: condition $2 not satisfied])
	fi

	if test "${have_$1}" = "yes"; then
		OPENBOOK_CFLAGS="${OPENBOOK_CFLAGS} ${$1_CFLAGS}"
		OPENBOOK_LIBS="${OPENBOOK_LIBS} ${$1_LIBS}"
	fi
])

AC_DEFUN([_OFA_CHECK_MODULE_MSG],[
	if test "$1" = "no"; then
		AC_MSG_RESULT([warning: $2])
	else
		AC_MSG_WARN([$2])
		let ofa_fatal_count+=1
	fi
])
