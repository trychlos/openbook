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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-ledger-arc-treeview.h"
#include "ui/ofa-ledger-properties.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;
	ofoLedger           *ledger;

	/* runtime
	 */
	gchar               *settings_prefix;
	GtkWindow           *actual_parent;
	gboolean             is_writable;
	gboolean             is_new;

	/* data
	 */
	gchar               *mnemo;
	gchar               *label;
    gchar               *upd_user;
    myStampVal          *upd_stamp;
	GDate                closing;

	/* UI
	 */
	GtkWidget           *current_expander;
	GtkWidget           *archived_expander;
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;
}
	ofaLedgerPropertiesPrivate;

/* columns displayed in the exercice combobox
 */
enum {
	EXE_COL_BEGIN = 0,
	EXE_COL_END,
	EXE_N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ledger-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_balances_page( ofaLedgerProperties *self );
static void     init_currency_balance( ofaLedgerProperties *self, GtkGrid *grid, ofoCurrency *currency, gint *row );
static void     init_currency_amount( ofaLedgerProperties *self, GtkGrid *grid, ofoCurrency *currency, gint row, const gchar *balance, ofxAmount debit, ofxAmount credit );
static void     on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void     check_for_enable_dlg( ofaLedgerProperties *self );
static gboolean is_dialog_validable( ofaLedgerProperties *self );
static void     on_ok_clicked( ofaLedgerProperties *self );
static gboolean do_update( ofaLedgerProperties *self, gchar **msgerr );
static void     set_msgerr( ofaLedgerProperties *self, const gchar *msg );
static void     read_settings( ofaLedgerProperties *self );
static void     write_settings( ofaLedgerProperties *self );

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

	g_free( priv->settings_prefix );
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->upd_user );
	my_stamp_free( priv->upd_stamp );

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

		write_settings( OFA_LEDGER_PROPERTIES( instance ));

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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
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
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @ledger: the #ofoLedger to be displayed/updated.
 *
 * Update the properties of an ledger
 */
void
ofa_ledger_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoLedger *ledger )
{
	static const gchar *thisfn = "ofa_ledger_properties_run";
	ofaLedgerProperties *self;
	ofaLedgerPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, ledger=%p",
				thisfn, ( void * ) getter, ( void * ) parent, ( void * ) ledger );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_LEDGER_PROPERTIES, NULL );

	priv = ofa_ledger_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->ledger = ledger;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_ledger_properties_iwindow_init";
	ofaLedgerPropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_ledger_get_mnemo( priv->ledger ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
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
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * ledger
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_ledger_properties_idialog_init";
	ofaLedgerPropertiesPrivate *priv;
	ofaHub *hub;
	gchar *title, *str;
	const gchar *jou_mnemo;
	GtkWidget *entry, *label, *last_close_entry, *btn, *expander;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ledger_properties_get_instance_private( OFA_LEDGER_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

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
	str = my_date_to_str( &priv->closing, ofa_prefs_date_get_display_format( priv->getter ));
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	last_close_entry = entry;

	init_balances_page( OFA_LEDGER_PROPERTIES( instance ));

	my_utils_container_notes_init( instance, ledger );
	my_utils_container_crestamp_init( instance, ledger );
	my_utils_container_updstamp_init( instance, ledger );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );
	my_utils_widget_set_editable( last_close_entry, FALSE );

	/* setup the expanders */
	expander = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p2-current-expander" );
	g_return_if_fail( expander && GTK_IS_EXPANDER( expander ));
	priv->current_expander = expander;

	expander = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p2-archived-expander" );
	g_return_if_fail( expander && GTK_IS_EXPANDER( expander ));
	priv->archived_expander = expander;

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	read_settings( OFA_LEDGER_PROPERTIES( instance ));

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
	ofaLedgerArcTreeview *tview;
	GtkWidget *grid, *parent;
	GList *currencies, *it;
	const gchar *code;
	ofoCurrency *currency;
	gint row;

	priv = ofa_ledger_properties_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-balances-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	row = 1;
	currencies = ofo_ledger_currency_get_list( priv->ledger );
	for( it=currencies ; it ; it=it->next ){
		code = ( const gchar * ) it->data;
		g_return_if_fail( my_strlen( code ));
		currency = ofo_currency_get_by_code( priv->getter, code );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		init_currency_balance( self, GTK_GRID( grid ), currency, &row );
	}
	g_list_free( currencies );

	tview = ofa_ledger_arc_treeview_new( priv->getter, priv->ledger );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-archives" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( tview ));
}

