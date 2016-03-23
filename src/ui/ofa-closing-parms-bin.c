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

#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-ientry-ope-template.h"
#include "api/ofa-ihubber.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-currency-combo.h"
#include "core/ofa-main-window.h"
#include "core/ofa-ope-template-select.h"

#include "ui/ofa-closing-parms-bin.h"

/* private instance data
 */
struct _ofaClosingParmsBinPrivate {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	GtkWidget      *forward;
	ofaMainWindow  *main_window;
	ofaHub         *hub;
	ofoDossier     *dossier;
	GSList         *currencies;			/* used currencies, from entries */

	/* the closing operations
	 */
	GtkWidget      *sld_ope;
	GtkWidget      *sld_ope_label;
	GtkWidget      *for_ope;
	GtkWidget      *for_ope_label;

	/* the balancing accounts per currency
	 */
	GtkGrid        *grid;
	gint            count;				/* total count of rows in the grid */
};

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define DATA_COMBO                      "ofa-data-combo"

/* the columns in the dynamic grid
 */
enum {
	COL_ADD = 0,
	COL_CURRENCY,
	COL_ACCOUNT,
	COL_REMOVE,
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

static void     setup_bin( ofaClosingParmsBin *self );
static void     setup_closing_opes( ofaClosingParmsBin *bin );
static void     setup_currency_accounts( ofaClosingParmsBin *bin );
static void     ientry_ope_template_iface_init( ofaIEntryOpeTemplateInterface *iface );
static void     on_sld_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self );
static void     on_for_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self );
static void     on_ope_changed( ofaClosingParmsBin *self, GtkWidget *entry, GtkWidget *label );
static void     add_empty_row( ofaClosingParmsBin *self );
static void     add_button( ofaClosingParmsBin *self, const gchar *stock_id, gint column, gint row );
static void     on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaClosingParmsBin *self );
static void     on_account_changed( GtkEntry *entry, ofaClosingParmsBin *self );
static void     on_button_clicked( GtkButton *button, ofaClosingParmsBin *self );
static void     remove_row( ofaClosingParmsBin *self, gint row );
static void     set_currency( ofaClosingParmsBin *self, gint row, const gchar *code );
static void     set_account( ofaClosingParmsBin *self, const gchar *currency, const gchar *account );
static gint     find_currency_row( ofaClosingParmsBin *self, const gchar *currency );
static GObject *get_currency_combo_at( ofaClosingParmsBin *self, gint row );
static void     check_bin( ofaClosingParmsBin *bin );
static gboolean check_for_ope( ofaClosingParmsBin *self, GtkWidget *entry, gchar **msg );
static gboolean check_for_accounts( ofaClosingParmsBin *self, gchar **msg );

G_DEFINE_TYPE_EXTENDED( ofaClosingParmsBin, ofa_closing_parms_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaClosingParmsBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IENTRY_OPE_TEMPLATE, ientry_ope_template_iface_init ))

static void
closing_parms_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLOSING_PARMS_BIN( instance ));

	/* free data members here */

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
 */
ofaClosingParmsBin *
ofa_closing_parms_bin_new( ofaMainWindow *main_window )
{
	ofaClosingParmsBin *bin;
	ofaClosingParmsBinPrivate *priv;;

	bin = g_object_new( OFA_TYPE_CLOSING_PARMS_BIN, NULL );

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	priv->main_window = main_window;

	priv->hub = ofa_main_window_get_hub( main_window );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	priv->dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ), NULL );

	setup_bin( bin );
	setup_closing_opes( bin );
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
	ofa_ientry_ope_template_init(
			OFA_IENTRY_OPE_TEMPLATE( bin ), OFA_MAIN_WINDOW( priv->main_window ), GTK_ENTRY( entry ));
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
	ofa_ientry_ope_template_init(
			OFA_IENTRY_OPE_TEMPLATE( bin ), OFA_MAIN_WINDOW( priv->main_window ), GTK_ENTRY( entry ));
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
setup_closing_opes( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	const gchar *cstr;

	priv = ofa_closing_parms_bin_get_instance_private( bin );

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

static void
setup_currency_accounts( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	gint i;
	GSList *currencies, *it;
	const gchar *currency, *account;

	priv = ofa_closing_parms_bin_get_instance_private( bin );

	priv->grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-grid" ));
	priv->count = 1;
	add_button( bin, "gtk-add", COL_ADD, priv->count );

	/* display all used currencies (from entries)
	 * then display the corresponding account number (if set) */
	priv->currencies = ofo_entry_get_currencies( priv->hub );
	for( i=1, it=priv->currencies ; it ; ++i, it=it->next ){
		add_empty_row( bin );
		set_currency( bin, i, ( const gchar * ) it->data );
	}

	/* for currencies already recorded in dossier_cur,
	 * set the account number  */
	currencies = ofo_dossier_get_currencies( priv->dossier );
	for( it=currencies ; it ; it=it->next ){
		currency = ( const gchar * ) it->data;
		account = ofo_dossier_get_sld_account( priv->dossier, currency );
		set_account( bin, currency, account );
	}
}

/*
 * ofaIEntryOpeTemplate interface management
 */
static void
ientry_ope_template_iface_init( ofaIEntryOpeTemplateInterface *iface )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_ientry_ope_template_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
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
	ofaHub *hub;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	hub = ofa_main_window_get_hub( priv->main_window );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	template = ofo_ope_template_get_by_mnemo( hub, gtk_entry_get_text( GTK_ENTRY( entry )));
	gtk_label_set_text( GTK_LABEL( label ), template ? ofo_ope_template_get_label( template ) : "" );

	check_bin( self );
}

