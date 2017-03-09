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

#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-page.h"

#include "ui/ofa-nomodal-page.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	GtkWindow  *parent;
	gchar      *title;
	GtkWidget  *top_widget;				/* is also an ofaPage */
}
	ofaNomodalPagePrivate;

static GList *st_list                   = NULL;

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void on_finalized_page( void *empty, GObject *finalized_page );

G_DEFINE_TYPE_EXTENDED( ofaNomodalPage, ofa_nomodal_page, GTK_TYPE_WINDOW, 0,
		G_ADD_PRIVATE( ofaNomodalPage ) \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init ))

static void
nomodal_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_nomodal_page_finalize";
	ofaNomodalPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_NOMODAL_PAGE( instance ));

	/* free data members here */
	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	g_free( priv->title );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_nomodal_page_parent_class )->finalize( instance );
}

static void
nomodal_page_dispose( GObject *instance )
{
	ofaNomodalPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_NOMODAL_PAGE( instance ));

	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_nomodal_page_parent_class )->dispose( instance );
}

static void
ofa_nomodal_page_init( ofaNomodalPage *self )
{
	static const gchar *thisfn = "ofa_nomodal_page_init";
	ofaNomodalPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_NOMODAL_PAGE( self ));

	priv = ofa_nomodal_page_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_nomodal_page_class_init( ofaNomodalPageClass *klass )
{
	static const gchar *thisfn = "ofa_nomodal_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = nomodal_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = nomodal_page_finalize;
}

/**
 * ofa_nomodal_page_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @title: the title of the window.
 * @page: the #GtkWidget top widget.
 *
 * Creates or represents a #ofaNomodalPage non-modal window.
 */
void
ofa_nomodal_page_run( ofaIGetter *getter, GtkWindow *parent, const gchar *title, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_nomodal_page_run";
	ofaNomodalPage *self;
	ofaNomodalPagePrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, title=%s, page=%p",
			thisfn, ( void * ) getter, ( void * ) parent, title, ( void * ) page );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( my_strlen( title ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	self = g_object_new( OFA_TYPE_NOMODAL_PAGE,
				"type", GTK_WINDOW_TOPLEVEL,
				NULL );

	priv = ofa_nomodal_page_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->title = g_strdup( title );
	priv->top_widget = page;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));

	if( MY_IS_IWINDOW( self )){
		st_list = g_list_prepend( st_list, self );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_finalized_page, NULL );
	}
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_nomodal_page_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	ofaNomodalPagePrivate *priv;
	gint x, y, width, height;

	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
	my_iwindow_set_identifier( instance, G_OBJECT_TYPE_NAME( priv->top_widget ));
	my_iwindow_set_manage_geometry( instance, FALSE );
	my_iwindow_set_allow_transient( instance, FALSE );

	gtk_window_set_title( GTK_WINDOW( instance ), priv->title );
	gtk_window_set_resizable( GTK_WINDOW( instance ), TRUE );
	gtk_window_set_modal( GTK_WINDOW( instance ), FALSE );

	gtk_container_add( GTK_CONTAINER( instance ), priv->top_widget );

	/* we set the default size and position to those of the main window
	 * so that we are sure they are suitable for the page
	 */
	if( priv->parent ){
		gtk_window_get_position( priv->parent, &x, &y );
		gtk_window_move( GTK_WINDOW( instance ), x+100, y+100 );
		gtk_window_get_size( priv->parent, &width, &height );
		gtk_window_resize( GTK_WINDOW( instance ), width-150, height-150 );
	}
}

static void
on_finalized_page( void *empty, GObject *finalized_page )
{
	static const gchar *thisfn = "ofa_nomodal_page_on_finalized_page";

	g_debug( "%s: empty=%p, finalized_page=%p", thisfn, ( void * ) empty, ( void * ) finalized_page );

	st_list = g_list_remove( st_list, finalized_page );
}

/**
 * ofa_nomodal_page_present_by_type:
 * @type: the GType of the searched page.
 *
 * Returns: %TRUE if the page has been found.
 */
gboolean
ofa_nomodal_page_present_by_type( GType type )
{
	static const gchar *thisfn = "ofa_nomodal_page_present_by_type";
	ofaNomodalPage *page;
	ofaNomodalPagePrivate *priv;
	GList *it;

	g_debug( "%s: type=%lu", thisfn, type );

	for( it=st_list ; it ; it=it->next ){
		page = OFA_NOMODAL_PAGE( it->data );
		priv = ofa_nomodal_page_get_instance_private( page );
		if( G_OBJECT_TYPE( OFA_PAGE( priv->top_widget )) == type ){
			g_debug( "%s: found page=%p (ofaPage=%p (%s))",
					thisfn, ( void * ) page, ( void * ) priv->top_widget, G_OBJECT_TYPE_NAME( priv->top_widget ));
			my_iwindow_present( MY_IWINDOW( page ));
			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 * ofa_nomodal_page_close_all:
 *
 * Close all opened pages.
 */
void
ofa_nomodal_page_close_all( void )
{
	static const gchar *thisfn = "ofa_nomodal_page_close_all";

	g_debug( "%s:", thisfn );

	while( st_list ){
		gtk_widget_destroy( GTK_WIDGET( st_list->data ));
	}
}
