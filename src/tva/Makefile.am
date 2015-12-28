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

plugin_id = ofa-tva

pkglib_LTLIBRARIES = libofa-tva.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)								\
	-I $(top_srcdir)/src							\
	$(OPENBOOK_CFLAGS)								\
	-DG_LOG_DOMAIN=\"OFA\"							\
	-DPLUGINUIDIR=\"$(plugin_datadir)\"				\
	$(NULL)

libofa_tva_la_SOURCES = \
	ofa-module.c									\
	ofa-tva.c										\
	ofa-tva.h										\
	ofa-tva-dbcustomer.c							\
	ofa-tva-dbcustomer.h							\
	ofa-tva-define-page.c							\
	ofa-tva-define-page.h							\
	ofa-tva-form-properties.c						\
	ofa-tva-form-properties.h						\
	ofa-tva-form-store.c							\
	ofa-tva-form-store.h							\
	ofo-tva-form.c									\
	ofo-tva-form.h									\
	$(NULL)

libofa_tva_la_LIBADD = \
	$(OPENBOOK_LIBS)								\
	$(NULL)

libofa_tva_la_LDFLAGS = \
	-module 										\
	-no-undefined									\
	-avoid-version									\
	$(NULL)

ui_files = \
	ofa-tva-form-properties.ui						\
	$(NULL)

images_files = \
	$(NULL)

plugin_data_DATA = \
	$(ui_files)										\
	$(images_files)									\
	$(NULL)

EXTRA_DIST = \
	$(plugin_data_DATA)								\
	$(NULL)