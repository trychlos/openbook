/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-extension.h"

#include "import-bourso-pdf/ofa-importer.h"

/* the count of GType types provided by this extension
 * each new GType type must
 * - be registered in ofa_extension_startup()
 * - be addressed in ofa_extension_list_types().
 */
#define TYPES_COUNT	1

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, GApplication *application )
{
	static const gchar *thisfn = "import-bourso-pdf/ofa_module_ofa_extension_startup";

	g_debug( "%s: module=%p, application=%p", thisfn, ( void * ) module, ( void * ) application  );

	ofa_bourso_pdf_importer_register_type( module );

	return( TRUE );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "import-bourso-pdf/ofa_module_ofa_extension_list_types";
	static GType types_list [1+TYPES_COUNT];

	g_debug( "%s: types=%p", thisfn, ( void * ) types );

	types_list[0] = OFA_TYPE_BOURSO_PDF_IMPORTER;

	types_list[TYPES_COUNT] = 0;
	*types = types_list;

	return( TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * mandatory starting with v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "import-bourso-pdf/ofa_module_ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
