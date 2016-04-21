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

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
}
	myDndBookPrivate;

static void idnd_detach_iface_init( myIDndDetachInterface *iface );
static void on_page_added( myDndBook *book, GtkWidget *child, guint page_num, void *empty );
static void dnd_book_drag_begin( GtkWidget *self, GdkDragContext *context );
static void dnd_book_drag_end( GtkWidget *self, GdkDragContext *context );

G_DEFINE_TYPE_EXTENDED( myDndBook, my_dnd_book, GTK_TYPE_NOTEBOOK, 0,
		G_ADD_PRIVATE( myDndBook )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDND_DETACH, idnd_detach_iface_init ))

static void
dnd_book_finalize( GObject *object )
{
	static const gchar *thisfn = "my_dnd_book_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_DND_BOOK( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_book_parent_class )->finalize( object );
}

static void
dnd_book_dispose( GObject *object )
{
	myDndBookPrivate *priv;

	g_return_if_fail( object && MY_IS_DND_BOOK( object ));

	priv = my_dnd_book_get_instance_private( MY_DND_BOOK( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dnd_book_parent_class )->dispose( object );
}

static void
my_dnd_book_init( myDndBook *self )
{
	static const gchar *thisfn = "my_dnd_book_init";
	myDndBookPrivate *priv;

	g_return_if_fail( self && MY_IS_DND_BOOK( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = my_dnd_book_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_dnd_book_class_init( myDndBookClass *klass )
{
	static const gchar *thisfn = "my_dnd_book_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dnd_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = dnd_book_finalize;

	GTK_WIDGET_CLASS( klass )->drag_begin = dnd_book_drag_begin;
	GTK_WIDGET_CLASS( klass )->drag_end = dnd_book_drag_end;
}

/*
 * myIDndDetach interface management
 */
static void
idnd_detach_iface_init( myIDndDetachInterface *iface )
{
	static const gchar *thisfn = "my_dnd_book_idnd_detach_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/**
 * my_dnd_book_new:
 *
 * Returns: a new #myDndBook object which should be g_object_unref()
 * by the caller.
 */
myDndBook *
my_dnd_book_new( void )
{
	myDndBook *book;

	book = g_object_new( MY_TYPE_DND_BOOK, NULL );

	g_signal_connect( book, "page-added", G_CALLBACK( on_page_added ), NULL );

	return( book );
}

/*
 * We want intercept the 'button_press_event' signal when the user
 * clicks on the tab of the page.
 *
 * As the 'button_press_event' signal is connected to the page's tab,
 * there is nothing to do when the page is removed (because the page's
 * tab is destroyed).
 */
static void
on_page_added( myDndBook *book, GtkWidget *child, guint page_num, void *empty )
{
	gtk_notebook_set_tab_detachable( GTK_NOTEBOOK( book ), child, TRUE );
#if 0
	GtkWidget *tab_label;

	/* set the new page as a source for dnd */
	tab_label = gtk_notebook_get_tab_label( GTK_NOTEBOOK( book ), child );
	my_idnd_detach_set_source_widget( MY_IDND_DETACH( book ), child, tab_label );
#endif
}

static void
dnd_book_drag_begin( GtkWidget *self, GdkDragContext *context )
{
	g_debug( "dnd_book_drag_begin: self=%p", self );
	gint page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( self ));
	GtkWidget *page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self ), page_n );
	g_debug( "page_n=%d, page_w=%p (%s)", page_n, page_w, G_OBJECT_TYPE_NAME( page_w ));
}

static void
dnd_book_drag_end( GtkWidget *self, GdkDragContext *context )
{
	g_debug( "dnd_book_drag_end: self=%p", self );
}
