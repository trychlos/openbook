/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-progress-bar.h"
#include "my/my-style.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;
}
	myProgressBarPrivate;

/* signals defined here
 */
enum {
	DOUBLE = 0,
	TEXT,
	PULSE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void on_double( myProgressBar *self, gdouble progress, void *empty );
static void on_text( myProgressBar *self, const gchar *text, void *empty );
static void on_pulse( myProgressBar *self, void *empty );

G_DEFINE_TYPE_EXTENDED( myProgressBar, my_progress_bar, GTK_TYPE_PROGRESS_BAR, 0,
		G_ADD_PRIVATE( myProgressBar ))

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

	priv = my_progress_bar_get_instance_private( MY_PROGRESS_BAR( instance ));

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
	myProgressBarPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	priv = my_progress_bar_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_progress_bar_class_init( myProgressBarClass *klass )
{
	static const gchar *thisfn = "my_progress_bar_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = progress_bar_dispose;
	G_OBJECT_CLASS( klass )->finalize = progress_bar_finalize;

	/**
	 * myProgressBar::my-double:
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
				"my-double",
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
	 * myProgressBar::my-text:
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
				"my-text",
				MY_TYPE_PROGRESS_BAR,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * myProgressBar::my-pulse:
	 *
	 * This signal may be sent to make the bar pulsing.
	 *
	 * Handler is of type:
	 * void ( *handler )( myProgressBar *bar,
	 * 						gpointer     user_data );
	 */
	st_signals[ PULSE ] = g_signal_new_class_handler(
				"my-pulse",
				MY_TYPE_PROGRESS_BAR,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * my_progress_bar_new:
 */
myProgressBar *
my_progress_bar_new( void )
{
	myProgressBar *self;

	self = g_object_new( MY_TYPE_PROGRESS_BAR, NULL );

	gtk_widget_set_hexpand( GTK_WIDGET( self ), TRUE );
	my_style_add( GTK_WIDGET( self ), "bar12" );

	g_signal_connect( G_OBJECT( self ), "my-double", G_CALLBACK( on_double ), NULL );
	g_signal_connect( G_OBJECT( self ), "my-text", G_CALLBACK( on_text ), NULL );
	g_signal_connect( G_OBJECT( self ), "my-pulse", G_CALLBACK( on_pulse ), NULL );

	return( self );
}

static void
on_double( myProgressBar *self, gdouble progress, void *empty )
{
	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( self ), progress );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

static void
on_text( myProgressBar *self, const gchar *text, void *empty )
{
	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	gtk_progress_bar_set_show_text( GTK_PROGRESS_BAR( self ), TRUE );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( self ), text );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

static void
on_pulse( myProgressBar *self, void *empty )
{
	g_return_if_fail( self && MY_IS_PROGRESS_BAR( self ));

	gtk_progress_bar_pulse( GTK_PROGRESS_BAR( self ));

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}
