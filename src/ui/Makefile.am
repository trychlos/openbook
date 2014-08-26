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
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"						\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

BUILT_SOURCES = \
	$(NULL)

openbook_SOURCES = \
	main.c												\
	my-assistant.c										\
	my-assistant.h										\
	my-dialog.c											\
	my-dialog.h											\
	my-window.c											\
	my-window.h											\
	my-window-prot.h									\
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
	ofa-backup.c										\
	ofa-backup.h										\
	ofa-bat-common.c									\
	ofa-bat-common.h									\
	ofa-bat-properties.c								\
	ofa-bat-properties.h								\
	ofa-bat-select.c									\
	ofa-bat-select.h									\
	ofa-bat-set.c										\
	ofa-bat-set.h										\
	ofa-class-properties.c								\
	ofa-class-properties.h								\
	ofa-classes-set.c									\
	ofa-classes-set.h									\
	ofa-devise-combo.c									\
	ofa-devise-combo.h									\
	ofa-devise-properties.c								\
	ofa-devise-properties.h								\
	ofa-devises-set.c									\
	ofa-devises-set.h									\
	ofa-dossier-manager.c								\
	ofa-dossier-manager.h								\
	ofa-dossier-delete.c								\
	ofa-dossier-delete.h								\
	ofa-dossier-delete-prefs.c							\
	ofa-dossier-delete-prefs.h							\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-dossier-properties.c							\
	ofa-dossier-properties.h							\
	ofa-guided-common.c									\
	ofa-guided-common.h									\
	ofa-guided-ex.c										\
	ofa-guided-ex.h										\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-export.c										\
	ofa-export.h										\
	ofa-iimporter.c										\
	ofa-import.c										\
	ofa-import.h										\
	ofa-importer.c										\
	ofa-importer.h										\
	ofa-int-closing.c									\
	ofa-int-closing.h									\
	ofa-ipreferences.c									\
	ofa-journal-combo.c									\
	ofa-journal-combo.h									\
	ofa-journal-properties.c							\
	ofa-journal-properties.h							\
	ofa-journals-set.c									\
	ofa-journals-set.h									\
	ofa-journal-treeview.c								\
	ofa-journal-treeview.h								\
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
	ofa-plugin-manager.c								\
	ofa-plugin-manager.h								\
	ofa-preferences.c									\
	ofa-preferences.h									\
	ofa-print-reconcil.c								\
	ofa-print-reconcil.h								\
	ofa-rappro.c										\
	ofa-rappro.h										\
	ofa-tab-label.c										\
	ofa-tab-label.h										\
	ofa-taux-properties.c								\
	ofa-taux-properties.h								\
	ofa-taux-set.c										\
	ofa-taux-set.h										\
	ofa-view-entries.c									\
	ofa-view-entries.h									\
	$(NULL)

openbook_LDADD = \
	$(top_builddir)/src/core/libofa-core.la				\
	$(OPENBOOK_LIBS)									\
	$(NULL)

actions_files = \
	ofa-app-menubar.ui									\
	ofa-dos-menubar.ui									\
	$(NULL)

rc_files = \
	ofa.css												\
	$(NULL)

ui_files = \
	ofa-account-notebook.ui								\
	ofa-account-properties.ui							\
	ofa-account-select.ui								\
	ofa-bat-common.ui									\
	ofa-bat-properties.ui								\
	ofa-bat-select.ui									\
	ofa-class-properties.ui								\
	ofa-devise-properties.ui							\
	ofa-dossier-manager.ui								\
	ofa-dossier-delete.ui								\
	ofa-dossier-delete-prefs.piece.ui					\
	ofa-dossier-new.ui									\
	ofa-dossier-open.ui									\
	ofa-dossier-properties.ui							\
	ofa-export.ui										\
	ofa-guided-input.ui									\
	ofa-import.ui										\
	ofa-int-closing.ui									\
	ofa-journal-properties.ui							\
	ofa-model-properties.ui								\
	ofa-plugin-manager.ui								\
	ofa-preferences.ui									\
	ofa-print-reconcil.ui								\
	ofa-taux-properties.ui								\
	ofa-view-entries.piece.ui							\
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
