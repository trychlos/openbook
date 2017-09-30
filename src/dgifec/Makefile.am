# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

plugin_id = ofa-dgifec

pkglib_LTLIBRARIES = libofa-dgifec.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)								\
	-I $(top_srcdir)/src							\
	$(OPENBOOK_CFLAGS)								\
	-DG_LOG_DOMAIN=\"OFA\"							\
	-DPROVIDER_ID=\"$(plugin_id)\"					\
	-DPROVIDER_DATADIR=\"$(plugin_datadir)\"		\
	$(NULL)

libofa_dgifec_la_SOURCES = \
	ofa-extension.c									\
	ofa-fec-export.c								\
	ofa-fec-export.h								\
	ofa-fec-ident.c									\
	ofa-fec-ident.h									\
	$(NULL)

libofa_dgifec_la_LIBADD = \
	$(OPENBOOK_LIBS)								\
	$(NULL)

libofa_dgifec_la_LDFLAGS = \
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
