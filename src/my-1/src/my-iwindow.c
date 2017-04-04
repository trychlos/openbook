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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#define IWINDOW_LAST_VERSION              1
#define IWINDOW_DATA                    "my-iwindow-data"
#define IWINDOW_SHIFT                    36

static guint    st_initializations      = 0;		/* interface initialization count */
static gboolean st_dump_container       = FALSE;
static GList   *st_live_list            = NULL;		/* list of IWindow instances */

/* a data structure attached to each instance
 */
typedef struct {

	/* properties
	 */
	GtkWindow   *parent;				/* the set parent of the window */
	gchar       *identifier;
	myISettings *geometry_settings;
	gchar       *geometry_key;
	gboolean     manage_geometry;
	gboolean     allow_transient;
	gboolean     allow_close;

	/* runtime
	 */
	gboolean     initialized;
	gboolean     closed;
}
	sIWindow;

static GType     register_type( void );
static void      interface_base_init( myIWindowInterface *klass );
static void      interface_base_finalize( myIWindowInterface *klass );
static void      iwindow_init_window( myIWindow *instance );
static void      iwindow_init_set_transient_for( myIWindow *instance, sIWindow *sdata );
static gboolean  on_delete_event( GtkWidget *widget, GdkEvent *event, myIWindow *instance );
static void      do_close( myIWindow *instance );
static void      position_restore( myIWindow *instance, sIWindow *sdata );
static void      position_save( myIWindow *instance, sIWindow *sdata );
static void      position_shift( myIWindow *instance, myIWindow *prev );
static gchar    *get_default_identifier( const myIWindow *instance );
static gchar    *get_default_geometry_key( const myIWindow *instance );
static sIWindow *get_instance_data( const myIWindow *instance );
static void      on_instance_finalized( sIWindow *sdata, GObject *finalized_iwindow );
static void      dump_live_list( void );

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
 * main window of the application (if set).
 *
 * The returned reference is owned by the implementation, and should
 * not be released by the caller.
 */
GtkWindow *
my_iwindow_get_parent( const myIWindow *instance )
{
	sIWindow *sdata;

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	sdata = get_instance_data( instance );

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

	sdata = get_instance_data( instance );

	sdata->parent = parent;
}

/**
 * my_iwindow_set_identifier:
 * @instance: this #myIWindow instance.
 * @identifier: [allow-none]: the identifier of this #myIWindow instance.
 *
 * Sets the identifier.
 *
 * The identifier is primarily used to only display one window of every
 * distinct identifier.
 *
 * The identifier defaults to the class name of the @instance.
 */
void
my_iwindow_set_identifier( myIWindow *instance, const gchar *identifier )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	g_free( sdata->identifier );
	sdata->identifier = my_strlen( identifier ) ? g_strdup( identifier ) : get_default_identifier( instance );
}

/**
 * my_iwindow_set_geometry_settings:
 * @instance: this #myIWindow instance.
 * @settings: [allow-none]: the #myISettings implementation to be used.
 *
 * Sets the settings.
 */
void
my_iwindow_set_geometry_settings( myIWindow *instance, myISettings *settings )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));
	g_return_if_fail( !settings || MY_IS_ISETTINGS( settings ));

	sdata = get_instance_data( instance );

	sdata->geometry_settings = settings;
}

/**
 * my_iwindow_set_geometry_key:
 * @instance: this #myIWindow instance.
 * @key: [allow-none]: the key prefix in geometry settings.
 *
 * Sets the key prefix.
 *
 * The identifier is primarily used to only display one window of every
 * distinct identifier.
 *
 * The identifier defaults to the class name of the @instance.
 */
void
my_iwindow_set_geometry_key( myIWindow *instance, const gchar *key )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	g_free( sdata->geometry_key );
	sdata->geometry_key = my_strlen( key ) ? g_strdup( key ) : get_default_geometry_key( instance );
}

