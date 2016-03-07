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

plugin_id = ofa-tva

pkglib_LTLIBRARIES = libofa-tva.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	$(OPENBOOK_CFLAGS)									\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPLUGINUIDIR=\"$(plugin_datadir)\"					\
	$(NULL)

resdir = $(srcdir)
resxml = $(srcdir)/openbook.gresource.xml
resource_files = $(shell glib-compile-resources --sourcedir=$(resdir) --generate-dependencies $(resxml))
openbook-resources.c: $(resxml) $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(resdir) --generate-source --c-name openbook $(resxml)
openbook-resources.h: $(resxml) $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(resdir) --generate-header --c-name openbook $(resxml)

openbook_built_sources = \
	openbook-resources.c								\
	openbook-resources.h								\
	$(NULL)

BUILT_SOURCES = \
	$(openbook_built_sources)							\
	$(NULL)

nodist_openbook_SOURCES = \
	$(openbook_built_sources)							\
	$(NULL)

libofa_tva_la_SOURCES = \
	$(BUILT_SOURCES)									\
	ofa-module.c										\
	ofa-tva.c											\
	ofa-tva.h											\
	ofa-tva-dbmodel.c									\
	ofa-tva-dbmodel.h									\
	ofa-tva-declare-page.c								\
	ofa-tva-declare-page.h								\
	ofa-tva-execlose.c									\
	ofa-tva-execlose.h									\
	ofa-tva-form-properties.c							\
	ofa-tva-form-properties.h							\
	ofa-tva-form-store.c								\
	ofa-tva-form-store.h								\
	ofa-tva-main.c										\
	ofa-tva-main.h										\
	ofa-tva-manage-page.c								\
	ofa-tva-manage-page.h								\
	ofa-tva-record-new.c								\
	ofa-tva-record-new.h								\
	ofa-tva-record-properties.c							\
	ofa-tva-record-properties.h							\
	ofa-tva-record-store.c								\
	ofa-tva-record-store.h								\
	ofo-tva-form.c										\
	ofo-tva-form.h										\
	ofo-tva-record.c									\
	ofo-tva-record.h									\
	$(NULL)

libofa_tva_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_tva_la_LDFLAGS = \
	-module 											\
	-no-undefined										\
	-avoid-version										\
	$(NULL)

ui_files = \
	ofa-tva-record-properties.ui						\
	$(NULL)

resources_ui = \
	ofa-tva-form-properties.ui							\
	ofa-tva-record-new.ui								\
	$(NULL)

images_files = \
	$(NULL)

plugin_data_DATA = \
	$(ui_files)											\
	$(images_files)										\
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(plugin_data_DATA)									\
	$(resources_ui)										\
	$(resxml)											\
	$(NULL)
