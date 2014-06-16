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
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print-reconcil.h"
#include "api/ofo-account.h"
#include "api/ofo-entry.h"

/* private instance data
 */
struct _ofaPrintReconcilPrivate {

	/* internals
	 */
	ofoAccount *account;
	GDate       date;
	GList      *entries;

	/* UI
	 */
	GtkEntry   *account_entry;
	GtkLabel   *account_label;
	GtkLabel   *date_label;
};

/* count of entry lines per page */
#define NB_LINES_PER_PAGE        15

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-print-reconcil.ui";

G_DEFINE_TYPE( ofaPrintReconcil, ofa_print_reconcil, MY_TYPE_DIALOG )

static GObject  *on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self );
static void      on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintReconcil *self );
static void      on_account_changed( GtkEntry *entry, ofaPrintReconcil *self );
static void      on_account_select( GtkButton *button, ofaPrintReconcil *self );
static void      on_date_changed( GtkEntry *entry, ofaPrintReconcil *self );
static void      on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self );
static void      on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self );

static void
print_reconcil_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_print_reconcil_finalize";
	ofaPrintReconcilPrivate *priv;

	g_return_if_fail( OFA_IS_PRINT_RECONCIL( instance ));

	priv = OFA_PRINT_RECONCIL( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_reconcil_parent_class )->finalize( instance );
}

static void
print_reconcil_dispose( GObject *instance )
{
	ofaPrintReconcilPrivate *priv;

	g_return_if_fail( OFA_IS_PRINT_RECONCIL( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		priv = OFA_PRINT_RECONCIL( instance )->private;

		/* unref object members here */
		if( priv->entries ){
			ofo_entry_free_dataset( priv->entries );
			priv->entries = NULL;
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_reconcil_parent_class )->dispose( instance );
}

static void
ofa_print_reconcil_init( ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_instance_init";

	g_return_if_fail( OFA_IS_PRINT_RECONCIL( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaPrintReconcilPrivate, 1 );

	self->private->entries = NULL;
}

static void
ofa_print_reconcil_class_init( ofaPrintReconcilClass *klass )
{
	static const gchar *thisfn = "ofa_print_reconcil_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_reconcil_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_reconcil_finalize;
}

/**
 * ofa_print_reconcil_run:
 * @main: the main window of the application.
 *
 * Print the reconciliation summary
 */
gboolean
ofa_print_reconcil_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_print_reconcil_run";
	ofaPrintReconcil *self;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	gboolean printed;
	GtkWidget *dialog;
	gchar *str;
	GError *error;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_PRINT_RECONCIL,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					NULL );

	print = gtk_print_operation_new ();

	g_signal_connect( print, "create-custom-widget", G_CALLBACK( on_create_custom_widget ), self );
	g_signal_connect( print, "custom-widget-apply", G_CALLBACK( on_custom_widget_apply ), self );
	g_signal_connect( print, "begin_print", G_CALLBACK( on_begin_print ), self );
	g_signal_connect( print, "draw_page", G_CALLBACK( on_draw_page ), self );

	error = NULL;
	printed = FALSE;

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                GTK_WINDOW( main_window ),
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );
		g_error_free( error );

	} else if( res == GTK_PRINT_OPERATION_RESULT_APPLY ){
	      /*if (settings != NULL)
	        g_object_unref (settings);
	      settings = g_object_ref (gtk_print_operation_get_print_settings (print));
	    }*/
		printed = TRUE;
	}

	g_object_unref( self );

	return( printed );
}

static GObject*
on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_create_custom_widget";
	GtkWidget *box, *frame;
	GtkWidget *entry, *button, *label;

	g_debug( "%s: operation=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) self );

	box = my_utils_builder_load_from_path( st_ui_xml, "box-reconcil" );
	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "frame-reconcil" );
	g_object_ref( frame );
	gtk_container_remove( GTK_CONTAINER( box ), frame );
	g_object_unref( box );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	self->private->account_entry = GTK_ENTRY( entry );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-select" );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), NULL );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_account_select ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-label" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	self->private->account_label = GTK_LABEL( label );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-label" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	self->private->date_label = GTK_LABEL( label );

	return( G_OBJECT( frame ));
}

static void
on_account_changed( GtkEntry *entry, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	const gchar *str;

	priv = self->private;
	str = gtk_entry_get_text( entry );
	priv->account = ofo_account_get_by_number( MY_WINDOW( self )->protected->dossier, str );
	if( priv->account ){
		gtk_label_set_text( priv->account_label, ofo_account_get_label( priv->account ));
	} else {
		gtk_label_set_text( priv->account_label, "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPrintReconcil *self )
{
	gchar *number;

	number = ofa_account_select_run(
						MY_WINDOW( self )->protected->main_window,
						gtk_entry_get_text( self->private->account_entry ));

	gtk_entry_set_text( self->private->account_entry, number );
	g_free( number );
}

static void
on_date_changed( GtkEntry *entry, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	const gchar *str;

	priv = self->private;
	str = gtk_entry_get_text( entry );
	memcpy( &priv->date, my_utils_date_from_str( str ), sizeof( GDate ));
	if( g_date_valid( &priv->date )){
		gtk_label_set_text( priv->date_label, my_utils_display_from_date( &priv->date, MY_UTILS_DATE_DMMM ));
	} else {
		gtk_label_set_text( priv->date_label, "" );
	}
}

static void
on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	gint count;

	priv = self->private;

	if( g_date_valid( &priv->date ) &&
			priv->account && OFO_IS_ACCOUNT( priv->account )){

		priv->entries = ofo_entry_get_dataset_for_print_reconcil(
								MY_WINDOW( self )->protected->dossier,
								ofo_account_get_number( priv->account ),
								&priv->date );

		/* we estimate that about 15 lines are printed per pages */
		if( priv->entries ){
			count = g_list_length( priv->entries ) / NB_LINES_PER_PAGE;
			if( g_list_length( priv->entries ) % NB_LINES_PER_PAGE ){
				count += 1;
			}
			gtk_print_operation_set_n_pages( operation, count );
		}
	}
}

static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_begin_print";

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );
}

static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_draw_page";

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );
}
