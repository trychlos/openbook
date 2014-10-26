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
	my-cell-renderer-amount.c							\
	my-cell-renderer-amount.h							\
	my-editable-amount.c								\
	my-editable-amount.h								\
	my-editable-date.c									\
	my-editable-date.h									\
	my-tab-label.c										\
	my-tab-label.h										\
	ofa-account-properties.c							\
	ofa-account-properties.h							\
	ofa-accounts-book.c									\
	ofa-accounts-book.h									\
	ofa-accounts-page.c									\
	ofa-accounts-page.h									\
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
	ofa-currencies-set.c								\
	ofa-currencies-set.h								\
	ofa-currency-combo.c								\
	ofa-currency-combo.h								\
	ofa-currency-properties.c							\
	ofa-currency-properties.h							\
	ofa-dossier-manager.c								\
	ofa-dossier-manager.h								\
	ofa-dossier-delete.c								\
	ofa-dossier-delete.h								\
	ofa-dossier-delete-prefs.c							\
	ofa-dossier-delete-prefs.h							\
	ofa-dossier-login.c									\
	ofa-dossier-login.h									\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-dossier-properties.c							\
	ofa-dossier-properties.h							\
	ofa-export.c										\
	ofa-export.h										\
	ofa-guided-common.c									\
	ofa-guided-common.h									\
	ofa-guided-ex.c										\
	ofa-guided-ex.h										\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-iimporter.c										\
	ofa-import.c										\
	ofa-import.h										\
	ofa-importer.c										\
	ofa-importer.h										\
	ofa-int-closing.c									\
	ofa-int-closing.h									\
	ofa-ipreferences.c									\
	ofa-ledger-combo.c									\
	ofa-ledger-combo.h									\
	ofa-ledger-properties.c								\
	ofa-ledger-properties.h								\
	ofa-ledger-treeview.c								\
	ofa-ledger-treeview.h								\
	ofa-ledgers-page.c									\
	ofa-ledgers-page.h									\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-ope-template-properties.c						\
	ofa-ope-template-properties.h						\
	ofa-ope-templates-page.c							\
	ofa-ope-templates-page.h							\
	ofa-page.c											\
	ofa-page.h											\
	ofa-page-def.h										\
	ofa-page-prot.h										\
	ofa-page-book.c										\
	ofa-page-book.h										\
	ofa-page-tree.c										\
	ofa-page-tree.h										\
	ofa-plugin-manager.c								\
	ofa-plugin-manager.h								\
	ofa-preferences.c									\
	ofa-preferences.h									\
	ofa-print-reconcil.c								\
	ofa-print-reconcil.h								\
	ofa-rate-properties.c								\
	ofa-rate-properties.h								\
	ofa-rates-page.c									\
	ofa-rates-page.h									\
	ofa-reconciliation.c								\
	ofa-reconciliation.h								\
	ofa-restore.c										\
	ofa-restore.h										\
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
	$(NULL)

ui_files = \
	ofa-account-properties.ui							\
	ofa-account-select.ui								\
	ofa-accounts-book.ui								\
	ofa-bat-common.ui									\
	ofa-bat-properties.ui								\
	ofa-bat-select.ui									\
	ofa-class-properties.ui								\
	ofa-currency-properties.ui							\
	ofa-dossier-manager.ui								\
	ofa-dossier-delete.ui								\
	ofa-dossier-delete-prefs.piece.ui					\
	ofa-dossier-login.ui								\
	ofa-dossier-new.ui									\
	ofa-dossier-open.ui									\
	ofa-dossier-properties.ui							\
	ofa-export.ui										\
	ofa-guided-input.ui									\
	ofa-import.ui										\
	ofa-int-closing.ui									\
	ofa-ledger-properties.ui							\
	ofa-ope-template-properties.ui						\
	ofa-plugin-manager.ui								\
	ofa-preferences.ui									\
	ofa-print-reconcil.ui								\
	ofa-rate-properties.ui								\
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
