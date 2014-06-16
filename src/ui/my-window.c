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

#include "core/my-utils.h"
#include "ui/my-window.h"
#include "ui/my-window-prot.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-settings.h"

/* private instance data
 */
struct _myWindowPrivate {

	/* properties
	 */
	gchar         *window_xml;
	gchar         *window_name;
	gboolean       manage_size_position;

	/* this may be either a GtkDialog or a GtkAssistant
	 */
	GtkWindow     *toplevel;
};

/* class properties
 */
enum {
	PROP_MAIN_WINDOW_ID = 1,
	PROP_DOSSIER_ID,
	PROP_WINDOW_XML_ID,
	PROP_WINDOW_NAME_ID,
	PROP_SIZE_POSITION_ID
};

G_DEFINE_TYPE( myWindow, my_window, G_TYPE_OBJECT )

static void     restore_window_position( GtkWindow *toplevel, const gchar *name );
static void     int_list_to_position( GList *list, gint *x, gint *y, gint *width, gint *height );
static void     save_window_position( GtkWindow *toplevel, const gchar *name );
static GList   *position_to_int_list( gint x, gint y, gint width, gint height );

static void
my_window_finalize( GObject *instance )
{
	myWindow *self;

	self = MY_WINDOW( instance );

	/* free data members here */
	g_free( self->private->window_xml );
	g_free( self->private->window_name );
	g_free( self->private );
	g_free( self->protected );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_window_parent_class )->finalize( instance );
}

