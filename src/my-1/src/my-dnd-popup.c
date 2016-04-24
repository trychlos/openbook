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
#include "my/my-dnd-popup.h"
#include "my/my-dnd-window.h"
#include "my/my-iwindow.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	GtkWidget   *source_widget;
	gint         source_width;
	gint         source_height;
	cairo_t     *source_context;

	GtkWidget   *target_window;
}
	myDndPopupPrivate;

/* target format when source is a myDndBook child */
static const GtkTargetEntry st_dnd_format[] = {
	{ MY_DND_TARGET, 0, 0 },
};

typedef struct {
	GtkDragResult result;
	const gchar  *label;
}
	sResult;

static const sResult st_result[] = {
		{ GTK_DRAG_RESULT_SUCCESS,         "Success" },
		{ GTK_DRAG_RESULT_NO_TARGET,       "No target" },
		{ GTK_DRAG_RESULT_USER_CANCELLED,  "User cancelled" },
		{ GTK_DRAG_RESULT_TIMEOUT_EXPIRED, "Timeout expired" },
		{ GTK_DRAG_RESULT_GRAB_BROKEN,     "Grab broken" },
		{ 0 }
};

static void     setup_drag_icon( myDndPopup *self, GtkWidget *source );
static gboolean on_drag_icon_draw( GtkWidget *self, cairo_t *cr, void *empty );
static void     setup_target_window( myDndPopup *self );
static gboolean on_target_window_draw( GtkWidget *self, cairo_t *cr, void *empty );
static gboolean on_drag_motion( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty );
static void     on_drag_leave( GtkWidget *widget, GdkDragContext *context, guint time, void *empty );
static gboolean on_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty );
static void     on_drag_data_received( GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, myDndPopup *self );

G_DEFINE_TYPE_EXTENDED( myDndPopup, my_dnd_popup, GTK_TYPE_WINDOW, 0,
		G_ADD_PRIVATE( myDndPopup ))

static void
dnd_popup_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_dnd_popup_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DND_POPUP( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_popup_parent_class )->finalize( instance );
}

