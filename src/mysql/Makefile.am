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

# HAVE_MYSQL is set in PWI_CHECK_MYSQL m4 macro
if HAVE_MYSQL

plugin_id = ofa-mysql

pkglib_LTLIBRARIES = libofa-mysql.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)								\
	-I $(top_srcdir)/src							\
	$(OPENBOOK_CFLAGS)								\
	-DG_LOG_DOMAIN=\"OFA\"							\
	-DPROVIDER_ID=\"$(plugin_id)\"					\
	-DPROVIDER_DATADIR=\"$(plugin_datadir)\"		\
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\"			\
	$(NULL)

libofa_mysql_la_SOURCES = \
	ofa-mysql.c										\
	ofa-mysql.h										\
	ofa-mysql-cmdline.c								\
	ofa-mysql-cmdline.h								\
	ofa-mysql-connect.c								\
	ofa-mysql-connect.h								\
	ofa-mysql-editor-display.c						\
	ofa-mysql-editor-display.h						\
	ofa-mysql-editor-enter.c						\
	ofa-mysql-editor-enter.h						\
	ofa-mysql-meta.c								\
	ofa-mysql-meta.h								\
	ofa-mysql-idbprovider.c							\
	ofa-mysql-idbprovider.h							\
	ofa-mysql-iprefs-provider.c						\
	ofa-mysql-iprefs-provider.h						\
	ofa-mysql-period.c								\
	ofa-mysql-period.h								\
	ofa-mysql-prefs-bin.c							\
	ofa-mysql-prefs-bin.h							\
	ofa-mysql-user-prefs.c							\
	ofa-mysql-user-prefs.h							\
	ofa-module.c									\
	$(NULL)

libofa_mysql_la_LIBADD = \
	$(OPENBOOK_LIBS)								\
	$(NULL)

libofa_mysql_la_LDFLAGS = \
	-module 										\
	-no-undefined									\
	-avoid-version									\
	$(NULL)

ui_files = \
	ofa-mysql-editor-display.ui						\
	ofa-mysql-editor-enter.ui						\
	ofa-mysql-prefs-bin.ui							\
	$(NULL)

images_files = \
	$(NULL)

plugin_data_DATA = \
	$(ui_files)										\
	$(images_files)									\
	$(NULL)

#pkglibexec_SCRIPTS = \
#	ofa-mysql-clone.sh								\
#	$(NULL)

EXTRA_DIST = \
	$(plugin_data_DATA)								\
	$(NULL)

endif
