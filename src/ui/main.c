/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 *
 * To display debug messages, run the command:
 *   $ G_DEBUG=fatal_warnings G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/ofa-application.h"

int
main( int argc, char *argv[] )
{
	int ret;
	ofaApplication *appli;

	/* main: argv[0]=/home/pierre/data/eclipse/openbook/_install/bin/openbook */
	g_debug( "main: argv[0]=%s", argv[0] );

	ret = -1;
	appli = ofa_application_new();

	if( appli ){
		ret = ofa_application_run_with_args( appli, argc, argv );
		g_object_unref( appli );
	}

	return( ret );
}
