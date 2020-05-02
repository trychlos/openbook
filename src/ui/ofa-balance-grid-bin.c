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

#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofo-currency.h"

#include "ui/ofa-balance-grid-bin.h"

#define BALANCE_GRID_GROUP              "ofa-balance-grid-bin-group"
#define BALANCE_GRID_CURRENCY           "ofa-balance-grid-bin-currency"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* UI
	 */
	GtkGrid    *grid;						/* top grid */
}
	ofaBalanceGridBinPrivate;

/* columns of the balance grid
 */
enum {
	COL_LABEL = 0,
	COL_DEBIT,
	COL_SPACE,
	COL_CREDIT,
	COL_CURRENCY,
	COL_STATUS,
	COL_PERIOD,
	COL_REF
};

/* signals defined here
 */
enum {
	UPDATE = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void setup_grid( ofaBalanceGridBin *self );
static void on_update( ofaBalanceGridBin *self, guint group, const gchar *currency, gdouble debit, gdouble credit, void *empty );
static void do_update( ofaBalanceGridBin *self, guint group, const gchar *currency, gdouble debit, gdouble credit );
static gint find_currency_row( ofaBalanceGridBin *self, guint group, const gchar *currency );
static gint add_currency_row( ofaBalanceGridBin *self, guint group, const gchar *currency );
static void grid_insert_row( ofaBalanceGridBin *self, guint group, gint row );
static void write_double( ofaBalanceGridBin *self, gdouble amount, gint left, gint top );

G_DEFINE_TYPE_EXTENDED( ofaBalanceGridBin, ofa_balance_grid_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaBalanceGridBin ))

static void
balance_grid_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_balance_grid_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BALANCE_GRID_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_grid_bin_parent_class )->finalize( instance );
}

static void
balance_grid_bin_dispose( GObject *instance )
{
	ofaBalanceGridBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BALANCE_GRID_BIN( instance ));

	priv = ofa_balance_grid_bin_get_instance_private( OFA_BALANCE_GRID_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_grid_bin_parent_class )->dispose( instance );
}

static void
ofa_balance_grid_bin_init( ofaBalanceGridBin *self )
{
	static const gchar *thisfn = "ofa_balance_grid_bin_init";
	ofaBalanceGridBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BALANCE_GRID_BIN( self ));

	priv = ofa_balance_grid_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_balance_grid_bin_class_init( ofaBalanceGridBinClass *klass )
{
	static const gchar *thisfn = "ofa_balance_grid_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = balance_grid_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = balance_grid_bin_finalize;

	/**
	 * ofaBalanceGridBin::ofa-update:
	 *
	 * This signal is to be sent to update the balance grid.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBalanceGridBin *grid,
	 *						guint            group,
	 *						const gchar     *currency,
	 * 						gdouble          debit,
	 * 						gdouble          credit,
	 * 						gpointer         user_data );
	 */
	st_signals[ UPDATE ] = g_signal_new_class_handler(
				"ofa-update",
				OFA_TYPE_BALANCE_GRID_BIN,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				4,
				G_TYPE_UINT, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE );
}

/**
 * ofa_balance_grid_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofaBalanceGridBin instance.
 */
ofaBalanceGridBin *
ofa_balance_grid_bin_new( ofaIGetter *getter )
{
	ofaBalanceGridBin *self;
	ofaBalanceGridBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	self = g_object_new( OFA_TYPE_BALANCE_GRID_BIN, NULL );

	priv = ofa_balance_grid_bin_get_instance_private( self );

	priv->getter = getter;

	setup_grid( self );

	return( self );
}