static void
my_window_dispose( GObject *instance )
{
	myWindow *self;
	myWindowPrivate *priv;

	g_return_if_fail( instance && MY_IS_WINDOW( instance ));

	self = MY_WINDOW( instance );
	priv = self->private;

	if( !self->protected->dispose_has_run ){

		self->protected->dispose_has_run = TRUE;

		if( priv->manage_size_position && priv->toplevel && priv->window_name ){

			save_window_position( priv->toplevel, priv->window_name );
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
my_window_constructed( GObject *instance )
{
	myWindowPrivate *priv;
	GtkWidget *toplevel;

	g_return_if_fail( instance && MY_IS_WINDOW( instance ));
	g_return_if_fail( !MY_WINDOW( instance )->protected->dispose_has_run );

	priv = MY_WINDOW( instance )->private;

	/* first, chain up to the parent class */
	G_OBJECT_CLASS( my_window_parent_class )->dispose( instance );

	/* then it is time to load the toplevel from builder
	 * NB: even if properties are not set by the derived class, then
	 *     the variables are set, though empty */
	if( priv->window_xml && g_utf8_strlen( priv->window_xml, -1 ) &&
		priv->window_name && g_utf8_strlen( priv->window_name, -1 )){

		toplevel = my_utils_builder_load_from_path( priv->window_xml, priv->window_name );
		if( toplevel && GTK_IS_WINDOW( toplevel )){

			priv->toplevel = GTK_WINDOW( toplevel );
			if( priv->manage_size_position ){

				restore_window_position( priv->toplevel, priv->window_name );
			}
		}
	}
}

static void
my_window_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	myWindow *self;

	g_return_if_fail( object && MY_IS_WINDOW( object ));

	self = MY_WINDOW( object );

	if( !self->protected->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				g_value_set_pointer( value, self->protected->main_window );
				break;

			case PROP_DOSSIER_ID:
				g_value_set_pointer( value, self->protected->dossier );
				break;

			case PROP_WINDOW_XML_ID:
				g_value_set_string( value, self->private->window_xml);
				break;

			case PROP_WINDOW_NAME_ID:
				g_value_set_string( value, self->private->window_name );
				break;

			case PROP_SIZE_POSITION_ID:
				g_value_set_boolean( value, self->private->manage_size_position );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
my_window_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	myWindow *self;

	g_return_if_fail( object && MY_IS_WINDOW( object ));

	self = MY_WINDOW( object );

	if( !self->protected->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				self->protected->main_window = g_value_get_pointer( value );
				break;

			case PROP_DOSSIER_ID:
				self->protected->dossier = g_value_get_pointer( value );
				break;

			case PROP_WINDOW_XML_ID:
				g_free( self->private->window_xml );
				self->private->window_xml = g_value_dup_string( value );
				break;

			case PROP_WINDOW_NAME_ID:
				g_free( self->private->window_name );
				self->private->window_name = g_value_dup_string( value );
				break;

			case PROP_SIZE_POSITION_ID:
				self->private->manage_size_position = g_value_get_boolean( value );
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

	self->private = g_new0( myWindowPrivate, 1 );
	self->protected = g_new0( myWindowProtected, 1 );

	self->private->window_xml = NULL;
	self->private->window_name = NULL;
	self->private->toplevel = NULL;

	self->protected->dispose_has_run = FALSE;
}

static void
my_window_class_init( myWindowClass *klass )
{
	static const gchar *thisfn = "my_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = my_window_get_property;
	G_OBJECT_CLASS( klass )->set_property = my_window_set_property;
	G_OBJECT_CLASS( klass )->constructed = my_window_constructed;
	G_OBJECT_CLASS( klass )->dispose = my_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_window_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_MAIN_WINDOW_ID,
			g_param_spec_pointer(
					MY_PROP_MAIN_WINDOW,
					"Main window",
					"The main window of the application",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_DOSSIER_ID,
			g_param_spec_pointer(
					MY_PROP_DOSSIER,
					"Dossier",
					"The currently opened dossier (if any)",
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

/*
 * na_gtk_utils_restore_position_window:
 * @toplevel: the #GtkWindow window.
 * @wsp_name: the string which handles the window size and position in user preferences.
 *
 * Position the specified window on the screen.
 *
 * A window position is stored as a list of integers "x,y,width,height".
 */
static void
restore_window_position( GtkWindow *toplevel, const gchar *name )
{
	static const gchar *thisfn = "my_window_restore_window_position";
	gchar *key;
	GList *list;
	gint x=0, y=0, width=0, height=0;

	g_debug( "%s: toplevel=%p (%s), name=%s",
			thisfn, ( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ), name );

	key = g_strdup_printf( "%s-pos", name );
	list = ofa_settings_get_uint_list( key );
	g_free( key );
	g_debug( "%s: list=%p (count=%d)", thisfn, ( void * ) list, g_list_length( list ));

	if( list ){
		int_list_to_position( list, &x, &y, &width, &height );
		g_debug( "%s: name=%s, x=%d, y=%d, width=%d, height=%d", thisfn, name, x, y, width, height );
		g_list_free( list );

		gtk_window_move( toplevel, x, y );
		gtk_window_resize( toplevel, width, height );
	}
}

/*
 * extract the position of the window from the list of unsigned integers
 */
static void
int_list_to_position( GList *list, gint *x, gint *y, gint *width, gint *height )
{
	GList *it;
	int i;

	g_assert( x );
	g_assert( y );
	g_assert( width );
	g_assert( height );

	for( it=list, i=0 ; it ; it=it->next, i+=1 ){
		switch( i ){
			case 0:
				*x = GPOINTER_TO_UINT( it->data );
				break;
			case 1:
				*y = GPOINTER_TO_UINT( it->data );
				break;
			case 2:
				*width = GPOINTER_TO_UINT( it->data );
				break;
			case 3:
				*height = GPOINTER_TO_UINT( it->data );
				break;
		}
	}
}

/*
 * na_gtk_utils_save_window_position:
 * @toplevel: the #GtkWindow window.
 * @wsp_name: the string which handles the window size and position in user preferences.
 *
 * Save the size and position of the specified window.
 */
static void
save_window_position( GtkWindow *toplevel, const gchar *name )
{
	static const gchar *thisfn = "my_window_save_window_position";
	gint x, y, width, height;
	gchar *key;
	GList *list;

	gtk_window_get_position( toplevel, &x, &y );
	gtk_window_get_size( toplevel, &width, &height );
	g_debug( "%s: wsp_name=%s, x=%d, y=%d, width=%d, height=%d", thisfn, name, x, y, width, height );

	list = position_to_int_list( x, y, width, height );

	key = g_strdup_printf( "%s-pos", name );
	ofa_settings_set_uint_list( key, list );

	g_free( key );
	g_list_free( list );
}

/*
 * the returned list should be g_list_free() by the caller
 */
static GList *
position_to_int_list( gint x, gint y, gint width, gint height )
{
	GList *list = NULL;

	list = g_list_append( list, GUINT_TO_POINTER( x ));
	list = g_list_append( list, GUINT_TO_POINTER( y ));
	list = g_list_append( list, GUINT_TO_POINTER( width ));
	list = g_list_append( list, GUINT_TO_POINTER( height ));

	return( list );
}

/**
 * my_window_get_dossier:
 */
ofoDossier *
my_window_get_dossier( const myWindow *self )
{
	g_return_val_if_fail( self && MY_IS_WINDOW( self ), NULL );

	if( !self->protected->dispose_has_run ){

		return( self->protected->dossier );
	}

	return( NULL );
}

/**
 * my_window_get_main_window:
 */
ofaMainWindow *
my_window_get_main_window( const myWindow *self )
{
	g_return_val_if_fail( self && MY_IS_WINDOW( self ), NULL );

	if( !self->protected->dispose_has_run ){

		return( self->protected->main_window );
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

	if( !self->protected->dispose_has_run ){

		return( self->private->toplevel );
	}

	return( NULL );
}

/**
 * my_window_has_valid_toplevel:
 */
gboolean
my_window_has_valid_toplevel( const myWindow *self )
{
	g_return_val_if_fail( self && MY_IS_WINDOW( self ), FALSE );

	if( !self->protected->dispose_has_run ){

		return( self->private->toplevel && GTK_IS_WINDOW( self->private->toplevel ));
	}

	return( FALSE );
}
