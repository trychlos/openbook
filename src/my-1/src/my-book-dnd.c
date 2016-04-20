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

#include "my/my-book-dnd.h"
#include "my/my-ibook-detach.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
}
	myBookDndPrivate;

static void     ibook_detach_iface_init( myIBookDetachInterface *iface );
static void     on_page_added( myBookDnd *book, GtkWidget *child, guint page_num, void *empty );

G_DEFINE_TYPE_EXTENDED( myBookDnd, my_book_dnd, GTK_TYPE_NOTEBOOK, 0,
		G_ADD_PRIVATE( myBookDnd )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBOOK_DETACH, ibook_detach_iface_init ))

static void
book_dnd_finalize( GObject *object )
{
	static const gchar *thisfn = "my_book_dnd_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_BOOK_DND( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_book_dnd_parent_class )->finalize( object );
}

static void
book_dnd_dispose( GObject *object )
{
	myBookDndPrivate *priv;

	g_return_if_fail( object && MY_IS_BOOK_DND( object ));

	priv = my_book_dnd_get_instance_private( MY_BOOK_DND( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_book_dnd_parent_class )->dispose( object );
}

static void
my_book_dnd_init( myBookDnd *self )
{
	static const gchar *thisfn = "my_book_dnd_init";
	myBookDndPrivate *priv;

	g_return_if_fail( self && MY_IS_BOOK_DND( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = my_book_dnd_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_book_dnd_class_init( myBookDndClass *klass )
{
	static const gchar *thisfn = "my_book_dnd_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = book_dnd_dispose;
	G_OBJECT_CLASS( klass )->finalize = book_dnd_finalize;
}

/*
 * myIBookDetach interface management
 */
static void
ibook_detach_iface_init( myIBookDetachInterface *iface )
{
	static const gchar *thisfn = "my_book_dnd_ibook_detach_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/**
 * my_book_dnd_new:
 *
 * Returns: a new #myBookDnd object which should be g_object_unref()
 * by the caller.
 */
myBookDnd *
my_book_dnd_new( void )
{
	myBookDnd *book;

	book = g_object_new( MY_TYPE_BOOK_DND, NULL );

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
on_page_added( myBookDnd *book, GtkWidget *child, guint page_num, void *empty )
{
	GtkWidget *tab_label;

	tab_label = gtk_notebook_get_tab_label( GTK_NOTEBOOK( book ), child );

	my_ibook_detach_set_source_widget( MY_IBOOK_DETACH( book ), tab_label );
}
