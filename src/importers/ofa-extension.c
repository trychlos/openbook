/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-extension.h"

#include "importers/ofa-importer-csv.h"
#include "importers/ofa-importer-ident.h"
#include "importers/ofa-importer-pdf-bourso.h"
#include "importers/ofa-importer-pdf-lcl.h"
#include "importers/ofa-importer-txt-bourso.h"
#include "importers/ofa-importer-txt-lcl.h"

#define EXTENSION_VERSION_NUMBER        2

/* ******************************************************************* *
 *     The part below implements the software extension API v 2        *
 * ******************************************************************* */

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, ofaIGetter *getter )
{
	static const gchar *thisfn = "importers/ofa_extension_startup";

	g_debug( "%s: module=%p, getter=%p", thisfn, ( void * ) module, ( void * ) getter  );

	return( TRUE );
}

/*
 * ofa_extension_enum_types:
 *
 * mandatory starting with API v. 2.
 */
void
ofa_extension_enum_types( GTypeModule *module, ofaExtensionEnumTypesCb cb, void *user_data )
{
	static const gchar *thisfn = "importers/ofa_extension_enum_types";

	g_debug( "%s: module=%p, cb=%p, user_data=%p",
			thisfn, ( void * ) module, ( void * ) cb, ( void * ) user_data );

	cb( OFA_TYPE_IMPORTER_IDENT, user_data );
	cb( OFA_TYPE_IMPORTER_CSV, user_data );
	cb( OFA_TYPE_IMPORTER_PDF_BOURSO, user_data );
	cb( OFA_TYPE_IMPORTER_PDF_LCL, user_data );
	cb( OFA_TYPE_IMPORTER_TXT_BOURSO, user_data );
	cb( OFA_TYPE_IMPORTER_TXT_LCL, user_data );
}

/*
 * ofa_extension_shutdown:
 *
 * optional as of API v. 1.
 */
void
ofa_extension_shutdown( GTypeModule *module )
{
	static const gchar *thisfn = "importers/ofa_extension_shutdown";

	g_debug( "%s: module=%p", thisfn, ( void * ) module );
}

/*
 * ofa_extension_get_version_number:
 *
 * optional as of API v. 1.
 */
guint
ofa_extension_get_version_number( void )
{
	static const gchar *thisfn = "importers/ofa_extension_get_version_number";

	g_debug( "%s: version_number=%u", thisfn, EXTENSION_VERSION_NUMBER );

	return( EXTENSION_VERSION_NUMBER );
}
