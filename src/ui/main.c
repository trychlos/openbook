/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
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

	appli = ofa_application_new();
	ret = ofa_application_run_with_args( appli, argc, argv );
	g_object_unref( appli );

	return( ret );
}
