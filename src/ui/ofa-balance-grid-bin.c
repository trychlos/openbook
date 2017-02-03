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

#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-balance-grid-bin.h"

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
	COL_DEBIT = 0,
	COL_SPACE,
	COL_CREDIT,
	COL_CURRENCY
};

/* signals defined here
 */
enum {
	UPDATE = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void setup_grid( ofaBalanceGridBin *self );
static void on_update( ofaBalanceGridBin *self, const gchar *currency, gdouble debit, gdouble credit, void *empty );
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
	 * ofaBalanceGridBin::update:
	 *
	 * This signal may be sent to update the balance grid.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBalanceGridBin *grid,
	 *						const gchar   *currency,
	 * 						gdouble        debit,
	 * 						gdouble        credit,
	 * 						gpointer       user_data );
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
				3,
				G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE );
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
	GtkWidget *grid;

	priv = ofa_balance_grid_bin_get_instance_private( self );

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( self ), grid );

	priv->grid = GTK_GRID( grid );
	gtk_grid_set_column_spacing( priv->grid, 4 );

	g_signal_connect( self, "ofa-update", G_CALLBACK( on_update ), NULL );

	gtk_widget_show_all( GTK_WIDGET( self ));
}

static void
on_update( ofaBalanceGridBin *self, const gchar *currency, gdouble debit, gdouble credit, void *empty )
{
	ofaBalanceGridBinPrivate *priv;
	gint i;
	gboolean found;
	GtkWidget *widget;
	const gchar *cstr;

	g_return_if_fail( self && OFA_IS_BALANCE_GRID_BIN( self ));

	priv = ofa_balance_grid_bin_get_instance_private( self );

	found = FALSE;

	for( i=0 ; ; ++i ){
		widget = gtk_grid_get_child_at( priv->grid, COL_CURRENCY, i );
		if( !widget ){
			break;
		}
		g_return_if_fail( GTK_IS_LABEL( widget ));
		cstr = gtk_label_get_text( GTK_LABEL( widget ));
		if( !g_utf8_collate( cstr, currency )){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		widget = gtk_label_new( NULL );
		my_utils_widget_set_xalign( widget, 1.0 );
		gtk_label_set_width_chars( GTK_LABEL( widget ), 12 );
		gtk_grid_attach( priv->grid, widget, COL_DEBIT, i, 1, 1 );
		write_double( self, debit, COL_DEBIT, i );

		widget = gtk_label_new( "" );
		gtk_widget_set_hexpand( widget, TRUE );
		gtk_grid_attach( priv->grid, widget, COL_SPACE, i, 1, 1 );

		widget = gtk_label_new( NULL );
		my_utils_widget_set_xalign( widget, 1.0 );
		gtk_label_set_width_chars( GTK_LABEL( widget ), 12 );
		gtk_grid_attach( priv->grid, widget, COL_CREDIT, i, 1, 1 );
		write_double( self, credit, COL_CREDIT, i );

		widget = gtk_label_new( NULL );
		my_utils_widget_set_xalign( widget, 0 );
		gtk_grid_attach( priv->grid, widget, COL_CURRENCY, i, 1, 1 );
		gtk_label_set_text( GTK_LABEL( widget ), currency );

	} else {
		write_double( self, debit, COL_DEBIT, i );
		write_double( self, credit, COL_CREDIT, i );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
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
