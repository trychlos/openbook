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

#include <my-1/my/my-dnd-book.h>
#include <my-1/my/my-idnd-detach.h>

#define IDND_DETACH_LAST_VERSION        1
#define IDND_DETACH_DATA               "my-idnd_detach-data"

/* a data structure attached to the source widget
 */
typedef struct {
	myIDndDetach *instance;
	GtkWidget     *page;
	gulong	       on_drag_end_handler;
}
	sDrag;


static GtkTargetEntry dnd_source_formats[] = {
	//{ "XdndOpenbookDetach", GTK_TARGET_SAME_APP, 0 },
	{ "XdndOpenbookDetach", 0, 0 },
};

static guint st_initializations = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( myIDndDetachInterface *klass );
static void     interface_base_finalize( myIDndDetachInterface *klass );
static gboolean on_button_press_event( GtkWidget *widget, GdkEventButton *event, sDrag *sdata );
static gboolean on_button_release_event( GtkWidget *widget, GdkEventButton *event, sDrag *sdata );
static void     on_drag_begin( GtkWidget *widget, GdkDragContext *context, sDrag *sdata );
static void     on_drag_end( GtkWidget *widget, GdkDragContext *context, sDrag *sdata );
static void     stop_drag_operation( GtkWidget *widget, sDrag *sdata );
static sDrag   *get_page_data( myIDndDetach *instance, GtkWidget *widget );

/**
 * my_idnd_detach_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_idnd_detach_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_idnd_detach_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_idnd_detach_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIDndDetachInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIDndDetach", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIDndDetachInterface *klass )
{
	static const gchar *thisfn = "my_idnd_detach_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIDndDetachInterface *klass )
{
	static const gchar *thisfn = "my_idnd_detach_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_idnd_detach_get_interface_last_version:
 * @instance: this #myIDndDetach instance.
 *
 * Returns: the last version number of this interface.
 */
guint
my_idnd_detach_get_interface_last_version( void )
{
	return( IDND_DETACH_LAST_VERSION );
}

/**
 * my_idnd_detach_get_interface_version:
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
my_idnd_detach_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IDND_DETACH );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIDndDetachInterface * ) iface )->get_interface_version ){
		version = (( myIDndDetachInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIDndDetach::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_idnd_detach_set_source_widget:
 * @instance: this #myIDndDetach instance.
 * @window: the page of the #GtkNotebook we are about do move.
 * @source: the #GtkWidget which is to be used by the user to detach
 *  the page (aka the tab label).
 *
 * Initialize @widget as a drag-and-drop source.
 * This is to be called on each page creation of the @instance notebook.
 *
 * Since: version 1.
 */
void
my_idnd_detach_set_source_widget( myIDndDetach *instance, GtkWidget *window, GtkWidget *source )
{
	static const gchar *thisfn = "my_idnd_detach_set_source_widget";
	sDrag *sdata;

	g_debug( "%s: instance=%p, window=%p, source=%p",
			thisfn, ( void * ) instance, ( void * ) window, ( void * ) source );

	g_return_if_fail( instance && MY_IS_IDND_DETACH( instance ));
	g_return_if_fail( window && GTK_IS_WIDGET( window ));
	g_return_if_fail( source && GTK_IS_WIDGET( source ));

	gtk_notebook_set_tab_detachable( GTK_NOTEBOOK( instance ), window, TRUE );

	gtk_widget_add_events( source,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );

	gtk_drag_source_set( source,
			GDK_BUTTON1_MASK, dnd_source_formats, G_N_ELEMENTS( dnd_source_formats ), GDK_ACTION_MOVE );

	sdata = get_page_data( instance, source );
	sdata->page = window;

	g_signal_connect( source, "button-press-event", G_CALLBACK( on_button_press_event ), sdata );
	g_signal_connect( source, "button-release-event", G_CALLBACK( on_button_release_event ), sdata );
	g_signal_connect( source, "drag-begin", G_CALLBACK( on_drag_begin ), sdata );
}

/*
 * Returns: %TRUE to stop other handlers from being invoked for the event.
 *  %FALSE to propagate the event further.
 *
 * Note: the 'motion-notify-event' signal (which requires
 * GDK_POINTER_MOTION_MASK) is only sent while the mouse pointer is
 * inside of the source @widget. It is not very useful in our case.
 */
static gboolean
on_button_press_event( GtkWidget *widget, GdkEventButton *event, sDrag *sdata )
{
	static const gchar *thisfn = "my_idnd_detach_on_button_press_event";
	GtkTargetList *target_list;
	GdkDragContext *context;

	g_debug( "%s", thisfn );

	if( 1 ){
	/* do not handle anything else than simple click */
	if( event->type != GDK_BUTTON_PRESS ){
		g_debug( "%s: returning False because not GDK_BUTTON_PRESS", thisfn );
		return( FALSE );
	}
	/* do not handle even simple click if any modifier is set */
	if( event->state != 0 ){
		g_debug( "%s: returning False because state=%d", thisfn, event->state );
		return( FALSE );
	}

	sdata->on_drag_end_handler =
					g_signal_connect( widget, "drag-end", G_CALLBACK( on_drag_end ), sdata );

	target_list = gtk_target_list_new( dnd_source_formats, G_N_ELEMENTS( dnd_source_formats ));

	context = gtk_drag_begin_with_coordinates(
					widget, target_list, GDK_ACTION_PRIVATE,
					event->button, ( GdkEvent * ) event, event->x, event->y );

	gtk_drag_set_icon_default( context );

	gtk_target_list_unref( target_list );

	return( TRUE );
	}
	return( FALSE );
}

/*
 * Returns: %TRUE to stop other handlers from being invoked for the event.
 *  %FALSE to propagate the event further.
 *
 * Note: the 'motion-notify-event' signal (which requires
 * GDK_POINTER_MOTION_MASK) is only sent while the mouse pointer is
 * inside of the @widget. It is not very useful in our case.
 */
static gboolean
on_button_release_event( GtkWidget *widget, GdkEventButton *event, sDrag *sdata )
{
	static const gchar *thisfn = "my_idnd_detach_on_button_release_event";

	g_debug( "%s", thisfn );

	return( FALSE );
}

static void
on_drag_begin( GtkWidget *widget, GdkDragContext *context, sDrag *sdata )
{
	g_debug( "on_drag_begin: widget=%p", ( void * ) widget );
}

static void
on_drag_end( GtkWidget *widget, GdkDragContext *context, sDrag *sdata )
{
	g_debug( "on_drag_end" );

	stop_drag_operation( widget, sdata );
}

static void
stop_drag_operation( GtkWidget *widget, sDrag *sdata )
{
	if( sdata->on_drag_end_handler ){
		g_signal_handler_disconnect( widget, sdata->on_drag_end_handler );
		sdata->on_drag_end_handler = 0;
	}
}

static sDrag *
get_page_data( myIDndDetach *instance, GtkWidget *widget )
{
	sDrag *sdata;

	sdata = ( sDrag * ) g_object_get_data( G_OBJECT( widget ), IDND_DETACH_DATA );

	if( !sdata ){
		sdata = g_new0( sDrag, 1 );
		g_object_set_data( G_OBJECT( widget ), IDND_DETACH_DATA, sdata );
		sdata->instance = instance;
	}

	return( sdata );
}
