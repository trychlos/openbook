# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = ofa_tva
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

tva_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(tva_built_sources)								\
	$(NULL)

nodist_libofa_tva_la_SOURCES = \
	$(tva_built_sources)								\
	$(NULL)

libofa_tva_la_SOURCES = \
	ofa-extension.c										\
	ofa-tva-dbmodel.c									\
	ofa-tva-dbmodel.h									\
	ofa-tva-execlose.c									\
	ofa-tva-execlose.h									\
	ofa-tva-form-page.c									\
	ofa-tva-form-page.h									\
	ofa-tva-form-properties.c							\
	ofa-tva-form-properties.h							\
	ofa-tva-form-store.c								\
	ofa-tva-form-store.h								\
	ofa-tva-form-treeview.c								\
	ofa-tva-form-treeview.h								\
	ofa-tva-main.c										\
	ofa-tva-main.h										\
	ofa-tva-record-new.c								\
	ofa-tva-record-new.h								\
	ofa-tva-record-page.c								\
	ofa-tva-record-page.h								\
	ofa-tva-record-properties.c							\
	ofa-tva-record-properties.h							\
	ofa-tva-record-store.c								\
	ofa-tva-record-store.h								\
	ofa-tva-record-treeview.c							\
	ofa-tva-record-treeview.h							\
	ofa-tva-style.c										\
	ofa-tva-style.h										\
	ofa-tva-tree-adder.c								\
	ofa-tva-tree-adder.h								\
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

CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(resfiles)											\
	$(resxml)											\
	$(NULL)
