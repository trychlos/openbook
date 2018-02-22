# Open Firm Accounting
# A double-entry accounting application for professional services.
#
# Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

include ../make.inc

pkglib_LTLIBRARIES = liborgtrychlosmy1.la

css_datadir = $(pkgdatadir)/css

AM_CPPFLAGS += \
	-I $(top_srcdir)									\
	-I $(top_srcdir)/src								\
	-DG_LOG_DOMAIN=\"MY\"								\
	-DPKGCSSDIR=\"$(css_datadir)\"						\
	$(OPENBOOK_CFLAGS)									\
	$(NULL)

# Resources are bundled as GResource binary data in the library.
# They are distributed in the package, but not installed.
resname = my
resxml = $(srcdir)/resources.xml
resfiles = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(resxml))
resources.c: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name $(resname) $(resxml)
resources.h: $(resxml) $(resfiles)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name $(resname) $(resxml)

my_built_sources = \
	resources.c											\
	resources.h											\
	$(NULL)

BUILT_SOURCES = \
	$(my_built_sources)									\
	$(NULL)

nodist_liborgtrychlosmy1_la_SOURCES = \
	$(my_built_sources)									\
	$(NULL)

liborgtrychlosmy1_la_SOURCES = \
	my-accel-group.c									\
	my-char.c											\
	my-date.c											\
	my-date-combo.c										\
	my-date-editable.c									\
	my-date-renderer.c									\
	my-decimal-combo.c									\
	my-dnd-book.c										\
	my-dnd-popup.c										\
	my-dnd-window.c										\
	my-double.c											\
	my-double-editable.c								\
	my-double-renderer.c								\
	my-field-combo.c									\
	my-file-monitor.c									\
	my-iassistant.c										\
	my-ibin.c											\
	my-icollectionable.c								\
	my-icollector.c										\
	my-idialog.c										\
	my-iident.c											\
	my-igridlist.c										\
	my-iprogress.c										\
	my-iscope-map.c										\
	my-isettings.c										\
	my-iwindow.c										\
	my-period.c											\
	my-period-bin.c										\
	my-progress-bar.c									\
	my-scope-mapper.c									\
	my-settings.c										\
	my-signal.c											\
	my-stamp.c											\
	my-style.c											\
	my-tab.c											\
	my-thousand-combo.c									\
	my-timeout.c										\
	my-utils.c											\
	$(NULL)

liborgtrychlosmy1_la_LIBADD = \
	$(PWI_LIBS)											\
	$(NULL)

liborgtrychlosmy1_la_LDFLAGS = \
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
