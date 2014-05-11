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
#include <stdlib.h>

#include "ui/ofa-main-window.h"
#include "ui/ofo-dossier.h"

static gboolean pref_confirm_on_altf4 = FALSE;

/* private class data
 */
struct _ofaMainWindowClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaMainWindowPrivate {
	gboolean    dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	GtkGrid    *grid;
	GtkMenuBar *menubar;
	GMenuModel *menu;					/* the menu model when a dossier is opened */
	GtkPaned   *pane;
	ofoDossier *dossier;
};

/* signals defined here
 */
enum {
	OPEN_DOSSIER,
	LAST_SIGNAL
};

static void on_close   ( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void on_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data );

static const GActionEntry st_dos_entries[] = {
		{ "close",    on_close,    NULL, NULL, NULL },
		{ "accounts", on_accounts, NULL, NULL, NULL },
};

static const gchar               *st_dosmenu_xml = PKGUIDIR "/ofa-dos-menubar.ui";
static const gchar               *st_dosmenu_id  = "dos-menu";

static GtkApplicationWindowClass *st_parent_class           = NULL;
static gint                       st_signals[ LAST_SIGNAL ] = { 0 };

static GType    register_type( void );
static void     class_init( ofaMainWindowClass *klass );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     instance_constructed( GObject *window );
static void     instance_dispose( GObject *window );
static void     instance_finalize( GObject *window );

static gboolean on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data );
static void     set_menubar( ofaMainWindow *window, GMenuModel *model );
static void     on_open_dossier( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data );
static void     on_open_dossier_cleanup_handler( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data );

GType
ofa_main_window_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_main_window_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaMainWindowClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMainWindow ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( GTK_TYPE_APPLICATION_WINDOW, "ofaMainWindow", &info, 0 );

	return( type );
}

