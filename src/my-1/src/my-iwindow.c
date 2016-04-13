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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#define IWINDOW_LAST_VERSION            1
#define IWINDOW_DATA                    "my-iwindow-data"

static guint    st_initializations      = 0;		/* interface initialization count */
static gboolean st_dump_container       = FALSE;
static GList   *st_live_list            = NULL;		/* list of IWindow instances */

/* a data structure attached to each instance
 */
typedef struct {
	GtkWindow   *parent;
	myISettings *settings;
	gboolean     initialized;
	gboolean     hide_on_close;
}
	sIWindow;

static GType      register_type( void );
static void       interface_base_init( myIWindowInterface *klass );
static void       interface_base_finalize( myIWindowInterface *klass );
static void       iwindow_init_application( myIWindow *instance );
static void       iwindow_init_window( myIWindow *instance, sIWindow *sdata );
static void       application_do_read_settings( myIWindow *instance, sIWindow *sdata );
static void       application_do_write_settings( myIWindow *instance, sIWindow *sdata );
static gboolean   iwindow_quit_on_escape( const myIWindow *instance );
static gboolean   on_delete_event( GtkWidget *widget, GdkEvent *event, myIWindow *instance );
static void       do_close( myIWindow *instance );
static gchar     *iwindow_get_identifier( const myIWindow *instance );
static gchar     *get_default_identifier( const myIWindow *instance );
static gchar     *iwindow_get_key_prefix( const myIWindow *instance );
static void       iwindow_set_default_size( myIWindow *instance );
static void       iwindow_set_transient_for( myIWindow *instance );
static void       position_restore( myIWindow *instance, sIWindow *sdata );
static void       position_save( myIWindow *instance, sIWindow *sdata );
static sIWindow  *get_iwindow_data( const myIWindow *instance );
static void       on_iwindow_finalized( sIWindow *sdata, GObject *finalized_iwindow );

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
my_iwindow_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IWINDOW );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIWindowInterface * ) iface )->get_interface_version ){
		version = (( myIWindowInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIWindow::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_iwindow_get_parent:
 * @instance: this #myIWindow instance.
 *
 * Returns: the #GtkWindow parent of this window, defaulting to the
 * main windoww of the application (if set).
 *
 * The returned reference is owned by the implementation, and should
 * not be released by the caller.
 */
GtkWindow *
my_iwindow_get_parent( const myIWindow *instance )
{
	sIWindow *sdata;

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	sdata = get_iwindow_data( instance );

	return( sdata->parent );
}

/**
 * my_iwindow_set_parent:
 * @instance: this #myIWindow instance.
 * @parent: [allow-none]: the #GtkWindow parent of this window.
 *
 * Sets the parent.
 */
void
my_iwindow_set_parent( myIWindow *instance, GtkWindow *parent )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	sdata = get_iwindow_data( instance );
	sdata->parent = parent;
}

/**
 * my_iwindow_get_settings:
 * @instance: this #myIWindow instance.
 *
 * Returns: the #myISettings object, as it has been previously set, or %NULL.
 */
myISettings *
my_iwindow_get_settings( const myIWindow *instance )
{
	sIWindow *sdata;

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	sdata = get_iwindow_data( instance );

	return( sdata->settings );
}

/**
 * my_iwindow_set_settings:
 * @instance: this #myIWindow instance.
 * @settings: [allow-none]: the #myISettings implementation to be used.
 *
 * Sets the settings.
 */
void
my_iwindow_set_settings( myIWindow *instance, myISettings *settings )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));
	g_return_if_fail( !settings || MY_IS_ISETTINGS( settings ));

	sdata = get_iwindow_data( instance );
	sdata->settings = settings;
}

/**
 * my_iwindow_get_keyname:
 * @instance: this #myIWindow instance.
 *
 * Returns: the default key name of the user settings, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * It is by no way mandatory for the application to use this key name,
 * nor this same settings implementation, to store the user preferences.
 * This is just the default proposed by the #myIWindow interface, and
 * used by #read_settings() and #write_settings() method.
 */
gchar *
my_iwindow_get_keyname( const myIWindow *instance )
{
	gchar *keyprefix, *keyname;

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	keyprefix = iwindow_get_key_prefix( instance );
	keyname = g_strdup_printf( "%s-settings", keyprefix );
	g_free( keyprefix );

	return( keyname );
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
		g_info( "%s: myIWindow's %s implementation does not provide 'init()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}
}

static void
iwindow_init_window( myIWindow *instance, sIWindow *sdata )
{
	iwindow_set_transient_for( instance );

	position_restore( instance, sdata );

	application_do_read_settings( instance, sdata );

	if( st_dump_container ){
		my_utils_container_dump( GTK_CONTAINER( instance ));
	}

	g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), instance );
}

