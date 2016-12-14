/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include <stdlib.h>

#include "my/my-date-editable.h"
#include "my/my-double-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ope-template-editable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ledger-combo.h"
#include "core/ofa-ledger-store.h"

#include "ui/ofa-entry-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	GtkWindow      *parent;
	ofoEntry       *entry;
	gboolean        editable;

	/* runtime
	 */
	ofaHub         *hub;
	gboolean        is_writable;		/* this are needed by macros, but not used in the code */
	gboolean        is_new;				/* this are needed by macros, but not used in the code */

	/* data
	 */
	GDate           dope;
	GDate           deffect;
	ofoAccount     *account;
	ofoLedger      *ledger;
	ofoCurrency    *currency;
	ofoOpeTemplate *template;

	/* UI
	 */
	GtkWidget      *dope_entry;
	GtkWidget      *deffect_entry;
	GtkWidget      *account_entry;
	GtkWidget      *account_label;
	GtkWidget      *account_currency;
	ofaLedgerCombo *ledger_combo;
	GtkWidget      *label_entry;
	GtkWidget      *ref_entry;
	GtkWidget      *template_entry;
	GtkWidget      *template_label;
	GtkWidget      *sens_combo;
	GtkWidget      *amount_entry;
	GtkWidget      *ok_btn;
	GtkWidget      *msg_label;
}
	ofaEntryPropertiesPrivate;

/* a combobox to select between debit and credit
 */
typedef struct {
	const gchar *code;
	const gchar *label;
}
	sSens;

static sSens st_sens[] = {
		{ "DB", N_( "Debit" )},
		{ "CR", N_( "Credit" )},
		{ 0 }
};

enum {
	SENS_COL_CODE = 0,
	SENS_COL_LABEL,
	SENS_N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-entry-properties.ui";

static void       iwindow_iface_init( myIWindowInterface *iface );
static void       iwindow_init( myIWindow *instance );
static void       idialog_iface_init( myIDialogInterface *iface );
static void       idialog_init( myIDialog *instance );
static void       setup_ui_properties( ofaEntryProperties *self );
static GtkWidget *setup_sens_combo( ofaEntryProperties *self );
static void       setup_ui_settlement( ofaEntryProperties *self );
static void       setup_ui_reconciliation( ofaEntryProperties *self );
static void       setup_data( ofaEntryProperties *self );
static void       on_dope_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       on_deffect_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       on_account_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaEntryProperties *self );
static void       on_label_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       on_template_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       on_amount_changed( GtkEntry *entry, ofaEntryProperties *self );
static void       check_for_enable_dlg( ofaEntryProperties *self );
static gboolean   is_dialog_validable( ofaEntryProperties *self );
static gboolean   do_update( ofaEntryProperties *self, gchar **msgerr );
//static void       set_msgerr( ofaEntryProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaEntryProperties, ofa_entry_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaEntryProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
entry_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_properties_parent_class )->finalize( instance );
}

static void
entry_properties_dispose( GObject *instance )
{
	ofaEntryPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_PROPERTIES( instance ));

	priv = ofa_entry_properties_get_instance_private( OFA_ENTRY_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_properties_parent_class )->dispose( instance );
}

