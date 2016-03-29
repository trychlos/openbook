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

#include "ofa-tva.h"
#include "ofa-tva-register.h"

static guint  iregister_get_interface_version( const ofaIRegister *instance );
static GList *iregister_get_for_type( ofaIRegister *instance, GType type );

/*
 * #ofaIRegister interface setup
 */
void
ofa_tva_register_iface_init( ofaIRegisterInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_register_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iregister_get_interface_version;
	iface->get_for_type = iregister_get_for_type;
}

/*
 * the version of the #ofaIRegister interface implemented by the module
 */
static guint
iregister_get_interface_version( const ofaIRegister *instance )
{
	return( 1 );
}

static GList *
iregister_get_for_type( ofaIRegister *instance, GType type )
{
	GList *vat_objects, *typed_objects, *it;

	vat_objects = ofa_tva_get_registered_types( OFA_TVA( instance ));
	typed_objects = NULL;

	for( it=vat_objects ; it ; it=it->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( it->data ), type )){
			typed_objects = g_list_prepend( typed_objects, g_object_ref( it->data ));
		}
	}

	return( typed_objects );
}
