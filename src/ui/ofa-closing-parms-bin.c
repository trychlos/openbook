/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include "my/my-igridlist.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ope-template-editable.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/ofa-currency-combo.h"
#include "core/ofa-ope-template-select.h"

#include "ui/ofa-closing-parms-bin.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;

	/* runtime data
	 */
	GtkWidget      *forward;
	ofoDossier     *dossier;
	gboolean        is_writable;
	GList          *currencies;			/* knwon currencies */
	gchar          *detail_account;

	/* the closing operations
	 */
	GtkWidget      *sld_ope;
	GtkWidget      *sld_ope_label;
	GtkWidget      *for_ope;
	GtkWidget      *for_ope_label;

	/* the balancing accounts per currency
	 */
	GtkWidget      *acc_grid;
}
	ofaClosingParmsBinPrivate;

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define DATA_COMBO                      "ofa-data-combo"

/* the columns in the dynamic grid
 */
enum {
	COL_CURRENCY = 0,
	COL_ACCOUNT,
	N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-closing-parms-bin.ui";

static void       setup_bin( ofaClosingParmsBin *self );
static void       setup_closing_opes( ofaClosingParmsBin *self );
static void       setup_currencies( ofaClosingParmsBin *self );
static void       setup_currency_accounts( ofaClosingParmsBin *self );
static void       igridlist_iface_init( myIGridlistInterface *iface );
static guint      igridlist_get_interface_version( void );
static void       igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *currency );
static void       setup_detail_widgets( ofaClosingParmsBin *self, GtkGrid *grid, guint row, const gchar *currency );
static void       set_detail_values( ofaClosingParmsBin *self, GtkGrid *grid, guint row, const gchar *currency );
static void       on_sld_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self );
static void       on_for_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self );
static void       on_ope_changed( ofaClosingParmsBin *self, GtkWidget *entry, GtkWidget *label );
static void       on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaClosingParmsBin *self );
static void       on_account_changed( GtkEntry *entry, ofaClosingParmsBin *self );
static void       on_detail_count_changed( myIGridlist *instance, void *empty );
static GtkWidget *get_currency_combo_at( ofaClosingParmsBin *self, gint row );
static void       check_bin( ofaClosingParmsBin *bin );
static gboolean   check_for_ope( ofaClosingParmsBin *self, GtkWidget *entry, gchar **msg );
static gchar     *get_detail_account( ofaClosingParmsBin *self );
static gboolean   check_for_accounts( ofaClosingParmsBin *self, gchar **msg );

G_DEFINE_TYPE_EXTENDED( ofaClosingParmsBin, ofa_closing_parms_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaClosingParmsBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
closing_parms_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_finalize";
	ofaClosingParmsBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLOSING_PARMS_BIN( instance ));

	/* free data members here */
	priv = ofa_closing_parms_bin_get_instance_private( OFA_CLOSING_PARMS_BIN( instance ));

	g_free( priv->detail_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_closing_parms_bin_parent_class )->finalize( instance );
}

static void
closing_parms_bin_dispose( GObject *instance )
{
	ofaClosingParmsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLOSING_PARMS_BIN( instance ));

	priv = ofa_closing_parms_bin_get_instance_private( OFA_CLOSING_PARMS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_closing_parms_bin_parent_class )->dispose( instance );
}

static void
ofa_closing_parms_bin_init( ofaClosingParmsBin *self )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_init";
	ofaClosingParmsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CLOSING_PARMS_BIN( self ));

	priv = ofa_closing_parms_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->currencies = NULL;
	priv->detail_account = NULL;
}

static void
ofa_closing_parms_bin_class_init( ofaClosingParmsBinClass *klass )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = closing_parms_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = closing_parms_bin_finalize;

	/**
	 * ofaClosingParmsBin::changed:
	 *
	 * This signal is sent when one of the field is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaClosingParmsBin *bin,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_CLOSING_PARMS_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

/**
 * ofa_closing_parms_bin_new:
 * @getter: a #ofaIGetter instance.
 */
