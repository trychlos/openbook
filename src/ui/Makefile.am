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
	ofa-account-notebook.c								\
	ofa-account-notebook.h								\
	ofa-account-properties.c							\
	ofa-account-properties.h							\
	ofa-accounts-chart.c								\
	ofa-accounts-chart.h								\
	ofa-account-select.c								\
	ofa-account-select.h								\
	ofa-application.c									\
	ofa-application.h									\
	ofa-base-dialog.c									\
	ofa-base-dialog.h									\
	ofa-base-dialog-prot.h								\
	ofa-devise-properties.c								\
	ofa-devise-properties.h								\
	ofa-devises-set.c									\
	ofa-devises-set.h									\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-dossier-properties.c							\
	ofa-dossier-properties.h							\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-journal-combo.c									\
	ofa-journal-combo.h									\
	ofa-journal-properties.c							\
	ofa-journal-properties.h							\
	ofa-journals-set.c									\
	ofa-journals-set.h									\
	ofa-main-page.c										\
	ofa-main-page.h										\
	ofa-main-page-def.h									\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-main-window-def.h								\
	ofa-model-properties.c								\
	ofa-model-properties.h								\
	ofa-models-set.c									\
	ofa-models-set.h									\
	ofa-settings.c										\
	ofa-settings.h										\
	ofa-taux-properties.c								\
	ofa-taux-properties.h								\
	ofa-taux-set.c										\
	ofa-taux-set.h										\
	ofo-account.c										\
	ofo-account.h										\
	ofo-account-def.h									\
	ofo-base.c											\
	ofo-base.h											\
	ofo-base-def.h										\
	ofo-base-prot.h										\
	ofo-devise.c										\
	ofo-devise.h										\
	ofo-devise-def.h									\
	ofo-dossier.c										\
	ofo-dossier.h										\
	ofo-dossier-def.h									\
	ofo-entry.c											\
	ofo-entry.h											\
	ofo-journal.c										\
	ofo-journal.h										\
	ofo-journal-def.h									\
	ofo-sgbd.c											\
	ofo-sgbd.h											\
	ofo-sgbd-def.h										\
	ofo-model.c											\
	ofo-model.h											\
	ofo-model-def.h										\
	ofo-taux.c											\
	ofo-taux.h											\
	ofo-taux-def.h										\
	$(BUILT_SOURCES)									\
	$(NULL)

openbook_LDADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

actions_files = \
	ofa-app-menubar.ui									\
	ofa-dos-menubar.ui									\
	$(NULL)

rc_files = \
	ofa.css
	$(NULL)

ui_files = \
	ofa-account-properties.ui							\
	ofa-account-select.ui								\
	ofa-devise-properties.ui							\
	ofa-dossier-new.ui									\
	ofa-dossier-open.ui									\
	ofa-dossier-properties.ui							\
	ofa-guided-input.ui									\
	ofa-journal-properties.ui							\
	ofa-model-properties.ui								\
	ofa-taux-properties.ui								\
	$(NULL)

images_files = \
	$(NULL)

ui_data_DATA = \
	$(actions_files)									\
	$(rc_files)											\
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
	$(NULL)
