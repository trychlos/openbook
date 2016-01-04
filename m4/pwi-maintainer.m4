# pwi-maintainer.m4
#
# Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

# serial 2 creation
#          PWI_MAINTAINER_CHECK_MODE
#          PWI_MAINTAINER_CHECK_FOR_DEPRECATED

dnl define PWI_MAINTAINER_CHECK_MODE
dnl
dnl Don''t agree with maintainer mode use
dnl See http://www.gnu.org/software/automake/manual/automake.html#maintainer_002dmode
dnl but gnome-autogen.sh forces its usage and gnome_common_init requires it
dnl is nonetheless explicitely required by gnome_maintainer_mode_defines macro

AC_DEFUN([PWI_MAINTAINER_CHECK_MODE],[
	AC_REQUIRE([GNOME_MAINTAINER_MODE_DEFINES])

	msg_maintainer_mode="disabled"
	AC_MSG_CHECKING([whether to enable maintainer mode])
	AC_MSG_RESULT([${USE_MAINTAINER_MODE}])

	if test "${USE_MAINTAINER_MODE}" = "yes"; then
		AC_DEFINE([PWI_MAINTAINER_MODE],[1],[Define to 1 if we are in maintainer mode])
		AC_SUBST([AM_CPPFLAGS],["${AM_CPPFLAGS} ${DISABLE_DEPRECATED} -DGSEAL_ENABLED"])
		AC_SUBST([AM_CFLAGS],["${AM_CFLAGS} -Werror -O0"])
		msg_maintainer_mode="enabled"
	fi

	AM_CONDITIONAL([PWI_MAINTAINER_MODE], [test "${USE_MAINTAINER_MODE}" = "yes"])
])

AC_DEFUN([PWI_MAINTAINER_CHECK_FOR_DEPRECATED],[
	AC_ARG_ENABLE(
		[deprecated],
		AC_HELP_STRING(
			[--enable-deprecated],
			[whether to enable deprecated symbols]),
		[enable_deprecated=$enableval],
		[enable_deprecated="no"])

	AC_MSG_CHECKING([whether deprecated symbols should be enabled])
	AC_MSG_RESULT([${enable_deprecated}])

	if test "${enable_deprecated}" = "yes"; then
		AC_DEFINE([PWI_ENABLE_DEPRECATED],[1],[Define to 1 if deprecated symbols should be enabled])
	elif test "${pwi_request_for_deprecated}" = "yes"; then
		AC_MSG_WARN([API documentation will be incomplete as deprecated symbols are disabled])
	fi

	AM_CONDITIONAL([ENABLE_DEPRECATED], [test "${enable_deprecated}" = "yes"])
])