static void
init_currency_balance( ofaLedgerProperties *self, GtkGrid *grid, ofoCurrency *currency, gint *first_row )
{
	ofaLedgerPropertiesPrivate *priv;
	const gchar *code;
	GtkWidget *label;
	gchar *str;
	gint row;

	priv = ofa_ledger_properties_get_instance_private( self );

	code = ofo_currency_get_code( currency );

	row = *first_row;
	str = g_strdup_printf( _( "Balance for %s currency" ), code );
	label = gtk_label_new( str );
	g_free( str );
	gtk_grid_attach( grid, label, 0, row, 3, 1 );

	row += 1;
	label = gtk_label_new( "" );
	gtk_label_set_width_chars( GTK_LABEL( label ), 2 );
	gtk_grid_attach( grid, label, 0, row, 1, 1 );

	label = gtk_label_new( _( "Current period" ));
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_grid_attach( grid, label, 1, row, 2, 1 );

	row += 1;
	init_currency_amount( self, grid, currency, row,
			_( "Rough balance :" ),
			ofo_ledger_get_current_rough_debit( priv->ledger, code ),
			ofo_ledger_get_current_rough_credit( priv->ledger, code ));

	row += 1;
	init_currency_amount( self, grid, currency, row,
			_( "Validated balance :" ),
			ofo_ledger_get_current_val_debit( priv->ledger, code ),
			ofo_ledger_get_current_val_credit( priv->ledger, code ));

	row += 1;
	label = gtk_label_new( "" );
	gtk_label_set_width_chars( GTK_LABEL( label ), 2 );
	gtk_grid_attach( grid, label, 0, row, 1, 1 );

	label = gtk_label_new( _( "Future period" ));
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_grid_attach( grid, label, 1, row, 2, 1 );

	row += 1;
	init_currency_amount( self, grid, currency, row,
			_( "Rough balance :" ),
			ofo_ledger_get_futur_rough_debit( priv->ledger, code ),
			ofo_ledger_get_futur_rough_credit( priv->ledger, code ));

	row += 1;
	init_currency_amount( self, grid, currency, row,
			_( "Validated balance :" ),
			ofo_ledger_get_futur_val_debit( priv->ledger, code ),
			ofo_ledger_get_futur_val_credit( priv->ledger, code ));
}

static void
init_currency_amount( ofaLedgerProperties *self, GtkGrid *grid, ofoCurrency *currency, gint row, const gchar *balance, ofxAmount debit, ofxAmount credit )
{
	ofaLedgerPropertiesPrivate *priv;
	GtkWidget *label;
	gchar *str;
	const gchar *symbol;

	priv = ofa_ledger_properties_get_instance_private( self );

	symbol = ofo_currency_get_symbol( currency );

	label = gtk_label_new( "" );
	gtk_label_set_width_chars( GTK_LABEL( label ), 2 );
	gtk_grid_attach( grid, label, 0, row, 1, 1 );

	label = gtk_label_new( "" );
	gtk_label_set_width_chars( GTK_LABEL( label ), 2 );
	gtk_grid_attach( grid, label, 1, row, 1, 1 );

	label = gtk_label_new( balance );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	gtk_grid_attach( grid, label, 2, row, 1, 1 );

	str = ofa_amount_to_str( debit, currency, priv->getter );
	label = gtk_label_new( str );
	g_free( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	gtk_grid_attach( grid, label, 3, row, 1, 1 );

	label = gtk_label_new( symbol );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_grid_attach( grid, label, 4, row, 1, 1 );

	str = ofa_amount_to_str( credit, currency, priv->getter );
	label = gtk_label_new( str );
	g_free( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	gtk_grid_attach( grid, label, 5, row, 1, 1 );

	label = gtk_label_new( symbol );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_grid_attach( grid, label, 6, row, 1, 1 );
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

	if( priv->is_writable ){
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
		exists = ofo_ledger_get_by_mnemo( priv->getter, priv->mnemo );
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
static void
on_ok_clicked( ofaLedgerProperties *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

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
		ok = ofo_ledger_insert( priv->ledger );
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
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}

/*
 * settings: current_expander;archived_expander;
 */
static void
read_settings( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	myISettings *settings;
	gchar *key;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_ledger_properties_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	gtk_expander_set_expanded( GTK_EXPANDER( priv->current_expander ), my_utils_boolean_from_str( cstr ));

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	gtk_expander_set_expanded( GTK_EXPANDER( priv->archived_expander ), my_utils_boolean_from_str( cstr ));

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

/*
 * settings: current_expander;archived_expander;
 */
static void
write_settings( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_ledger_properties_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;",
			gtk_expander_get_expanded( GTK_EXPANDER( priv->current_expander )) ? "True":"False",
			gtk_expander_get_expanded( GTK_EXPANDER( priv->archived_expander )) ? "True":"False" );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
}
