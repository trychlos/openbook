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

#include "my/my-iident.h"

#include "api/ofa-hub.h"
#include "api/ofa-iproperties.h"

/* some data attached to each widget returned by init().
 */
typedef struct {
	ofaIProperties *instance;
}
	sIProperties;

#define IPROPERTIES_LAST_VERSION        1
#define IPROPERTIES_DATA                "iproperties-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIPropertiesInterface *klass );
static void          interface_base_finalize( ofaIPropertiesInterface *klass );
static sIProperties *get_iproperties_data( GtkWidget *widget );
static void          on_widget_finalized( sIProperties *data, GObject *finalized_widget );

/**
 * ofa_iproperties_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iproperties_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iproperties_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iproperties_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPropertiesInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIProperties", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPropertiesInterface *klass )
{
	static const gchar *thisfn = "ofa_iproperties_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/*klass->get_interface_version = ipreferences_get_interface_version;*/
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPropertiesInterface *klass )
{
	static const gchar *thisfn = "ofa_iproperties_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iproperties_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iproperties_get_interface_last_version( void )
{
	return( IPROPERTIES_LAST_VERSION );
}

/**
 * ofa_iproperties_get_interface_version:
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
ofa_iproperties_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IPROPERTIES );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIPropertiesInterface * ) iface )->get_interface_version ){
		version = (( ofaIPropertiesInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIProperties::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iproperties_init:
 * @instance: this #ofaIProperties instance.
 * @hub: the #ofaHub object of the application.
 *
 * Returns: the newly created page.
 */
GtkWidget *
ofa_iproperties_init( ofaIProperties *instance, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_iproperties_init";
	GtkWidget *widget;
	sIProperties *sdata;

	g_debug( "%s: instance=%p (%s), hub=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) hub );

	g_return_val_if_fail( instance && OFA_IS_IPROPERTIES( instance ), NULL );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( OFA_IPROPERTIES_GET_INTERFACE( instance )->init ){
		widget = OFA_IPROPERTIES_GET_INTERFACE( instance )->init( instance, hub );
		if( widget ){
			sdata = get_iproperties_data( widget );
			sdata->instance = instance;
		}
		return( widget );
	}

	g_info( "%s: ofaIProperties's %s implementation does not provide 'init()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iproperties_get_valid:
 * @widget: the #GtkWidget returned by init().
 * @msgerr: [allow-none][out]: an error message to be returned.
 *
 * Returns: %TRUE if the page is valid, %FALSE else.
 */
gboolean
ofa_iproperties_get_valid( GtkWidget *widget, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_iproperties_get_valid";
	sIProperties *sdata;

	g_debug( "%s: widget=%p (%s), msgerr=%p",
			thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ), ( void * ) msgerr );

	g_return_val_if_fail( widget && GTK_IS_WIDGET( widget ), FALSE );

	sdata = get_iproperties_data( widget );

	if( sdata && sdata->instance ){
		if( OFA_IPROPERTIES_GET_INTERFACE( sdata->instance )->get_valid ){
			return( OFA_IPROPERTIES_GET_INTERFACE( sdata->instance )->get_valid( sdata->instance, widget, msgerr ));

		} else {
			g_info( "%s: ofaIProperties's %s implementation does not provide 'get_valid()' method",
					thisfn, G_OBJECT_TYPE_NAME( sdata->instance ));
		}
	} else {
		g_warning( "%s: widget has not been initialized for ofaIProperties use", thisfn );
	}

	return( FALSE );
}

/**
 * ofa_iproperties_apply:
 * @widget: the #GtkWidget returned by init().
 * @msgerr: [allow-none][out]: an error message to be returned.
 *
 * Saves the user preferences.
 */
void
ofa_iproperties_apply( GtkWidget *widget )
{
	static const gchar *thisfn = "ofa_iproperties_apply";
	sIProperties *sdata;

	g_debug( "%s: widget=%p (%s)",
			thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ));

	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

	sdata = get_iproperties_data( widget );

	if( sdata && sdata->instance ){
		if( OFA_IPROPERTIES_GET_INTERFACE( sdata->instance )->apply ){
			OFA_IPROPERTIES_GET_INTERFACE( sdata->instance )->apply( sdata->instance, widget );
			return;

		} else {
			g_info( "%s: ofaIProperties's %s implementation does not provide 'apply()' method",
					thisfn, G_OBJECT_TYPE_NAME( sdata->instance ));
		}
	} else {
		g_warning( "%s: widget has not been initialized for ofaIProperties use", thisfn );
	}
}

/*
 * ofa_iproperties_get_title:
 * @instance: this #ofaIProperties instance.
 *
 * Returns: the displayable name of the @instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * The returned string is used as the tab label in user preferences
 * notebook. If %NULL, then the corresponding page will not be
 * displayed.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the @instance class.
 */
gchar *
ofa_iproperties_get_title( const ofaIProperties *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IPROPERTIES( instance ), NULL );

	return( MY_IS_IIDENT( instance ) ? my_iident_get_display_name( MY_IIDENT( instance ), NULL ) : NULL );
}

static sIProperties *
get_iproperties_data( GtkWidget *widget )
{
	sIProperties *data;

	data = ( sIProperties * ) g_object_get_data( G_OBJECT( widget ), IPROPERTIES_DATA );

	if( !data ){
		data = g_new0( sIProperties, 1 );
		g_object_set_data( G_OBJECT( widget ), IPROPERTIES_DATA, data );
		g_object_weak_ref( G_OBJECT( widget ), ( GWeakNotify ) on_widget_finalized, data );
	}

	return( data );
}

static void
on_widget_finalized( sIProperties *sdata, GObject *finalized_widget )
{
	static const gchar *thisfn = "ofa_iproperties_on_widget_finalized";

	g_debug( "%s: sdata=%p, finalized_widget=%p", thisfn, ( void * ) sdata, ( void * ) finalized_widget );

	g_free( sdata );
}
