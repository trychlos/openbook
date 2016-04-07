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
	ofa-account-editable.c								\
	ofa-account-frame-bin.c								\
	ofa-account-frame-bin.h								\
	ofa-account-properties.c							\
	ofa-account-properties.h							\
	ofa-account-select.c								\
	ofa-account-select.h								\
	ofa-account-store.c									\
	ofa-account-store.h									\
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
	ofa-extender-collection.c							\
	ofa-extender-module.c								\
	ofa-guided-input.c									\
	ofa-guided-input.h									\
	ofa-guided-input-bin.c								\
	ofa-guided-input-bin.h								\
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
	ofa-iexe-close.c									\
	ofa-iexportable.c									\
	ofa-igetter.c										\
	ofa-iimporter.c										\
	ofa-iimportable.c									\
	ofa-import-duplicate.c								\
	ofa-iproperties.c									\
	ofa-isingle-keeper.c								\
	ofa-istore.c										\
	ofa-itheme-manager.c								\
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
	ofa-ope-template-store.h							\
	ofa-open-prefs-bin.c								\
	ofa-open-prefs-bin.h								\
	ofa-page.c											\
	ofa-periodicity.c									\
	ofa-periodicity-bin.c								\
	ofa-portfolio-collection.c							\
	ofa-preferences.c									\
	ofa-settings.c										\
	ofa-stream-format.c									\
	ofa-stream-format-bin.c								\
	ofa-stream-format-bin.h								\
	ofa-stream-format-disp.c							\
	ofa-stream-format-disp.h							\
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
	ofs-account-balance.c								\
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

CLEANFILES = \
	$(BUILT_SOURCES)									\
	$(NULL)

EXTRA_DIST = \
	$(resfiles)											\
	$(resxml)											\
	$(NULL)
