# Open Freelance Accounting
# A double-entry accounting application for freelances.
#
# Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
#
# $Id$

pkglib_LTLIBRARIES = libofa-core.la

core_datadir = $(pkgdatadir)/core

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPKGUIDIR=\"$(core_datadir)\"						\
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"						\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

BUILT_SOURCES = \
	ofo-marshal.c										\
	ofo-marshal.h										\
	$(NULL)

libofa_core_la_SOURCES = \
	$(BUILT_SOURCES)									\
	my-date.c											\
	my-dialog.c											\
	my-dialog.h											\
	my-double.c											\
	my-utils.c											\
	my-window.c											\
	my-window.h											\
	my-window-prot.h									\
	\
	ofa-application-def.h								\
	ofa-dblogin.c										\
	ofa-dblogin.h										\
	ofa-idbms.c											\
	ofa-main-window-def.h								\
	ofa-plugin.c										\
	ofa-plugin.h										\
	ofa-settings.c										\
	\
	ofo-account.c										\
	ofo-base.c											\
	ofo-bat.c											\
	ofo-bat-line.c										\
	ofo-class.c											\
	ofo-currency.c										\
	ofo-dossier.c										\
	ofo-entry.c											\
	ofo-ledger.c										\
	ofo-ope-template.c									\
	ofo-rate.c											\
	ofo-sgbd.c											\
	$(NULL)

ofo-marshal.h: ofo-marshal.def $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ofo_cclosure_marshal > $@

ofo-marshal.c: ofo-marshal.def $(GLIB_GENMARSHAL)
	echo "#include \"ofo-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ofo_cclosure_marshal >> $@

libofa_core_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_core_la_LDFLAGS = \
	-no-undefined										\
	-avoid-version										\
	-fPIC												\
	$(NULL)

rc_files = \
	ofa.css												\
	$(NULL)

ui_files = \
	ofa-dblogin.ui										\
	$(NULL)

core_data_DATA = \
	$(rc_files)											\
	$(ui_files)											\
	$(NULL)
	
CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	ofo-marshal.def										\
	$(core_data_DATA)									\
	$(NULL)
