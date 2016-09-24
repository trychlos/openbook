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

plugin_id = ofa-recurrent

pkglib_LTLIBRARIES = libofa-recurrent.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	$(OPENBOOK_CFLAGS)									\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPROVIDER_ID=\"$(plugin_id)\"						\
	-DPROVIDER_DATADIR=\"$(plugin_datadir)\"			\
	$(NULL)

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = ofa_recurrent
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

recurrent_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(recurrent_built_sources)							\
	$(NULL)

nodist_libofa_recurrent_la_SOURCES = \
	$(recurrent_built_sources)							\
	$(NULL)

libofa_recurrent_la_SOURCES = \
	ofa-extension.c										\
	ofa-recurrent-dbmodel.c								\
	ofa-recurrent-dbmodel.h								\
	ofa-recurrent-execlose.c							\
	ofa-recurrent-execlose.h							\
	ofa-recurrent-generate.c							\
	ofa-recurrent-generate.h							\
	ofa-recurrent-main.c								\
	ofa-recurrent-main.h								\
	ofa-recurrent-model-page.c							\
	ofa-recurrent-model-page.h							\
	ofa-recurrent-model-properties.c					\
	ofa-recurrent-model-properties.h					\
	ofa-recurrent-model-store.c							\
	ofa-recurrent-model-store.h							\
	ofa-recurrent-model-treeview.c						\
	ofa-recurrent-model-treeview.h						\
	ofa-recurrent-run-page.c							\
	ofa-recurrent-run-page.h							\
	ofa-recurrent-run-store.c							\
	ofa-recurrent-run-store.h							\
	ofa-recurrent-run-treeview.c						\
	ofa-recurrent-run-treeview.h						\
	ofa-recurrent-tree-adder.c							\
	ofa-recurrent-tree-adder.h							\
	ofo-recurrent-gen.c									\
	ofo-recurrent-gen.h									\
	ofo-recurrent-model.c								\
	ofo-recurrent-model.h								\
	ofo-recurrent-run.c									\
	ofo-recurrent-run.h									\
	$(NULL)

libofa_recurrent_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_recurrent_la_LDFLAGS = \
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
