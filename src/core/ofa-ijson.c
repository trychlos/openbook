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

#include "my/my-utils.h"

#include "api/ofa-ijson.h"

#define IJSON_LAST_VERSION                1

/* a data structure attached to the instance
 * - action_groups are indexed by their name.
 */
#define IJSON_DATA                       "ofa-ijson-data"

typedef struct {
}
	sIJson;

static guint st_initializations         = 0;	/* interface initialization count */

static GType   register_type( void );
static void    interface_base_init( ofaIJsonInterface *klass );
static void    interface_base_finalize( ofaIJsonInterface *klass );
//static sIJson *get_instance_data( ofaIJson *instance );
//static void    on_instance_finalized( sIJson *sdata, GObject *finalized_instance );

/**
 * ofa_ijson_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ijson_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ijson_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ijson_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIJsonInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIJson", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIJsonInterface *klass )
{
	static const gchar *thisfn = "ofa_ijson_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIJsonInterface *klass )
{
	static const gchar *thisfn = "ofa_ijson_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ijson_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ijson_get_interface_last_version( void )
{
	return( IJSON_LAST_VERSION );
}

/**
 * ofa_ijson_get_interface_version:
 * @type: the #GType of the implentation class.
 *
 * Returns: the version number of this interface implemented.
 *
 * Defaults to 1.
 */
guint
ofa_ijson_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IJSON );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIJsonInterface * ) iface )->get_interface_version ){
		version = (( ofaIJsonInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIJson::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_ijson_get_title:
 * @type: the #GType of the implentation class.
 *
 * Returns: the title to be associated to the implementation, as an new
 * string which has to be #g_free() by the caller.
 *
 * Defaults to the implementation class name.
 */
gchar *
ofa_ijson_get_title( GType type )
{
	gpointer klass, iface;
	gchar *title;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, NULL );

	iface = g_type_interface_peek( klass, OFA_TYPE_IJSON );
	g_return_val_if_fail( iface, NULL );

	title = NULL;

	if((( ofaIJsonInterface * ) iface )->get_title ){
		title = (( ofaIJsonInterface * ) iface )->get_title();

	} else {
		g_info( "%s implementation does not provide 'ofaIJson::get_title()' method",
				g_type_name( type ));
		title = g_strdup( g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( title );
}

/**
 * ofa_ijson_get_as_string:
 * @instance: this #ofaIJson instance.
 *
 * Returns: the JSON string as a newly allocated string which should be
 * #g_free() by the caller.
 */
gchar *
ofa_ijson_get_as_string( ofaIJson *instance )
{
	static const gchar *thisfn = "ofa_ijson_get_as_string";
	gchar *str;

	g_return_val_if_fail( instance && OFA_IS_IJSON( instance ), NULL );

	str = NULL;

	if( OFA_IJSON_GET_INTERFACE( instance )->get_as_string ){

		str = OFA_IJSON_GET_INTERFACE( instance )->get_as_string( instance );

	} else {
		g_info( "%s: ofaIJson's %s implementation does not provide 'get_as_string()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}

	return( str );
}

#if 0
/*
 * returns the sIJson data structure attached to the instance
 */
static sIJson *
get_instance_data( ofaIJson *instance )
{
	sIJson *sdata;
	GtkWidget *widget;

	sdata = ( sIJson * ) g_object_get_data( G_OBJECT( instance ), IJSON_DATA );

	if( !sdata ){
		sdata = g_new0( sIJson, 1 );
		g_object_set_data( G_OBJECT( instance ), IJSON_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIJson *sdata, GObject *instance )
{
	static const gchar *thisfn = "ofa_ijson_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_free( sdata );
}
#endif
