# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
#
# Open Firm Accounting is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# Open Firm Accounting is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Open Firm Accounting; see the file COPYING. If not,
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
	ofa-importer-ident.c							\
	ofa-importer-ident.h							\
	ofa-importer-pdf.c								\
	ofa-importer-pdf.h								\
	ofa-importer-pdf-bourso.c						\
	ofa-importer-pdf-bourso.h						\
	ofa-importer-pdf-lcl.c							\
	ofa-importer-pdf-lcl.h							\
	ofa-importer-txt.c								\
	ofa-importer-txt.h								\
	ofa-importer-txt-bourso.c						\
	ofa-importer-txt-bourso.h						\
	ofa-importer-txt-lcl.c							\
	ofa-importer-txt-lcl.h							\
	$(NULL)

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
