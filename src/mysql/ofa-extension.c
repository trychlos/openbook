/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-igetter.h"

#include "mysql/ofa-mysql-dbmodel.h"
#include "mysql/ofa-mysql-dbprovider.h"
#include "mysql/ofa-mysql-main.h"
#include "mysql/ofa-mysql-properties.h"

/* ******************************************************************* *
 *       The part below implements the software extension API          *
 * ******************************************************************* */

/*
 * The count of GType types provided by this extension.
 * Each of these GType types must be addressed in #ofa_extension_list_types().
 * Only the GTypeModule has to be registered from #ofa_extension_startup().
 */
#define TYPES_COUNT	 4

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, ofaIGetter *getter )
{
	static const gchar *thisfn = "mysql/ofa_extension_startup";

	g_debug( "%s: module=%p, getter=%p", thisfn, ( void * ) module, ( void * ) getter  );

	ofa_mysql_main_register_type_ex( module );

	return( TRUE );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with API v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "mysql/ofa_extension_list_types";
	static GType types_list [1+TYPES_COUNT];
	gint i = 0;

	g_debug( "%s: types=%p, count=%u", thisfn, ( void * ) types, TYPES_COUNT );

	types_list[i++] = OFA_TYPE_MYSQL_MAIN;
	types_list[i++] = OFA_TYPE_MYSQL_DBMODEL;
	types_list[i++] = OFA_TYPE_MYSQL_DBPROVIDER;
	types_list[i++] = OFA_TYPE_MYSQL_PROPERTIES;

	g_return_val_if_fail( i == TYPES_COUNT, 0 );
	types_list[i] = 0;
	*types = types_list;

	return( TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * optional as of API v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "mysql/ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
