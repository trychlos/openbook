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

libofa_recurrent_la_SOURCES = \
	$(BUILT_SOURCES)									\
	ofa-module.c										\
	ofa-recurrent.c										\
	ofa-recurrent.h										\
	ofa-recurrent-dbmodel.c								\
	ofa-recurrent-dbmodel.h								\
	ofa-recurrent-execlose.c							\
	ofa-recurrent-execlose.h							\
	ofa-recurrent-main.c								\
	ofa-recurrent-main.h								\
	ofa-recurrent-manage-page.c							\
	ofa-recurrent-manage-page.h							\
	ofa-recurrent-model-properties.c					\
	ofa-recurrent-model-properties.h					\
	ofa-recurrent-model-store.c							\
	ofa-recurrent-model-store.h							\
	ofa-recurrent-run-page.c							\
	ofa-recurrent-run-page.h							\
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

resources_ui = \
	ofa-recurrent-model-properties.ui					\
	ofa-recurrent-run-page.ui							\
	$(NULL)

images_files = \
	$(NULL)

plugin_data_DATA = \
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
