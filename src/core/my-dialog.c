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

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/my-dialog.h"
#include "api/my-window-prot.h"

/* private instance data
 */
struct _myDialogPrivate {

	/* internals
	 */
	gboolean       init_has_run;
};

G_DEFINE_TYPE( myDialog, my_dialog, MY_TYPE_WINDOW )

static void     do_init_dialog( myDialog *dialog );
static gint     do_run_dialog( myDialog *dialog );
static gint     v_run_dialog( myDialog *dialog );
static gboolean ok_to_terminate( myDialog *self, gint code );
static gboolean do_quit_on_delete_event( myDialog *dialog );
static gboolean v_quit_on_delete_event( myDialog *dialog );
static gboolean do_quit_on_cancel( myDialog *dialog );
static gboolean v_quit_on_cancel( myDialog *dialog );
static gboolean do_quit_on_close( myDialog *dialog );
static gboolean v_quit_on_close( myDialog *dialog );
static gboolean do_quit_on_ok( myDialog *dialog );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_quit_on_code( myDialog *dialog, gint code );
static gboolean v_quit_on_code( myDialog *dialog, gint code );

static void
dialog_finalize( GObject *instance )
{
	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dialog_parent_class )->finalize( instance );
}

static void
dialog_dispose( GObject *instance )
{
	g_return_if_fail( instance && MY_IS_DIALOG( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref member objects here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dialog_parent_class )->dispose( instance );
}

static void
my_dialog_init( myDialog *self )
{
	static const gchar *thisfn = "my_dialog_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_DIALOG, myDialogPrivate );
}

static void
my_dialog_class_init( myDialogClass *klass )
{
	static const gchar *thisfn = "my_dialog_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dialog_dispose;
	G_OBJECT_CLASS( klass )->finalize = dialog_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = NULL;
	MY_DIALOG_CLASS( klass )->run_dialog = v_run_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_delete_event = v_quit_on_delete_event;
	MY_DIALOG_CLASS( klass )->quit_on_cancel = v_quit_on_cancel;
	MY_DIALOG_CLASS( klass )->quit_on_close = v_quit_on_close;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
	MY_DIALOG_CLASS( klass )->quit_on_code = v_quit_on_code;

	g_type_class_add_private( klass, sizeof( myDialogPrivate ));
}

/**
 * my_dialog_init_dialog:
 */
gboolean
my_dialog_init_dialog( myDialog *self )
{
	GtkWindow *toplevel, *window;
	const gchar *name;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		toplevel = my_window_get_toplevel( MY_WINDOW( self ));

		if( toplevel && GTK_IS_WINDOW( toplevel ) && !self->priv->init_has_run ){

			do_init_dialog( self );

			window = ( GtkWindow * ) my_window_get_main_window( MY_WINDOW( self ));
			name = my_window_get_name( MY_WINDOW( self ));

			if( window && GTK_IS_WINDOW( window ) && my_strlen( name )){
				g_signal_emit_by_name( window, "my-dialog-init", name, window );
			}

			gtk_widget_show_all( GTK_WIDGET( toplevel ));

			self->priv->init_has_run = TRUE;

			return( TRUE );
		}
	}

	return( FALSE );
}

static void
do_init_dialog( myDialog *self )
{
	static const gchar *thisfn = "my_dialog_do_init_dialog";

	g_return_if_fail( self && MY_IS_DIALOG( self ));

	if( MY_DIALOG_GET_CLASS( self )->init_dialog ){
		MY_DIALOG_GET_CLASS( self )->init_dialog( self );

	} else {
		g_debug( "%s: self=%p (%s)",
				thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
	}
}

/**
 * my_dialog_run_dialog:
 */
gint
my_dialog_run_dialog( myDialog *self )
{
	gint code;

	code = GTK_RESPONSE_CANCEL;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), code );

	if( !MY_WINDOW( self )->prot->dispose_has_run ){

		if( self->priv->init_has_run || my_dialog_init_dialog( self )){

			code = do_run_dialog( self );
		}
	}

	return( code );
}

static gint
do_run_dialog( myDialog *self )
{
	gint code;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), GTK_RESPONSE_CANCEL );

	if( MY_DIALOG_GET_CLASS( self )->run_dialog ){
		code = MY_DIALOG_GET_CLASS( self )->run_dialog( self );

	} else {
		code = v_run_dialog( self );
	}

	return( code );
}

