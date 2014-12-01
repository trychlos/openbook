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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/my-assistant.h"
#include "ui/my-editable-date.h"
#include "ui/ofa-exe-closing.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaExeClosingPrivate {
	void *empty;
};

/* the pages of this assistant
 */
enum {
	PAGE_INTRO = 0,						/* Intro */
	PAGE_PARMS,							/* Content */
	PAGE_CONFIRM,						/* Confirm */
	PAGE_SUMMARY						/* Summary */
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-exe-closing.ui";
static const gchar  *st_ui_id  = "ExeClosingAssistant";

G_DEFINE_TYPE( ofaExeClosing, ofa_exe_closing, MY_TYPE_ASSISTANT )

static void on_prepare( GtkAssistant *assistant, GtkWidget *page_widget, ofaExeClosing *self );
static void on_page_forward( ofaExeClosing *self, GtkWidget *page_widget, gint page_num, void *empty );
static void p1_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static void p8_do_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static void on_apply( GtkAssistant *assistant, ofaExeClosing *self );
static void p9_do_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );

static void
exe_closing_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exe_closing_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->finalize( instance );
}

static void
exe_closing_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->dispose( instance );
}

static void
ofa_exe_closing_init( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXE_CLOSING, ofaExeClosingPrivate );
}

static void
ofa_exe_closing_class_init( ofaExeClosingClass *klass )
{
	static const gchar *thisfn = "ofa_exe_closing_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exe_closing_dispose;
	G_OBJECT_CLASS( klass )->finalize = exe_closing_finalize;

	g_type_class_add_private( klass, sizeof( ofaExeClosingPrivate ));
}

/**
 * ofa_exe_closing_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_exe_closing_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_exe_closing_run";
	ofaExeClosing *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_EXE_CLOSING,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	g_signal_connect(
			G_OBJECT( self ), MY_SIGNAL_PAGE_FORWARD, G_CALLBACK( on_page_forward ), NULL );

	my_assistant_signal_connect( MY_ASSISTANT( self ), "prepare", G_CALLBACK( on_prepare ));
	my_assistant_signal_connect( MY_ASSISTANT( self ), "apply", G_CALLBACK( on_apply ));

	my_assistant_run( MY_ASSISTANT( self ));
}

static void
on_prepare( GtkAssistant *assistant, GtkWidget *page_widget, ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_on_prepare";
	gint page_num;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( page_widget && GTK_IS_WIDGET( page_widget ));
	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));

	page_num = gtk_assistant_get_current_page( assistant );

	g_debug( "%s: assistant=%p, page_widget=%p, page_num=%d, self=%p",
			thisfn, ( void * ) assistant, ( void * ) page_widget, page_num, ( void * ) self );

	switch( page_num ){
		/* 0 [Intro] Introduction */
		case PAGE_INTRO:
			break;

		/* 1 [Content] Check and parms */
		case PAGE_PARMS:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page_widget )){
				p1_do_init( self, assistant, page_widget );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page_widget, TRUE );
			}
			break;

		/* 8 [Confirm] Confirm the informations before closing */
		case PAGE_CONFIRM:
			p8_do_display( self, assistant, page_widget );
			break;

		/* 9 [Summary] Close the exercice and print the result */
		case PAGE_SUMMARY:
			p9_do_display( self, assistant, page_widget );
			break;
	}
}

static void
on_page_forward( ofaExeClosing *self, GtkWidget *page_widget, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_exe_closing_on_page_forward";

	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));
	g_return_if_fail( page_widget && GTK_IS_WIDGET( page_widget ));

	g_debug( "%s: self=%p, page_widget=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page_widget, page_num, ( void * ) empty );
}

static void
p1_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	gtk_assistant_set_page_complete( assistant, page_widget, TRUE );
}

static void
p8_do_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{

}

static void
on_apply( GtkAssistant *assistant, ofaExeClosing *self )
{

}

static void
p9_do_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{

}