/**
 * my_iwindow_set_manage_geometry:
 * @instance: this #myIWindow instance.
 * @manage: whether the @instance should try to restore (resp. save)
 *  its geometry (size and position).
 *
 * Sets the 'manage' flag.
 *
 * The #myIWindow interface defaults to try to restore (resp. save) the
 * geometry settings - size and position - of the corresponding
 * #GtkWindow.
 */
void
my_iwindow_set_manage_geometry( myIWindow *instance, gboolean manage )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	sdata->manage_geometry = manage;
}

/**
 * my_iwindow_set_allow_transient:
 * @instance: this #myIWindow instance.
 * @allow: whether this @instance should be transient for the parent.
 *
 * Sets the @allow indicator.
 *
 * When cleared, the @allow indicator prevents the @instance to be set
 * transient for its parent.
 *
 * Defaults to %TRUE.
 */
void
my_iwindow_set_allow_transient( myIWindow *instance, gboolean allow )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	sdata->allow_transient = allow;
}

/**
 * my_iwindow_set_allow_close:
 * @instance: this #myIWindow instance.
 * @allow: whether destroying this window is allowed.
 *
 * Sets the @allow indicator.
 *
 * The @allow indicator prevents the @instance to be closed by the
 * my_iwindow_close() function.
 *
 * When this indicator is set, only a direct call to gtk_widget_destroy()
 * is able to close the window.
 */
