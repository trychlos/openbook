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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-ledger-properties.h"

/* private instance data
 */
struct _ofaLedgerPropertiesPrivate {
	gboolean             dispose_has_run;

	/* internals
	 */
	ofaHub              *hub;
	ofoDossier          *dossier;
	gboolean             is_current;
	ofoLedger           *ledger;
	gboolean             is_new;

	/* data
	 */
	gchar               *mnemo;
	gchar               *label;
	gchar               *upd_user;
	GTimeVal             upd_stamp;
	GDate                closing;

	/* UI
	 */
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;
};

/* columns displayed in the exercice combobox
 */
enum {
	EXE_COL_BEGIN = 0,
	EXE_COL_END,
	EXE_N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ledger-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      init_balances_page( ofaLedgerProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      check_for_enable_dlg( ofaLedgerProperties *self );
static gboolean  is_dialog_validable( ofaLedgerProperties *self );
static gboolean  do_update( ofaLedgerProperties *self, gchar **msgerr );
static void      set_msgerr( ofaLedgerProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaLedgerProperties, ofa_ledger_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaLedgerProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
ledger_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_properties_finalize";
	ofaLedgerPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_properties_parent_class )->finalize( instance );
}

static void
ledger_properties_dispose( GObject *instance )
{
	ofaLedgerPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_PROPERTIES( instance ));

	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_properties_parent_class )->dispose( instance );
}

static void
ofa_ledger_properties_init( ofaLedgerProperties *self )
{
	static const gchar *thisfn = "ofa_ledger_properties_init";
	ofaLedgerPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_PROPERTIES( self ));

	priv = ofa_ledger_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;
	my_date_clear( &priv->closing );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_ledger_properties_class_init( ofaLedgerPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_ledger_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @ledger: the #ofoLedger to be displayed/updated.
 *
 * Update the properties of an ledger
 */
void
ofa_ledger_properties_run( const ofaMainWindow *main_window, ofoLedger *ledger )
{
	static const gchar *thisfn = "ofa_ledger_properties_run";
	ofaLedgerProperties *self;
	ofaLedgerPropertiesPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, ledger=%p",
				thisfn, ( void * ) main_window, ( void * ) ledger );

	self = g_object_new( OFA_TYPE_LEDGER_PROPERTIES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_ledger_properties_get_instance_private( self );
	priv->ledger = ledger;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and ledger mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaLedgerPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_ledger_get_mnemo( priv->ledger ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * ledger
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_ledger_properties_idialog_init";
	ofaLedgerPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *title, *str;
	const gchar *jou_mnemo;
	GtkWidget *entry, *label, *last_close_entry;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	priv->is_current = ofo_dossier_is_current( priv->dossier );

	jou_mnemo = ofo_ledger_get_mnemo( priv->ledger );
	if( !jou_mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new ledger" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » ledger" ), jou_mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	/* mnemonic */
	priv->mnemo = g_strdup( jou_mnemo );
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->mnemo ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->mnemo );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_mnemo_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* label */
	priv->label = g_strdup( ofo_ledger_get_label( priv->ledger ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->label ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->label );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	my_date_set_from_date( &priv->closing, ofo_ledger_get_last_close( priv->ledger ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-last-close" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	str = my_date_to_str( &priv->closing, ofa_prefs_date_display());
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	last_close_entry = entry;

	init_balances_page( OFA_LEDGER_PROPERTIES( instance ));

	my_utils_container_notes_init( instance, ledger );
	my_utils_container_updstamp_init( instance, ledger );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_current );
	my_utils_widget_set_editable( last_close_entry, FALSE );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_LEDGER_PROPERTIES( instance ));
}

/*
 * the "Debit" and "Credit" titles are already displayed on row 0
 * for each currency, we add
 * - a title line "balance for xxx currency"
 * - three lines for validated, rough and future balances
 */
static void
init_balances_page( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	GtkWidget *grid, *label;
	GList *currencies, *it;
	const gchar *code, *symbol;
	ofoCurrency *currency;
	gint i, count, digits;
	gchar *str;
	ofxAmount amount, tot_debit, tot_credit;

	priv = ofa_ledger_properties_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	currencies = ofo_ledger_get_currencies( priv->ledger );
	count = g_list_length( currencies );
	/* 5 lines by currency */
	gtk_grid_insert_row( GTK_GRID( grid ), 4*count );

	for( i=0, it=currencies ; it ; ++i, it=it->next ){
		code = ( const gchar * ) it->data;
		currency = ofo_currency_get_by_code( priv->hub, code );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );
		symbol = ofo_currency_get_symbol( currency );
		tot_debit = 0;
		tot_credit = 0;

		str = g_strdup_printf( _( "%s balance" ), code );
		label = gtk_label_new( str );
		g_free( str );
		my_utils_widget_set_style( label, "labelinfo" );
		my_utils_widget_set_xalign( label, 0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 5*i+1, 2, 1 );

		label = gtk_label_new( "" );
		gtk_label_set_width_chars( GTK_LABEL( label ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 5*i+2, 1, 1 );

		label = gtk_label_new( _( "Validated balance :" ));
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 5*i+2, 1, 1 );

		amount = ofo_ledger_get_val_debit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_debit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 5*i+2, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 5*i+2, 1, 1 );

		amount = ofo_ledger_get_val_credit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_credit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 5*i+2, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 5, 5*i+2, 1, 1 );

		label = gtk_label_new( "" );
		gtk_label_set_width_chars( GTK_LABEL( label ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 5*i+3, 1, 1 );

		label = gtk_label_new( _( "Rough balance :" ));
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 5*i+3, 1, 1 );

		amount = ofo_ledger_get_rough_debit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_debit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 5*i+3, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 5*i+3, 1, 1 );

		amount = ofo_ledger_get_rough_credit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_credit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 5*i+3, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 5, 5*i+3, 1, 1 );

		label = gtk_label_new( "" );
		gtk_label_set_width_chars( GTK_LABEL( label ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 5*i+4, 1, 1 );

		label = gtk_label_new( _( "Future balance :" ));
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 5*i+4, 1, 1 );

		amount = ofo_ledger_get_futur_debit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_debit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 5*i+4, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 5*i+4, 1, 1 );

		amount = ofo_ledger_get_futur_credit( priv->ledger, code );
		str = my_double_to_str_ex( amount, digits );
		label = gtk_label_new( str );
		g_free( str );
		tot_credit += amount;
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 5*i+4, 1, 1 );

		label = gtk_label_new( symbol );
		gtk_grid_attach( GTK_GRID( grid ), label, 5, 5*i+4, 1, 1 );

		label = gtk_label_new( "" );
		gtk_label_set_width_chars( GTK_LABEL( label ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 5*i+5, 1, 1 );

		str = g_strdup_printf( _( "Total %s :" ), code );
		label = gtk_label_new( str );
		g_free( str );
		my_utils_widget_set_xalign( label, 1.0 );
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 5*i+5, 1, 1 );

		str = my_double_to_str_ex( tot_debit, digits );
		label = gtk_label_new( str );
		g_free( str );
		my_utils_widget_set_xalign( label, 1.0 );
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 5*i+5, 1, 1 );

		label = gtk_label_new( symbol );
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 5*i+5, 1, 1 );

		str = my_double_to_str_ex( tot_credit, digits );
		label = gtk_label_new( str );
		g_free( str );
		my_utils_widget_set_xalign( label, 1.0 );
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 5*i+5, 1, 1 );

		label = gtk_label_new( symbol );
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_grid_attach( GTK_GRID( grid ), label, 5, 5*i+5, 1, 1 );
	}
}