static void
setup_grid( ofaBalanceGridBin *self )
{
	ofaBalanceGridBinPrivate *priv;
	GtkWidget *grid, *label;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( self ), grid );

	priv->grid = GTK_GRID( grid );
	gtk_grid_set_column_spacing( priv->grid, 4 );

	label = gtk_label_new( _( "Current rough :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->grid, label, COL_LABEL, 0, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, 0, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( BALANCEGRID_CURRENT_ROUGH ));

	label = gtk_label_new( _( "Current validated :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->grid, label, COL_LABEL, 1, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, 1, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( BALANCEGRID_CURRENT_VALIDATED ));

	label = gtk_label_new( _( "Future rough :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->grid, label, COL_LABEL, 2, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, 2, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( BALANCEGRID_FUTUR_ROUGH ));

	label = gtk_label_new( _( "Future validated :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->grid, label, COL_LABEL, 3, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, 3, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( BALANCEGRID_FUTUR_VALIDATED ));

	label = gtk_label_new( _( "Total :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->grid, label, COL_LABEL, 4, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, 4, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( BALANCEGRID_TOTAL ));

	g_signal_connect( self, "ofa-update", G_CALLBACK( on_update ), NULL );

	gtk_widget_show_all( GTK_WIDGET( self ));
}

/**
 * ofa_balance_grid_bin_set_amounts:
 * @bin: this #ofaBalanceGridBin widget.
 * @group: the targeted row group.
 * @currency: the currency code.
 * @debit: the debit to be displayed.
 * @credit: the credit to be displayed.
 *
 * Update the balance.
 */
void
ofa_balance_grid_bin_set_amounts( ofaBalanceGridBin *bin, guint group, const gchar *currency, ofxAmount debit, ofxAmount credit )
{
	ofaBalanceGridBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_BALANCE_GRID_BIN( bin ));

	priv = ofa_balance_grid_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	do_update( bin, group, currency, debit, credit );
}

/**
 * ofa_balance_grid_bin_set_currency:
 * @bin: this #ofaBalanceGridBin widget.
 * @group: the targeted row group.
 * @sbal: a #ofsCurrency to be displayed.
 *
 * Update the balance.
 */
void
ofa_balance_grid_bin_set_currency( ofaBalanceGridBin *bin, guint group, ofsCurrency *sbal )
{
	ofaBalanceGridBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_BALANCE_GRID_BIN( bin ));

	priv = ofa_balance_grid_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	do_update( bin, group, ofo_currency_get_code( sbal->currency ), sbal->debit, sbal->credit );
}

static void
on_update( ofaBalanceGridBin *self, guint group, const gchar *currency, gdouble debit, gdouble credit, void *empty )
{
	do_update( self, group, currency, debit, credit );
}

static void
do_update( ofaBalanceGridBin *self, guint group, const gchar *currency, gdouble debit, gdouble credit )
{
	ofaBalanceGridBinPrivate *priv;
	gint currency_row;

	g_return_if_fail( self && OFA_IS_BALANCE_GRID_BIN( self ));

	if( debit || credit ){

		priv = ofa_balance_grid_bin_get_instance_private( self );

		currency_row = find_currency_row( self, group, currency );

		if( currency_row == -1 ){
			currency_row = add_currency_row( self, group, currency );
		}

		if( currency_row == -1 ){
			return;
		}

		write_double( self, debit, COL_DEBIT, currency_row );
		write_double( self, credit, COL_CREDIT, currency_row );

		gtk_widget_show_all( GTK_WIDGET( priv->grid ));

		/* let Gtk update the display */
		while( gtk_events_pending()){
			gtk_main_iteration();
		}
	}
}

/*
 * @group:
 * @currency:
 *
 * Find the row for the specified @row and the specified @currency.
 *
 * Returns: the index of the row, or -1.
 */
static gint
find_currency_row( ofaBalanceGridBin *self, guint group, const gchar *currency )
{
	ofaBalanceGridBinPrivate *priv;
	guint i;
	GtkWidget *widget;
	guint widget_group;
	const gchar *widget_currency;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	for( i=0 ; ; ++i ){
		widget = gtk_grid_get_child_at( priv->grid, COL_REF, i );
		if( !widget ){
			break;
		}
		g_return_val_if_fail( GTK_IS_LABEL( widget ), -1 );

		widget_group = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( widget ), BALANCE_GRID_GROUP ));
		if( widget_group == group ){
			widget_currency = g_object_get_data( G_OBJECT( widget ), BALANCE_GRID_CURRENCY );
			if( !my_collate( widget_currency, currency )){
				return( i );
			}
		}
	}

	return( -1 );
}

/*
 * @group:
 * @currency:
 *
 * Insert a new row in the specified @group for the specified @currency.
 *
 * Returns: the index of the new row.
 */
static gint
add_currency_row( ofaBalanceGridBin *self, guint group, const gchar *currency )
{
	ofaBalanceGridBinPrivate *priv;
	guint i;
	GtkWidget *widget, *label;
	guint widget_group;
	const gchar *widget_currency;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	for( i=0 ; ; ++i ){
		widget_group = 0;
		widget_currency = NULL;
		widget = gtk_grid_get_child_at( priv->grid, COL_REF, i );
		if( !widget ){
			break;
		}
		g_return_val_if_fail( GTK_IS_LABEL( widget ), -1 );

		widget_group = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( widget ), BALANCE_GRID_GROUP ));
		if( widget_group == group ){
			widget_currency = g_object_get_data( G_OBJECT( widget ), BALANCE_GRID_CURRENCY );
			if( !widget_currency || my_collate( widget_currency, currency ) > 0 ){
				break;
			}
		} else if( widget_group > group ){
			break;
		}
	}

	/* i points past the last row - the grid will be automatically extended */
	if( !widget_group ){
		;

	/* i points to the first row of the next group - insert a row */
	} else if( widget_group > group ){
		grid_insert_row( self, group, i );

	/* i points to the right group but the currency is empty */
	} else if( !widget_currency ){
		;

	/* i points to the next currency */
	} else {
		grid_insert_row( self, group, i );
	}

	label = gtk_label_new( NULL );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_widget_set_hexpand( label, TRUE );
	gtk_label_set_width_chars( GTK_LABEL( label ), 15 );
	gtk_grid_attach( priv->grid, label, COL_DEBIT, i, 1, 1 );

	label = gtk_label_new( NULL );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_widget_set_hexpand( label, TRUE );
	gtk_label_set_width_chars( GTK_LABEL( label ), 15 );
	gtk_grid_attach( priv->grid, label, COL_CREDIT, i, 1, 1 );

	label = gtk_label_new( currency );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 4 );
	gtk_grid_attach( priv->grid, label, COL_CURRENCY, i, 1, 1 );

	label = gtk_grid_get_child_at( priv->grid, COL_REF, i );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), -1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_CURRENCY, g_strdup( currency ));

	return( i );
}

static void
grid_insert_row( ofaBalanceGridBin *self, guint group, gint row )
{
	ofaBalanceGridBinPrivate *priv;
	GtkWidget *label;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	gtk_grid_insert_row( priv->grid, row );

	label = gtk_label_new( "" );
	gtk_widget_set_hexpand( label, TRUE );
	gtk_grid_attach( priv->grid, label, COL_SPACE, row, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_grid_attach( priv->grid, label, COL_REF, row, 1, 1 );
	g_object_set_data( G_OBJECT( label ), BALANCE_GRID_GROUP, GINT_TO_POINTER( group ));
}

static void
write_double( ofaBalanceGridBin *self, gdouble amount, gint left, gint top )
{
	ofaBalanceGridBinPrivate *priv;
	GtkWidget *widget;
	gchar *str;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	widget = gtk_grid_get_child_at( priv->grid, left, top );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));

	str = ofa_amount_to_str( amount, NULL, priv->getter );
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );
}