/*
 * insert a row at the given position, counted from zero
 * priv->count maintains the count of rows in the grid, including the
 * headers, but not counting the last row with just an 'Add' button.
 */
static void
add_empty_row( ofaClosingParmsBin *self )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *widget;
	ofaCurrencyCombo *combo;
	gint row;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	row = priv->count;

	gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, COL_ADD, row ));

	/* currency combo box */
	widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_grid_attach( priv->grid, widget, COL_CURRENCY, row, 1, 1 );
	combo = ofa_currency_combo_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( combo ));
	ofa_currency_combo_set_columns( combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_hub( combo, priv->hub );
	g_signal_connect( combo, "ofa-changed", G_CALLBACK( on_currency_changed ), self );
	g_object_set_data( G_OBJECT( combo ), DATA_ROW, GINT_TO_POINTER( row ));
	g_object_set_data( G_OBJECT( widget ), DATA_COMBO, combo);

	/* account number */
	widget = gtk_entry_new();
	g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_entry_set_width_chars( GTK_ENTRY( widget ), 14 );
	gtk_grid_attach( priv->grid, widget, COL_ACCOUNT, row, 1, 1 );
	g_signal_connect( widget, "changed", G_CALLBACK( on_account_changed ), self );

	/* management buttons */
	add_button( self, "gtk-remove", COL_REMOVE, row );
	add_button( self, "gtk-add", COL_ADD, row+1 );

	priv->count += 1;
	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}

/*
 * add an 'Add' button to the row (counted from zero)
 */
static void
add_button( ofaClosingParmsBin *self, const gchar *stock_id, gint column, gint row )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *image, *button;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	button = gtk_button_new();
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( priv->grid, button, column, row, 1, 1 );
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
on_button_clicked( GtkButton *button, ofaClosingParmsBin *self )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	switch( column ){
		case COL_ADD:
			add_empty_row( self );
			break;
		case COL_REMOVE:
			remove_row( self, row );
			break;
	}
}

/*
 * we have clicked on the 'Remove' button on the 'row' row
 * row is counted from zero
 * priv->count maintains the count of rows in the grid, including the
 * headers, but not counting the last row with just an 'Add' button.
 */
static void
remove_row( ofaClosingParmsBin *self, gint row )
{
	ofaClosingParmsBinPrivate *priv;
	gint i, line;
	GtkWidget *widget;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	/* first remove the line
	 * note that there is no 'add' button in a used line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		if( i != COL_ADD ){
			gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, i, row ));
		}
	}

	/* then move the follow lines one row up */
	for( line=row+1 ; line<=priv->count+1 ; ++line ){
		for( i=0 ; i<N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( priv->grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( priv->grid ), widget );
				gtk_grid_attach( priv->grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
	}

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));

	/* last update the lines count */
	priv->count -= 1;
}

/*
 * at initialization time, select the given currency at the given row
 */
static void
set_currency( ofaClosingParmsBin *self, gint row, const gchar *code )
{
	GObject *combo;

	combo = get_currency_combo_at( self, row );
	if( combo ){
		g_return_if_fail( OFA_IS_CURRENCY_COMBO( combo ));
		ofa_currency_combo_set_selected( OFA_CURRENCY_COMBO( combo ), code );
	}
}

/*
 * at initialization time, set the given account for the given currency
 */
static void
set_account( ofaClosingParmsBin *self, const gchar *currency, const gchar *account )
{
	ofaClosingParmsBinPrivate *priv;
	gint row;
	GtkWidget *entry;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	row = find_currency_row( self, currency );

	if( !row ){
		add_empty_row( self );
		row = priv->count;
		set_currency( self, row, currency );
	}

	entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
	if( entry ){
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		gtk_entry_set_text( GTK_ENTRY( entry ), account );
		ofa_account_editable_init(
				GTK_EDITABLE( entry ), priv->main_window, ACCOUNT_ALLOW_DETAIL );
	}
}

