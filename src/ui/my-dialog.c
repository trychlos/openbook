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

#include "ui/my-dialog.h"
#include "ui/my-window-prot.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

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

static void
my_dialog_finalize( GObject *instance )
{
	myDialog *self;

	self = MY_DIALOG( instance );

	/* free data members here */
	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_dialog_parent_class )->finalize( instance );
}

static void
my_dialog_dispose( GObject *instance )
{
	g_return_if_fail( MY_IS_DIALOG( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

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

	self->private = g_new0( myDialogPrivate, 1 );
}

static void
my_dialog_class_init( myDialogClass *klass )
{
	static const gchar *thisfn = "my_dialog_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = my_dialog_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_dialog_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = NULL;
	MY_DIALOG_CLASS( klass )->run_dialog = v_run_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_delete_event = v_quit_on_delete_event;
	MY_DIALOG_CLASS( klass )->quit_on_cancel = v_quit_on_cancel;
	MY_DIALOG_CLASS( klass )->quit_on_close = v_quit_on_close;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * my_dialog_init_dialog:
 */
gboolean
my_dialog_init_dialog( myDialog *self )
{
	g_return_val_if_fail( self && MY_IS_DIALOG( self ), FALSE );

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		if( my_window_has_valid_toplevel( MY_WINDOW( self )) &&
				!self->private->init_has_run ){

			do_init_dialog( self );

			gtk_widget_show_all( GTK_WIDGET( my_window_get_toplevel( MY_WINDOW( self ))));

			self->private->init_has_run = TRUE;

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

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		if( self->private->init_has_run || my_dialog_init_dialog( self )){

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
		g_debug( "%s: gtk_dialog_run return code=%d", thisfn, code );
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
