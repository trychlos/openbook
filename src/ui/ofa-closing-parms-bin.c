/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-template-select.h"

/* private instance data
 */
struct _ofaClosingParmsBinPrivate {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	GtkWidget      *forward;
	ofaMainWindow  *main_window;
	gboolean        is_current;
	ofoDossier     *dossier;
	GSList         *currencies;			/* used currencies, from entries */

	/* the closing operations
	 */
	GtkWidget      *sld_ope;
	GtkWidget      *for_ope;

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
	COL_SELECT,
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

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-closing-parms-bin.ui";
static const gchar *st_ui_id            = "ClosingParmsBin";

G_DEFINE_TYPE( ofaClosingParmsBin, ofa_closing_parms_bin, GTK_TYPE_BIN )

static void     load_dialog( ofaClosingParmsBin *self );
static void     setup_dialog( ofaClosingParmsBin *self );
static void     setup_closing_opes( ofaClosingParmsBin *bin );
static void     setup_currency_accounts( ofaClosingParmsBin *bin );
static void     on_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self );
static void     on_sld_ope_select( GtkButton *button, ofaClosingParmsBin *bin );
static void     on_for_ope_select( GtkButton *button, ofaClosingParmsBin *bin );
static void     add_empty_row( ofaClosingParmsBin *self );
static void     add_button( ofaClosingParmsBin *self, const gchar *stock_id, gint column, gint row );
static void     on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaClosingParmsBin *self );
static void     on_account_changed( GtkEntry *entry, ofaClosingParmsBin *self );
static void     on_account_select( ofaClosingParmsBin *self, gint row );
static void     on_button_clicked( GtkButton *button, ofaClosingParmsBin *self );
static void     remove_row( ofaClosingParmsBin *self, gint row );
static void     set_currency( ofaClosingParmsBin *self, gint row, const gchar *code );
static void     set_account( ofaClosingParmsBin *self, const gchar *currency, const gchar *account );
static gint     find_currency_row( ofaClosingParmsBin *self, const gchar *currency );
static GObject *get_currency_combo_at( ofaClosingParmsBin *self, gint row );
static void     check_bin( ofaClosingParmsBin *bin );
static gboolean check_for_ope( ofaClosingParmsBin *self, GtkWidget *entry, gchar **msg );
static gboolean check_for_accounts( ofaClosingParmsBin *self, gchar **msg );

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

	priv = OFA_CLOSING_PARMS_BIN( instance )->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CLOSING_PARMS_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_CLOSING_PARMS_BIN, ofaClosingParmsBinPrivate );
}

static void
ofa_closing_parms_bin_class_init( ofaClosingParmsBinClass *klass )
{
	static const gchar *thisfn = "ofa_closing_parms_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = closing_parms_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = closing_parms_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaClosingParmsBinPrivate ));

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
				"changed",
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
ofa_closing_parms_bin_new( void )
{
	ofaClosingParmsBin *self;

	self = g_object_new( OFA_TYPE_CLOSING_PARMS_BIN, NULL );

	load_dialog( self );

	return( self );
}

static void
load_dialog( ofaClosingParmsBin *bin )
{
	GtkWidget *window;
	GtkWidget *top_widget;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "closing-top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));
}

/**
 * ofa_closing_parms_bin_set_main_window:
 */
void
ofa_closing_parms_bin_set_main_window( ofaClosingParmsBin *bin, ofaMainWindow *main_window )
{
	ofaClosingParmsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CLOSING_PARMS_BIN( bin ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->is_current = ofo_dossier_is_current( priv->dossier );

		setup_dialog( bin );
	}
}

static void
setup_dialog( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;

	priv = bin->priv;

	if( priv->dossier ){
		setup_closing_opes( bin );
		setup_currency_accounts( bin );
	}

	gtk_widget_show_all( GTK_WIDGET( bin ));
}

static void
setup_closing_opes( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	const gchar *cstr;
	GtkWidget *widget, *image;

	priv = bin->priv;

	/* operation mnemo for closing entries
	 * - have a default value */
	priv->sld_ope = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-bope-entry" );
	g_return_if_fail( priv->sld_ope && GTK_IS_ENTRY( priv->sld_ope ));
	g_signal_connect(
			G_OBJECT( priv->sld_ope ), "changed", G_CALLBACK( on_ope_changed ), bin );

	cstr = ofo_dossier_get_sld_ope( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->sld_ope ), cstr );
	}
	gtk_widget_set_can_focus( priv->sld_ope, priv->is_current );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-bope-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect(
			G_OBJECT( widget ), "clicked", G_CALLBACK( on_sld_ope_select ), bin );
	gtk_widget_set_sensitive( widget, priv->is_current );

	/* forward ope template */
	priv->for_ope = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-fope-entry" );
	g_return_if_fail( priv->for_ope && GTK_IS_ENTRY( priv->for_ope ));
	g_signal_connect(
			G_OBJECT( priv->for_ope ), "changed", G_CALLBACK( on_ope_changed ), bin );

	cstr = ofo_dossier_get_forward_ope( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->for_ope ), cstr );
	}
	gtk_widget_set_can_focus( priv->for_ope, priv->is_current );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-fope-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_for_ope_select ), bin );
	gtk_widget_set_sensitive( widget, priv->is_current );
}

