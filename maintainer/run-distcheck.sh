#!/bin/sh
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

function distcheck_fn {
	
	maintainer_dir=$(cd ${0%/*}; pwd)
	top_srcdir="${maintainer_dir%/*}"
	
	PkgName=`autoconf --trace 'AC_INIT:$1' "${top_srcdir}/configure.ac"`
	pkgname=$(echo $PkgName | tr '[[:upper:]]' '[[:lower:]]')
	
	# a openbook-x.y may remain after an aborted make distcheck
	# such a directory breaks gnome-autogen.sh generation
	# so clean it here
	for d in $(find ${top_srcdir} -maxdepth 2 -type d -name "${pkgname}-*"); do
		echo "> Removing $d"
		chmod -R u+w $d
		rm -fr $d
	done
	
	builddir="./_build"
	installdir="./_install"
	
	rm -fr ${builddir}
	rm -fr ${installdir}
	#find ${top_srcdir}/docs/manual -type f -name '*.html' -o -name '*.pdf' | xargs rm -f
	#find ${top_srcdir}/docs/manual \( -type d -o -type l \) -name 'stylesheet-images' -o -name 'admon' | xargs rm -fr
	
	${maintainer_dir}/run-autogen.sh &&
		${maintainer_dir}/check-po.sh -nodummy &&
		${maintainer_dir}/check-headers.sh -nodummy &&
		desktop-file-validate ${installdir}/share/applications/openbook.desktop &&
		make -C ${builddir} distcheck
}

time distcheck_fn