ofaClosingParmsBin *
ofa_closing_parms_bin_new( ofaIGetter *getter )
{
	ofaClosingParmsBin *bin;
	ofaClosingParmsBinPrivate *priv;
	ofaHub *hub;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_CLOSING_PARMS_BIN, NULL );

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	priv->getter = getter;

	hub = ofa_igetter_get_hub( getter );
	priv->dossier = ofa_hub_get_dossier( hub );
	g_return_val_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ), NULL );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	setup_bin( bin );
	setup_closing_opes( bin );
	setup_currencies( bin );
	setup_currency_accounts( bin );

	return( bin );
}

static void
setup_bin( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label, *prompt;

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "cpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	/* balancing accounts operation
	 * aka. solde */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-bope-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->sld_ope = entry;
	ofa_ope_template_editable_init( GTK_EDITABLE( entry ), priv->getter );
	g_signal_connect( entry, "changed", G_CALLBACK( on_sld_ope_changed ), bin );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-bope-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-bope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->sld_ope_label = label;

	/* carried forward entries operation */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-fope-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->for_ope = entry;
	ofa_ope_template_editable_init( GTK_EDITABLE( entry ), priv->getter );
	g_signal_connect( entry, "changed", G_CALLBACK( on_for_ope_changed ), bin );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-fope-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-fope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->for_ope_label = label;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_closing_opes( ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;
	const gchar *cstr;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	/* operation mnemo for closing entries
	 * - have a default value */
	cstr = ofo_dossier_get_sld_ope( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->sld_ope ), cstr );
	}

	/* forward ope template */
	cstr = ofo_dossier_get_forward_ope( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->for_ope ), cstr );
	}
}

/*
 * Store in our currencies list all known currencies:
 * - all distinct currencies found in entries
 * - distinct currencies already archived in dossier
 * - dossier default currency
 */
static void
setup_currencies( ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;
	GSList *currencies, *it;
	const gchar *cur;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	/* all used currencies from entries
	 * take the ownership of the string */
	currencies = ofo_entry_get_currencies( priv->getter );
	for( it=currencies ; it ; it=it->next ){
		priv->currencies = g_list_insert_sorted( priv->currencies, it->data, ( GCompareFunc ) my_collate );
	}
	g_slist_free( currencies );

	/* currencies already archived in dossier_cur
	 * normally all archived currencies should have at least one entry,
	 * so be already in the list */
	currencies = ofo_dossier_get_currencies( priv->dossier );
	for( it=currencies ; it ; it=it->next ){
		cur = ( const gchar * ) it->data;
		if( !g_list_find_custom( priv->currencies, cur, ( GCompareFunc ) my_collate )){
			priv->currencies = g_list_insert_sorted( priv->currencies, g_strdup( cur ), ( GCompareFunc ) my_collate );
		}
	}
	ofo_dossier_free_currencies( currencies );

	/* add the default currency for the dossier */
	cur = ofo_dossier_get_default_currency( priv->dossier );
	if( !g_list_find_custom( priv->currencies, cur, ( GCompareFunc ) my_collate )){
		priv->currencies = g_list_insert_sorted( priv->currencies, g_strdup( cur ), ( GCompareFunc ) my_collate );
	}
}

/*
 * Setup one row for each known currency
 */
static void
setup_currency_accounts( ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;
	GList *it;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	priv->acc_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-grid" );
	g_return_if_fail( priv->acc_grid && GTK_IS_GRID( priv->acc_grid ));

	my_igridlist_init(
			MY_IGRIDLIST( self ), GTK_GRID( priv->acc_grid ),
			TRUE, priv->is_writable, N_COLUMNS );

	for( it=priv->currencies ; it ; it=it->next ){
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->acc_grid ), it->data );
	}

	g_signal_connect( self, "my-row-changed", G_CALLBACK( on_detail_count_changed ), NULL );
}

/*
 * myIGridlist interface management
 */
