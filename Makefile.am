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

ACLOCAL_AMFLAGS = -I m4

# misc/ directory hosts some test programs which themselves heavily
# rely on orgtrychlosmy and core libraries
# => so build misc/ after src/
SUBDIRS = \
	data								\
	docs								\
	m4									\
	maintainer							\
	po									\
	src									\
	misc								\
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-silent-rules				\
	--enable-iso-c						\
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

# just run "make install-txz" and that's done
# the output file may later be installed with a command like
# '(cd ~/.local && tar -xJv --strip-components=4 -f /tmp/openbook-0.42-install.tar.xz)'
install-txz:
	(cd $(top_srcdir) && \
	 my_topdir=`pwd` && \
	 tmpdir=`mktemp -d /tmp/tmpXXXX` && \
	 mkdir $${tmpdir}/_build && \
	 cd $${tmpdir}/_build && \
	 $${my_topdir}/configure --prefix=$$HOME/.local/@PACKAGE@-@VERSION@ && \
	 make && \
	 make DESTDIR=$${tmpdir}/_install/$(PACKAGE)-$(VERSION) install && \
	 cd $${tmpdir}/_install && \
	 tar -cvJf - * > /tmp/$(PACKAGE)-$(VERSION)-install.tar.xz && \
	 cd /tmp && \
	 rm -fr $${tmpdir})
	@echo "==="
	@echo "=== /tmp/$(PACKAGE)-$(VERSION)-install.tar.xz available"
	@echo "==="
