# Open Freelance Accounting
# A double-entry accounting application for freelances.
#
# Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

plugin_id = ofa-importers

pkglib_LTLIBRARIES = libofa-importers.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)								\
	-I $(top_srcdir)/src							\
	$(OPENBOOK_CFLAGS)								\
	-DG_LOG_DOMAIN=\"OFA\"							\
	-DPROVIDER_ID=\"$(plugin_id)\"					\
	-DPROVIDER_DATADIR=\"$(plugin_datadir)\"		\
	$(NULL)

libofa_importers_la_SOURCES = \
	ofa-extension.c									\
	ofa-importer-csv.c								\
	ofa-importer-csv.h								\
	$(NULL)

#	ofa-importer-txt.c								\
#	ofa-importer-txt.h								\
#	ofa-importer-txt-bourso.c						\
#	ofa-importer-txt-bourso.h						\
#	ofa-importer-txt-lcl.c							\
#	ofa-importer-txt-lcl.h							

libofa_importers_la_LIBADD = \
	$(OPENBOOK_LIBS)								\
	$(NULL)

libofa_importers_la_LDFLAGS = \
	-module 										\
	-no-undefined									\
	-avoid-version									\
	$(NULL)

images_files = \
	$(NULL)

plugin_data_DATA = \
	$(images_files)									\
	$(NULL)

EXTRA_DIST = \
	$(plugin_data_DATA)								\
	$(NULL)
