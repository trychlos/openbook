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

#include "ui/base-window.h"
#include "ui/base-window-prot.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-settings.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

/* private instance data
 */
struct _BaseWindowPrivate {

	/* properties
	 */
	ofaMainWindow *main_window;
	gchar         *window_xml;
	gchar         *window_name;
};

/* class properties
 */
enum {
	PROP_MAIN_WINDOW_ID = 1,
	PROP_WINDOW_XML_ID,
	PROP_WINDOW_NAME_ID
};

G_DEFINE_TYPE( BaseWindow, base_window, G_TYPE_OBJECT )

static gboolean load_from_builder( BaseWindow *window );
static void     restore_window_position( GtkWindow *toplevel, const gchar *name );
static void     int_list_to_position( GList *list, gint *x, gint *y, gint *width, gint *height );
static void     save_window_position( GtkWindow *toplevel, const gchar *name );
static GList   *position_to_int_list( gint x, gint y, gint width, gint height );

static void
base_window_finalize( GObject *instance )
{
	BaseWindow *self;

	self = BASE_WINDOW( instance );

	/* free data members here */
	g_free( self->private->window_xml );
	g_free( self->private->window_name );
	g_free( self->private );
	g_free( self->protected );

	/* chain up to the parent class */
	G_OBJECT_CLASS( base_window_parent_class )->finalize( instance );
}

static void
base_window_dispose( GObject *instance )
{
	g_return_if_fail( IS_BASE_WINDOW( instance ));

	if( !BASE_WINDOW( instance )->protected->dispose_has_run ){

		BASE_WINDOW( instance )->protected->dispose_has_run = TRUE;

		save_window_position(
				BASE_WINDOW( instance )->protected->window,
				BASE_WINDOW( instance )->private->window_name );

		/* unref member objects here */

		gtk_widget_destroy( GTK_WIDGET( BASE_WINDOW( instance )->protected->window ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( base_window_parent_class )->dispose( instance );
}

static void
base_window_constructed( GObject *instance )
{
	g_return_if_fail( IS_BASE_WINDOW( instance ));

	if( !BASE_WINDOW( instance )->protected->dispose_has_run ){

		/* first, chain up to the parent class */
		G_OBJECT_CLASS( base_window_parent_class )->dispose( instance );

		/* then it is time to load the toplevel from builder */
		if( load_from_builder( BASE_WINDOW( instance ))){

			restore_window_position(
					BASE_WINDOW( instance )->protected->window,
					BASE_WINDOW( instance )->private->window_name );
		}
	}
}

static void
base_window_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	BaseWindowPrivate *priv;

	g_return_if_fail( IS_BASE_WINDOW( object ));

	priv = BASE_WINDOW( object )->private;

	if( !BASE_WINDOW( object )->protected->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			case PROP_WINDOW_XML_ID:
				g_value_set_string( value, priv->window_xml);
				break;

			case PROP_WINDOW_NAME_ID:
				g_value_set_string( value, priv->window_name );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
base_window_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	BaseWindowPrivate *priv;

	g_return_if_fail( IS_BASE_WINDOW( object ));

	priv = BASE_WINDOW( object )->private;

	if( !BASE_WINDOW( object )->protected->dispose_has_run ){

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			case PROP_WINDOW_XML_ID:
				g_free( priv->window_xml );
				priv->window_xml = g_value_dup_string( value );
				break;

			case PROP_WINDOW_NAME_ID:
				g_free( priv->window_name );
				priv->window_name = g_value_dup_string( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
base_window_init( BaseWindow *self )
{
	static const gchar *thisfn = "base_window_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( BaseWindowPrivate, 1 );
	self->protected = g_new0( BaseWindowProtected, 1 );
}

static void
base_window_class_init( BaseWindowClass *klass )
{
	static const gchar *thisfn = "base_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = base_window_get_property;
	G_OBJECT_CLASS( klass )->set_property = base_window_set_property;
	G_OBJECT_CLASS( klass )->constructed = base_window_constructed;
	G_OBJECT_CLASS( klass )->dispose = base_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = base_window_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_MAIN_WINDOW_ID,
			g_param_spec_pointer(
					BASE_PROP_MAIN_WINDOW,
					"Main window",
					"The main window of the application",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_XML_ID,
			g_param_spec_string(
					BASE_PROP_WINDOW_XML,
					"UI XML pathname",
					"The pathname to the file which contains the UI definition",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_NAME_ID,
			g_param_spec_string(
					BASE_PROP_WINDOW_NAME,
					"Window box name",
					"The unique name of the managed window box",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

static gboolean
load_from_builder( BaseWindow *window )
{
	static const gchar *thisfn = "base_window_load_from_builder";
	gboolean loaded;
	BaseWindowPrivate *priv;
	GError *error;
	GtkBuilder *builder;

	loaded = FALSE;
	priv = window->private;
	error = NULL;
	builder = gtk_builder_new();

	if( gtk_builder_add_from_file( builder, priv->window_xml, &error )){

		window->protected->window =
					GTK_WINDOW( gtk_builder_get_object( builder, priv->window_name ));

		if( !window->protected->window ){
			g_warning( "%s: unable to find '%s' object in '%s' file",
									thisfn, priv->window_name, priv->window_xml );

		} else {
			loaded = TRUE;
		}

	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}

	g_object_unref( builder );

	return( loaded );
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
	static const gchar *thisfn = "base_window_restore_window_position";
	gchar *key;
	GList *list;
	gint x=0, y=0, width=0, height=0;

	g_debug( "%s: toplevel=%p (%s), name=%s",
			thisfn, ( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ), name );

	key = g_strdup_printf( "%s-pos", name );
	list = ofa_settings_get_uint_list( key );
	g_free( key );

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
	static const gchar *thisfn = "base_window_save_window_position";
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
 * base_window_get_dossier:
 */
ofoDossier *
base_window_get_dossier( const BaseWindow *window )
{
	g_return_val_if_fail( IS_BASE_WINDOW( window ), NULL );

	if( !window->protected->dispose_has_run ){

		return( ofa_main_window_get_dossier( window->private->main_window ));
	}

	return( NULL );
}

/**
 * base_window_get_main_window:
 */
ofaMainWindow *
base_window_get_main_window( const BaseWindow *window )
{
	g_return_val_if_fail( IS_BASE_WINDOW( window ), NULL );

	if( !window->protected->dispose_has_run ){

		return( window->private->main_window );
	}

	return( NULL );
}