static void
on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;

	priv = ofa_ledger_properties_get_instance_private( self );

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;

	priv = ofa_ledger_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;

	priv = ofa_ledger_properties_get_instance_private( self );

	if( priv->is_current ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	ofoLedger *exists;
	gboolean ok;
	gchar *msgerr;

	priv = ofa_ledger_properties_get_instance_private( self );

	ok = ofo_ledger_is_valid_data( priv->mnemo, priv->label, &msgerr );

	if( ok ){
		exists = ofo_ledger_get_by_mnemo( priv->hub, priv->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_ledger_get_mnemo( priv->ledger )));
		if( !ok ){
			msgerr = g_strdup( _( "Ledger already exists" ));
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * either creating a new ledger (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaLedgerProperties *self, gchar **msgerr )
{
	ofaLedgerPropertiesPrivate *priv;
	gchar *prev_mnemo;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_ledger_properties_get_instance_private( self );

	prev_mnemo = g_strdup( ofo_ledger_get_mnemo( priv->ledger ));

	/* the new mnemo is not yet used,
	 * or it is used by this same ledger (does not have been modified yet)
	 */
	ofo_ledger_set_mnemo( priv->ledger, priv->mnemo );
	ofo_ledger_set_label( priv->ledger, priv->label );
	my_utils_container_notes_get( self, ledger );

	if( priv->is_new ){
		ok = ofo_ledger_insert( priv->ledger, priv->hub );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new ledger" ));
		}
	} else {
		ok = ofo_ledger_update( priv->ledger, prev_mnemo );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the ledger" ));
		}
	}

	g_free( prev_mnemo );

	return( ok );
}

static void
set_msgerr( ofaLedgerProperties *self, const gchar *msg )
{
	ofaLedgerPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_ledger_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
