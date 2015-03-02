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

# serial 1 creation
#          OFA_CHECK_FOR_GTK

dnl check that we have the required Gtk+ version
dnl set in configure.ac::gtk_required variable

AC_DEFUN([OFA_CHECK_FOR_GTK],[
	AC_REQUIRE([_AC_OFA_CHECK_GTK])dnl
])

AC_DEFUN([_AC_OFA_CHECK_GTK],[
	_AC_OFA_CHECK_FOR_GTK3
	if test "${have_gtk3}" != "yes"; then
		AC_MSG_WARN([unable to find any suitable Gtk+ development library])
		let ofa_fatal_count+=1
	fi
])

dnl test for Gtk+-3.0 and its dependancies
dnl set have_gtk3=yes if all is ok

AC_DEFUN([_AC_OFA_CHECK_FOR_GTK3],[
	PKG_CHECK_MODULES([GTK3],[gtk+-3.0 >= ${gtk_required}],[have_gtk3=yes],[have_gtk3=no])
	if test "${have_gtk3}" = "yes"; then
		msg_gtk_version=$(pkg-config --modversion gtk+-3.0)
		OPENBOOK_CFLAGS="${OPENBOOK_CFLAGS} ${GTK3_CFLAGS}"
		OPENBOOK_LIBS="${OPENBOOK_LIBS} ${GTK3_LIBS}"
	fi
])