static void
ofa_entry_properties_init( ofaEntryProperties *self )
{
	static const gchar *thisfn = "ofa_entry_properties_init";
	ofaEntryPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ENTRY_PROPERTIES( self ));

	priv = ofa_entry_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;
	my_date_clear( &priv->dope );
	my_date_clear( &priv->deffect );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_entry_properties_class_init( ofaEntryPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_entry_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_entry_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @entry: the #ofoEntry to be displayed/updated.
 * @editable: whether the updates are allowed.
 *
 * Display or update the properties of an entry.
 *
 * Note that not all properties are updatable.
 */
void
ofa_entry_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoEntry *entry, gboolean editable )
{
	static const gchar *thisfn = "ofa_entry_properties_run";
	ofaEntryProperties *self;
	ofaEntryPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, entry=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) entry );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_ENTRY_PROPERTIES, NULL );

	priv = ofa_entry_properties_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->entry = entry;
	priv->editable = editable;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_entry_properties_iwindow_init";
	ofaEntryPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_entry_properties_get_instance_private( OFA_ENTRY_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * This dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * entry.
 *
 * As of v0.62, update of an #ofaEntry is not handled here.
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_entry_properties_idialog_init";
	ofaEntryPropertiesPrivate *priv;
	gchar *title;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_entry_properties_get_instance_private( OFA_ENTRY_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	/* v 0.62 */
	/*priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );*/
	priv->is_writable = FALSE;

	if( !ofo_entry_get_number( priv->entry )){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new entry" ));
	} else {
		title = g_strdup( _( "Updating an entry" ));
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	setup_ui_properties( OFA_ENTRY_PROPERTIES( instance ));
	setup_ui_settlement( OFA_ENTRY_PROPERTIES( instance ));
	setup_ui_reconciliation( OFA_ENTRY_PROPERTIES( instance ));
	setup_data( OFA_ENTRY_PROPERTIES( instance ));

	my_utils_container_updstamp_init( instance, entry );
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->editable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_ENTRY_PROPERTIES( instance ));
}

static void
setup_ui_properties( ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;
	GtkWidget *prompt, *entry, *label, *parent;
	static const gint st_ledger_cols[] = { LEDGER_COL_LABEL, -1 };

	priv = ofa_entry_properties_get_instance_private( self );

	/* operation date */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->dope );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite());
	priv->dope_entry = entry;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_dope_changed ), self );

	/* effect date */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->deffect );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite());
	priv->deffect_entry = entry;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_deffect_changed ), self );

	/* account */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	ofa_account_editable_init( GTK_EDITABLE( entry ), priv->getter, ACCOUNT_ALLOW_DETAIL );
	priv->account_entry = entry;
	priv->account_label = label;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_account_changed ), self );

	/* ledger */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->ledger_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->ledger_combo ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, st_ledger_cols );
	ofa_ledger_combo_set_hub( priv->ledger_combo, priv->hub );
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), GTK_WIDGET( priv->ledger_combo ));
	g_signal_connect( priv->ledger_combo, "ofa-changed", G_CALLBACK( on_ledger_changed ), self );

	/* label */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->label_entry = entry;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );

	/* piece reference */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->ref_entry = entry;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	/* operation template */
	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-template-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-template-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-template-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	ofa_ope_template_editable_init( GTK_EDITABLE( entry ), priv->getter );
	priv->template_entry = entry;
	priv->template_label = label;
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_template_changed ), self );

	/* debit/credit amount and currency */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-sens-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-amount-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->sens_combo = setup_sens_combo( self );
	priv->amount_entry = entry;
	priv->account_currency = label;
	gtk_container_add( GTK_CONTAINER( parent ), priv->sens_combo );
	my_double_editable_init_ex( GTK_EDITABLE( entry ),
			g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
			ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), CUR_DEFAULT_DIGITS );
	g_signal_connect( entry, "changed", G_CALLBACK( on_amount_changed ), self );

	/* operation number - ignored for now */
	/* entry number - ignored for now */
}

static GtkWidget *
setup_sens_combo( ofaEntryProperties *self )
{
	GtkWidget *combo;
	GtkListStore *store;
	GtkCellRenderer *cell;
	gint i;
	GtkTreeIter iter;

	combo = gtk_combo_box_new();
	store = gtk_list_store_new( SENS_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1, 0.5 );
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SENS_COL_LABEL );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( combo ), SENS_COL_CODE );

	for( i=0 ; st_sens[i].code ; ++i ){
		gtk_list_store_insert_with_values( store, &iter, -1,
				SENS_COL_CODE, st_sens[i].code, SENS_COL_LABEL, st_sens[i].label, -1 );
	}

	return( combo );
}

static void
setup_ui_settlement( ofaEntryProperties *self )
{
	/* ignored for now */
}

static void
setup_ui_reconciliation( ofaEntryProperties *self )
{
	/* ignored for now */
}

