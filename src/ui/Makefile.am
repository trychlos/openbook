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

bin_PROGRAMS = \
	openbook											\
	$(NULL)

ui_datadir = $(pkgdatadir)/ui

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPKGUIDIR=\"$(ui_datadir)\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"						\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

BUILT_SOURCES = \
	$(NULL)

openbook_SOURCES = \
	main.c												\
	my-utils.c											\
	my-utils.h											\
	ofa-application.c									\
	ofa-application.h									\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-settings.c										\
	ofa-settings.h										\
	ofa-sgbd.c											\
	ofa-sgbd.h											\
	ofo-dossier.c										\
	ofo-dossier.h										\
	$(BUILT_SOURCES)									\
	$(NULL)

openbook_LDADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

actions_files = \
	ofa-app-menubar.ui									\
	$(NULL)

ui_files = \
	ofa-dossier-new.ui									\
	ofa-dossier-open.ui									\
	$(NULL)

images_files = \
	$(NULL)

ui_data_DATA = \
	$(actions_files)									\
	$(ui_files)											\
	$(images_files)										\
	$(NULL)

@INTLTOOL_DESKTOP_RULE@

#applications_in_files = nact.desktop.in
applications_in_files = \
	$(NULL)

#applications_files = $(applications_in_files:.desktop.in=.desktop)
applications_files = \
	$(NULL)

applicationsdir = $(datadir)/applications

applications_DATA = $(applications_files)

CLEANFILES = \
	$(applications_files)								\
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(applications_in_files)							\
	$(ui_data_DATA)										\
	ofa-marshal.def										\
	$(NULL)
