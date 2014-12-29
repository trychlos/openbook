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

#include "api/my-double.h"

#include "ui/ofa-balances-grid.h"

/* private instance data
 */
struct _ofaBalancesGridPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkContainer   *container;
	GtkGrid        *grid;				/* top grid */
};

/* signals defined here
 */
enum {
	UPDATE = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaBalancesGrid, ofa_balances_grid, G_TYPE_OBJECT )

static void on_widget_finalized( ofaBalancesGrid *self, gpointer finalized_parent );
static void on_update( ofaBalancesGrid *self, const gchar *currency, gdouble debit, gdouble credit, void *empty );
static void write_double( ofaBalancesGrid *self, gdouble amount, gint left, gint top );

static void
progress_bar_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_balances_grid_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BALANCES_GRID( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balances_grid_parent_class )->finalize( instance );
}

static void
progress_bar_dispose( GObject *instance )
{
	ofaBalancesGridPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BALANCES_GRID( instance ));

	priv = ( OFA_BALANCES_GRID( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balances_grid_parent_class )->dispose( instance );
}

static void
ofa_balances_grid_init( ofaBalancesGrid *self )
{
	static const gchar *thisfn = "ofa_balances_grid_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BALANCES_GRID( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_BALANCES_GRID, ofaBalancesGridPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_balances_grid_class_init( ofaBalancesGridClass *klass )
{
	static const gchar *thisfn = "ofa_balances_grid_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = progress_bar_dispose;
	G_OBJECT_CLASS( klass )->finalize = progress_bar_finalize;

	g_type_class_add_private( klass, sizeof( ofaBalancesGridPrivate ));

	/**
	 * ofaBalancesGrid::update:
	 *
	 * This signal may be sent to update the balance grid.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBalancesGrid *grid,
	 *						const gchar   *currency,
	 * 						gdouble        debit,
	 * 						gdouble        credit,
	 * 						gpointer       user_data );
	 */
	st_signals[ UPDATE ] = g_signal_new_class_handler(
				"update",
				OFA_TYPE_BALANCES_GRID,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				3,
				G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE );
}

static void
on_widget_finalized( ofaBalancesGrid *self, gpointer finalized_widget )
{
	static const gchar *thisfn = "ofa_balances_grid_on_widget_finalized";

	g_debug( "%s: self=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) self, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	g_return_if_fail( self && OFA_IS_BALANCES_GRID( self ));

	g_object_unref( self );
}

/**
 * ofa_balances_grid_new:
 */
ofaBalancesGrid *
ofa_balances_grid_new( void )
{
	ofaBalancesGrid *self;

	self = g_object_new( OFA_TYPE_BALANCES_GRID, NULL );

	return( self );
}

/**
 * ofa_balances_grid_attach_to:
 */
void
ofa_balances_grid_attach_to( ofaBalancesGrid *self, GtkContainer *new_parent )
{
	ofaBalancesGridPrivate *priv;
	GtkWidget *grid;

	g_return_if_fail( self && OFA_IS_BALANCES_GRID( self ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		grid = gtk_grid_new();
		gtk_container_add( new_parent, grid );
		priv->grid = GTK_GRID( grid );
		gtk_grid_set_column_spacing( priv->grid, 4 );

		g_object_weak_ref( G_OBJECT( grid ), ( GWeakNotify ) on_widget_finalized, self );

		g_signal_connect( G_OBJECT( self ), "update", G_CALLBACK( on_update ), NULL );

		gtk_widget_show_all( GTK_WIDGET( new_parent ));
	}
}

static void
on_update( ofaBalancesGrid *self, const gchar *currency, gdouble debit, gdouble credit, void *empty )
{
	ofaBalancesGridPrivate *priv;
	gint i;
	gboolean found;
	GtkWidget *widget;
	const gchar *cstr;

	g_return_if_fail( self && OFA_IS_BALANCES_GRID( self ));

	priv = self->priv;
	found = FALSE;

	for( i=0 ; ; ++i ){
		widget = gtk_grid_get_child_at( priv->grid, 2, i );
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
		gtk_misc_set_alignment( GTK_MISC( widget ), 1.0, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( widget ), 12 );
		gtk_grid_attach( priv->grid, widget, 0, i, 1, 1 );
		write_double( self, debit, 0, i );

		widget = gtk_label_new( NULL );
		gtk_misc_set_alignment( GTK_MISC( widget ), 1.0, 0.5 );
		gtk_label_set_width_chars( GTK_LABEL( widget ), 12 );
		gtk_grid_attach( priv->grid, widget, 1, i, 1, 1 );
		write_double( self, credit, 1, i );

		widget = gtk_label_new( NULL );
		gtk_misc_set_alignment( GTK_MISC( widget ), 0, 0.5 );
		gtk_grid_attach( priv->grid, widget, 2, i, 1, 1 );
		gtk_label_set_text( GTK_LABEL( widget ), currency );

	} else {
		write_double( self, debit, 0, i );
		write_double( self, credit, 1, i );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

static void
write_double( ofaBalancesGrid *self, gdouble amount, gint left, gint top )
{
	ofaBalancesGridPrivate *priv;
	GtkWidget *widget;
	gchar *str;

	priv = self->priv;

	widget = gtk_grid_get_child_at( priv->grid, left, top );
	str = my_double_to_str( amount );
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );
}
