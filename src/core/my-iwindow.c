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

#include <glib/gi18n.h>

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"

#define IWINDOW_LAST_VERSION            1
#define IWINDOW_DATA                    "my-iwindow-data"

static guint    st_initializations      = 0;		/* interface initialization count */
static gboolean st_dump_container       = FALSE;
static GList   *st_live_list            = NULL;		/* list of IWindow instances */

/* a data structure attached to each instance
 */
typedef struct {
	GtkApplicationWindow *main_window;
	GtkWindow            *parent;
	gchar                *settings_name;
	gboolean              initialized;
	gboolean              hide_on_close;
}
	sIWindow;

static GType     register_type( void );
static void      interface_base_init( myIWindowInterface *klass );
static void      interface_base_finalize( myIWindowInterface *klass );
static void      iwindow_init_application( myIWindow *instance );
static void      iwindow_init_window( myIWindow *instance, sIWindow *sdata );
static gboolean  iwindow_quit_on_escape( const myIWindow *instance );
static gboolean  on_delete_event( GtkWidget *widget, GdkEvent *event, myIWindow *instance );
static void      do_close( myIWindow *instance );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static gchar    *iwindow_get_settings_name( myIWindow *instance );
static void      iwindow_set_default_size( myIWindow *instance );
static void      iwindow_set_transient_for( myIWindow *instance );
static sIWindow *get_iwindow_data( const myIWindow *instance );
static void      on_iwindow_finalized( sIWindow *sdata, GObject *finalized_iwindow );

/**
 * my_iwindow_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iwindow_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iwindow_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iwindow_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIWindowInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIWindow", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_WINDOW );

	return( type );
}

static void
interface_base_init( myIWindowInterface *klass )
{
	static const gchar *thisfn = "my_iwindow_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIWindowInterface *klass )
{
	static const gchar *thisfn = "my_iwindow_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iwindow_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_iwindow_get_interface_last_version( void )
{
	return( IWINDOW_LAST_VERSION );
}

/**
 * my_iwindow_get_interface_version:
 * @instance: this #myIWindow instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
my_iwindow_get_interface_version( const myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), 0 );

	if( MY_IWINDOW_GET_INTERFACE( instance )->get_interface_version ){
		return( MY_IWINDOW_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: myIWindow instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * my_iwindow_get_main_window:
 * @instance: this #myIWindow instance.
 *
 * Returns: the #GtkApplicationWindow which has been previously set as
 * the main window.
 *
 * The returned reference is owned by the implementation, and should
 * not be released by the caller.
 */
GtkApplicationWindow *
my_iwindow_get_main_window( const myIWindow *instance )
{
	sIWindow *sdata;

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	sdata = get_iwindow_data( instance );

	return( sdata->main_window );
}

/**
 * my_iwindow_set_main_window:
 * @instance: this #myIWindow instance.
 * @main_window: the #GtkApplicationWindow main window of the application.
 *
 * Sets the main window, which happens to be the default parent.
 *
 * This method should be called by the implementation right after the
 * instanciation of the window, and at latest before presenting (if
 * non-modal) or running (if modal) the window.
 */
void
my_iwindow_set_main_window( myIWindow *instance, GtkApplicationWindow *main_window )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));
	g_return_if_fail( main_window && GTK_IS_APPLICATION_WINDOW( main_window ));

	sdata = get_iwindow_data( instance );
	sdata->main_window = main_window;
}

/**
 * my_iwindow_set_hide_on_close:
 * @instance: this #myIWindow instance.
 * @hide_on_close: whether the #GtkwINDOW must be hidden on close, rather
 *  than being destroyed.

 * Set the @hide_on_close indicator.
 */
void
my_iwindow_set_hide_on_close( myIWindow *instance, gboolean hide_on_close )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_iwindow_data( instance );
	sdata->hide_on_close = hide_on_close;
}

/**
 * my_iwindow_init:
 * @instance: this #myIWindow instance.
 *
 * One-time initialization of the @instance.
 */
