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

pkglib_LTLIBRARIES = libofa-core.la

css_datadir = $(pkgdatadir)/css

init1_datadir = $(pkgdatadir)/init_utf8_comma_pipe_fr

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	-DG_LOG_DOMAIN=\"OFA\"								\
	-DPKGCSSDIR=\"$(css_datadir)\"						\
	-DPKGLIBDIR=\""$(pkglibdir)"\"						\
	-DINIT1DIR=\""$(init1_datadir)"\"					\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = ofa_core
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

res_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(res_built_sources)								\
	$(NULL)

nodist_libofa_core_la_SOURCES = \
	$(res_built_sources)								\
	$(NULL)

libofa_core_la_SOURCES = \
	ofa-account-arc-store.c								\
	ofa-account-arc-store.h								\
	ofa-account-arc-treeview.c							\
	ofa-account-arc-treeview.h							\
	ofa-account-editable.c								\
	ofa-account-frame-bin.c								\
	ofa-account-frame-bin.h								\
	ofa-account-properties.c							\
	ofa-account-properties.h							\
	ofa-account-select.c								\
	ofa-account-select.h								\
	ofa-account-store.c									\
	ofa-account-store.h									\
	ofa-account-treeview.c								\
	ofa-account-treeview.h								\
	ofa-action-page.c									\
	ofa-admin-credentials-bin.c							\
	ofa-admin-credentials-bin.h							\
	ofa-amount.c										\
	ofa-box.c											\
	ofa-buttons-box.c									\
	ofa-core.c											\
	ofa-counter.c										\
	ofa-currency-combo.c								\
	ofa-currency-combo.h								\
	ofa-currency-store.c								\
	ofa-currency-store.h								\
	ofa-date-filter-hv-bin.c							\
	ofa-dbms-root-bin.c									\
	ofa-dbms-root-bin.h									\
	ofa-dossier-delete-prefs-bin.c						\
	ofa-dossier-delete-prefs-bin.h						\
	ofa-dossier-prefs.c									\
	ofa-entry-store.c									\
	ofa-entry-store.h									\
	ofa-extender-collection.c							\
	ofa-extender-module.c								\
	ofa-formula-engine.c								\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-guided-input-bin.c								\
	ofa-guided-input-bin.h								\
	ofa-hub.c											\
	ofa-iabout.c										\
	ofa-iactionable.c									\
	ofa-iactioner.c										\
	ofa-icontext.c										\
	ofa-iconcil.c										\
	ofa-iconcil.h										\
	ofa-idate-filter.c									\
	ofa-idbconnect.c									\
	ofa-idbeditor.c										\
	ofa-idbmeta.c										\
	ofa-idbmodel.c										\
	ofa-idbperiod.c										\
	ofa-idbprovider.c									\
	ofa-iexe-close.c									\
	ofa-iexportable.c									\
	ofa-igetter.c										\
	ofa-iimporter.c										\
	ofa-iimportable.c									\
	ofa-import-duplicate.c								\
	ofa-iprintable.c									\
	ofa-iproperties.c									\
	ofa-irenderable.c									\
	ofa-isignal-hub.c									\
	ofa-istore.c										\
	ofa-itheme-manager.c								\
	ofa-itree-adder.c									\
	ofa-itvcolumnable.c									\
	ofa-itvfilterable.c									\
	ofa-itvsortable.c									\
	ofa-ledger-combo.c									\
	ofa-ledger-combo.h									\
	ofa-ledger-store.c									\
	ofa-ledger-store.h									\
	ofa-list-store.c									\
	ofa-ope-template-editable.c							\
	ofa-ope-template-frame-bin.c						\
	ofa-ope-template-frame-bin.h						\
	ofa-ope-template-help.c								\
	ofa-ope-template-help.h								\
	ofa-ope-template-properties.c						\
	ofa-ope-template-properties.h						\
	ofa-ope-template-select.c							\
	ofa-ope-template-select.h							\
	ofa-ope-template-store.c							\
	ofa-ope-template-treeview.c							\
	ofa-ope-template-treeview.h							\
	ofa-open-prefs-bin.c								\
	ofa-open-prefs-bin.h								\
	ofa-paimean-editable.c								\
	ofa-paimean-frame-bin.c								\
	ofa-paimean-frame-bin.h								\
	ofa-paimean-properties.c							\
	ofa-paimean-properties.h							\
	ofa-paimean-select.c								\
	ofa-paimean-select.h								\
	ofa-paimean-store.c									\
	ofa-paimean-store.h									\
	ofa-paimean-treeview.c								\
	ofa-paimean-treeview.h								\
	ofa-paned-page.c									\
	ofa-page.c											\
	ofa-periodicity.c									\
	ofa-periodicity-bin.c								\
	ofa-portfolio-collection.c							\
	ofa-preferences.c									\
	ofa-rate-store.c									\
	ofa-rate-store.h									\
	ofa-render-page.c									\
	ofa-settings.c										\
	ofa-stream-format.c									\
	ofa-stream-format-bin.c								\
	ofa-stream-format-bin.h								\
	ofa-stream-format-disp.c							\
	ofa-stream-format-disp.h							\
	ofa-tree-store.c									\
	ofa-tvbin.c											\
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
	ofo-paimean.c										\
	ofo-rate.c											\
	\
	ofs-account-balance.c								\
	ofs-concil-id.c										\
	ofs-currency.c										\
	ofs-ledger-balance.c								\
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

CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(resfiles)											\
	$(resxml)											\
	$(NULL)
