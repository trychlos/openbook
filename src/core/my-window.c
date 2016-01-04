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

#include "api/my-utils.h"
#include "api/my-window.h"
#include "api/my-window-prot.h"

/* private instance data
 */
struct _myWindowPrivate {

	/* properties
	 */
	gchar                *window_xml;
	gchar                *window_name;
	gboolean              manage_size_position;
	GtkApplicationWindow *main_window;
	GtkWindow            *parent;

	/* this may be either a GtkDialog or a GtkAssistant
	 */
	GtkWindow            *toplevel;
};

/* class properties
 */
enum {
	PROP_MAIN_WINDOW_ID = 1,
	PROP_PARENT_ID,
	PROP_WINDOW_XML_ID,
	PROP_WINDOW_NAME_ID,
	PROP_SIZE_POSITION_ID
};

G_DEFINE_TYPE( myWindow, my_window, G_TYPE_OBJECT )

static void
window_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_window_finalize";
	myWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	priv = MY_WINDOW( instance )->priv;

	g_free( priv->window_xml );
	g_free( priv->window_name );

	g_free( MY_WINDOW( instance )->prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_window_parent_class )->finalize( instance );
}

static void
window_dispose( GObject *instance )
{
	myWindow *self;
	myWindowProtected *prot;
	myWindowPrivate *priv;

	g_return_if_fail( instance && MY_IS_WINDOW( instance ));

	self = MY_WINDOW( instance );
	prot = self->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;
		priv = self->priv;

		if( priv->manage_size_position &&
				priv->toplevel && GTK_IS_WINDOW( priv->toplevel ) &&
				priv->window_name ){
			my_utils_window_save_position( priv->toplevel, priv->window_name );
		}

		/* unref member objects here */

		if( priv->toplevel ){
			gtk_widget_destroy( GTK_WIDGET( priv->toplevel ));
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_window_parent_class )->dispose( instance );
}

static void
window_constructed( GObject *instance )
{
	static const gchar *thisfn = "my_window_constructed";
	myWindowPrivate *priv;
	GtkWidget *toplevel;

	g_return_if_fail( instance && MY_IS_WINDOW( instance ));
	g_return_if_fail( !MY_WINDOW( instance )->prot->dispose_has_run );

	/* first, chain up to the parent class */
	G_OBJECT_CLASS( my_window_parent_class )->constructed( instance );

	/* then it is time to load the toplevel from builder
	 * NB: even if properties are not set by the derived class, then
	 *     the variables are set, though empty */
	priv = MY_WINDOW( instance )->priv;

	if( my_strlen( priv->window_xml ) && my_strlen( priv->window_name )){
		toplevel = my_utils_builder_load_from_path( priv->window_xml, priv->window_name );

		if( toplevel && GTK_IS_WINDOW( toplevel )){
			priv->toplevel = GTK_WINDOW( toplevel );

			if( priv->manage_size_position ){
				my_utils_window_restore_position( priv->toplevel, priv->window_name );
			}
		}
	}

	if( priv->toplevel ){
		if( !priv->parent && priv->main_window ){
			priv->parent = GTK_WINDOW( priv->main_window );
		}
		if( priv->parent ){
			gtk_window_set_transient_for( priv->toplevel, priv->parent );
		}

	} else {
		g_warning( "%s: main_window=%p, parent=%p, toplevel=%p",
				thisfn,
				( void * ) priv->main_window,
				( void * ) priv->parent,
				( void * ) priv->toplevel );
	}
}

static void
window_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	myWindow *self;

	g_return_if_fail( object && MY_IS_WINDOW( object ));

	self = MY_WINDOW( object );

	if( !self->prot->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				g_value_set_pointer( value, self->priv->main_window );
				break;

			case PROP_PARENT_ID:
				g_value_set_pointer( value, self->priv->parent );
				break;

			case PROP_WINDOW_XML_ID:
				g_value_set_string( value, self->priv->window_xml );
				break;

			case PROP_WINDOW_NAME_ID:
				g_value_set_string( value, self->priv->window_name );
				break;

			case PROP_SIZE_POSITION_ID:
				g_value_set_boolean( value, self->priv->manage_size_position );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
window_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	myWindow *self;

	g_return_if_fail( object && MY_IS_WINDOW( object ));

	self = MY_WINDOW( object );

	if( !self->prot->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				self->priv->main_window = g_value_get_pointer( value );
				break;

			case PROP_PARENT_ID:
				self->priv->parent = g_value_get_pointer( value );
				break;

			case PROP_WINDOW_XML_ID:
				g_free( self->priv->window_xml );
				self->priv->window_xml = g_value_dup_string( value );
				break;

			case PROP_WINDOW_NAME_ID:
				g_free( self->priv->window_name );
				self->priv->window_name = g_value_dup_string( value );
				break;

			case PROP_SIZE_POSITION_ID:
				self->priv->manage_size_position = g_value_get_boolean( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
my_window_init( myWindow *self )
{
	static const gchar *thisfn = "my_window_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->prot = g_new0( myWindowProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_WINDOW, myWindowPrivate );
	self->priv->window_xml = NULL;
	self->priv->window_name = NULL;
	self->priv->toplevel = NULL;

}

static void
my_window_class_init( myWindowClass *klass )
{
	static const gchar *thisfn = "my_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = window_get_property;
	G_OBJECT_CLASS( klass )->set_property = window_set_property;
	G_OBJECT_CLASS( klass )->constructed = window_constructed;
	G_OBJECT_CLASS( klass )->dispose = window_dispose;
	G_OBJECT_CLASS( klass )->finalize = window_finalize;

	g_type_class_add_private( klass, sizeof( myWindowPrivate ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_MAIN_WINDOW_ID,
			g_param_spec_pointer(
					MY_PROP_MAIN_WINDOW,
					"Main window",
					"The main #GtkApplicationWindow window of the application",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_PARENT_ID,
			g_param_spec_pointer(
					MY_PROP_PARENT,
					"Parent window",
					"The parent window",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_XML_ID,
			g_param_spec_string(
					MY_PROP_WINDOW_XML,
					"UI XML pathname",
					"The pathname to the file which contains the UI definition",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_NAME_ID,
			g_param_spec_string(
					MY_PROP_WINDOW_NAME,
					"Window box name",
					"The unique name of the managed window box",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_SIZE_POSITION_ID,
			g_param_spec_boolean(
					MY_PROP_SIZE_POSITION,
					"Manage size and position",
					"Whether to manage size and position of the toplevel",
					TRUE,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

/**
 * my_window_get_main_window:
 */
GtkApplicationWindow *
my_window_get_main_window( const myWindow *self )
{
	myWindowPrivate *priv;
	GtkApplicationWindow *window;

	priv = self->priv;
	window = NULL;

	g_return_val_if_fail( self && MY_IS_WINDOW( self ), NULL );

	if( !self->prot->dispose_has_run ){

		window = priv->main_window;
	}

	return( window );
}

/**
 * my_window_get_name:
 *
 * Returns: the name of the most top GTkBuildable widget.
 */
const gchar *
my_window_get_name( const myWindow *self )
{
	g_return_val_if_fail( self && MY_IS_WINDOW( self ), NULL );

	if( !self->prot->dispose_has_run ){

		return( self->priv->window_name );
	}

	return( NULL );
}

/**
 * my_window_get_toplevel:
 */
GtkWindow *
my_window_get_toplevel( const myWindow *self )
{
	g_return_val_if_fail( self && MY_IS_WINDOW( self ), NULL );

	if( !self->prot->dispose_has_run ){

		return( self->priv->toplevel );
	}

	return( NULL );
}
