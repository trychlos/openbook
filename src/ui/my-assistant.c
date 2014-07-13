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

#include "core/my-utils.h"

#include "ui/my-window-prot.h"
#include "ui/my-assistant.h"
#include "ui/ofa-preferences.h"

/* private instance data
 */
struct _myAssistantPrivate {

	gboolean       escape_key_pressed;
};

G_DEFINE_TYPE( myAssistant, my_assistant, MY_TYPE_WINDOW )

static void     do_setup_assistant( myAssistant *self );
static gboolean on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myAssistant *self );
static void     on_cancel( GtkAssistant *assistant, myAssistant *self );
static gboolean is_willing_to_quit( myAssistant *self );
static void     on_close( GtkAssistant *assistant, myAssistant *self );
static void     do_close( myAssistant *self );

static void
my_assistant_finalize( GObject *instance )
{
	myAssistant *self;

	self = MY_ASSISTANT( instance );

	/* free data members here */
	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_assistant_parent_class )->finalize( instance );
}

static void
my_assistant_dispose( GObject *instance )
{
	g_return_if_fail( instance && MY_IS_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref member objects here */

		gtk_main_quit();
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_assistant_parent_class )->dispose( instance );
}

static void
my_assistant_constructed( GObject *instance )
{
	myAssistant *self;

	g_return_if_fail( instance && MY_IS_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* first, chain up to the parent class */
		if( G_OBJECT_CLASS( my_assistant_parent_class )->constructed ){
			G_OBJECT_CLASS( my_assistant_parent_class )->constructed( instance );
		}

		self = MY_ASSISTANT( instance );
		do_setup_assistant( self );
	}
}

static void
my_assistant_init( myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( myAssistantPrivate, 1 );
}

static void
my_assistant_class_init( myAssistantClass *klass )
{
	static const gchar *thisfn = "my_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = my_assistant_constructed;
	G_OBJECT_CLASS( klass )->dispose = my_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_assistant_finalize;
}

static void
do_setup_assistant( myAssistant *self )
{
	static const gchar *thisfn = "my_assistant_do_setup_assistant";
	GtkAssistant *assistant;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	/* deals with 'Esc' key */
	g_signal_connect(
			assistant,
			"key-press-event", G_CALLBACK( on_key_pressed_event ), self );

	g_signal_connect( assistant, "cancel",  G_CALLBACK( on_cancel ),  self );
	g_signal_connect( assistant, "close",   G_CALLBACK( on_close ),   self );

	gtk_widget_show_all( GTK_WIDGET( assistant ));
}

static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myAssistant *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( self && MY_IS_ASSISTANT( self ), FALSE );

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		if( event->keyval == GDK_KEY_Escape &&
				ofa_prefs_assistant_quit_on_escape()){

				self->private->escape_key_pressed = TRUE;
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

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		if(( self->private->escape_key_pressed &&
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
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_QUIT, GTK_RESPONSE_OK,
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

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

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

/**
 * my_assistant_run:
 */
void
my_assistant_run( myAssistant *assistant )
{
	g_return_if_fail( assistant && MY_IS_ASSISTANT( assistant ));

	if( !MY_WINDOW( assistant )->protected->dispose_has_run ){

		gtk_main();
	}
}

/**
 * my_assistant_get_page_num:
 *
 * Returns: the index of the given page, or -1 if not found
 */
gint
my_assistant_get_page_num( myAssistant *assistant, GtkWidget *page )
{
	gint count, i;
	GtkWidget *page_n;
	GtkAssistant *toplevel;

	g_return_val_if_fail( assistant && MY_IS_ASSISTANT( assistant ), -1 );
	g_return_val_if_fail( page && GTK_IS_WIDGET( page ), -1 );

	if( !MY_WINDOW( assistant )->protected->dispose_has_run ){

		toplevel = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( assistant ));
		g_return_val_if_fail( toplevel && GTK_IS_ASSISTANT( toplevel ), -1 );

		count = gtk_assistant_get_n_pages( toplevel );
		page_n = NULL;
		for( i=0 ; i<count ; ++i ){
			page_n = gtk_assistant_get_nth_page( toplevel, i );
			if( page_n == page ){
				return( i );
			}
		}
	}

	return( -1 );
}
