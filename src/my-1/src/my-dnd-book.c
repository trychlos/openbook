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

#include "my/my-dnd-book.h"
#include "my/my-dnd-common.h"
#include "my/my-dnd-popup.h"
#include "my/my-tab.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	myDndPopup *drag_popup;			/* while detaching a page to a myDndWindow */
}
	myDndBookPrivate;

static const GtkTargetEntry st_dnd_format[] = {
	{ MY_DND_TARGET, 0, 0 },
};

static void     on_page_added( myDndBook *book, GtkWidget *child, guint page_num, void *empty );
static void     dnd_book_drag_begin( GtkWidget *self, GdkDragContext *context );
static void     dnd_book_drag_data_get( GtkWidget *self, GdkDragContext *context, GtkSelectionData *data, guint info, guint time );
static gboolean dnd_book_drag_failed( GtkWidget *self, GdkDragContext *context, GtkDragResult result );
static void     dnd_book_drag_end( GtkWidget *self, GdkDragContext *context );
static void     dnd_book_drag_data_delete( GtkWidget *self, GdkDragContext *context );

G_DEFINE_TYPE_EXTENDED( myDndBook, my_dnd_book, GTK_TYPE_NOTEBOOK, 0,
		G_ADD_PRIVATE( myDndBook ))

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

	priv->drag_popup = NULL;

	gtk_drag_source_set( GTK_WIDGET( self ),
			GDK_BUTTON1_MASK, st_dnd_format, G_N_ELEMENTS( st_dnd_format ), GDK_ACTION_MOVE );

	g_signal_connect( self, "page-added", G_CALLBACK( on_page_added ), NULL );
}

static void
my_dnd_book_class_init( myDndBookClass *klass )
{
	static const gchar *thisfn = "my_dnd_book_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dnd_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = dnd_book_finalize;

	/* override all drag virtuals else GtkNotebook will handle them */
	/* source side */
	GTK_WIDGET_CLASS( klass )->drag_begin = dnd_book_drag_begin;
	GTK_WIDGET_CLASS( klass )->drag_data_get = dnd_book_drag_data_get;
	GTK_WIDGET_CLASS( klass )->drag_failed = dnd_book_drag_failed;
	GTK_WIDGET_CLASS( klass )->drag_end = dnd_book_drag_end;
	GTK_WIDGET_CLASS( klass )->drag_data_delete = dnd_book_drag_data_delete;
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

	return( book );
}

/*
 * Make the page detacheable in order to take advantage of the GtkNotebook
 * DnD initialization. But all handlers are only ours.
 */
static void
on_page_added( myDndBook *book, GtkWidget *child, guint page_num, void *empty )
{
	gtk_notebook_set_tab_detachable( GTK_NOTEBOOK( book ), child, TRUE );
}

static void
dnd_book_drag_begin( GtkWidget *self, GdkDragContext *context )
{
	myDndBookPrivate *priv;
	gint page_n;
	GtkWidget *page_w;

	priv = my_dnd_book_get_instance_private( MY_DND_BOOK( self ));

	page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( self ));
	page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self ), page_n );
	priv->drag_popup = my_dnd_popup_new( page_w );
	gtk_drag_set_icon_widget( context, GTK_WIDGET( priv->drag_popup ), MY_DND_SHIFT, MY_DND_SHIFT );
}

static void
dnd_book_drag_data_get( GtkWidget *self, GdkDragContext *context, GtkSelectionData *data, guint info, guint time )
{
	myDndData *sdata;
	gint page_n;
	GtkWidget *page_w, *tab;
	gchar *title;
	GdkAtom data_target, expected_target;

	data_target = gtk_selection_data_get_target( data );
	expected_target = gdk_atom_intern_static_string( MY_DND_TARGET );

	if( data_target == expected_target ){
		page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( self ));
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self ), page_n );

		tab = gtk_notebook_get_tab_label( GTK_NOTEBOOK( self ), page_w );
		if( MY_IS_TAB( tab )){
			title = my_tab_get_label( MY_TAB( tab ));
		} else {
			title = g_strdup( gtk_notebook_get_tab_label_text( GTK_NOTEBOOK( self ), page_w ));
		}

		g_object_ref( page_w );
		gtk_notebook_remove_page( GTK_NOTEBOOK( self ), page_n );

		sdata = g_new0( myDndData, 1 );
		sdata->page = page_w;
		sdata->title = my_utils_str_remove_underlines( title );
		g_free( title );

		gtk_selection_data_set( data, data_target, sizeof( gpointer ), ( void * ) &sdata, sizeof( gpointer ));
	}
}

/*
 * Returns: %TRUE is the failure has been already handled (not showing
 * the default "drag operation failed" animation), otherwise it returns
 * %FALSE.
 */
static gboolean
dnd_book_drag_failed( GtkWidget *self, GdkDragContext *context, GtkDragResult result )
{
	static const gchar *thisfn = "my_dnd_book_drag_failed";
	const gchar *cstr;

	cstr = my_dnd_popup_get_result_label( result );

	g_debug( "%s: self=%p, context=%p, result=%d (%s)",
			thisfn, ( void * ) self, ( void * ) context, result, cstr );

	return( FALSE );
}

static void
dnd_book_drag_end( GtkWidget *self, GdkDragContext *context )
{
	static const gchar *thisfn = "my_dnd_book_drag_data_end";
	myDndBookPrivate *priv;

	priv = my_dnd_book_get_instance_private( MY_DND_BOOK( self ));

	g_debug( "%s: self=%p, context=%p", thisfn, ( void * ) self, ( void * ) context );

	if( priv->drag_popup ){
		gtk_widget_destroy( GTK_WIDGET( priv->drag_popup ));
		priv->drag_popup = NULL;
	}
}

/*
 * useless here but defined to make sure GtkNotebook's one is not run
 */
static void
dnd_book_drag_data_delete( GtkWidget *self, GdkDragContext *context )
{
	static const gchar *thisfn = "my_dnd_book_drag_data_delete";

	g_debug( "%s: self=%p, context=%p", thisfn, ( void * ) self, ( void * ) context );
}
