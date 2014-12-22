/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <api/ofa-extension.h>

#include "import-bourso/ofa-importer.h"

/* the count of GType types provided by this extension
 * each new GType type must
 * - be registered in ofa_extension_startup()
 * - be addressed in ofa_extension_list_types().
 */
#define OFA_TYPES_COUNT	1

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module )
{
	static const gchar *thisfn = "import-bourso/ofa_module_ofa_extension_startup";

	g_debug( "%s: module=%p", thisfn, ( void * ) module );

	ofa_importer_register_type( module );

	return( TRUE );
}

/*
 * ofa_extension_get_api_version:
 *
 * optional, defaults to 1.
 */
guint
ofa_extension_get_api_version( void )
{
	static const gchar *thisfn = "import-bourso/ofa_module_ofa_extension_get_api_version";
	guint version;

	version = 1;

	g_debug( "%s: version=%d", thisfn, version );

	return( version );
}

/*
 * ofa_extension_get_name:
 *
 * optional, defaults to NULL.
 */
const gchar *
ofa_extension_get_name( void )
{
	return( "Boursorama tabulated BAT Importer #1" );
}

/*
 * ofa_extension_get_version_number:
 *
 * optional, defaults to NULL.
 */
const gchar *
ofa_extension_get_version_number( void )
{
	return( PACKAGE_VERSION );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "import-bourso/ofa_module_ofa_extension_list_types";
	static GType types_list [1+OFA_TYPES_COUNT];

	g_debug( "%s: types=%p", thisfn, ( void * ) types );

	types_list[0] = OFA_TYPE_IMPORTER;

	types_list[OFA_TYPES_COUNT] = 0;
	*types = types_list;

	return( OFA_TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * mandatory starting with v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "import-bourso/ofa_module_ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
