# Open Freelance Accounting
# A double-entry accounting application for freelances.
#
# Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

# these two next lines to be kept synchronized with data/icons/Makefile.am
icondir = $(datadir)/icons/hicolor/48x48/apps
icondata = openbook-48x48.png

bin_PROGRAMS = \
	openbook											\
	$(NULL)

core_datadir = $(pkgdatadir)/core

ui_datadir = $(pkgdatadir)/ui

css_datadir = $(pkgdatadir)/css

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPKGUIDIR=\"$(ui_datadir)\"						\
	-DPKGCOREDIR=\"$(core_datadir)\"					\
	-DPKGCSSDIR=\"$(css_datadir)\"						\
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"						\
	-DICONFNAME=\"$(icondir)/$(icondata)\"				\
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
	my-cell-renderer-date.c								\
	my-cell-renderer-date.h								\
	my-editable-amount.c								\
	my-editable-amount.h								\
	my-editable-date.c									\
	my-editable-date.h									\
	my-tab-label.c										\
	my-tab-label.h										\
	ofa-about.c											\
	ofa-about.h											\
	ofa-account-book-bin.c								\
	ofa-account-book-bin.h								\
	ofa-account-book-render.c							\
	ofa-account-book-render.h							\
	ofa-account-chart-bin.c								\
	ofa-account-chart-bin.h								\
	ofa-account-filter-vv-bin.c							\
	ofa-account-filter-vv-bin.h							\
	ofa-account-frame-bin.c								\
	ofa-account-frame-bin.h								\
	ofa-account-page.c									\
	ofa-account-page.h									\
	ofa-account-properties.c							\
	ofa-account-properties.h							\
	ofa-account-select.c								\
	ofa-account-select.h								\
	ofa-account-selector-bin.c							\
	ofa-account-selector-bin.h							\
	ofa-account-store.c									\
	ofa-account-store.h									\
	ofa-application.c									\
	ofa-application.h									\
	ofa-application-def.h								\
	ofa-backup.c										\
	ofa-backup.h										\
	ofa-balance-bin.c									\
	ofa-balance-bin.h									\
	ofa-balance-grid-bin.c								\
	ofa-balance-grid-bin.h								\
	ofa-balance-render.c								\
	ofa-balance-render.h								\
	ofa-bat-page.c										\
	ofa-bat-page.h										\
	ofa-bat-properties.c								\
	ofa-bat-properties.h								\
	ofa-bat-properties-bin.c							\
	ofa-bat-properties-bin.h							\
	ofa-bat-select.c									\
	ofa-bat-select.h									\
	ofa-bat-store.c										\
	ofa-bat-store.h										\
	ofa-bat-treeview.c									\
	ofa-bat-treeview.h									\
	ofa-bat-utils.c										\
	ofa-bat-utils.h										\
	ofa-buttons-box.c									\
	ofa-buttons-box.h									\
	ofa-check-balances.c								\
	ofa-check-balances.h								\
	ofa-check-balances-bin.c							\
	ofa-check-balances-bin.h							\
	ofa-check-integrity.c								\
	ofa-check-integrity.h								\
	ofa-check-integrity-bin.c							\
	ofa-check-integrity-bin.h							\
	ofa-class-page.c									\
	ofa-class-page.h									\
	ofa-class-properties.c								\
	ofa-class-properties.h								\
	ofa-closing-parms-bin.c								\
	ofa-closing-parms-bin.h								\
	ofa-currency-page.c									\
	ofa-currency-page.h									\
	ofa-currency-combo.c								\
	ofa-currency-combo.h								\
	ofa-currency-properties.c							\
	ofa-currency-properties.h							\
	ofa-currency-store.c								\
	ofa-currency-store.h								\
	ofa-date-filter-hv-bin.c							\
	ofa-date-filter-hv-bin.h							\
	ofa-dossier-delete.c								\
	ofa-dossier-delete.h								\
	ofa-dossier-display-notes.c							\
	ofa-dossier-display-notes.h							\
	ofa-dossier-login.c									\
	ofa-dossier-login.h									\
	ofa-dossier-manager.c								\
	ofa-dossier-manager.h								\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-new-bin.c								\
	ofa-dossier-new-bin.h								\
	ofa-dossier-new-mini.c								\
	ofa-dossier-new-mini.h								\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-dossier-properties.c							\
	ofa-dossier-properties.h							\
	ofa-dossier-store.c									\
	ofa-dossier-store.h									\
	ofa-dossier-treeview.c								\
	ofa-dossier-treeview.h								\
	ofa-entry-columns.c									\
	ofa-entry-columns.h									\
	ofa-entry-page.c									\
	ofa-entry-page.h									\
	ofa-exercice-combo.c								\
	ofa-exercice-combo.h								\
	ofa-exercice-close-assistant.c						\
	ofa-exercice-close-assistant.h						\
	ofa-exercice-store.c								\
	ofa-exercice-store.h								\
	ofa-export-assistant.c								\
	ofa-export-assistant.h								\
	ofa-guided-ex.c										\
	ofa-guided-ex.h										\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-guided-input-bin.c								\
	ofa-guided-input-bin.h								\
	ofa-iaccount-filter.c								\
	ofa-iaccount-filter.h								\
	ofa-icolumns.c										\
	ofa-icolumns.h										\
	ofa-idate-filter.c									\
	ofa-idate-filter.h									\
	ofa-import-assistant.c								\
	ofa-import-assistant.h								\
	ofa-iprintable.c									\
	ofa-iprintable.h									\
	ofa-irenderable.c									\
	ofa-irenderable.h									\
	ofa-istore.c										\
	ofa-istore.h										\
	ofa-ledger-book-bin.c								\
	ofa-ledger-book-bin.h								\
	ofa-ledger-book-render.c							\
	ofa-ledger-book-render.h							\
	ofa-ledger-close.c									\
	ofa-ledger-close.h									\
	ofa-ledger-combo.c									\
	ofa-ledger-combo.h									\
	ofa-ledger-page.c									\
	ofa-ledger-page.h									\
	ofa-ledger-properties.c								\
	ofa-ledger-properties.h								\
	ofa-ledger-store.c									\
	ofa-ledger-store.h									\
	ofa-ledger-treeview.c								\
	ofa-ledger-treeview.h								\
	ofa-list-store.c									\
	ofa-list-store.h									\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-main-window-def.h								\
	ofa-ope-template-book-bin.c							\
	ofa-ope-template-book-bin.h							\
	ofa-ope-template-frame-bin.c						\
	ofa-ope-template-frame-bin.h						\
	ofa-ope-template-help.c								\
	ofa-ope-template-help.h								\
	ofa-ope-template-page.c								\
	ofa-ope-template-page.h								\
	ofa-ope-template-properties.c						\
	ofa-ope-template-properties.h						\
	ofa-ope-template-select.c							\
	ofa-ope-template-select.h							\
	ofa-ope-template-store.c							\
	ofa-ope-template-store.h							\
	ofa-page.c											\
	ofa-page.h											\
	ofa-page-def.h										\
	ofa-page-prot.h										\
	ofa-plugin-manager.c								\
	ofa-plugin-manager.h								\
	ofa-rate-page.c										\
	ofa-rate-page.h										\
	ofa-rate-properties.c								\
	ofa-rate-properties.h								\
	ofa-reconcil-bin.c									\
	ofa-reconcil-bin.h									\
	ofa-reconcil-render.c								\
	ofa-reconcil-render.h								\
	ofa-reconciliation.c								\
	ofa-reconciliation.h								\
	ofa-render-page.c									\
	ofa-render-page.h									\
	ofa-restore-assistant.c								\
	ofa-restore-assistant.h								\
	ofa-settlement.c									\
	ofa-settlement.h									\
	ofa-tree-store.c									\
	ofa-tree-store.h									\
	ofa-user-credentials-bin.c							\
	ofa-user-credentials-bin.h							\
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
	ofa-account-book-bin.ui								\
	ofa-account-filter-vv-bin.ui						\
	ofa-account-properties.ui							\
	ofa-account-select.ui								\
	ofa-account-selector-bin.ui							\
	ofa-balance-bin.ui									\
	ofa-bat-properties.ui								\
	ofa-bat-properties-bin.ui							\
	ofa-bat-select.ui									\
	ofa-check-balances.ui								\
	ofa-check-balances-bin.ui							\
	ofa-check-integrity.ui								\
	ofa-check-integrity-bin.ui							\
	ofa-class-properties.ui								\
	ofa-closing-parms-bin.ui							\
	ofa-currency-properties.ui							\
	ofa-date-filter-hv-bin.ui							\
	ofa-dossier-manager.ui								\
	ofa-dossier-delete.ui								\
	ofa-dossier-display-notes.ui						\
	ofa-dossier-login.ui								\
	ofa-dossier-new.ui									\
	ofa-dossier-new-bin.ui								\
	ofa-dossier-new-mini.ui								\
	ofa-dossier-open.ui									\
	ofa-dossier-properties.ui							\
	ofa-entry-page.ui									\
	ofa-exercice-close-assistant.ui						\
	ofa-export-assistant.ui								\
	ofa-guided-input.ui									\
	ofa-guided-input-bin.ui								\
	ofa-import-assistant.ui								\
	ofa-ledger-close.ui									\
	ofa-ledger-properties.ui							\
	ofa-ledger-book-bin.ui								\
	ofa-ope-template-help.ui							\
	ofa-ope-template-properties.ui						\
	ofa-ope-template-select.ui							\
	ofa-plugin-manager.ui								\
	ofa-reconcil-bin.ui									\
	ofa-reconciliation.ui								\
	ofa-render-page.ui									\
	ofa-restore-assistant.ui							\
	ofa-rate-properties.ui								\
	ofa-settlement.ui									\
	ofa-user-credentials-bin.ui							\
	$(NULL)

images_files = \
	arrow-down.png										\
	filler.png											\
	green-checkmark.png									\
	notes1.png											\
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
