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

pkglib_LTLIBRARIES = libofa-core.la

core_datadir = $(pkgdatadir)/core

css_datadir = $(pkgdatadir)/css

init1_datadir = $(pkgdatadir)/init_utf8_comma_pipe_fr

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPKGUIDIR=\"$(core_datadir)\"						\
	-DPKGCSSDIR=\"$(css_datadir)\"						\
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"						\
	-DINIT1DIR=\""$(init1_datadir)"\"					\
	$(OPENBOOK_CFLAGS)									\
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

libofa_core_la_SOURCES = \
	$(BUILT_SOURCES)									\
	my-date.c											\
	my-date-combo.c										\
	my-date-combo.h										\
	my-decimal-combo.c									\
	my-decimal-combo.h									\
	my-dialog.c											\
	my-double.c											\
	my-editable-amount.c								\
	my-editable-date.c									\
	my-field-combo.c									\
	my-field-combo.h									\
	my-file-monitor.c									\
	my-igridlist.c										\
	my-isettings.c										\
	my-iwindow.c										\
	my-progress-bar.c									\
	my-settings.c										\
	my-settings.h										\
	my-timeout.c										\
	my-utils.c											\
	my-window.c											\
	\
	ofa-admin-credentials-bin.c							\
	ofa-admin-credentials-bin.h							\
	ofa-box.c											\
	ofa-buttons-box.c									\
	ofa-date-filter-hv-bin.c							\
	ofa-dbms-root-bin.c									\
	ofa-dbms-root-bin.h									\
	ofa-dossier-delete-prefs-bin.c						\
	ofa-dossier-delete-prefs-bin.h						\
	ofa-file-dir.c										\
	ofa-file-dir.h										\
	ofa-file-format.c									\
	ofa-file-format-bin.c								\
	ofa-file-format-bin.h								\
	ofa-hub.c											\
	ofa-iabout.c										\
	ofa-icollectionable.c								\
	ofa-icollector.c									\
	ofa-iconcil.c										\
	ofa-iconcil.h										\
	ofa-idate-filter.c									\
	ofa-idbconnect.c									\
	ofa-idbeditor.c										\
	ofa-idbmeta.c										\
	ofa-idbmodel.c										\
	ofa-idbperiod.c										\
	ofa-idbprovider.c									\
	ofa-iexeclose-close.c								\
	ofa-iexportable.c									\
	ofa-ihubber.c										\
	ofa-iimporter.c										\
	ofa-iimportable.c									\
	ofa-iprefs-page.c									\
	ofa-iprefs-provider.c								\
	ofa-istore.c										\
	ofa-list-store.c									\
	ofa-main-window.h									\
	ofa-page.c											\
	ofa-plugin.c										\
	ofa-preferences.c									\
	ofa-settings.c										\
	\
	ofo-account.c										\
	ofo-base.c											\
	ofo-bat.c											\
	ofo-bat-line.c										\
	ofo-class.c											\
	ofo-concil.c										\
	ofo-currency.c										\
	ofo-dossier.c										\
	ofo-entry.c											\
	ofo-ledger.c										\
	ofo-ope-template.c									\
	ofo-rate.c											\
	\
	ofs-account-balance.c								\
	ofs-bat.c											\
	ofs-concil-id.c										\
	ofs-currency.c										\
	ofs-ope.c											\
	$(NULL)

libofa_core_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_core_la_LDFLAGS = \
	-no-undefined										\
	-avoid-version										\
	-fPIC												\
	$(NULL)

ui_files = \
	ofa-idbmodel.ui										\
	$(NULL)

resources_ui = \
	ofa-admin-credentials-bin.ui						\
	ofa-date-filter-hv-bin.ui							\
	ofa-dbms-root-bin.ui								\
	ofa-dossier-delete-prefs-bin.ui						\
	ofa-file-format-bin.ui								\
	ofa-preferences.ui									\
	$(NULL)

core_data_DATA = \
	$(ui_files)											\
	$(NULL)
	
CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(core_data_DATA)									\
	$(resources_ui)										\
	$(NULL)
