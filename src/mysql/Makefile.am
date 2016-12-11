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

# HAVE_MYSQL is set in PWI_CHECK_MYSQL m4 macro
if HAVE_MYSQL

plugin_id = ofa-mysql

pkglib_LTLIBRARIES = libofa-mysql.la

plugin_datadir = $(pkgdatadir)/$(plugin_id)

init1_datadir = $(pkgdatadir)/init_utf8_comma_pipe_fr

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	$(OPENBOOK_CFLAGS)									\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPROVIDER_ID=\"$(plugin_id)\"						\
	-DPROVIDER_DATADIR=\"$(plugin_datadir)\"			\
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\"				\
	-DINIT1DIR=\""$(init1_datadir)"\"					\
	$(NULL)

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = ofa_mysql
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

mysql_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(mysql_built_sources)								\
	$(NULL)

nodist_libofa_mysql_la_SOURCES = \
	$(mysql_built_sources)								\
	$(NULL)

libofa_mysql_la_SOURCES = \
	ofa-extension.c										\
	ofa-mysql-cmdline.c									\
	ofa-mysql-cmdline.h									\
	ofa-mysql-connect.c									\
	ofa-mysql-connect.h									\
	ofa-mysql-dbmodel.c									\
	ofa-mysql-dbmodel.h									\
	ofa-mysql-dossier-meta.c							\
	ofa-mysql-dossier-meta.h							\
	ofa-mysql-dbprovider.c								\
	ofa-mysql-dbprovider.h								\
	ofa-mysql-editor-display.c							\
	ofa-mysql-editor-display.h							\
	ofa-mysql-editor-enter.c							\
	ofa-mysql-editor-enter.h							\
	ofa-mysql-exercice-meta.c							\
	ofa-mysql-exercice-meta.h							\
	ofa-mysql-prefs-bin.c								\
	ofa-mysql-prefs-bin.h								\
	ofa-mysql-properties.c								\
	ofa-mysql-properties.h								\
	ofa-mysql-user-prefs.c								\
	ofa-mysql-user-prefs.h								\
	$(NULL)

libofa_mysql_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_mysql_la_LDFLAGS = \
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

endif
