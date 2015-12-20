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

BUILT_SOURCES = \
	ofo-marshal.c										\
	ofo-marshal.h										\
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
	my-field-combo.c									\
	my-field-combo.h									\
	my-file-monitor.c									\
	my-isettings.c										\
	my-progress-bar.c									\
	my-progress-bar.h									\
	my-settings.c										\
	my-settings.h										\
	my-timeout.c										\
	my-utils.c											\
	my-window.c											\
	\
	ofa-admin-credentials-bin.c							\
	ofa-admin-credentials-bin.h							\
	ofa-box.c											\
	ofa-collection.c									\
	ofa-collection.h									\
	ofa-collection-prot.h								\
	ofa-concil-collection.c								\
	ofa-concil-collection.h								\
	ofa-dbms-root-bin.c									\
	ofa-dbms-root-bin.h									\
	ofa-ddl-update.c									\
	ofa-ddl-update.h									\
	ofa-dossier-delete-prefs-bin.c						\
	ofa-dossier-delete-prefs-bin.h						\
	ofa-dossier-misc.c									\
	ofa-dossier-misc.h									\
	ofa-file-dir.c										\
	ofa-file-dir.h										\
	ofa-file-format.c									\
	ofa-file-format-bin.c								\
	ofa-file-format-bin.h								\
	ofa-iabout.c										\
	ofa-icollector.c									\
	ofa-icollector.h									\
	ofa-iconcil.c										\
	ofa-iconcil.h										\
	ofa-idataset.c										\
	ofa-idbconnect.c									\
	ofa-idbcustomer.c									\
	ofa-idbeditor.c										\
	ofa-idbmeta.c										\
	ofa-idbperiod.c										\
	ofa-idbprovider.c									\
	ofa-iexportable.c									\
	ofa-iimporter.c										\
	ofa-iimportable.c									\
	ofa-iprefs-page.c									\
	ofa-iprefs-provider.c								\
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

ofo-marshal.h: ofo-marshal.def $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=ofo_cclosure_marshal > $@

ofo-marshal.c: ofo-marshal.def $(GLIB_GENMARSHAL)
	echo "#include \"ofo-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=ofo_cclosure_marshal >> $@

libofa_core_la_LIBADD = \
	$(OPENBOOK_LIBS)									\
	$(NULL)

libofa_core_la_LDFLAGS = \
	-no-undefined										\
	-avoid-version										\
	-fPIC												\
	$(NULL)

ui_files = \
	ofa-admin-credentials-bin.ui						\
	ofa-dbms-root-bin.ui								\
	ofa-ddl-update.ui									\
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
	ofo-marshal.def										\
	$(core_data_DATA)									\
	$(NULL)
