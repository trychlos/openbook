/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "api/ofo-base.h"
#include "api/ofa-idoc.h"

#define IDOC_LAST_VERSION            1

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIDocInterface *klass );
static void  interface_base_finalize( ofaIDocInterface *klass );

/**
 * ofa_idoc_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idoc_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idoc_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idoc_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDocInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDoc", &info, 0 );

	g_type_interface_add_prerequisite( type, OFO_TYPE_BASE );

	return( type );
}

static void
interface_base_init( ofaIDocInterface *klass )
{
	static const gchar *thisfn = "ofa_idoc_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDocInterface *klass )
{
	static const gchar *thisfn = "ofa_idoc_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idoc_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idoc_get_interface_last_version( void )
{
	return( IDOC_LAST_VERSION );
}

/**
 * ofa_idoc_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idoc_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDOC );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDocInterface * ) iface )->get_interface_version ){
		version = (( ofaIDocInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDoc::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idoc_get_class_orphans:
 * @type: the implementation's GType.
 *
 * Returns: the full list of document orphans at class level.
 *
 * Defaults to %NULL.
 *
 * Since: version 1.
 */
GList *
ofa_idoc_get_class_orphans( GType type )
{
	return( NULL );
}

/**
 * ofa_idoc_get_count:
 * @instance: this #ofaIDoc instance.
 *
 * Returns: the count of #ofoDoc documents attached to the @instance,
 * or -1 in case of the error.
 */
ofxCounter
ofa_idoc_get_count( const ofaIDoc *instance )
{
	static const gchar *thisfn = "ofa_idoc_get_count";

	g_return_val_if_fail( instance && OFA_IS_IDOC( instance ), -1 );

	if( OFA_IDOC_GET_INTERFACE( instance )->get_count ){
		return( OFA_IDOC_GET_INTERFACE( instance )->get_count( instance ));
	}

	g_info( "%s: ofaIDoc's %s implementation does not provide 'get_count()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 0 );
}

/**
 * ofa_idoc_foreach:
 * @instance: this #ofaIDoc instance.
 * @cb: a #ofaIDocEnumerateCb callback function.
 * @user_data: user data to be passed to the @cb.
 *
 * Calls the provided @cb one time per document attached to the instance.
 *
 * The enumeration is not guaranted to be complete as the implementation
 * may decide to stop it at any time.
 *
 * The enumeration is guaranted to respect the initial insertion order
 * of the documents.
 */
void
ofa_idoc_foreach( const ofaIDoc *instance, ofaIDocEnumerateCb cb, void *user_data )
{
	static const gchar *thisfn = "ofa_idoc_foreach";

	g_return_if_fail( instance && OFA_IS_IDOC( instance ));

	if( OFA_IDOC_GET_INTERFACE( instance )->foreach ){
		OFA_IDOC_GET_INTERFACE( instance )->foreach( instance, cb, user_data );
		return;
	}

	g_info( "%s: ofaIDoc's %s implementation does not provide 'foreach()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * ofa_idoc_get_orphans:
 * @instance: this #ofaIDoc instance.
 *
 * Returns: the list of document identifiers which are referenced by the
 * @instance, but do not exist as actual #ofoDoc documents.
 *
 * The returned list is a list of #ofoDoc identifiers.
 * It should be #ofa_idoc_free_orphans() by the caller.
 */
GList *
ofa_idoc_get_orphans( const ofaIDoc *instance )
{
	static const gchar *thisfn = "ofa_idoc_get_orphans";

	g_return_val_if_fail( instance && OFA_IS_IDOC( instance ), NULL );

	if( OFA_IDOC_GET_INTERFACE( instance )->get_orphans ){
		return( OFA_IDOC_GET_INTERFACE( instance )->get_orphans( instance ));
	}

	g_info( "%s: ofaIDoc's %s implementation does not provide 'get_orphans()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}
