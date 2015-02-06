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

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	data								\
	m4									\
	maintainer							\
	misc								\
	po									\
	src									\
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-silent-rules				\
	--enable-as-needed					\
	--disable-scrollkeeper				\
	$(NULL)

EXTRA_DIST = \
	intltool-extract.in					\
	intltool-merge.in					\
	intltool-update.in					\
	$(NULL)

DISTCLEANFILES = \
	intltool-extract					\
	intltool-merge						\
	intltool-update						\
	$(NULL)

pkgdocdir = $(datarootdir)/doc/@PACKAGE@-@VERSION@

dist_pkgdoc_DATA = \
	AUTHORS								\
	ChangeLog							\
	COPYING								\
	COPYING-DOCS						\
	INSTALL								\
	MAINTAINERS							\
	NEWS								\
	README								\
	TODO								\
	$(NULL)

uninstall-hook:
	-\rm -fr $(DESTDIR)$(pkgdocdir)
