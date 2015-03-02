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
	ofa-mysql-backup.c								\
	ofa-mysql-backup.h								\
	ofa-mysql-connect-display-piece.c				\
	ofa-mysql-connect-display-piece.h				\
	ofa-mysql-connect-enter-piece.c					\
	ofa-mysql-connect-enter-piece.h					\
	ofa-mysql-idbms.c								\
	ofa-mysql-idbms.h								\
	ofa-mysql-ipreferences.c						\
	ofa-mysql-ipreferences.h						\
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
	ofa-mysql-connect-display-piece.ui				\
	ofa-mysql-connect-enter-piece.ui				\
	ofa-mysql-prefs.piece.ui						\
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