void
my_iwindow_set_allow_close( myIWindow *instance, gboolean allow )
{
	sIWindow *sdata;

	g_return_if_fail( instance && MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	sdata->allow_close = allow;
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

	sdata = get_instance_data( instance );

	if( !sdata->initialized ){

		g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

		iwindow_init_window( instance );

		if( sdata->allow_transient ){
			iwindow_init_set_transient_for( instance, sdata );
		}

		if( sdata->manage_geometry ){
			position_restore( instance, sdata );
		}

		if( st_dump_container ){
			my_utils_container_dump( GTK_CONTAINER( instance ));
		}

		g_signal_connect( instance, "delete-event", G_CALLBACK( on_delete_event ), instance );

		if( MY_IS_IDIALOG( instance )){
			my_idialog_init( MY_IDIALOG( instance ));
		}

		sdata->initialized = TRUE;
	}
}

/*
 * Let the implementation init its window
 */
static void
iwindow_init_window( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_init_window";

	if( MY_IWINDOW_GET_INTERFACE( instance )->init ){
		MY_IWINDOW_GET_INTERFACE( instance )->init( instance );

	} else {
		g_info( "%s: myIWindow's %s implementation does not provide 'init()' method",
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
iwindow_init_set_transient_for( myIWindow *instance, sIWindow *sdata )
{
	GtkWindow *parent;

	parent = my_iwindow_get_parent( instance );

	if( parent ){
		gtk_window_set_transient_for( GTK_WINDOW( instance ), parent );
	}
}

/**
 * my_iwindow_present:
 * @instance: this #myIWindow instance.
 *
 * Present this window, or a previous window with the same identifier,
 * for a non-modal user interaction.
 *
 * If a previous window with the same identifier is eventually found,
 * then this current @instance is closed, and the previous window is
 * displayed and returned instead.
 *
 * After the call, the @instance may so be invalid.
 *
 * Returns: the actually shown instance.
 *
 * As a reminder, application should not used a non-modal window when:
 * - it wants to wait for its termination
 * - or it wants the called function return a meaningful value.
 * Instead, non-modal window should:
 * - either be only used to display informations,
 * - or be self-contained, updating themselves their datas.
 *
 * A #myIWindow running as non-modal should have at least one button to
 * let the user terminate it:
 * - if it is a #GtkDialog, the probably most simple way is to
 *   #g_signal_connect( dialog, "response", G_CALLBACK( my_iwindow_close ), NULL );
 * - if this is not a #GtkDialog, an alternative may probably be
 *   #g_signal_connect_swapped( button, "clicked", G_CALLBACK( my_iwindow_close ), window ).
 *
 * A non-modal #myIWindow may want validate its content, and update its
 * repository accordingly (while still not returning any value to its
 * caller). The behavior is so:
 * - on cancel, close the window (most often)
 * - on ok, try to update and
 *   > close always
 *   > or close only if successful.
 * It is recommanded that the caller implements itself this behavior so
 * that it will be visually explicit to the maintainer.
 */
myIWindow *
my_iwindow_present( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_present";
	GList *it;
	sIWindow *sdata_instance, *sdata_other;
	myIWindow *other, *found, *prev;
	const gchar *instance_class, *other_class;

	g_debug( "%s: instance=%p (%s), st_live_list_count=%d",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), g_list_length( st_live_list ));

	g_return_val_if_fail( instance && MY_IS_IWINDOW( instance ), NULL );

	my_iwindow_init( instance );
	found = NULL;
	prev = NULL;
	sdata_instance = get_instance_data( instance );
	instance_class = G_OBJECT_TYPE_NAME( instance );

	for( it=st_live_list ; it ; it=it->next ){

		/* if we find the same instance, just break the search */
		other = MY_IWINDOW( it->data );
		if( other == instance ){
			found = other;
			break;
		}

		/* if we find another instance with same identifier, break the search too */
		sdata_other = get_instance_data( other );
		if( my_collate( sdata_instance->identifier, sdata_other->identifier ) == 0 ){
			found = other;
			break;

		/* check for another instance of same class */
		} else if( !prev ){
			other_class = G_OBJECT_TYPE_NAME( other );
			if( my_collate( instance_class, other_class ) == 0 ){
				prev = other;
			}
		}
	}

	/* we have :
	 * - either found this same instance
	 *   -> just display it (found)
	 * - either found another instance with same identifier
	 *   -> close the provided @instance, and display the 'other' one (found)
	 * - either not found anything relevant
	 *   -> record this instance and initialize and display it
	 *   -> position is shifted from 'prev'
	 */
	if( found ){
		if( found != instance ){
			do_close( instance );
		}

	} else {
		g_return_val_if_fail( !g_list_find( st_live_list, instance ), NULL );
		st_live_list = g_list_prepend( st_live_list, instance );
		found = instance;
		if( prev ){
			position_shift( instance, prev );
		}
		dump_live_list();
	}

	g_debug( "%s: presenting %p (%s)", thisfn, ( void * ) found, G_OBJECT_TYPE_NAME( found ));
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
	static const gchar *thisfn = "my_iwindow_close";

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

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
	sIWindow *sdata;
	GList *it;
	gboolean has_closed_something;

	g_debug( "%s:", thisfn );

	/* first reset the closing indicator */
	for( it=st_live_list ; it ; it=it->next ){
		sdata = get_instance_data( MY_IWINDOW( it->data ));
		sdata->closed = FALSE;
	}

	/* then close all allowed window */
	has_closed_something = TRUE;
	while( has_closed_something ){
		has_closed_something = FALSE;

		for( it=st_live_list ; it ; it=it->next ){
			sdata = get_instance_data( MY_IWINDOW( it->data ));
			if( !sdata->closed ){
				sdata->closed = TRUE;
				do_close( MY_IWINDOW( it->data ));
				has_closed_something = TRUE;
				break;
			}
		}
	}
}

static gboolean
on_delete_event( GtkWidget *widget, GdkEvent *event, myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_on_delete_event";

	g_debug( "%s: widget=%p, event=%p, instance=%p",
			thisfn, ( void * ) widget, ( void * ) event, ( void * ) instance );

	do_close( instance );

	return( TRUE );
}

/*
 * This closes the GtkWindow without any user confirmation.
 *
 * Openbook actually manages three ways of closing a window:
 * - closing and destroying it (the most common);
 * - hiding it (eg. ofaAccountSelect, ofaOpeTemplateSelect);
 * - doing nothing (eg. ofaExerciceCloseAssistant, ofaRestoreAssistant).
 *
 * The 'allow_close' indicator only manages the first and last
 * items. Hding a window must be explicitely done in the actual
 * application code.
 */
static void
do_close( myIWindow *instance )
{
	static const gchar *thisfn = "my_iwindow_do_close";
	sIWindow *sdata;

	sdata = get_instance_data( instance );

	if( sdata->allow_close ){
		g_debug( "%s: allow_close=%s, destroying widget=%p (%s)",
				thisfn, sdata->allow_close ? "True":"False",
				( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		if( sdata->manage_geometry ){
			position_save( instance, sdata );
		}

		gtk_widget_destroy( GTK_WIDGET( instance ));
	}
}

/*
 * Save/restore the size and position for each identified myIWindow.
 * Have a default to ClassName
 */
static void
position_restore( myIWindow *instance, sIWindow *sdata )
{
	gchar *key_prefix;

	if( sdata->geometry_settings ){
		key_prefix = g_strdup( sdata->geometry_key );

		if( !my_utils_window_position_get_has_pos( sdata->geometry_settings, key_prefix )){
			g_free( key_prefix );
			key_prefix = get_default_geometry_key( instance );
		}

		my_utils_window_position_restore( GTK_WINDOW( instance ), sdata->geometry_settings, key_prefix );
		g_free( key_prefix );
	}
}

static void
position_save( myIWindow *instance, sIWindow *sdata )
{
	gchar *key_prefix, *default_key;

	if( sdata->geometry_settings ){
		key_prefix = g_strdup( sdata->geometry_key );
		my_utils_window_position_save( GTK_WINDOW( instance ), sdata->geometry_settings, key_prefix );
		g_free( key_prefix );

		default_key = get_default_geometry_key( instance );
		if( !my_utils_window_position_get_has_pos( sdata->geometry_settings, default_key )){
			my_utils_window_position_save( GTK_WINDOW( instance ), sdata->geometry_settings, default_key );
		}
		g_free( default_key );
	}
}

/*
 * When opening a new window of the same class that already displayed,
 * just position the new position with a shift from the previous one.
 */
static void
position_shift( myIWindow *instance, myIWindow *prev )
{
	gint x, y;

	gtk_window_get_position( GTK_WINDOW( prev ), &x, &y );
	gtk_window_move( GTK_WINDOW( instance ), x+IWINDOW_SHIFT, y+IWINDOW_SHIFT );
}

static gchar *
get_default_identifier( const myIWindow *instance )
{
	return( g_strdup( G_OBJECT_TYPE_NAME( instance )));
}

static gchar *
get_default_geometry_key( const myIWindow *instance )
{
	return( g_strdup( G_OBJECT_TYPE_NAME( instance )));
}

static sIWindow *
get_instance_data( const myIWindow *instance )
{
	sIWindow *sdata;

	sdata = ( sIWindow * ) g_object_get_data( G_OBJECT( instance ), IWINDOW_DATA );

	if( !sdata ){
		sdata = g_new0( sIWindow, 1 );
		g_object_set_data( G_OBJECT( instance ), IWINDOW_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		sdata->parent = NULL;
		sdata->identifier = get_default_identifier( instance );
		sdata->geometry_settings = NULL;
		sdata->geometry_key = get_default_geometry_key( instance );
		sdata->manage_geometry = TRUE;
		sdata->allow_transient = TRUE;
		sdata->allow_close = TRUE;
		sdata->initialized = FALSE;
	}

	return( sdata );
}

static void
on_instance_finalized( sIWindow *sdata, GObject *finalized_iwindow )
{
	static const gchar *thisfn = "my_iwindow_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_iwindow=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_iwindow );

	g_free( sdata->identifier );
	g_free( sdata->geometry_key );
	g_free( sdata );

	st_live_list = g_list_remove( st_live_list, finalized_iwindow );

	dump_live_list();
}

static void
dump_live_list( void )
{
	static const gchar *thisfn = "my_iwindow_dump_live_list";
	GList *it;

	g_debug( "%s: st_live_list=%p", thisfn, ( void * ) st_live_list );
	for( it=st_live_list ; it ; it=it->next ){
		g_debug( "%s: it->data=%p (%s)", thisfn, it->data, G_OBJECT_TYPE_NAME( it->data ));
	}
}