static gint
v_run_dialog( myDialog *self )
{
	static const gchar *thisfn = "my_dialog_v_run_dialog";
	GtkDialog *dialog;
	gint code;

	g_debug( "%s: calling gtk_dialog_run", thisfn );
	dialog = ( GtkDialog * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_val_if_fail( dialog && GTK_IS_DIALOG( dialog ), GTK_RESPONSE_CANCEL );

	do {
		code = gtk_dialog_run( dialog );
		g_debug( "%s: gtk_dialog_run returns code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	return( code );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( myDialog *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_DELETE_EVENT:
			quit = do_quit_on_delete_event( self );
			break;
		case GTK_RESPONSE_CLOSE:
			quit = do_quit_on_close( self );
			break;
		case GTK_RESPONSE_CANCEL:
			quit = do_quit_on_cancel( self );
			break;
		case GTK_RESPONSE_OK:
			quit = do_quit_on_ok( self );
			break;
		default:
			quit = do_quit_on_code( self, code );
			break;
	}

	return( quit );
}

static gboolean
do_quit_on_delete_event( myDialog *self )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( MY_DIALOG_GET_CLASS( self )->quit_on_delete_event ){
		ok_to_quit = MY_DIALOG_GET_CLASS( self )->quit_on_delete_event( self );

	} else {
		ok_to_quit = v_quit_on_delete_event( self );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_delete_event( myDialog *self )
{
	return( TRUE );
}

static gboolean
do_quit_on_cancel( myDialog *self )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( MY_DIALOG_GET_CLASS( self )->quit_on_cancel ){
		ok_to_quit = MY_DIALOG_GET_CLASS( self )->quit_on_cancel( self );

	} else {
		ok_to_quit = v_quit_on_cancel( self );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_cancel( myDialog *self )
{
	return( TRUE );
}

static gboolean
do_quit_on_close( myDialog *self )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( MY_DIALOG_GET_CLASS( self )->quit_on_close ){
		ok_to_quit = MY_DIALOG_GET_CLASS( self )->quit_on_close( self );

	} else {
		ok_to_quit = v_quit_on_close( self );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_close( myDialog *self )
{
	return( TRUE );
}

static gboolean
do_quit_on_ok( myDialog *self )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( MY_DIALOG_GET_CLASS( self )->quit_on_ok ){
		ok_to_quit = MY_DIALOG_GET_CLASS( self )->quit_on_ok( self );

	} else {
		ok_to_quit = v_quit_on_ok( self );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_ok( myDialog *self )
{
	return( TRUE );
}

static gboolean
do_quit_on_code( myDialog *dialog, gint code )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( dialog && MY_IS_DIALOG( dialog ), FALSE );

	if( MY_DIALOG_GET_CLASS( dialog )->quit_on_code ){
		ok_to_quit = MY_DIALOG_GET_CLASS( dialog )->quit_on_code( dialog, code );

	} else {
		ok_to_quit = v_quit_on_code( dialog, code );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_code( myDialog *dialog, gint code )
{
	return( FALSE );
}

/**
 * my_dialog_set_readonly_buttons:
 *
 * Replace the OK/Cancel buttons with a Close one.
 *
 * Returns: the newly added button.
 */
GtkWidget *
my_dialog_set_readonly_buttons( myDialog *dialog )
{
	GtkWindow *toplevel;
	GtkWidget *button;

	g_return_val_if_fail( dialog && MY_IS_DIALOG( dialog ), NULL );

	if( !MY_WINDOW( dialog )->prot->dispose_has_run ){

		toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
		g_return_val_if_fail( toplevel && GTK_IS_WINDOW( toplevel ), NULL );

		button = gtk_dialog_get_widget_for_response( GTK_DIALOG( toplevel ), GTK_RESPONSE_OK );
		if( button ){
			gtk_widget_destroy( button );
		}

		button = gtk_dialog_get_widget_for_response( GTK_DIALOG( toplevel ), GTK_RESPONSE_CANCEL );
		if( button ){
			gtk_widget_destroy( button );
		}

		return( gtk_dialog_add_button( GTK_DIALOG( toplevel ), _( "Close" ), GTK_RESPONSE_CANCEL ));
	}

	g_return_val_if_reached( NULL );
}