void
my_iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_init";
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_iwindow_data( instance );

	if( !sdata->initialized ){
		g_debug( "%s: instance=%p", thisfn, ( void * ) instance );
		sdata->initialized = TRUE;

		iwindow_init_application( instance );
		iwindow_init_window( instance, sdata );

		if( MY_IS_IDIALOG( instance )){
			my_idialog_init( MY_IDIALOG( instance ));
		}
	}
}

static void
iwindow_init_application( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_init_application";

	if( MY_IWINDOW_GET_INTERFACE( instance )->init ){
		MY_IWINDOW_GET_INTERFACE( instance )->init( instance );

	} else {
		g_info( "%s: myIWindow instance %p (%s) does not provide 'init()' method",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));
	}
}

static void
iwindow_init_window( myIWindow *instance, sIWindow *sdata )
{
	gchar *settings_name;

	iwindow_set_transient_for( instance );

	settings_name = iwindow_get_settings_name( instance );
	if( !my_utils_window_restore_position( GTK_WINDOW( instance ), settings_name )){
		iwindow_set_default_size( instance );
	}
	g_free( settings_name );

	if( st_dump_container ){
		my_utils_container_dump( GTK_CONTAINER( instance ));
	}

	g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), instance );
}

/**
 * my_iwindow_present:
 * @instance: this #myIWindow instance.
 *
 * Present this window, or a previous window with the same identifier,
 * for a non-modal user interaction.
 *
 * If a previous window with the same identifier is eventually found,
 * then this current @instance is #g_object_unref(), and the previous
 * window is returned instead.
 *
 * After the call, the @instance may so be invalid.
 *
 * Returns: the actually shown instance.
 */
myIWindow *
my_iwindow_present( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_present";
	GList *it;
	myIWindow *other, *found;
	gchar *instance_id, *other_id;
	gint cmp;

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	found = NULL;
	instance_id = iwindow_get_identifier( instance );

	for( it=st_live_list ; it ; it=it->next ){
		other = MY_IWINDOW( it->data );
		if( other == instance ){
			continue;
		}
		other_id = iwindow_get_identifier( other );
		cmp = g_utf8_collate( instance_id, other_id );
		g_free( other_id );
		if( cmp == 0 ){
			found = other;
			break;
		}
	}

	g_free( instance_id );

	if( found ){
		do_close( instance );

	} else {
		my_iwindow_init( instance );
		st_live_list = g_list_prepend( st_live_list, instance );
		found = instance;
	}

	g_debug( "%s: found=%p (%s)", thisfn, ( void * ) found, G_OBJECT_TYPE_NAME( found ));
	gtk_window_present( GTK_WINDOW( found ));

	return( found );
}

/**
 * my_iwindow_close:
 * @instance: this #myIWindow instance.
 *
 * Close the #myIWindow instance withour further confirmation.
 */
void
my_iwindow_close( myIWindow *instance )
{
	g_return_if_fail( instance && MY_IWINDOW( instance ));

	do_close( instance );
}

/*
 * iwindow_quit_on_escape:
 * @instance: this #myIWindow instance.
 *
 * Let the implementation decide if it accepts to quit a dialog on
 * Escape key.
 *
 * Default is %TRUE.
 */
static gboolean
iwindow_quit_on_escape( const myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_quit_on_escape";

	if( MY_IWINDOW_GET_INTERFACE( instance )->quit_on_escape ){
		return( MY_IWINDOW_GET_INTERFACE( instance )->quit_on_escape( instance ));
	}

	g_info( "%s: myIWindow instance %p does not provide 'quit_on_escape()' method",
			thisfn, ( void * ) instance );
	return( TRUE );
}

static gboolean
on_delete_event( GtkWidget *widget, GdkEvent *event, myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_on_delete_event";

	g_debug( "%s: widget=%p, event=%p, instance=%p",
			thisfn, ( void * ) widget, ( void * ) event, ( void * ) instance );

	if( iwindow_quit_on_escape( instance )){
		do_close( instance );
	}

	return( TRUE );
}

/*
 * this closes the GtkWindow without any user confirmation
 */
