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

#include "my/my-dnd-common.h"
#include "my/my-dnd-window.h"
#include "my/my-iwindow.h"
#include "my/my-tab.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean   dispose_has_run;

	GtkNotebook *source_book;
	gchar       *title;
	GtkWidget   *top_widget;
}
	myDndWindowPrivate;

static GtkTargetEntry dnd_format[] = {
	{ MY_DND_TARGET, 0, 0 },
};

static void     iwindow_iface_init( myIWindowInterface *iface );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     iwindow_init( myIWindow *instance );
static void     iwindow_get_default_size( myIWindow *instance, gint *x, gint *y, gint *cx, gint *cy );
static void     set_title( myDndWindow *self, const gchar *title );
static void     set_content( myDndWindow *self );
static gboolean dnd_window_drag_motion( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time );
static void     dnd_window_drag_leave( GtkWidget *widget, GdkDragContext *context, guint time );
static gboolean on_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty );
static gboolean dnd_window_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time );
static void     dnd_window_drag_data_received( GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time );

G_DEFINE_TYPE_EXTENDED( myDndWindow, my_dnd_window, GTK_TYPE_WINDOW, 0,
		G_ADD_PRIVATE( myDndWindow )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init ))

static void
nomodal_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_dnd_window_finalize";
	myDndWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DND_WINDOW( instance ));

	/* free data members here */
	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	g_free( priv->title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_window_parent_class )->finalize( instance );
}

static void
nomodal_page_dispose( GObject *instance )
{
	myDndWindowPrivate *priv;

	g_return_if_fail( instance && MY_IS_DND_WINDOW( instance ));

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_window_parent_class )->dispose( instance );
}

static void
my_dnd_window_init( myDndWindow *self )
{
	static const gchar *thisfn = "my_dnd_window_init";
	myDndWindowPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_DND_WINDOW( self ));

	priv = my_dnd_window_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_drag_dest_set( GTK_WIDGET( self ), 0, dnd_format, G_N_ELEMENTS( dnd_format ), GDK_ACTION_MOVE );

	g_signal_connect( self, "drag-drop", G_CALLBACK( on_drag_drop ), NULL );
}

static void
my_dnd_window_class_init( myDndWindowClass *klass )
{
	static const gchar *thisfn = "my_dnd_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = nomodal_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = nomodal_page_finalize;

	GTK_WIDGET_CLASS( klass )->drag_motion = dnd_window_drag_motion;
	GTK_WIDGET_CLASS( klass )->drag_leave = dnd_window_drag_leave;
	GTK_WIDGET_CLASS( klass )->drag_drop = dnd_window_drag_drop;
	GTK_WIDGET_CLASS( klass )->drag_data_received = dnd_window_drag_data_received;
}

/**
 * my_dnd_window_new:
 * @book: the source #GtkNotebook.
 * @page: the being-dragged page.
 *
 * Creates a #myDndWindow non-modal window.
 * Configure it as DnD target.
 */
myDndWindow *
my_dnd_window_new( GtkNotebook *book, GtkWidget *page )
{
	myDndWindow *window;
	myDndWindowPrivate *priv;
	GtkWindow *parent;
	GtkWidget *tab;

	g_return_val_if_fail( book && GTK_IS_NOTEBOOK( book ), NULL );
	g_return_val_if_fail( page && GTK_IS_WIDGET( page ), NULL );

	window = g_object_new( MY_TYPE_DND_WINDOW, NULL );

	priv = my_dnd_window_get_instance_private( window );

	parent = ( GtkWindow * ) gtk_widget_get_toplevel( GTK_WIDGET( book ));
	my_iwindow_set_parent( MY_IWINDOW( window ), parent );

	my_iwindow_set_restore_pos( MY_IWINDOW( window ), FALSE );

	tab = gtk_notebook_get_tab_label( book, page );
	if( MY_IS_TAB( tab )){
		set_title( window, my_tab_get_label( MY_TAB( tab )));
	} else {
		set_title( window, gtk_notebook_get_tab_label_text( book, page ));
	}

	priv->source_book = book;
	priv->top_widget = page;

	my_iwindow_init( MY_IWINDOW( window ));

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

	iface->get_identifier = iwindow_get_identifier;
	iface->init = iwindow_init;
	iface->get_default_size = iwindow_get_default_size;
}

static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	myDndWindowPrivate *priv;
	const gchar *cstr;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	cstr = G_OBJECT_TYPE_NAME( priv->top_widget );

	return( g_strdup( cstr ));
}

static void
iwindow_init( myIWindow *instance )
{
	myDndWindowPrivate *priv;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	gtk_window_set_title( GTK_WINDOW( instance ), priv->title );
	gtk_window_set_resizable( GTK_WINDOW( instance ), TRUE );
	gtk_window_set_modal( GTK_WINDOW( instance ), FALSE );
}

/*
 * we set the default size and position to 75% of those of the main window
 * so that we are sure they are suitable for the page
 */
static void
iwindow_get_default_size( myIWindow *instance, gint *x, gint *y, gint *cx, gint *cy )
{
	GtkWindow *parent;
	gint mw_width, mw_height;

	*x = 0;
	*y = 0;
	*cx = 0;
	*cy = 0;
	parent = my_iwindow_get_parent( instance );
	if( parent && GTK_IS_WINDOW( parent )){
		gtk_window_get_size( parent, &mw_width, &mw_height );
		*cx = mw_width * 3/4;
		*cy = mw_height * 3/4;
	}
}

static void
set_title( myDndWindow *self, const gchar *title )
{
	myDndWindowPrivate *priv;

	priv = my_dnd_window_get_instance_private( self );

	g_free( priv->title );
	priv->title = g_strdup( title );
}

static void
set_content( myDndWindow *self )
{
	myDndWindowPrivate *priv;
	gint page_n;

	priv = my_dnd_window_get_instance_private( self );

	g_object_ref( priv->top_widget );
	page_n = gtk_notebook_page_num( priv->source_book, priv->top_widget );
	gtk_notebook_remove_page( priv->source_book, page_n );
	gtk_container_add( GTK_CONTAINER( self ), priv->top_widget );
	g_object_unref( priv->top_widget );
}

static gboolean
dnd_window_drag_motion( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time )
{
	GdkAtom op_target, expected_target;

	op_target = gtk_drag_dest_find_target( widget, context, NULL );
	expected_target = gdk_atom_intern_static_string( MY_DND_TARGET );

	if( op_target != expected_target ){
		//g_debug( "dnd_window_drag_motion: False" );
		return( FALSE );
	}

	//g_debug( "dnd_window_drag_motion: True" );

	gdk_drag_status( context, GDK_ACTION_MOVE, time );

	return( TRUE );
}

static void
dnd_window_drag_leave( GtkWidget *widget, GdkDragContext *context, guint time )
{
	//g_debug( "dnd_window_drag_leave" );

	/* this is needed in order the 'drag-drop' signal be emitted */
	my_iwindow_present( MY_IWINDOW( widget ));
}

static gboolean
on_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty )
{
	//g_debug( "on_drag_drop" );

	set_content( MY_DND_WINDOW( widget ));

	return( TRUE );
}

/*
 * useless here but defined to make sure GtkNotebook's one is not run
 */
static gboolean
dnd_window_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time )
{
	//g_debug( "dnd_window_drag_drop" );

	return( TRUE );
}

/*
 * useless here but defined to make sure GtkNotebook's one is not run
 */
static void
dnd_window_drag_data_received( GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time )
{
}