static void
igridlist_iface_init( myIGridlistInterface *iface )
{
	static const gchar *thisfn = "ofa_ofa_ope_template_properties_igridlist_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = igridlist_get_interface_version;
	iface->setup_row = igridlist_setup_row;
}

static guint
igridlist_get_interface_version( void )
{
	return( 1 );
}

static void
igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *currency )
{
	ofaClosingParmsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLOSING_PARMS_BIN( instance ));

	priv = ofa_closing_parms_bin_get_instance_private( OFA_CLOSING_PARMS_BIN( instance ));
	g_return_if_fail( grid == GTK_GRID( priv->acc_grid ));

	setup_detail_widgets( OFA_CLOSING_PARMS_BIN( instance ), grid, row, ( const gchar * ) currency );
	set_detail_values( OFA_CLOSING_PARMS_BIN( instance ), grid, row, ( const gchar * ) currency );
}

static void
setup_detail_widgets( ofaClosingParmsBin *self, GtkGrid *grid, guint row, const gchar *currency )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *box, *entry;
	ofaCurrencyCombo *combo;
	static const gint st_currency_cols[] = { CURRENCY_COL_CODE, -1 };

	priv = ofa_closing_parms_bin_get_instance_private( self );

	/* currency combo box */
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	combo = ofa_currency_combo_new();
	gtk_widget_set_sensitive( GTK_WIDGET( combo ), priv->is_writable );
	gtk_container_add( GTK_CONTAINER( box ), GTK_WIDGET( combo ));
	ofa_currency_combo_set_columns( combo, st_currency_cols );
	ofa_currency_combo_set_getter( combo, priv->getter );
	g_signal_connect( combo, "ofa-changed", G_CALLBACK( on_currency_changed ), self );
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, box, 1+COL_CURRENCY, row, 1, 1 );

	/* account number */
	entry = gtk_entry_new();
	gtk_widget_set_sensitive( entry, priv->is_writable );
	ofa_account_editable_init( GTK_EDITABLE( entry ), priv->getter, ACCOUNT_ALLOW_DETAIL );
	g_signal_connect( entry, "changed", G_CALLBACK( on_account_changed ), self );
	my_igridlist_set_widget( MY_IGRIDLIST( self ), grid, entry, 1+COL_ACCOUNT, row, 1, 1 );
}

static void
set_detail_values( ofaClosingParmsBin *self, GtkGrid *grid, guint row, const gchar *currency )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *combo, *entry;
	const gchar *account;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	if( my_strlen( currency )){
		combo = get_currency_combo_at( self, row );
		ofa_currency_combo_set_selected( OFA_CURRENCY_COMBO( combo ), currency );

		entry = gtk_grid_get_child_at( GTK_GRID( priv->acc_grid ), 1+COL_ACCOUNT, row );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		account = ofo_dossier_get_sld_account( priv->dossier, currency );
		gtk_entry_set_text( GTK_ENTRY( entry ), account ? account : "" );
	}
}

static void
on_sld_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	on_ope_changed( self, priv->sld_ope, priv->sld_ope_label );
}

static void
on_for_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	on_ope_changed( self, priv->for_ope, priv->for_ope_label );
}

static void
on_ope_changed( ofaClosingParmsBin *self, GtkWidget *entry, GtkWidget *label )
{
	ofaClosingParmsBinPrivate *priv;
	ofoOpeTemplate *template;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	template = ofo_ope_template_get_by_mnemo( priv->getter, gtk_entry_get_text( GTK_ENTRY( entry )));
	gtk_label_set_text( GTK_LABEL( label ), template ? ofo_ope_template_get_label( template ) : "" );

	check_bin( self );
}

static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaClosingParmsBin *self )
{
	check_bin( self );
}

static void
on_account_changed( GtkEntry *entry, ofaClosingParmsBin *self )
{
	check_bin( self );
}

static void
on_detail_count_changed( myIGridlist *instance, void *empty )
{
	check_bin( OFA_CLOSING_PARMS_BIN( instance ));
}

