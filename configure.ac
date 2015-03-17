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

AC_PREREQ([2.53])

AC_INIT([OpenBook],[0.37],[openbook@trychlos.org],,[http://trychlos.github.io/openbook/])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/ui/main.c])
AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_MACRO_DIR([m4])

# check for compiler characteristics and options
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_STDC_HEADERS
AM_DISABLE_STATIC

# define specific compilation options
PWI_COMPILER_WARNINGS([],[-ansi -Wno-overlength-strings -Wformat=2])
PWI_COMPILER_LINK_AS_NEEDED
AC_SUBST([AM_CFLAGS],["${AM_CFLAGS} ${WARN_CFLAGS}"])

# other traditional tools
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Gnome stuff
GNOME_COMMON_INIT
PWI_MAINTAINER_CHECK_MODE

# libtool
AM_PROG_LIBTOOL

# localization
# note that this same version is also required by autogen.sh
IT_PROG_INTLTOOL([0.35.5])
GETTEXT_PACKAGE=${PACKAGE}
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["${GETTEXT_PACKAGE}"],[gettext package])
AM_GLIB_GNU_GETTEXT

# a counter of fatal warnings emitted
let pwi_fatal_count=0
pwi_fatal_list=""

# we are using pkgconfig for all development libraries we need
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "${PKG_CONFIG}" = "no"; then
	AC_MSG_ERROR([You need to install pkgconfig])
fi

# Oldest supported distribution as of May 2014: ?
glib_required=2.28
gtk3_required=3.4
PWI_CHECK_MODULE([GTK3],        [gtk+-3.0],    [${gtk3_required}])
PWI_CHECK_MODULE([GLIB],        [glib-2.0],    [${glib_required}])
PWI_CHECK_MODULE([GMODULE],     [gmodule-2.0], [${glib_required}])
PWI_CHECK_MODULE([POPPLER],     [poppler],     [0.24],            [no])
PWI_CHECK_MODULE([POPPLER_GLIB],[poppler-glib],[0.24],            [no])
PWI_CHECK_MYSQL([5.5],[no])

OPENBOOK_CFLAGS="${OPENBOOK_CFLAGS} ${PWI_CFLAGS}"
OPENBOOK_LIBS="${OPENBOOK_LIBS} ${PWI_LIBS}"

# GLib marshaling
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal, no)
if test "${GLIB_GENMARSHAL}" = "no"; then
	AC_MSG_WARN([glib2-devel installed, but glib-genmarshal not found])
	let ofa_fatal_count+=1
fi

AC_SUBST([OPENBOOK_CFLAGS])
AC_SUBST([OPENBOOK_LIBS])

AC_DEFINE([OPENBOOK_DEBUG],["OPENBOOK_DEBUG"],[Debug environment variable])

# documentation
PWI_MAINTAINER_CHECK_FOR_DEPRECATED

# built plugins
have_IMPORT_BOURSO="yes"				# always built
have_IMPORT_BOURSO_PDF="no"				# requires Poppler libraries
if test "${have_POPPLER}" = "yes" -a "${have_POPPLER_GLIB}" = "yes"; then
	have_IMPORT_BOURSO_PDF="yes"
fi
have_IMPORT_LCL="yes"					# always built
have_IMPORT_LCL_PDF="no"				# requires Poppler libraries
if test "${have_POPPLER}" = "yes" -a "${have_POPPLER_GLIB}" = "yes"; then
	have_IMPORT_LCL_PDF="yes"
fi
have_POSTGRE="no"						# not built as of 0.35
AM_CONDITIONAL([HAVE_POSTGRE], [test "${have_POSTGRE}" = "yes"])

# display and keep configuration informations
config_options="`$as_echo "$ac_configure_args" | sed 's/^ //; s/[\\""\`\$]/\\\\&/g'`" 
AC_DEFINE_UNQUOTED([OFA_CONFIG_OPTIONS],["$0 ${config_options}"],["Configure options"])

AC_CONFIG_FILES([
	Makefile
	data/Makefile
	data/desktop/Makefile
	data/icons/Makefile
	data/init_utf8_comma_pipe_fr/Makefile
	m4/Makefile
	maintainer/Makefile
	misc/Makefile
	misc/t_amounts/Makefile
	misc/t_delete/Makefile
	misc/t_dynbook/Makefile
	misc/t_nonmodal/Makefile
	misc/t_spawn/Makefile
	misc/t_weakref/Makefile
	po/Makefile.in
	src/Makefile
	src/api/Makefile
	src/core/Makefile
	src/css/Makefile
	src/import-bourso/Makefile
	src/import-bourso-pdf/Makefile
	src/import-lcl/Makefile
	src/import-lcl-pdf/Makefile
	src/mysql/Makefile
	src/postgre/Makefile
	src/recurrent/Makefile
	src/ui/Makefile
])

AC_OUTPUT

$as_echo "
	${PACKAGE_STRING} configuration summary:

	Installation prefix             ${prefix}
	Build system type               ${ac_cv_build}
	Gtk+ version                    ${GTK3_msg_version}
	GLib version                    ${GLIB_msg_version}
	Maintainer mode                 ${msg_maintainer_mode}
	Enable deprecated functions     ${enable_deprecated}
	Built plugins:
	  Import_Boursorama             ${have_IMPORT_BOURSO}"
if test "${have_IMPORT_BOURSO_PDF}" = "yes"; then
	$as_echo "	  Import_Boursorama_PDF         ${have_IMPORT_BOURSO_PDF} (with Poppler ${POPPLER_msg_version})"
else
	$as_echo "	  Import_Boursorama_PDF         ${have_IMPORT_BOURSO_PDF}"
fi
	$as_echo "	  Import_LCL                    ${have_IMPORT_LCL}"
if test "${have_IMPORT_LCL_PDF}" = "yes"; then
	$as_echo "	  Import_LCL_PDF                ${have_IMPORT_LCL_PDF} (with Poppler ${POPPLER_msg_version})"
else
	$as_echo "	  Import_LCL_PDF                ${have_IMPORT_LCL_PDF}"
fi
if test "${have_MYSQL}" = "yes"; then
	$as_echo "	  MySQL                         ${have_MYSQL} (with MySQL ${MYSQL_msg_version})"
else
	$as_echo "	  MySQL                         ${have_MYSQL}"
fi
if test "${have_POSTGRE}" = "yes"; then
	$as_echo "	  PostgreSQL                    ${have_POSTGRE} (with PostgreSQL ${POSTGRE_msg_version})"
else
	$as_echo "	  PostgreSQL                    ${have_POSTGRE}"
fi
	$as_echo "	  Recurrent                     yes"
	$as_echo " "

if test "${have_MYSQL}" = "no" && test "${have_POSTGRE}" = "no"; then
	$as_echo "	Warning: no built DBMS plugin
"
fi

if test ${pwi_fatal_count} -gt 0; then
	$as_echo "${pwi_fatal_list}
"
	exit 1
fi