static void
do_close( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_do_close";
	sIWindow *sdata;
	gchar *settings_name;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	sdata = get_iwindow_data( instance );

	settings_name = iwindow_get_settings_name( instance );
	my_utils_window_save_position( GTK_WINDOW( instance ), settings_name );
	g_free( settings_name );

	if( sdata->hide_on_close ){
		gtk_widget_hide( GTK_WIDGET( instance ));

	} else {
		gtk_widget_destroy( GTK_WIDGET( instance ));
	}
}

/*
 * iwindow_get_identifier:
 * @instance: this #myIWindow instance.
 *
 * Returns: the instance identifier as a newly allocated string which
 * should be g_free() by the caller.
 *
 * Defaults to the class name of the window implementation.
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	gchar *identifier;

	identifier = NULL;

	if( MY_IWINDOW_GET_INTERFACE( instance )->get_identifier ){
		identifier = MY_IWINDOW_GET_INTERFACE( instance )->get_identifier( instance );
	}

	if( !my_strlen( identifier )){
		g_free( identifier );
		identifier = g_strdup( G_OBJECT_TYPE_NAME( instance ));
	}

	return( identifier );
}

/*
 * Returns: the settings key as a newly allocated string which should
 * be g_free() by the caller.
 *
 * The key defaults to the identifier.
 */
static gchar *
iwindow_get_settings_name( myIWindow *instance )
{
	sIWindow *sdata;

	sdata = get_iwindow_data( instance );

	if( !my_strlen( sdata->settings_name )){
		sdata->settings_name = iwindow_get_identifier( instance );
	}

	return( g_strdup( sdata->settings_name ));
}

/*
 * Let the immplementation provide its own default size and position
 * when no previous size and position have already been recorded.
 * This is only used when no record is found in user settings.
 */
static void
iwindow_set_default_size( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_set_default_size";
	guint x, y, cx, cy;

	if( MY_IWINDOW_GET_INTERFACE( instance )->get_default_size ){
		MY_IWINDOW_GET_INTERFACE( instance )->get_default_size( instance, &x, &y, &cx, &cy );
		gtk_window_move( GTK_WINDOW( instance ), x, y );
		gtk_window_resize( GTK_WINDOW( instance ), cx, cy );

	} else {
		g_info( "%s: myIWindow instance %p (%s) does not provide 'get_default_size()' method",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));
	}
}

/*
 * Set the new window transient regarding its parent.
 * If not explicitly set via #my_iwindow_set_parent() method, the parent
 * defaults to the main window.
 *
 * This fonction is called at first-time-initialisation time.
 */
static void
iwindow_set_transient_for( myIWindow *instance )
{
	sIWindow *sdata;

	sdata = get_iwindow_data( instance );

	if( !sdata->parent ){
		sdata->parent = GTK_WINDOW( sdata->main_window );
	}
	if( sdata->parent ){
		gtk_window_set_transient_for( GTK_WINDOW( instance ), sdata->parent );
	}
}

static sIWindow *
get_iwindow_data( const myIWindow *instance )
{
	sIWindow *sdata;

	sdata = ( sIWindow * ) g_object_get_data( G_OBJECT( instance ), IWINDOW_DATA );

	if( !sdata ){
		sdata = g_new0( sIWindow, 1 );
		g_object_set_data( G_OBJECT( instance ), IWINDOW_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_iwindow_finalized, sdata );

		sdata->main_window = NULL;
		sdata->parent = NULL;
		sdata->settings_name = NULL;
		sdata->initialized = FALSE;
		sdata->hide_on_close = FALSE;
	}

	return( sdata );
}

static void
on_iwindow_finalized( sIWindow *sdata, GObject *finalized_iwindow )
{
	static const gchar *thisfn = "my_iwindow_on_iwindow_finalized";

	g_debug( "%s: sdata=%p, finalized_iwindow=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_iwindow );

	g_free( sdata->settings_name );
	g_free( sdata );

	st_live_list = g_list_remove( st_live_list, finalized_iwindow );
}