static GtkWidget *
get_currency_combo_at( ofaClosingParmsBin *self, gint row )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *box, *combo;
	GList *children;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	box = gtk_grid_get_child_at( GTK_GRID( priv->acc_grid ), 1+COL_CURRENCY, row );
	g_return_val_if_fail( box && GTK_IS_BOX( box ), NULL );
	children = gtk_container_get_children( GTK_CONTAINER( box ));
	combo = ( GtkWidget * ) children->data;
	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );
	g_list_free( children );

	return( combo );
}

static void
check_bin( ofaClosingParmsBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_closing_parms_bin_is_valid:
 */
gboolean
ofa_closing_parms_bin_is_valid( ofaClosingParmsBin *bin, gchar **msg )
{
	ofaClosingParmsBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_CLOSING_PARMS_BIN( bin ), FALSE );

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = TRUE;
	if( msg ){
		*msg = NULL;
	}
	if( ok ) ok &= check_for_ope( bin, priv->sld_ope, msg );
	if( ok ) ok &= check_for_ope( bin, priv->for_ope, msg );
	if( ok ) ok &= check_for_accounts( bin, msg );

	return( ok );
}

/*
 * operation template must exist
 * and be autonomous: be able to generate valid entries
 */
static gboolean
check_for_ope( ofaClosingParmsBin *self, GtkWidget *entry, gchar **msg )
{
	ofaClosingParmsBinPrivate *priv;
	const gchar *cstr;
	ofoOpeTemplate *template;
	ofsOpe *ope;
	GDate date;
	ofsOpeDetail *detail;
	GList *currencies;
	gboolean ok;
	gchar *message;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !my_strlen( cstr )){
		if( msg ){
			*msg = g_strdup( _( "Empty operation template mnemonic" ));
		}
		return( FALSE );
	}
	template = ofo_ope_template_get_by_mnemo( priv->getter, cstr );
	if( !template ){
		if( msg ){
			*msg = g_strdup_printf( _( "Operation template not found: %s" ), cstr );
		}
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( template ), FALSE );

	/* check that the ope.template is able to generate valid entries */
	ope = ofs_ope_new( template );
	my_date_set_now( &date );
	my_date_set_from_date( &ope->dope, &date );
	ope->dope_user_set = TRUE;
	detail = ( ofsOpeDetail * ) ope->detail->data;
	detail->account = get_detail_account( self );
	detail->account_user_set = TRUE;
	detail->debit = 100.0;
	detail->debit_user_set = TRUE;

	ofs_ope_apply_template( ope );

	ok = ofs_ope_is_valid( ope, &message, &currencies );
	if( !ok ){
		g_free( message );
		*msg = g_strdup_printf( _( "%s operation template is not valid" ), cstr );
	}
	ofs_currency_list_free( &currencies );
	ofs_ope_free( ope );

	return( ok );
}

static gchar *
get_detail_account( ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;
	GList *dataset, *it;
	ofoAccount *account;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	if( !priv->detail_account ){
		dataset = ofo_account_get_dataset( priv->getter );
		for( it=dataset ; it ; it=it->next ){
			account = OFO_ACCOUNT( it->data );
			if( !ofo_account_is_root( account )){
				priv->detail_account = g_strdup( ofo_account_get_number( account ));
				break;
			}
		}
	}

	return( g_strdup( priv->detail_account ));
}