static void
application_do_read_settings( myIWindow *instance, sIWindow *sdata )
{
	static const gchar *thisfn = "my_iwindow_application_do_read_settings";
	gchar *key_name;

	key_name = my_iwindow_get_keyname( instance );

	if( MY_IWINDOW_GET_INTERFACE( instance )->read_settings ){
		MY_IWINDOW_GET_INTERFACE( instance )->read_settings( instance, sdata->settings, key_name );

	} else {
		g_info( "%s: myIWindow's %s implementation does not provide 'read_settings()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}

	g_free( key_name );
}

static void
application_do_write_settings( myIWindow *instance, sIWindow *sdata )
{
	static const gchar *thisfn = "my_iwindow_application_do_write_settings";
	gchar *key_name;

	key_name = my_iwindow_get_keyname( instance );

	if( MY_IWINDOW_GET_INTERFACE( instance )->write_settings ){
		MY_IWINDOW_GET_INTERFACE( instance )->write_settings( instance, sdata->settings, key_name );

	} else {
		g_info( "%s: myIWindow's %s implementation does not provide 'write_settings()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}

	g_free( key_name );
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

/**
 * my_iwindow_close_all:
 *
 * Close all current #myIWindow windows.
 */
void
my_iwindow_close_all( void )
{
	static const gchar *thisfn = "my_iwindow_close_all";
	void *it;

	while( st_live_list ){
		it = st_live_list->data;
		g_debug( "%s: closing iwindow=%p (%s)",
				thisfn, ( void * ) it, G_OBJECT_TYPE_NAME( it ));
		do_close( MY_IWINDOW( it ));
	}
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

	g_info( "%s: myIWindow's %s implementation does not provide 'quit_on_escape()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
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

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	sdata = get_iwindow_data( instance );

	application_do_write_settings( instance, sdata );

	position_save( instance, sdata );

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
	static const gchar *thisfn = "my_iwindow_get_identifier";

	if( MY_IWINDOW_GET_INTERFACE( instance )->get_identifier ){
		return( MY_IWINDOW_GET_INTERFACE( instance )->get_identifier( instance ));
	}

	g_info( "%s: myIWindow's %s implementation does not provide 'get_identifier()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( get_default_identifier( instance ));
}

static gchar *
get_default_identifier( const myIWindow *instance )
{
	return( g_strdup( G_OBJECT_TYPE_NAME( instance )));
}

/*
 * Returns: the settings key as a newly allocated string which should
 * be g_free() by the caller.
 *
 * The key defaults to the identifier.
 */
static gchar *
iwindow_get_key_prefix( const myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_get_key_prefix";

	if( MY_IWINDOW_GET_INTERFACE( instance )->get_key_prefix ){
		return( MY_IWINDOW_GET_INTERFACE( instance )->get_key_prefix( instance ));
	}

	g_info( "%s: myIWindow's %s implementation does not provide 'get_key_prefix()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	return( iwindow_get_identifier( instance ));
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
		g_info( "%s: myIWindow's %s implementation does not provide 'get_default_size()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
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
	GtkWindow *parent;

	sdata = get_iwindow_data( instance );
	parent = sdata->parent;

	if( parent ){
		gtk_window_set_transient_for( GTK_WINDOW( instance ), parent );
	}
}

/**
 * my_iwindow_msg_dialog:
 * @instance: this #myIWindow instance.
 * @type: the type of the displayed #GtkMessageDialog.
 * @msg: the message to be displayed.
 *
 * Display a message dialog.
 */
void
my_iwindow_msg_dialog( myIWindow *instance, GtkMessageType type, const gchar *msg )
{
	g_return_if_fail( instance && MY_IWINDOW( instance ));
	g_return_if_fail( my_strlen( msg ));

	my_utils_msg_dialog( GTK_WINDOW( instance ), type, msg );
}

/*
 * Save/restore the size and position for each identified myIWindow.
 * Have a default to ClassName
 */
static void
position_restore( myIWindow *instance, sIWindow *sdata )
{
	gchar *key_prefix;

	key_prefix = iwindow_get_key_prefix( instance );

	if( !my_utils_window_position_get_has_pos( sdata->settings, key_prefix )){
		g_free( key_prefix );
		key_prefix = get_default_identifier( instance );
	}
	if( !my_utils_window_position_restore( GTK_WINDOW( instance ), sdata->settings, key_prefix )){
		iwindow_set_default_size( instance );
	}
	g_free( key_prefix );
}

static void
position_save( myIWindow *instance, sIWindow *sdata )
{
	gchar *key_prefix, *default_key;

	key_prefix = iwindow_get_key_prefix( instance );
	my_utils_window_position_save( GTK_WINDOW( instance ), sdata->settings, key_prefix );
	g_free( key_prefix );

	default_key = get_default_identifier( instance );
	if( !my_utils_window_position_get_has_pos( sdata->settings, default_key )){
		my_utils_window_position_save( GTK_WINDOW( instance ), sdata->settings, default_key );
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

		sdata->parent = NULL;
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

	g_free( sdata );

	st_live_list = g_list_remove( st_live_list, finalized_iwindow );
}