static void
class_init( ofaMainWindowClass *klass )
{
	static const gchar *thisfn = "ofa_main_window_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaMainWindowClassPrivate, 1 );

	/*
	 * ofaMainWindow::main-signal-open-dossier:
	 *
	 * This signal is to be sent to the main window when someone asks
	 * for opening a dossier.
	 *
	 * Arguments are the name of the dossier, along with the connection
	 * parameters. The connection itself is supposed to have already
	 * been validated.
	 *
	 * They are passed in a ofaOpenDossier structure, that the cleanup handler
	 * takes care of freeing.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaMainWindow *window,
	 * 						ofaOpenDossier *struct,
	 * 						gpointer user_data );
	 */
	st_signals[ OPEN_DOSSIER ] = g_signal_new_class_handler(
				MAIN_SIGNAL_OPEN_DOSSIER,
				OFA_TYPE_MAIN_WINDOW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_open_dossier_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_main_window_instance_init";
	ofaMainWindow *self;
	ofaMainWindowPrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MAIN_WINDOW( instance );
	self->private = g_new0( ofaMainWindowPrivate, 1 );
	priv = self->private;
	priv->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_constructed";
	ofaMainWindowPrivate *priv;
	GError *error;
	GtkBuilder *builder;
	GMenuModel *menu;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	priv = OFA_MAIN_WINDOW( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->constructed ){
			G_OBJECT_CLASS( st_parent_class )->constructed( window );
		}

		/* define the main window actions
		 */
		g_action_map_add_action_entries(
				G_ACTION_MAP( window ),
		        st_dos_entries, G_N_ELEMENTS( st_dos_entries ),
		        ( gpointer ) window );

		/* define a traditional menubar
		 * the program will abort if GtkBuilder is not able to be parsed
		 * from the given file
		 */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_dosmenu_xml, &error )){
			menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_dosmenu_id ));
			if( menu ){
				priv->menu = menu;
			} else {
				g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_dosmenu_id, st_dosmenu_xml );
			}
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_object_unref( builder );

		/* build the main window
		 */
		priv->grid = GTK_GRID( gtk_grid_new());
		gtk_grid_set_row_homogeneous( priv->grid, FALSE );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( priv->grid ));

		gtk_window_set_default_size( GTK_WINDOW( window ), 200, 200 );

		/* connect some signals
		 */
		g_signal_connect( window, "delete-event", G_CALLBACK( on_delete_event ), NULL );

		g_signal_connect( window, MAIN_SIGNAL_OPEN_DOSSIER, G_CALLBACK( on_open_dossier ), NULL );
	}
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_dispose";
	ofaMainWindowPrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	priv = OFA_MAIN_WINDOW( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		if( priv->menu ){
			g_object_unref( priv->menu );
		}

		if( priv->dossier ){
			g_object_unref( priv->dossier );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_main_window_instance_finalize";
	ofaMainWindow *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_MAIN_WINDOW( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * Returns a newly allocated ofaMainWindow object.
 */
ofaMainWindow *
ofa_main_window_new( const ofaApplication *application )
{
	static const gchar *thisfn = "ofa_main_window_new";
	ofaMainWindow *window;

	g_return_val_if_fail( OFA_IS_APPLICATION( application ), NULL );

	g_debug( "%s: application=%p", thisfn, application );

	window = g_object_new( OFA_TYPE_MAIN_WINDOW,
					"application", application,
					NULL );

	set_menubar( window, ofa_application_get_menu_model( application ));

	return( window );
}

/*
 * triggered when the user clicks on the top right [X] button
 * returns %TRUE to stop the signal to be propagated (which would cause
 * the window to be destroyed); instead we gracefully quit the application.
 * Or, in other terms:
 * If you return FALSE in the "delete_event" signal handler, GTK will
 * emit the "destroy" signal. Returning TRUE means you don't want the
 * window to be destroyed.
 */
static gboolean
on_delete_event( GtkWidget *toplevel, GdkEvent *event, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_delete_event";
	gboolean ok_to_quit;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( toplevel ), FALSE );

	g_debug( "%s: toplevel=%p (%s), event=%p, user_data=%p",
			thisfn,
			( void * ) toplevel, G_OBJECT_TYPE_NAME( toplevel ),
			( void * ) event, ( void * ) user_data );

	ok_to_quit = !pref_confirm_on_altf4 || ofa_main_window_is_willing_to_quit( OFA_MAIN_WINDOW( toplevel ));

	return( !ok_to_quit );
}

gboolean
ofa_main_window_is_willing_to_quit( ofaMainWindow *window )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_( "Are you sure you want terminate the application ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_QUIT, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
set_menubar( ofaMainWindow *window, GMenuModel *model )
{
	GtkWidget *menubar;

	if( window->private->menubar ){
		gtk_widget_destroy( GTK_WIDGET( window->private->menubar ));
		window->private->menubar = NULL;
	}

	menubar = gtk_menu_bar_new_from_model( model );
	gtk_grid_attach( window->private->grid, menubar, 0, 0, 1, 1 );
	gtk_widget_show_all( GTK_WIDGET( window ));

	window->private->menubar = GTK_MENU_BAR( menubar );
}

static void
on_open_dossier( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier";

	g_debug( "%s: window=%p, sod=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) sod, ( void * ) user_data );
	g_debug( "%s: name=%s", thisfn, sod->dossier );
	g_debug( "%s: host=%s", thisfn, sod->host );
	g_debug( "%s: port=%d", thisfn, sod->port );
	g_debug( "%s: socket=%s", thisfn, sod->socket );
	g_debug( "%s: dbname=%s", thisfn, sod->dbname );
	g_debug( "%s: account=%s", thisfn, sod->account );
	g_debug( "%s: password=%s", thisfn, sod->password );

	window->private->dossier = ofo_dossier_new( sod->dossier );

	if( !ofo_dossier_open(
			window->private->dossier, GTK_WINDOW( window ),
			sod->host, sod->port, sod->socket, sod->dbname, sod->account, sod->password )){

		g_object_unref( window->private->dossier );
		window->private->dossier = NULL;
		return;
	}

	window->private->pane = GTK_PANED( gtk_paned_new( GTK_ORIENTATION_HORIZONTAL ));
	gtk_grid_attach( window->private->grid, GTK_WIDGET( window->private->pane ), 0, 1, 1, 1 );

	set_menubar( window, window->private->menu );
}

static void
on_open_dossier_cleanup_handler( ofaMainWindow *window, ofaOpenDossier* sod, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_open_dossier_final_handler";

	g_debug( "%s: window=%p, sod=%p, user_data=%p",
			thisfn, ( void * ) window, ( void * ) sod, ( void * ) user_data );

	g_free( sod->dossier );
	g_free( sod->host );
	g_free( sod->socket );
	g_free( sod->dbname );
	g_free( sod->account );
	g_free( sod->password );
	g_free( sod );
}

static void
on_close( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_close";
	ofaApplication *appli;
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->private;

	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	g_object_unref( priv->dossier );
	priv->dossier = NULL;

	gtk_widget_destroy( GTK_WIDGET( priv->pane ));
	priv->pane = NULL;
	appli = OFA_APPLICATION( gtk_window_get_application( GTK_WINDOW( user_data )));
	set_menubar( OFA_MAIN_WINDOW( user_data ), ofa_application_get_menu_model( appli ));
}

static void
on_accounts( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_main_window_on_accounts";
	ofaMainWindowPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_MAIN_WINDOW( user_data ));
	priv = OFA_MAIN_WINDOW( user_data )->private;

	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));
}
