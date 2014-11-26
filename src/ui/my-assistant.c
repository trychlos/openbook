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

#include <glib/gi18n.h>

#include "api/my-utils.h"

#include "core/my-window-prot.h"

#include "ui/my-assistant.h"
#include "core/ofa-preferences.h"

/* private instance data
 */
struct _myAssistantPrivate {

	/* runtime data
	 */
	gint           prev_page_num;
	gboolean       escape_key_pressed;
};

/* signals defined here
 */
enum {
	PAGE_FORWARD,
	N_SIGNALS
};

/* a data structure set against each #GtkWidget page
 */
typedef struct {
	gint     page_num;
	gboolean initialized;
}
	sAssistant;

#define DATA_PAGE                       "ofa-assistant-data-page"

static guint st_signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE( myAssistant, my_assistant, MY_TYPE_WINDOW )

static void        do_setup_assistant( myAssistant *self );
static void        on_prepare( GtkAssistant *assistant, GtkWidget *page, myAssistant *self );
static gboolean    on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myAssistant *self );
static void        on_cancel( GtkAssistant *assistant, myAssistant *self );
static gboolean    is_willing_to_quit( myAssistant *self );
static void        on_close( GtkAssistant *assistant, myAssistant *self );
static void        do_close( myAssistant *self );
static void        on_page_weak_notify( sAssistant *sdata, gpointer finalized_page );
static sAssistant *assistant_get_data( myAssistant *self, GtkWidget *page );

static void
assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_assistant_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_assistant_parent_class )->finalize( instance );
}

static void
assistant_dispose( GObject *instance )
{
	g_return_if_fail( instance && MY_IS_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref member objects here */

		gtk_main_quit();
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_assistant_parent_class )->dispose( instance );
}

static void
assistant_constructed( GObject *instance )
{
	g_return_if_fail( instance && MY_IS_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* first, chain up to the parent class */
		if( G_OBJECT_CLASS( my_assistant_parent_class )->constructed ){
			G_OBJECT_CLASS( my_assistant_parent_class )->constructed( instance );
		}

		do_setup_assistant( MY_ASSISTANT( instance ));
	}
}

static void
my_assistant_init( myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_ASSISTANT, myAssistantPrivate );

	self->priv->prev_page_num = -1;
}

static void
my_assistant_class_init( myAssistantClass *klass )
{
	static const gchar *thisfn = "my_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = assistant_constructed;
	G_OBJECT_CLASS( klass )->dispose = assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = assistant_finalize;

	g_type_class_add_private( klass, sizeof( myAssistantPrivate ));

	/**
	 * myAssistant::ofa-signal-page-forward:
	 *
	 * This signal is sent when the user has aknowledged the page
	 * par asking for 'forward', before even creating te next page.
	 *
	 * Handler is of type:
	 * void ( *handler )( myAssistant *assistant,
	 * 						GtkWidget *page,
	 * 						gint       page_num,
	 * 						gpointer   user_data );
	 */
	st_signals[ PAGE_FORWARD ] = g_signal_new_class_handler(
				MY_SIGNAL_PAGE_FORWARD,
				MY_TYPE_ASSISTANT,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER,
				G_TYPE_INT );
}

/*
 * called during myAssistant construction
 */
static void
do_setup_assistant( myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_do_setup_assistant";
	GtkAssistant *assistant;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	gtk_window_set_modal( GTK_WINDOW( assistant ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( assistant ),
									GTK_WINDOW( MY_WINDOW( self )->prot->main_window ));

	/* deals with 'Esc' key */
	g_signal_connect(
			assistant,
			"key-press-event", G_CALLBACK( on_key_pressed_event ), self );

	g_signal_connect( assistant, "prepare",  G_CALLBACK( on_prepare ),  self );
	g_signal_connect( assistant, "cancel",  G_CALLBACK( on_cancel ),  self );
	g_signal_connect( assistant, "close",   G_CALLBACK( on_close ),   self );

	gtk_widget_show_all( GTK_WIDGET( assistant ));
}

/*
 * Preparing a page:
 * - if this is the first time, then first send the 'create' message
 * - only then, send the 'display' message
 */
static void
on_prepare( GtkAssistant *assistant, GtkWidget *page, myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_on_prepare";
	myAssistantPrivate *priv;
	sAssistant *sdata;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( self && MY_IS_ASSISTANT( self ));

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		g_debug( "%s: assistant=%p, page=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) page, ( void * ) self );

		priv = self->priv;
		sdata = assistant_get_data( self, page );
		g_return_if_fail( sdata );

		if( priv->prev_page_num >= 0 && priv->prev_page_num < sdata->page_num ){
			g_signal_emit_by_name( self, MY_SIGNAL_PAGE_FORWARD, page, priv->prev_page_num );
		}
		priv->prev_page_num = sdata->page_num;
	}
}

static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myAssistant *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( self && MY_IS_ASSISTANT( self ), FALSE );

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		if( event->keyval == GDK_KEY_Escape &&
				ofa_prefs_assistant_quit_on_escape()){

				self->priv->escape_key_pressed = TRUE;
				g_signal_emit_by_name(
						my_window_get_toplevel( MY_WINDOW( self )), "cancel", self );
				stop = TRUE;
		}
	}

	return( stop );
}