static gint
find_currency_row( ofaClosingParmsBin *self, const gchar *currency )
{
	ofaClosingParmsBinPrivate *priv;
	GObject *combo;
	gint row, cmp;
	gchar *selected;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	for( row=1 ; row<=priv->count ; ++row ){

		combo = get_currency_combo_at( self, row );
		if( combo ){
			g_return_val_if_fail( OFA_IS_CURRENCY_COMBO( combo ), -1 );

			selected = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
			cmp = g_utf8_collate( selected, currency );
			g_free( selected );

			if( cmp == 0 ){
				return( row );
			}
		}
	}

	return( -1 );
}

static GObject *
get_currency_combo_at( ofaClosingParmsBin *self, gint row )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *align;
	GObject *combo;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	align = gtk_grid_get_child_at( priv->grid, COL_CURRENCY, row );
	if( !align ){
		return( NULL );
	}
	g_return_val_if_fail( GTK_IS_CONTAINER( align ), NULL );

	combo = g_object_get_data( G_OBJECT( align ), DATA_COMBO );
	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );

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
 */
static gboolean
check_for_ope( ofaClosingParmsBin *self, GtkWidget *entry, gchar **msg )
{
	ofaClosingParmsBinPrivate *priv;
	const gchar *cstr;
	ofoOpeTemplate *ope;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !my_strlen( cstr )){
		if( msg ){
			*msg = g_strdup( _( "Empty operation template mnemonic" ));
		}
		return( FALSE );
	}
	ope = ofo_ope_template_get_by_mnemo( priv->hub, cstr );
	if( !ope ){
		if( msg ){
			*msg = g_strdup_printf( _( "Operation template not found: %s" ), cstr );
		}
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( ope ), FALSE );

	return( TRUE );
}

static gboolean
check_for_accounts( ofaClosingParmsBin *self, gchar **msg )
{
	ofaClosingParmsBinPrivate *priv;
	GSList *it;
	gint row;
	const gchar *acc_number, *acc_currency, *cstr;
	GtkWidget *entry;
	GObject *combo;
	ofoAccount *account;
	gchar *code;
	GSList *cursets, *find;
	gboolean ok;

	priv = ofa_closing_parms_bin_get_instance_private( self );

	ok = TRUE;
	cursets = NULL;

	for( row=1 ; row<priv->count ; ++row ){
		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( OFA_IS_CURRENCY_COMBO( combo ), FALSE );

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( my_strlen( code )){

			entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));
			if( !my_strlen( acc_number )){
				if( msg ){
					*msg = g_strdup_printf( _( "%s: empty account number" ), code );
				}
				ok = FALSE;
				break;
			}

			account = ofo_account_get_by_number( priv->hub, acc_number );
			if( !account ){
				if( msg ){
					*msg = g_strdup_printf(
								_( "%s: invalid account number: %s" ), code, acc_number );
				}
				ok = FALSE;
				break;
			}

			g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
			if( ofo_account_is_root( account )){
				if( msg ){
					*msg = g_strdup_printf(
								_( "%s: unauthorized root account: %s" ), code, acc_number );
				}
				ok = FALSE;
				break;
			}

			if( ofo_account_is_closed( account )){
				if( msg ){
					*msg = g_strdup_printf(
								_( "%s: unauthorized closed account: %s" ), code, acc_number );
				}
				ok = FALSE;
				break;
			}

			acc_currency = ofo_account_get_currency( account );
			if( g_utf8_collate( code, acc_currency )){
				if( msg ){
					*msg = g_strdup_printf(
								_( "%s: incompatible account currency: %s: %s" ), code, acc_number, acc_currency );
				}
				ok = FALSE;
				break;
			}

			find = g_slist_find_custom( cursets, code, ( GCompareFunc ) g_utf8_collate );
			if( find ){
				if( msg ){
					*msg = g_strdup_printf( _( "%s: duplicate currency" ), code );
				}
				ok = FALSE;
				break;
			}

			cursets = g_slist_prepend( cursets, g_strdup( code ));
		}
		g_free( code );
	}

	/* if all currencies are rightly set, also check that at least
	 * mandatory currencies are set */
	if( ok ){
		for( it=priv->currencies ; it ; it=it->next ){
			cstr = ( const gchar * ) it->data;
			find = g_slist_find_custom( cursets, cstr, ( GCompareFunc ) g_utf8_collate );
			if( !find ){
				if( msg ){
					*msg = g_strdup_printf( _( "%s: unset mandatory currency" ), cstr );
				}
				ok = FALSE;
				break;
			}
		}
	}

	g_slist_free_full( cursets, ( GDestroyNotify ) g_free );

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
	gint row;
	GtkWidget *entry;
	GObject *combo;
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

	for( row=1 ; row<priv->count ; ++row ){
		combo = get_currency_combo_at( bin, row );
		g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( my_strlen( code )){

			entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
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
