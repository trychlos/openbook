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

#include "my/my-dnd-common.h"
#include "my/my-dnd-popup.h"
#include "my/my-dnd-window.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean   dispose_has_run;

	/* initialization
	 */
	GtkWidget *child_widget;				/* the widget detached from the notebook */
	gchar     *title;
	gint       x;
	gint       y;
	gint       width;
	gint       height;

	/* runtime
	 */
	GtkWidget *drag_popup;					/* the popup during re-attaching */
}
	myDndWindowPrivate;

static GList *st_list                       = NULL;

/* source format when destination is a myDndBook book */
static const GtkTargetEntry st_dnd_format[] = {
	{ MY_DND_TARGET, 0, 0 },
};

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void on_realize_cb( GtkWidget *widget, void *empty );
static void on_drag_begin( GtkWidget *self, GdkDragContext *context, void *empty );
static void on_drag_data_get( GtkWidget *self, GdkDragContext *context, GtkSelectionData *data, guint info, guint time, void *empty );

G_DEFINE_TYPE_EXTENDED( myDndWindow, my_dnd_window, GTK_TYPE_WINDOW, 0,
		G_ADD_PRIVATE( myDndWindow )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init ))

static void
dnd_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_dnd_window_finalize";
	myDndWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s), st_list_count=%d",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), g_list_length( st_list ));

	g_return_if_fail( instance && MY_IS_DND_WINDOW( instance ));

	/* free data members here */
	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	g_free( priv->title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_window_parent_class )->finalize( instance );
}

static void
dnd_window_dispose( GObject *instance )
{
	myDndWindowPrivate *priv;

	g_return_if_fail( instance && MY_IS_DND_WINDOW( instance ));

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		if( priv->drag_popup ){
			gtk_widget_destroy( priv->drag_popup );
		}

		st_list = g_list_remove( st_list, instance );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_window_parent_class )->dispose( instance );
}

static void
my_dnd_window_init( myDndWindow *self )
{
	static const gchar *thisfn = "my_dnd_window_init";
	myDndWindowPrivate *priv;

	g_debug( "%s: self=%p (%s), st_list_count=%d",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ), g_list_length( st_list ));

	g_return_if_fail( self && MY_IS_DND_WINDOW( self ));

	priv = my_dnd_window_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_drag_source_set( GTK_WIDGET( self ),
			GDK_BUTTON1_MASK, st_dnd_format, G_N_ELEMENTS( st_dnd_format ), GDK_ACTION_MOVE );

	g_signal_connect( self, "drag-begin", G_CALLBACK( on_drag_begin ), NULL );
	g_signal_connect( self, "drag-data-get", G_CALLBACK( on_drag_data_get ), NULL );

	if( !g_list_find( st_list, self )){
		st_list = g_list_prepend( st_list, self );
	}
}

static void
my_dnd_window_class_init( myDndWindowClass *klass )
{
	static const gchar *thisfn = "my_dnd_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dnd_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = dnd_window_finalize;
}

/**
 * my_dnd_window_new:
 * @page: the GtkWidget to be displayed.
 * @title: the window title.
 * @x, @y: the position of the window.
 * @width, @height: the size of the window.
 *
 * Creates a #myDndWindow non-modal window.
 */
myDndWindow *
my_dnd_window_new( GtkWidget *widget, const gchar *title, gint x, gint y, gint width, gint height )
{
	static const gchar *thisfn = "my_dnd_window_new";
	myDndWindow *window;
	myDndWindowPrivate *priv;

	g_debug( "%s: widget=%p (%s), title=%s, x=%d, y=%d, width=%d, height=%d",
			thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ), title, x, y, width, height );

	/* just create a popup on which we are going to draw the page */
	window = g_object_new( MY_TYPE_DND_WINDOW,
					"type", GTK_WINDOW_TOPLEVEL,
					NULL );

	priv = my_dnd_window_get_instance_private( window );

	priv->child_widget = widget;
	priv->title = g_strdup( title );
	priv->x = x;
	priv->y = y;
	priv->width = width;
	priv->height = height;

	return( window );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "my_dnd_window_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	myDndWindowPrivate *priv;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	my_iwindow_set_identifier( instance, G_OBJECT_TYPE_NAME( priv->child_widget ));
	my_iwindow_set_manage_geometry( instance, FALSE );

	gtk_window_set_resizable( GTK_WINDOW( instance ), TRUE );
	gtk_window_set_modal( GTK_WINDOW( instance ), FALSE );

	/* See https://gna.org/bugs/?24474
	 * which works around this same bug by hiding/showing the widget */
	gtk_widget_hide( priv->child_widget );

	gtk_container_add( GTK_CONTAINER( instance ), priv->child_widget );

	g_signal_connect( instance, "realize", G_CALLBACK( on_realize_cb ), NULL );
}