static void
setup_currency_accounts( ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	gint i;
	GSList *currencies, *it;
	const gchar *currency, *account;

	priv = bin->priv;

	priv->grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-grid" ));
	priv->count = 1;
	add_button( bin, "gtk-add", COL_ADD, priv->count );

	/* display all used currencies (from entries)
	 * then display the corresponding account number (if set) */
	priv->currencies = ofo_entry_get_currencies( priv->dossier );
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

static void
on_ope_changed( GtkEditable *editable, ofaClosingParmsBin *self )
{
	check_bin( self );
}

static void
on_sld_ope_select( GtkButton *button, ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	gchar *mnemo;

	priv = bin->priv;

	mnemo = ofa_ope_template_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->sld_ope )));
	if( mnemo ){
		gtk_entry_set_text( GTK_ENTRY( priv->sld_ope ), mnemo );
		g_free( mnemo );
	}

	/* re-check even if the content has not changed
	 * which may happen after having created a operation template */
	check_bin( bin );
}

static void
on_for_ope_select( GtkButton *button, ofaClosingParmsBin *bin )
{
	ofaClosingParmsBinPrivate *priv;
	gchar *mnemo;

	priv = bin->priv;

	mnemo = ofa_ope_template_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->for_ope )));
	if( mnemo ){
		gtk_entry_set_text( GTK_ENTRY( priv->for_ope ), mnemo );
		g_free( mnemo );
	}

	/* re-check even if the content has not changed
	 * which may happen after having created a operation template */
	check_bin( bin );
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

	priv = self->priv;
	row = priv->count;

	gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, COL_ADD, row ));

	/* currency combo box */
	widget = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_grid_attach( priv->grid, widget, COL_CURRENCY, row, 1, 1 );
	combo = ofa_currency_combo_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( combo ));
	ofa_currency_combo_set_columns( combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_main_window( combo, priv->main_window );
	g_signal_connect( combo, "ofa-changed", G_CALLBACK( on_currency_changed ), self );
	g_object_set_data( G_OBJECT( combo ), DATA_ROW, GINT_TO_POINTER( row ));
	g_object_set_data( G_OBJECT( widget ), DATA_COMBO, combo);
	gtk_combo_box_set_button_sensitivity(
			GTK_COMBO_BOX( combo ), priv->is_current ? GTK_SENSITIVITY_AUTO : GTK_SENSITIVITY_OFF );

	/* account number */
	widget = gtk_entry_new();
	g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_entry_set_width_chars( GTK_ENTRY( widget ), 10 );
	gtk_grid_attach( priv->grid, widget, COL_ACCOUNT, row, 1, 1 );
	g_signal_connect( widget, "changed", G_CALLBACK( on_account_changed ), self );
	gtk_widget_set_can_focus( widget, priv->is_current );

	/* account select */
	add_button( self, "gtk-index", COL_SELECT, row );

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

	priv = self->priv;

	button = gtk_button_new();
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->priv->grid, button, column, row, 1, 1 );

	gtk_widget_set_sensitive( button, priv->is_current );
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
on_account_select( ofaClosingParmsBin *self, gint row )
{
	ofaClosingParmsBinPrivate *priv;
	GtkWidget *entry;
	gchar *acc_number;

	priv = self->priv;
	entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	acc_number = ofa_account_select_run(
						priv->main_window,
						gtk_entry_get_text( GTK_ENTRY( entry )), FALSE );

	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), acc_number );
	}

	g_free( acc_number );
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
		case COL_SELECT:
			on_account_select( self, row );
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

	priv = self->priv;

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

	priv = self->priv;
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
	}
}

