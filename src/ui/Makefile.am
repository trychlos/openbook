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
	-DPKGCSSDIR=\"$(css_datadir)\"						\
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DICONFNAME=\"$(icondir)/$(icondata)\"				\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = ofa_ui
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

main_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(main_built_sources)								\
	$(NULL)

nodist_openbook_SOURCES = \
	$(main_built_sources)								\
	$(NULL)

openbook_SOURCES = \
	main.c												\
	ofa-about.c											\
	ofa-about.h											\
	ofa-account-book-bin.c								\
	ofa-account-book-bin.h								\
	ofa-account-book-render.c							\
	ofa-account-book-render.h							\
	ofa-account-filter-vv-bin.c							\
	ofa-account-filter-vv-bin.h							\
	ofa-account-page.c									\
	ofa-account-page.h									\
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
	ofa-batline-treeview.c								\
	ofa-batline-treeview.h								\
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
	ofa-class-store.c									\
	ofa-class-store.h									\
	ofa-class-treeview.c								\
	ofa-class-treeview.h								\
	ofa-closing-parms-bin.c								\
	ofa-closing-parms-bin.h								\
	ofa-currency-page.c									\
	ofa-currency-page.h									\
	ofa-currency-properties.c							\
	ofa-currency-properties.h							\
	ofa-currency-treeview.c								\
	ofa-currency-treeview.h								\
	ofa-dossier-delete.c								\
	ofa-dossier-delete.h								\
	ofa-dossier-display-notes.c							\
	ofa-dossier-display-notes.h							\
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
	ofa-entry-page.c									\
	ofa-entry-page.h									\
	ofa-entry-treeview.c								\
	ofa-entry-treeview.h								\
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
	ofa-iaccount-filter.c								\
	ofa-iaccount-filter.h								\
	ofa-import-assistant.c								\
	ofa-import-assistant.h								\
	ofa-itreeview-column.c								\
	ofa-itreeview-column.h								\
	ofa-itreeview-display.c								\
	ofa-itreeview-display.h								\
	ofa-ledger-book-bin.c								\
	ofa-ledger-book-bin.h								\
	ofa-ledger-book-render.c							\
	ofa-ledger-book-render.h							\
	ofa-ledger-close.c									\
	ofa-ledger-close.h									\
	ofa-ledger-page.c									\
	ofa-ledger-page.h									\
	ofa-ledger-properties.c								\
	ofa-ledger-properties.h								\
	ofa-ledger-summary-bin.c							\
	ofa-ledger-summary-bin.h							\
	ofa-ledger-summary-render.c							\
	ofa-ledger-summary-render.h							\
	ofa-ledger-treeview.c								\
	ofa-ledger-treeview.h								\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-main-window-def.h								\
	ofa-misc-audit-item.c								\
	ofa-misc-audit-item.h								\
	ofa-misc-audit-ui.c									\
	ofa-misc-audit-ui.h									\
	ofa-misc-collector-item.c							\
	ofa-misc-collector-item.h							\
	ofa-misc-collector-ui.c								\
	ofa-misc-collector-ui.h								\
	ofa-ope-template-page.c								\
	ofa-ope-template-page.h								\
	ofa-period-close.c									\
	ofa-period-close.h									\
	ofa-plugin-manager.c								\
	ofa-plugin-manager.h								\
	ofa-rate-page.c										\
	ofa-rate-page.h										\
	ofa-rate-properties.c								\
	ofa-rate-properties.h								\
	ofa-rate-treeview.c									\
	ofa-rate-treeview.h									\
	ofa-reconcil-bin.c									\
	ofa-reconcil-bin.h									\
	ofa-reconcil-page.c									\
	ofa-reconcil-page.h									\
	ofa-reconcil-render.c								\
	ofa-reconcil-render.h								\
	ofa-reconcil-store.c								\
	ofa-reconcil-store.h								\
	ofa-restore-assistant.c								\
	ofa-restore-assistant.h								\
	ofa-settlement-page.c								\
	ofa-settlement-page.h								\
	ofa-user-credentials-bin.c							\
	ofa-user-credentials-bin.h							\
	$(NULL)

openbook_LDADD = \
	$(top_builddir)/src/core/libofa-core.la				\
	$(OPENBOOK_LIBS)									\
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
	$(resfiles)											\
	$(resxml)											\
	$(NULL)