static void
dnd_popup_dispose( GObject *instance )
{
	myDndPopupPrivate *priv;

	g_return_if_fail( instance && MY_IS_DND_POPUP( instance ));

	priv = my_dnd_popup_get_instance_private( MY_DND_POPUP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		if( priv->source_context ){
			cairo_destroy( priv->source_context );
		}
		if( priv->target_window ){
			gtk_widget_destroy( priv->target_window );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_popup_parent_class )->dispose( instance );
}

static void
my_dnd_popup_init( myDndPopup *self )
{
	static const gchar *thisfn = "my_dnd_popup_init";
	myDndPopupPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_DND_POPUP( self ));

	priv = my_dnd_popup_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_dnd_popup_class_init( myDndPopupClass *klass )
{
	static const gchar *thisfn = "my_dnd_popup_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dnd_popup_dispose;
	G_OBJECT_CLASS( klass )->finalize = dnd_popup_finalize;
}

/**
 * my_dnd_popup_new:
 * @source: the being-dragged widget.
 *
 * Creates a #myDndPopup window, configuring it as DnD target.
 */
myDndPopup *
my_dnd_popup_new( GtkWidget *source )
{
	myDndPopup *window;

	/* just create a popup on which we are going to draw the page */
	/*
	window = g_object_new( MY_TYPE_DND_POPUP, NULL );
	gtk_window_set_decorated( GTK_WINDOW( window ), FALSE );
	gtk_widget_hide( GTK_WIDGET( window ));
	*/

	window = g_object_new( MY_TYPE_DND_POPUP,
					"type", GTK_WINDOW_POPUP,
					NULL );

	setup_drag_icon( window, source );
	setup_target_window( window );

	return( window );
}

static void
setup_drag_icon( myDndPopup *self, GtkWidget *source )
{
	myDndPopupPrivate *priv;
	GtkAllocation rc;

	priv = my_dnd_popup_get_instance_private( self );

	priv->source_widget = source;

	gtk_window_set_screen( GTK_WINDOW( self ), gtk_widget_get_screen( source ));
	gtk_widget_get_allocation( source, &rc );
	gtk_widget_set_size_request( GTK_WIDGET( self ), rc.width * MY_DND_POPUP_SCALE, rc.height * MY_DND_POPUP_SCALE );

	g_signal_connect( self, "draw", G_CALLBACK( on_drag_icon_draw ), NULL );

	priv->source_width = rc.width;
	priv->source_height = rc.height;
}

/*
 * Create a surface where we will ask to widget to paint ifself
 * Then scale this surface to the target cairo context
 */
static gboolean
on_drag_icon_draw( GtkWidget *self, cairo_t *cr, void *empty )
{
	myDndPopupPrivate *priv;
	GtkAllocation rc;
	cairo_surface_t *cr_surface, *src_surface;

	priv = my_dnd_popup_get_instance_private( MY_DND_POPUP( self ));

	if( !priv->source_context ){
		cr_surface = cairo_get_target( cr );
		gtk_widget_get_allocation( priv->source_widget, &rc );
		src_surface = cairo_surface_create_similar( cr_surface, CAIRO_CONTENT_COLOR_ALPHA, rc.width, rc.height );
		priv->source_context = cairo_create( src_surface );
		cairo_surface_destroy( src_surface );
		gtk_widget_draw( priv->source_widget, priv->source_context );
	}

	cairo_scale( cr, MY_DND_POPUP_SCALE, MY_DND_POPUP_SCALE );
	cairo_set_source_surface( cr, cairo_get_target( priv->source_context ), 0, 0 );
	cairo_paint( cr );

	gtk_widget_set_opacity( GTK_WIDGET( self ), 0.67 );

	return TRUE;
}

/*
 * an invisible window which covers all the screen and acts as the
 * destination window
 */
static void
setup_target_window( myDndPopup *self )
{
	myDndPopupPrivate *priv;
	GdkScreen *screen;
	gint width, height;

	priv = my_dnd_popup_get_instance_private( self );

	screen = gtk_widget_get_screen( priv->source_widget );
	width = gdk_screen_get_width( screen );
	height = gdk_screen_get_height( screen );

	priv->target_window = gtk_window_new( GTK_WINDOW_POPUP );
	gtk_window_move( GTK_WINDOW	( priv->target_window ), 0, 0 );
	gtk_window_resize( GTK_WINDOW( priv->target_window ), width, height );
	gtk_widget_show( priv->target_window );

	g_signal_connect( priv->target_window, "draw", G_CALLBACK( on_target_window_draw ), NULL );

	gtk_drag_dest_set( priv->target_window, 0, st_dnd_format, G_N_ELEMENTS( st_dnd_format ), GDK_ACTION_MOVE );

	g_signal_connect( priv->target_window, "drag-motion", G_CALLBACK( on_drag_motion ), NULL );
	g_signal_connect( priv->target_window, "drag-leave", G_CALLBACK( on_drag_leave ), NULL );
	g_signal_connect( priv->target_window, "drag-drop", G_CALLBACK( on_drag_drop ), NULL );
	g_signal_connect( priv->target_window, "drag-data-received", G_CALLBACK( on_drag_data_received ), self );
}

static gboolean
on_target_window_draw( GtkWidget *self, cairo_t *cr, void *empty )
{
	gtk_widget_set_opacity( GTK_WIDGET( self ), 0 );

	return TRUE;
}

/*
 * Returns: %TRUE if a drop zone
 */
static gboolean
on_drag_motion( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty )
{
	static const gchar *thisfn = "my_dnd_popup_on_drag_motion";
	GdkAtom op_target, expected_target;

	op_target = gtk_drag_dest_find_target( widget, context, NULL );
	expected_target = gdk_atom_intern_static_string( MY_DND_TARGET );

	if( op_target != expected_target ){
		g_debug( "%s: unexpected target, returning False", thisfn );
		return( FALSE );
	}

	gdk_drag_status( context, GDK_ACTION_MOVE, time );

	return( TRUE );
}

static void
on_drag_leave( GtkWidget *widget, GdkDragContext *context, guint time, void *empty )
{
}

/*
 * @x, @y: coordinates relative to the top-left corner of the destination
 *  window.
 */
static gboolean
on_drag_drop( GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, void *empty )
{
	static const gchar *thisfn = "my_dnd_popup_on_drag_drop";
	GdkAtom op_target, expected_target;

	op_target = gtk_drag_dest_find_target( widget, context, NULL );
	expected_target = gdk_atom_intern_static_string( MY_DND_TARGET );

	if( op_target != expected_target ){
		g_debug( "%s: unexpected target, returning False", thisfn );
		return( FALSE );
	}

	gtk_drag_get_data( widget, context, op_target, time  );

	return( TRUE );
}

static void
on_drag_data_received( GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time, myDndPopup *self )
{
	myDndPopupPrivate *priv;
	myDndData **sdata;
	myDndWindow *window;

	priv = my_dnd_popup_get_instance_private( MY_DND_POPUP( self ));

	sdata = ( myDndData ** ) gtk_selection_data_get_data( data );

	window = my_dnd_window_new(( *sdata )->page, ( *sdata )->title, x, y, priv->source_width, priv->source_height );
	my_iwindow_set_parent( MY_IWINDOW( window ), ( *sdata )->parent );
	my_iwindow_set_restore_pos( MY_IWINDOW( window ), FALSE );
	my_iwindow_set_restore_size( MY_IWINDOW( window ), FALSE );
	my_iwindow_present( MY_IWINDOW( window ));

	g_object_unref(( *sdata )->page );
	g_free(( *sdata )->title );
	g_free( *sdata );

	gtk_drag_finish( context, TRUE, FALSE, time );
}

/**
 * my_dnd_popup_get_result_label:
 * @result: a #GtkDragResult result.
 *
 * Returns: the label associated with the @result.
 */
const gchar *
my_dnd_popup_get_result_label( GtkDragResult result )
{
	gint i;

	for( i=0 ; st_result[i].label ; ++i ){
		if( st_result[i].result == result ){
			return( st_result[i].label );
		}
	}

	return( "Unspecified error" );
}