static gint
find_currency_row( ofaClosingParmsBin *self, const gchar *currency )
{
	ofaClosingParmsBinPrivate *priv;
	GObject *combo;
	gint row, cmp;
	gchar *selected;

	priv = self->priv;

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

	priv = self->priv;

	align = gtk_grid_get_child_at( priv->grid, COL_CURRENCY, row );
	if( !align ){
		return( NULL );
	}
	g_return_val_if_fail( GTK_IS_ALIGNMENT( align ), NULL );

	combo = g_object_get_data( G_OBJECT( align ), DATA_COMBO );
	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );

	return( combo );
}

static void
check_bin( ofaClosingParmsBin *self )
{
	g_signal_emit_by_name( self, "changed" );
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

	priv = bin->priv;
	ok = FALSE;

	if( !priv->dispose_has_run ){

		ok = TRUE;
		if( msg ){
			*msg = NULL;
		}
		if( ok ) ok &= check_for_ope( bin, priv->sld_ope, msg );
		if( ok ) ok &= check_for_ope( bin, priv->for_ope, msg );
		if( ok ) ok &= check_for_accounts( bin, msg );
	}

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

	priv = self->priv;

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		if( msg ){
			*msg = g_strdup( _( "Empty operation template mnemonic" ));
		}
		return( FALSE );
	}
	ope = ofo_ope_template_get_by_mnemo( priv->dossier, cstr );
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

	priv = self->priv;
	ok = TRUE;
	cursets = NULL;

	for( row=1 ; row<priv->count ; ++row ){
		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( OFA_IS_CURRENCY_COMBO( combo ), FALSE );

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( code && g_utf8_strlen( code, -1 )){

			entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));
			if( !acc_number || !g_utf8_strlen( acc_number, -1 )){
				if( msg ){
					*msg = g_strdup_printf( _( "%s: empty account number" ), code );
				}
				ok = FALSE;
				break;
			}

			account = ofo_account_get_by_number( priv->dossier, acc_number );
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
	ofaClosingParmsBinPrivate *priv;
	gint row;
	GtkWidget *entry;
	GObject *combo;
	gchar *code;
	const gchar *acc_number;

	g_return_if_fail( bin && OFA_IS_CLOSING_PARMS_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		ofo_dossier_set_forward_ope( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->for_ope )));

		ofo_dossier_set_sld_ope( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->sld_ope )));

		ofo_dossier_reset_currencies( priv->dossier );

		for( row=1 ; row<priv->count ; ++row ){
			combo = get_currency_combo_at( bin, row );
			g_return_if_fail( OFA_IS_CURRENCY_COMBO( combo ));

			code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
			if( code && g_utf8_strlen( code, -1 )){

				entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
				g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
				acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));

				if( acc_number && g_utf8_strlen( acc_number, -1 )){
					ofo_dossier_set_sld_account( priv->dossier, code, acc_number );
				}
			}
			g_free( code );
		}

		ofo_dossier_update_currencies( priv->dossier );
	}
}
