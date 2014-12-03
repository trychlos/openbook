/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/my-progress-bar.h"

/* private instance data
 */
struct _myProgressBarPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkContainer   *container;
	GtkProgressBar *bar;
};

/* signals defined here
 */
enum {
	DOUBLE = 0,
	TEXT,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( myProgressBar, my_progress_bar, G_TYPE_OBJECT )

static void on_parent_finalized( myProgressBar *self, gpointer finalized_parent );
static void on_double( myProgressBar *self, gdouble progress, void *empty );
static void on_text( myProgressBar *self, const gchar *text, void *empty );

static void
progress_bar_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_progress_bar_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_PROGRESS_BAR( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_progress_bar_parent_class )->finalize( instance );
}

static void
progress_bar_dispose( GObject *instance )
{
	myProgressBarPrivate *priv;

	g_return_if_fail( instance && MY_IS_PROGRESS_BAR( instance ));

	priv = ( MY_PROGRESS_BAR( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_progress_bar_parent_class )->dispose( instance );
}

static void
my_progress_bar_init( myProgressBar *self )
{
	static const gchar *thisfn = "my_progress_bar_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, MY_TYPE_PROGRESS_BAR, myProgressBarPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
my_progress_bar_class_init( myProgressBarClass *klass )
{
	static const gchar *thisfn = "my_progress_bar_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = progress_bar_dispose;
	G_OBJECT_CLASS( klass )->finalize = progress_bar_finalize;

	g_type_class_add_private( klass, sizeof( myProgressBarPrivate ));

	/**
	 * myProgressBar::double:
	 *
	 * This signal may be sent to make the bar progress.
	 *
	 * Arguments is a double between 0.0 and 1.0.
	 *
	 * Handler is of type:
	 * void ( *handler )( myProgressBar *bar,
	 * 						gdouble      progress,
	 * 						gpointer     user_data );
	 */
	st_signals[ DOUBLE ] = g_signal_new_class_handler(
				"double",
				MY_TYPE_PROGRESS_BAR,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_DOUBLE );

	/**
	 * myProgressBar::text:
	 *
	 * This signal may be sent to display a text in the bar.
	 *
	 * Arguments is the text to be displayed.
	 *
	 * Handler is of type:
	 * void ( *handler )( myProgressBar *bar,
	 * 						const gchar *text,
	 * 						gpointer     user_data );
	 */
	st_signals[ TEXT ] = g_signal_new_class_handler(
				"text",
				MY_TYPE_PROGRESS_BAR,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_DOUBLE );
}

static void
on_parent_finalized( myProgressBar *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "my_progress_bar_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	g_object_unref( self );
}

/**
 * my_progress_bar_new:
 */
myProgressBar *
my_progress_bar_new( void )
{
	myProgressBar *self;

	self = g_object_new( MY_TYPE_PROGRESS_BAR, NULL );

	return( self );
}

/**
 * my_progress_bar_attach_to:
 */
void
my_progress_bar_attach_to( myProgressBar *self, GtkContainer *new_parent )
{
	myProgressBarPrivate *priv;
	GtkWidget *box;

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( new_parent ), ( GWeakNotify ) on_parent_finalized, self );

		box = gtk_progress_bar_new();
		gtk_container_add( new_parent, box );
		priv->bar = GTK_PROGRESS_BAR( box );

		g_signal_connect( G_OBJECT( self ), "double", G_CALLBACK( on_double ), NULL );
		g_signal_connect( G_OBJECT( self ), "text", G_CALLBACK( on_text ), NULL );

		gtk_widget_show_all( GTK_WIDGET( new_parent ));
	}
}

static void
on_double( myProgressBar *self, gdouble progress, void *empty )
{
	myProgressBarPrivate *priv;

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	priv = self->priv;

	gtk_progress_bar_set_fraction( priv->bar, progress );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

static void
on_text( myProgressBar *self, const gchar *text, void *empty )
{
	myProgressBarPrivate *priv;

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	priv = self->priv;

	gtk_progress_bar_set_show_text( priv->bar, TRUE );
	gtk_progress_bar_set_text( priv->bar, text );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}