/*
 * the "cancel" message is sent when the user click on the "Cancel"
 * button, or if he hits the 'Escape' key and the 'Quit on escape'
 * preference is set
 */
static void
on_cancel( GtkAssistant *assistant, myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_on_cancel";

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( self && MY_IS_ASSISTANT( self ));

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		if(( self->priv->escape_key_pressed &&
				( !ofa_prefs_assistant_confirm_on_escape() || is_willing_to_quit( self ))) ||
					!ofa_prefs_assistant_confirm_on_cancel() || is_willing_to_quit( self )){

			do_close( self );
		}
	}
}

static gboolean
is_willing_to_quit( myAssistant *self )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_( "Are you sure you want to quit this assistant ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Quit" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
on_close( GtkAssistant *assistant, myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_on_close";

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( self && MY_IS_ASSISTANT( self ));

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		do_close( self );
	}
}

static void
do_close( myAssistant *self )
{
	g_object_unref( self );
}

static void
on_page_weak_notify( sAssistant *sdata, gpointer finalized_page )
{
	static const gchar *thisfn = "my_assistant_on_page_weak_notify";

	g_debug( "%s: sdata=%p, finalized_page=%p", thisfn, ( void * ) sdata, ( void * ) finalized_page );

	g_free( sdata );
}

static sAssistant *
assistant_get_data( myAssistant *self, GtkWidget *page )
{
	sAssistant *sdata;

	sdata = ( sAssistant * ) g_object_get_data( G_OBJECT( page ), DATA_PAGE );

	if( !sdata ){
		sdata = g_new0( sAssistant, 1 );
		sdata->initialized = FALSE;
		sdata->page_num = gtk_assistant_get_current_page(
									GTK_ASSISTANT( my_window_get_toplevel( MY_WINDOW( self ))));
		g_object_set_data( G_OBJECT( page ), DATA_PAGE, sdata );
		g_object_weak_ref( G_OBJECT( page ), ( GWeakNotify ) on_page_weak_notify, sdata );
	}

	return( sdata );
}

/**
 * my_assistant_run:
 */
void
my_assistant_run( myAssistant *assistant )
{
	g_return_if_fail( assistant && MY_IS_ASSISTANT( assistant ));

	if( !MY_WINDOW( assistant )->prot->dispose_has_run ){

		gtk_widget_show_all( GTK_WIDGET( my_window_get_toplevel( MY_WINDOW( assistant ))));
		gtk_main();
	}
}

/**
 * my_assistant_signal_connect:
 * @assistant: this #myAssistant instance.
 * @signal: the signal.
 * @cb: the callback.
 *
 * Connect to the desired @signal of the underlying #GtkAssistant,
 * passing #myAssistant @assistant instance as user data.
 */
gulong
my_assistant_signal_connect( myAssistant *assistant, const gchar *signal, GCallback cb )
{
	g_return_val_if_fail( assistant && MY_IS_ASSISTANT( assistant ), 0 );
	g_return_val_if_fail( signal && g_utf8_strlen( signal, -1 ), 0 );

	if( !MY_WINDOW( assistant )->prot->dispose_has_run ){

		return( g_signal_connect(
						G_OBJECT( my_window_get_toplevel( MY_WINDOW( assistant ))),
						signal, cb, assistant ));
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * my_assistant_is_page_initialized:
 */
gboolean
my_assistant_is_page_initialized( myAssistant *assistant, GtkWidget *page )
{
	sAssistant *sdata;

	g_return_val_if_fail( assistant && MY_IS_ASSISTANT( assistant ), FALSE );

	if( !MY_WINDOW( assistant )->prot->dispose_has_run ){

		sdata = assistant_get_data( assistant, page );
		g_return_val_if_fail( sdata, FALSE );

		return( sdata->initialized );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * my_assistant_set_page_initialized:
 */
void
my_assistant_set_page_initialized( myAssistant *assistant, GtkWidget *page, gboolean initialized )
{
	sAssistant *sdata;

	g_return_if_fail( assistant && MY_IS_ASSISTANT( assistant ));

	if( !MY_WINDOW( assistant )->prot->dispose_has_run ){

		sdata = assistant_get_data( assistant, page );
		g_return_if_fail( sdata );

		sdata->initialized = initialized;
	}
}