static gboolean
check_for_accounts( ofaClosingParmsBin *self, gchar **msg )
{
	ofaClosingParmsBinPrivate *priv;
	guint row, details_count;
	gchar *code;
	GList *cursets, *find, *it;
	ofoAccount *account;
	gboolean ok;
	const gchar *acc_number, *acc_currency, *cstr;
	GtkWidget *combo, *entry;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	ok = TRUE;
	cursets = NULL;
	details_count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->acc_grid ));

	for( row=1 ; row<=details_count ; ++row ){
		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), FALSE );

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( my_strlen( code )){

			find = g_list_find_custom( cursets, code, ( GCompareFunc ) my_collate );
			if( find ){
				if( msg ){
					*msg = g_strdup_printf( _( "The currency %s appears to be duplicated" ), code );
				}
				ok = FALSE;
				break;
			}
			cursets = g_list_prepend( cursets, g_strdup( code ));

			entry = gtk_grid_get_child_at( GTK_GRID( priv->acc_grid ), 1+COL_ACCOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));
			if( !my_strlen( acc_number )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "An account is mandatory (currency %s on row %u)" ), code, row );
				}
				ok = FALSE;
				break;
			}

			account = ofo_account_get_by_number( priv->getter, acc_number );
			if( !account ){
				if( msg ){
					*msg = g_strdup_printf(
							_( "The account number '%s' is invalid (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
			if( ofo_account_is_root( account )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "Root account '%s' is no allowed here (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			if( ofo_account_is_closed( account )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "Closed account '%s' is no allowed here (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			if( ofo_account_is_settleable( account )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "Settleable account '%s' is no allowed here (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			if( ofo_account_is_reconciliable( account )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "Reconciliable account '%s' is no allowed here (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			if( ofo_account_is_forwardable( account )){
				if( msg ){
					*msg = g_strdup_printf(
							_( "Forwardable account '%s' is no allowed here (currency %s on row %u)" ), acc_number, code, row );
				}
				ok = FALSE;
				break;
			}

			acc_currency = ofo_account_get_currency( account );
			if( my_collate( code, acc_currency ) != 0 ){
				if( msg ){
					*msg = g_strdup_printf(
							_( "The account '%s' manages %s currency, which is incompatible with currency %s on row %u" ),
							acc_number, acc_currency, code, row );
				}
				ok = FALSE;
				break;
			}
		}
		g_free( code );
	}

	/* if all currencies are rightly set, also check that at least
	 * mandatory currencies are set */
	if( ok ){
		for( it=priv->currencies ; it ; it=it->next ){
			cstr = ( const gchar * ) it->data;
			find = g_list_find_custom( cursets, cstr, ( GCompareFunc ) my_collate );
			if( !find ){
				if( msg ){
					*msg = g_strdup_printf( _( "The mandatory currency %s is not set" ), cstr );
				}
				ok = FALSE;
				break;
			}
		}
	}

	g_list_free_full( cursets, ( GDestroyNotify ) g_free );

	return( ok );
}

/**
 * ofa_closing_parms_bin_apply:
 */
void
ofa_closing_parms_bin_apply( ofaClosingParmsBin *bin )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_apply";
	ofaClosingParmsBinPrivate *priv;
	guint details_count, row;
	GtkWidget *entry, *combo;
	gchar *code;
	const gchar *acc_number;

	g_return_if_fail( bin && OFA_IS_CLOSING_PARMS_BIN( bin ));

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	ofo_dossier_set_forward_ope( priv->dossier,
				gtk_entry_get_text( GTK_ENTRY( priv->for_ope )));

	ofo_dossier_set_sld_ope( priv->dossier,
				gtk_entry_get_text( GTK_ENTRY( priv->sld_ope )));

	ofo_dossier_reset_currencies( priv->dossier );

	details_count = my_igridlist_get_details_count( MY_IGRIDLIST( bin ), GTK_GRID( priv->acc_grid ));

	for( row=1 ; row<=details_count ; ++row ){
		combo = get_currency_combo_at( bin, row );
		g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( my_strlen( code )){

			entry = gtk_grid_get_child_at( GTK_GRID( priv->acc_grid ), 1+COL_ACCOUNT, row );
			g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));

			if( my_strlen( acc_number )){
				g_debug( "%s: code=%s, acc_number=%s", thisfn, code, acc_number );
				ofo_dossier_set_sld_account( priv->dossier, code, acc_number );
			}
		}
		g_free( code );
	}

	ofo_dossier_update_currencies( priv->dossier );
}
