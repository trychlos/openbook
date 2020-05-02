# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
	ofa-accentry-store.c								\
	ofa-accentry-store.h								\
	ofa-accentry-treeview.c								\
	ofa-accentry-treeview.h								\
	ofa-account-balance-args.c							\
	ofa-account-balance-args.h							\
	ofa-account-balance-render.c						\
	ofa-account-balance-render.h						\
	ofa-account-book-args.c								\
	ofa-account-book-args.h								\
	ofa-account-book-render.c							\
	ofa-account-book-render.h							\
	ofa-account-filter-vv-bin.c							\
	ofa-account-filter-vv-bin.h							\
	ofa-account-page.c									\
	ofa-account-page.h									\
	ofa-admin-credentials-bin.c							\
	ofa-admin-credentials-bin.h							\
	ofa-application.c									\
	ofa-application.h									\
	ofa-application-def.h								\
	ofa-backup-assistant.c								\
	ofa-backup-assistant.h								\
	ofa-backup-display.c								\
	ofa-backup-display.h								\
	ofa-balance-grid-bin.c								\
	ofa-balance-grid-bin.h								\
	ofa-bat-page.c										\
	ofa-bat-page.h										\
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
	ofa-dbsu.c											\
	ofa-dbsu.h											\
	ofa-dossier-actions-bin.c							\
	ofa-dossier-actions-bin.h							\
	ofa-dossier-display-notes.c							\
	ofa-dossier-display-notes.h							\
	ofa-dossier-edit-bin.c								\
	ofa-dossier-edit-bin.h								\
	ofa-dossier-manager.c								\
	ofa-dossier-manager.h								\
	ofa-dossier-meta-bin.c								\
	ofa-dossier-meta-bin.h								\
	ofa-dossier-new.c									\
	ofa-dossier-new.h									\
	ofa-dossier-open.c									\
	ofa-dossier-open.h									\
	ofa-dossier-properties.c							\
	ofa-dossier-properties.h							\
	ofa-dossier-treeview.c								\
	ofa-dossier-treeview.h								\
	ofa-exercice-close-assistant.c						\
	ofa-exercice-close-assistant.h						\
	ofa-exercice-combo.c								\
	ofa-exercice-combo.h								\
	ofa-exercice-edit-bin.c								\
	ofa-exercice-edit-bin.h								\
	ofa-exercice-meta-bin.c								\
	ofa-exercice-meta-bin.h								\
	ofa-exercice-new.c									\
	ofa-exercice-new.h									\
	ofa-exercice-store.c								\
	ofa-exercice-store.h								\
	ofa-exercice-treeview.c								\
	ofa-exercice-treeview.h								\
	ofa-export-assistant.c								\
	ofa-export-assistant.h								\
	ofa-guided-ex.c										\
	ofa-guided-ex.h										\
	ofa-iaccount-filter.c								\
	ofa-iaccount-filter.h								\
	ofa-import-assistant.c								\
	ofa-import-assistant.h								\
	ofa-ledger-arc-store.c								\
	ofa-ledger-arc-store.h								\
	ofa-ledger-arc-treeview.c							\
	ofa-ledger-arc-treeview.h							\
	ofa-ledger-book-args.c								\
	ofa-ledger-book-args.h								\
	ofa-ledger-book-render.c							\
	ofa-ledger-book-render.h							\
	ofa-ledger-close.c									\
	ofa-ledger-close.h									\
	ofa-ledger-page.c									\
	ofa-ledger-page.h									\
	ofa-ledger-properties.c								\
	ofa-ledger-properties.h								\
	ofa-ledger-treeview.c								\
	ofa-ledger-treeview.h								\
	ofa-main-window.c									\
	ofa-main-window.h									\
	ofa-main-window-def.h								\
	ofa-maintainer.c									\
	ofa-maintainer.h									\
	ofa-misc-audit-store.c								\
	ofa-misc-audit-store.h								\
	ofa-misc-audit-treeview.c							\
	ofa-misc-audit-treeview.h							\
	ofa-misc-audit-ui.c									\
	ofa-misc-audit-ui.h									\
	ofa-misc-collector-ui.c								\
	ofa-misc-collector-ui.h								\
	ofa-nomodal-page.c									\
	ofa-nomodal-page.h									\
	ofa-ope-template-page.c								\
	ofa-ope-template-page.h								\
	ofa-paimean-page.c									\
	ofa-paimean-page.h									\
	ofa-period-close.c									\
	ofa-period-close.h									\
	ofa-plugin-manager.c								\
	ofa-plugin-manager.h								\
	ofa-preferences.c									\
	ofa-preferences.h									\
	ofa-rate-page.c										\
	ofa-rate-page.h										\
	ofa-rate-properties.c								\
	ofa-rate-properties.h								\
	ofa-rate-treeview.c									\
	ofa-rate-treeview.h									\
	ofa-recovery-assistant.c							\
	ofa-recovery-assistant.h							\
	ofa-restore-assistant.c								\
	ofa-restore-assistant.h								\
	ofa-target-chooser-bin.c							\
	ofa-target-chooser-bin.h							\
	ofa-unreconcil-page.c								\
	ofa-unreconcil-page.h								\
	ofa-unsettled-page.c								\
	ofa-unsettled-page.h								\
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
