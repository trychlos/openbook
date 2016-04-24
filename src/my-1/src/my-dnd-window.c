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
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	gchar   *class_name;
}
	myDndWindowPrivate;

static GList *st_list                   = NULL;

static void     iwindow_iface_init( myIWindowInterface *iface );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     iwindow_init( myIWindow *instance );

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

	g_free( priv->class_name );

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
	myDndWindow *window;
	myDndWindowPrivate *priv;

	/* just create a popup on which we are going to draw the page */
	window = g_object_new( MY_TYPE_DND_WINDOW, NULL );
	priv = my_dnd_window_get_instance_private( window );

	gtk_container_add( GTK_CONTAINER( window ), widget );
	priv->class_name = g_strdup( G_OBJECT_TYPE_NAME( widget ));

	gtk_window_set_title( GTK_WINDOW( window ), title );
	gtk_window_move( GTK_WINDOW( window ), x-MY_DND_SHIFT, y-MY_DND_SHIFT );
	gtk_window_resize( GTK_WINDOW( window ), 0.9*width, 0.9*height );

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
}

static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	myDndWindowPrivate *priv;

	priv = my_dnd_window_get_instance_private( MY_DND_WINDOW( instance ));

	return( g_strdup( priv->class_name ));
}

static void
iwindow_init( myIWindow *instance )
{
	gtk_window_set_resizable( GTK_WINDOW( instance ), TRUE );
	gtk_window_set_modal( GTK_WINDOW( instance ), FALSE );
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
	const gchar *ctype_name;

	ctype_name = g_type_name( type );

	g_debug( "%s: type=%lu, class_name=%s", thisfn, type, ctype_name );

	for( it=st_list ; it ; it=it->next ){
		window = MY_DND_WINDOW( it->data );
		priv = my_dnd_window_get_instance_private( window );
		if( !my_collate( priv->class_name, ctype_name )){
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
