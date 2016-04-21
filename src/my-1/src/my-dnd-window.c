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

#include "my/my-iwindow.h"
#include <my-1/my/my-dnd-window.h>
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;
}
	myDndWindowPrivate;

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      iwindow_get_default_size( myIWindow *instance, guint *x, guint *y, guint *cx, guint *cy );

G_DEFINE_TYPE_EXTENDED( myDndWindow, my_dnd_window, GTK_TYPE_WINDOW, 0,
		G_ADD_PRIVATE( myDndWindow ) \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init ))

static void
nomodal_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_dnd_window_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DND_WINDOW( instance ));

	/* free data members here */

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
}

static void
my_dnd_window_class_init( myDndWindowClass *klass )
{
	static const gchar *thisfn = "my_dnd_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = nomodal_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = nomodal_page_finalize;
}

/**
 * my_dnd_window_new:
 * @source: a #GtkWindow source window.
 *
 * Creates a #myDndWindow non-modal window.
 * Configure it as DnD target.
 */
myDndWindow *
my_dnd_window_new( GtkWindow *source )
{
	myDndWindow *window;
	GtkWindow *parent;

	g_return_val_if_fail( source && GTK_IS_WINDOW( source ), NULL );

	parent = ( GtkWindow * ) gtk_widget_get_toplevel( GTK_WIDGET( source ));

	window = g_object_new( MY_TYPE_DND_WINDOW, NULL );
	my_iwindow_set_parent( MY_IWINDOW( window ), parent );

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

	iface->get_default_size = iwindow_get_default_size;
}

/*
 * we set the default size and position to those of the main window
 * so that we are sure they are suitable for the page
 */
static void
iwindow_get_default_size( myIWindow *instance, guint *x, guint *y, guint *cx, guint *cy )
{
	GtkWindow *parent;
	gint mw_x, mw_y, mw_width, mw_height;

	*x = 0;
	*y = 0;
	*cx = 0;
	*cy = 0;
	parent = my_iwindow_get_parent( instance );
	if( parent && GTK_IS_WINDOW( parent )){
		gtk_window_get_position( parent, &mw_x, &mw_y );
		gtk_window_get_size( parent, &mw_width, &mw_height );
		*x = mw_x + 100;
		*y = mw_y + 100;
		*cx = mw_width - 150;
		*cy = mw_height - 150;
	}
}