static void
on_realize_cb( GtkWidget *widget, void *empty )
{
	static const gchar *thisfn = "ofa_nommodal_page_on_realize_cb";
	myDndWindowPrivate *priv;

	g_debug( "%s: GdkWindow=%p, GdkParentWindow=%p",
			thisfn, gtk_widget_get_window( widget ), gtk_widget_get_parent_window( widget ));

	g_return_if_fail( widget && MY_IS_DND_WINDOW( widget ));

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( widget ));

	gtk_window_set_title( GTK_WINDOW( widget ), priv->title );
	gtk_window_move( GTK_WINDOW( widget ), priv->x-MY_DND_SHIFT, priv->y-MY_DND_SHIFT );
	gtk_window_resize( GTK_WINDOW( widget ), MY_DND_WINDOW_SCALE*priv->width, MY_DND_WINDOW_SCALE*priv->height );

	gtk_widget_show_all( priv->child_widget );
}

/**
 * my_dnd_window_present_by_type:
 * @type: the GType of the searched page.
 *
 * Returns: %TRUE if the page has been found.
 */
gboolean
my_dnd_window_present_by_type( GType type )
{
	static const gchar *thisfn = "my_dnd_window_present_by_type";
	myDndWindow *window;
	myDndWindowPrivate *priv;
	GList *it;

	g_debug( "%s: type=%lu, class=%s", thisfn, type, g_type_name( type ));

	for( it=st_list ; it ; it=it->next ){
		window = MY_DND_WINDOW( it->data );
		priv = my_dnd_window_get_instance_private( window );
		if( G_OBJECT_TYPE( priv->child_widget ) == type ){
			g_debug( "%s: found window=%p", thisfn, ( void * ) window );
			my_iwindow_present( MY_IWINDOW( window ));
			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 * my_dnd_window_close_all:
 *
 * Close all opened pages.
 */
void
my_dnd_window_close_all( void )
{
	static const gchar *thisfn = "my_dnd_window_close_all";

	g_debug( "%s:", thisfn );

	while( st_list ){
		gtk_widget_destroy( GTK_WIDGET( st_list->data ));
	}
}

static void
on_drag_begin( GtkWidget *self, GdkDragContext *context, void *empty )
{
	myDndWindowPrivate *priv;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( self ));

	priv->drag_popup = GTK_WIDGET( my_dnd_popup_new( priv->child_widget, FALSE ));
	gtk_drag_set_icon_widget( context, priv->drag_popup, MY_DND_SHIFT, MY_DND_SHIFT );
}

/*
 * get the data for re-attach the widget to the notebook
 */
static void
on_drag_data_get( GtkWidget *self, GdkDragContext *context, GtkSelectionData *data, guint info, guint time, void *empty )
{
	myDndWindowPrivate *priv;
	myDndData *sdata;
	GdkAtom data_target, expected_target;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( self ));

	data_target = gtk_selection_data_get_target( data );
	expected_target = gdk_atom_intern_static_string( MY_DND_TARGET );

	if( data_target == expected_target ){

		g_object_ref( priv->child_widget );
		gtk_container_remove( GTK_CONTAINER( self ), priv->child_widget );

		sdata = g_new0( myDndData, 1 );
		sdata->page = priv->child_widget;
		sdata->title = g_strdup( gtk_window_get_title( GTK_WINDOW( self )));

		gtk_selection_data_set( data, data_target, sizeof( gpointer ), ( void * ) &sdata, sizeof( gpointer ));

		my_iwindow_close( MY_IWINDOW( self ));
	}
}
