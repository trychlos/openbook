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

#include "ui/my-utils.h"
#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-select.h"
#include "ui/ofo-account.h"

/* private class data
 */
struct _ofaAccountSelectClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountSelectPrivate {
	gboolean       dispose_has_run;

	/* input data
	 */
	ofaMainWindow *main_window;

	/* runtime
	 */
	GtkDialog     *dialog;
	ofaAccountNotebook *child;

	/* returned value
	 */
	gchar         *account_number;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-account-select.ui";
static const gchar  *st_ui_id        = "AccountSelectDlg";

static GObjectClass *st_parent_class = NULL;

static GType    register_type( void );
static void     class_init( ofaAccountSelectClass *klass );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     instance_dispose( GObject *instance );
static void     instance_finalize( GObject *instance );
static void     do_initialize_dialog( ofaAccountSelect *self, const gchar *asked_number );
static gboolean ok_to_terminate( ofaAccountSelect *self, gint code );
static void     on_account_activated( const gchar *number, ofaAccountSelect *self );
static void     check_for_enable_dlg( ofaAccountSelect *self );
static gboolean do_update( ofaAccountSelect *self );

GType
ofa_account_select_get_type( void )
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
	static const gchar *thisfn = "ofa_account_select_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountSelectClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountSelect ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaAccountSelect", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountSelectClass *klass )
{
	static const gchar *thisfn = "ofa_account_select_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaAccountSelectClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_account_select_instance_init";
	ofaAccountSelect *self;

	g_return_if_fail( OFA_IS_ACCOUNT_SELECT( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNT_SELECT( instance );

	self->private = g_new0( ofaAccountSelectPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_instance_dispose";
	ofaAccountSelectPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_SELECT( instance ));

	priv = ( OFA_ACCOUNT_SELECT( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_instance_finalize";
	ofaAccountSelect *self;

	g_return_if_fail( OFA_IS_ACCOUNT_SELECT( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_ACCOUNT_SELECT( instance );

	gtk_widget_destroy( GTK_WIDGET( self->private->dialog ));

	g_free( self->private->account_number );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_account_select_run:
 *
 * Returns the selected account number, as a newly allocated string
 * that must be g_free() by the caller
 */
gchar *
ofa_account_select_run( ofaMainWindow *main_window, const gchar *asked_number )
{
	static const gchar *thisfn = "ofa_account_select_run";
	ofaAccountSelect *self;
	gint code;
	gchar *number;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_number=%s",
			thisfn, ( void * ) main_window, asked_number );

	self = g_object_new( OFA_TYPE_ACCOUNT_SELECT, NULL );

	self->private->main_window = main_window;

	do_initialize_dialog( self, asked_number );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	number = g_strdup( self->private->account_number );

	g_object_unref( self );

	return( number );
}

static void
do_initialize_dialog( ofaAccountSelect *self, const gchar *asked_number )
{
	static const gchar *thisfn = "ofa_account_select_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaAccountSelectPrivate *priv;
	GtkWidget *book;
	ofaAccountNotebookParms parms;

	priv = self->private;

	/* create the GtkDialog */
	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
		priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
		if( !priv->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );

	/* initialize the newly created dialog */
	if( priv->dialog ){

		/*gtk_window_set_transient_for( GTK_WINDOW( priv->dialog ), GTK_WINDOW( main ));*/

		book = my_utils_container_get_child_by_type( GTK_CONTAINER( priv->dialog ), GTK_TYPE_NOTEBOOK );
		g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));

		parms.book = GTK_NOTEBOOK( book );
		parms.dossier = ofa_main_window_get_dossier( priv->main_window );
		parms.pfnSelect = NULL;
		parms.user_data_select = NULL;
		parms.pfnDoubleClic = ( ofaAccountNotebookCb ) on_account_activated;
		parms.user_data_double_clic = self;
		parms.account_number = asked_number;

		priv->child = ofa_account_notebook_init_dialog( &parms );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));

	check_for_enable_dlg( self );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaAccountSelect *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;

		case GTK_RESPONSE_OK:
			quit = do_update( self );
			break;
	}

	return( quit );
}

static void
on_account_activated( const gchar *number, ofaAccountSelect *self )
{
	gtk_dialog_response( self->private->dialog, GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{

}

static gboolean
do_update( ofaAccountSelect *self )
{
	ofoAccount *account;

	account = ofa_account_notebook_get_selected( self->private->child );
	if( account ){
		self->private->account_number = g_strdup( ofo_account_get_number( account ));
		g_object_unref( account );
	}

	return( TRUE );
}