static void
setup_data( ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;
	const GDate *cdate;
	const gchar *cstr;
	ofxAmount amount;

	priv = ofa_entry_properties_get_instance_private( self );

	/* operation date */
	cdate = ofo_entry_get_dope( priv->entry );
	if( my_date_is_valid( cdate )){
		my_date_editable_set_date( GTK_EDITABLE( priv->dope_entry ), cdate );
	}

	/* effect date */
	cdate = ofo_entry_get_deffect( priv->entry );
	if( my_date_is_valid( cdate )){
		my_date_editable_set_date( GTK_EDITABLE( priv->deffect_entry ), cdate );
	}

	/* account */
	cstr = ofo_entry_get_account( priv->entry );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	/* ledger */
	cstr = ofo_entry_get_ledger( priv->entry );
	if( my_strlen( cstr )){
		ofa_ledger_combo_set_selected( priv->ledger_combo, cstr );
	}

	/* label */
	cstr = ofo_entry_get_label( priv->entry );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr );
	}

	/* piece reference */
	cstr = ofo_entry_get_ref( priv->entry );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->ref_entry ), cstr );
	}

	/* template */
	cstr = ofo_entry_get_ope_template( priv->entry );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->template_entry ), cstr );
	}

	/* sens / amount */
	amount = ofo_entry_get_debit( priv->entry );
	if( amount ){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->sens_combo ), "DB" );
	} else {
		amount = ofo_entry_get_credit( priv->entry );
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->sens_combo ), "CR" );
	}
	my_double_editable_set_amount( GTK_EDITABLE( priv->amount_entry ), amount );

	/* ope number - ignored for now */

	/* entry number - ignore for now */
}

static void
on_dope_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;

	priv = ofa_entry_properties_get_instance_private( self );

	my_date_set_from_date( &priv->dope, my_date_editable_get_date( GTK_EDITABLE( entry ), NULL ));

	check_for_enable_dlg( self );
}

static void
on_deffect_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;

	priv = ofa_entry_properties_get_instance_private( self );

	my_date_set_from_date( &priv->deffect, my_date_editable_get_date( GTK_EDITABLE( entry ), NULL ));

	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;
	const gchar *cstr;

	priv = ofa_entry_properties_get_instance_private( self );

	priv->account = NULL;
	priv->currency = NULL;

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		priv->account = ofo_account_get_by_number( priv->hub, cstr );
		if( priv->account ){
			gtk_label_set_text( GTK_LABEL( priv->account_label ), ofo_account_get_label( priv->account ));
			if( !ofo_account_is_root( priv->account )){
				cstr = ofo_account_get_currency( priv->account );
				priv->currency = ofo_currency_get_by_code( priv->hub, cstr );
				if( priv->currency ){
					gtk_label_set_text( GTK_LABEL( priv->account_currency ), ofo_currency_get_label( priv->currency ));
				}
			}
		}
	}

	check_for_enable_dlg( self );
}

static void
on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;

	priv = ofa_entry_properties_get_instance_private( self );

	priv->ledger = NULL;
	if( my_strlen( mnemo )){
		priv->ledger = ofo_ledger_get_by_mnemo( priv->hub, mnemo );
	}

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_template_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;
	const gchar *cstr;

	priv = ofa_entry_properties_get_instance_private( self );

	priv->template = NULL;
	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		priv->template = ofo_ope_template_get_by_mnemo( priv->hub, cstr );
	}

	check_for_enable_dlg( self );
}

static void
on_amount_changed( GtkEntry *entry, ofaEntryProperties *self )
{
	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaEntryProperties *self )
{
	ofaEntryPropertiesPrivate *priv;

	priv = ofa_entry_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

/*
 * As of v0.62, update of an #ofaEntry is not handled here
 */
static gboolean
is_dialog_validable( ofaEntryProperties *self )
{
	return( TRUE );
}

static gboolean
do_update( ofaEntryProperties *self, gchar **msgerr )
{
	return( TRUE );
}

#if 0
static void
set_msgerr( ofaEntryProperties *self, const gchar *msg )
{
	ofaEntryPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_entry_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
#endif
