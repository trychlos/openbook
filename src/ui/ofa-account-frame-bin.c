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

#include "ui/ofa-account-chart-bin.h"
#include "ui/ofa-account-frame-bin.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountFrameBinPrivate {
	gboolean            dispose_has_run;

	GtkGrid            *grid;
	ofaMainWindow      *main_window;
	gint                buttons;

	ofaAccountChartBin *book;
	ofaButtonsBox      *box;

	GtkWidget          *new_btn;
	GtkWidget          *update_btn;
	GtkWidget          *delete_btn;
	GtkWidget          *view_entries_btn;
	GtkWidget          *settlement_btn;
	GtkWidget          *reconciliation_btn;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaAccountFrameBin, ofa_account_frame_bin, GTK_TYPE_BIN )

static void setup_top_grid( ofaAccountFrameBin *frame );
static void on_new_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_properties_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_delete_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *frame );
static void on_book_selection_changed( ofaAccountChartBin *book, const gchar *number, ofaAccountFrameBin *frame );
static void on_book_selection_activated( ofaAccountChartBin *book, const gchar *number, ofaAccountFrameBin *frame );
static void update_buttons_sensitivity( ofaAccountFrameBin *self, const gchar *number );

static void
accounts_frame_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_frame_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FRAME_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_frame_bin_parent_class )->finalize( instance );
}

static void
accounts_frame_dispose( GObject *instance )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FRAME_BIN( instance ));

	priv = ( OFA_ACCOUNT_FRAME_BIN( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_account_frame_bin_init( ofaAccountFrameBin *self )
{
	static const gchar *thisfn = "ofa_account_frame_bin_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_FRAME_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBinPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_account_frame_bin_class_init( ofaAccountFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_frame_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_frame_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountFrameBinPrivate ));

	/**
	 * ofaAccountFrameBin::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						const gchar    *number,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_ACCOUNT_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaAccountFrameBin::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountFrameBin *frame,
	 * 						const gchar    *number,
	 * 						gpointer        user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_ACCOUNT_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );
}

/**
 * ofa_account_frame_bin_new:
 *
 * Creates the structured content, i.e. The accounts notebook on the
 * left column, the buttons box on the right one.
 *
 * +-----------------------------------------------------------------------+
 * | parent container:                                                     |
 * |   this is the grid of the main page,                                  |
 * |   or any another container (i.e. a frame)                             |
 * | +-------------------------------------------------------------------+ |
 * | | creates a grid which will contain the frame and the buttons       | |
 * | | +---------------------------------------------+-----------------+ + |
 * | | | creates a notebook where each page contains | creates         | | |
 * | | |   the account of the corresponding class    |   a buttons box | | |
 * | | |   (cf. ofaAccountChartBin class)               |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountFrameBin *
ofa_account_frame_bin_new( void  )
{
	ofaAccountFrameBin *self;

	self = g_object_new( OFA_TYPE_ACCOUNT_FRAME_BIN, NULL );

	setup_top_grid( self );

	return( self );
}

/*
 * create the top grid which contains the accounts notebook and the
 * buttons, and attach it to our #GtkBin
 */
static void
setup_top_grid( ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *grid;

	priv = frame->priv;

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( frame ), grid );

	priv->grid = GTK_GRID( grid );
	my_utils_widget_set_margin_left( grid, 4 );
	gtk_widget_set_margin_bottom( grid, 4 );

	/* create the accounts notebook
	 */
	priv->book = ofa_account_chart_bin_new();
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->book ), 0, 0, 1, 1 );

	g_signal_connect(
			G_OBJECT( priv->book ), "changed", G_CALLBACK( on_book_selection_changed ), frame );
	g_signal_connect(
			G_OBJECT( priv->book ), "activated", G_CALLBACK( on_book_selection_activated ), frame );

	gtk_widget_show_all( GTK_WIDGET( frame ));
}

/**
 * ofa_account_frame_bin_set_main_window:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
void
ofa_account_frame_bin_set_main_window( ofaAccountFrameBin *frame, ofaMainWindow *main_window )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNT_FRAME_BIN( frame ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		ofa_account_chart_bin_set_main_window( priv->book, main_window );
		ofa_account_chart_bin_expand_all( priv->book );
	}
}

static void
on_new_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_PROPERTIES );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_DELETE );
}

static void
on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_VIEW_ENTRIES );
}

static void
on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_SETTLEMENT );
}

static void
on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_account_chart_bin_button_clicked( priv->book, BUTTON_RECONCILIATION );
}

/**
 * ofa_account_frame_bin_set_buttons:
 */
void
ofa_account_frame_bin_set_buttons( ofaAccountFrameBin *frame, gboolean view_entries, gboolean settlement, gboolean reconciliation )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNT_FRAME_BIN( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->box = ofa_buttons_box_new();
		gtk_grid_attach( priv->grid, GTK_WIDGET( priv->box ), 1, 0, 1, 1 );

		ofa_buttons_box_add_spacer( priv->box );		/* notebook label */
		ofa_buttons_box_add_spacer( priv->box );		/* treeview header */
		priv->new_btn = ofa_buttons_box_add_button( priv->box,
				BUTTON_NEW, TRUE, G_CALLBACK( on_new_clicked ), frame );
		priv->update_btn = ofa_buttons_box_add_button( priv->box,
				BUTTON_PROPERTIES, FALSE, G_CALLBACK( on_properties_clicked ), frame );
		priv->delete_btn = ofa_buttons_box_add_button( priv->box,
				BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), frame );

		if( view_entries ){
			ofa_buttons_box_add_spacer( priv->box );
			priv->view_entries_btn = ofa_buttons_box_add_button( priv->box,
					BUTTON_VIEW_ENTRIES, FALSE, G_CALLBACK( on_view_entries_clicked ), frame );
		}
		if( settlement ){
			priv->settlement_btn = ofa_buttons_box_add_button( priv->box,
					BUTTON_SETTLEMENT, FALSE, G_CALLBACK( on_settlement_clicked ), frame );
		}
		if( reconciliation ){
			priv->reconciliation_btn = ofa_buttons_box_add_button( priv->box,
					BUTTON_RECONCILIATION, FALSE, G_CALLBACK( on_reconciliation_clicked ), frame );
		}
	}
}

/**
 * ofa_account_frame_bin_get_chart:
 * @frame:
 *
 * Returns: the #ofaAccountChartBin book.
 */
ofaAccountChartBin *
ofa_account_frame_bin_get_chart( const ofaAccountFrameBin *frame )
{
	ofaAccountFrameBinPrivate *priv;
	ofaAccountChartBin *book;

	g_return_val_if_fail( frame && OFA_IS_ACCOUNT_FRAME_BIN( frame ), NULL );

	priv = frame->priv;
	book = NULL;

	if( !priv->dispose_has_run ){

		book = priv->book;
	}

	return( book );
}

static void
on_book_selection_changed( ofaAccountChartBin *book, const gchar *number, ofaAccountFrameBin *frame )
{
	update_buttons_sensitivity( frame, number );
	g_signal_emit_by_name( frame, "changed", number );
}

static void
on_book_selection_activated( ofaAccountChartBin *book, const gchar *number, ofaAccountFrameBin *frame )
{
	g_signal_emit_by_name( frame, "activated", number );
}

static void
update_buttons_sensitivity( ofaAccountFrameBin *self, const gchar *number )
{
	ofaAccountFrameBinPrivate *priv;
	ofoDossier *dossier;
	ofoAccount *account;
	gboolean has_account;

	priv = self->priv;
	account = NULL;
	has_account = FALSE;
	dossier = ofa_main_window_get_dossier( priv->main_window );

	if( number ){
		account = ofo_account_get_by_number( dossier, number );
		has_account = ( account && OFO_IS_ACCOUNT( account ));
	}

	gtk_widget_set_sensitive( priv->new_btn,
				ofo_dossier_is_current( dossier ));

	gtk_widget_set_sensitive( priv->update_btn,
				has_account );

	gtk_widget_set_sensitive( priv->delete_btn,
				has_account && ofo_account_is_deletable( account, dossier ));

	if( priv->view_entries_btn ){
		gtk_widget_set_sensitive( priv->view_entries_btn,
				has_account && !ofo_account_is_root( account ));
	}

	if( priv->settlement_btn ){
		gtk_widget_set_sensitive( priv->settlement_btn,
				has_account && ofo_account_is_settleable( account ));
	}

	if( priv->reconciliation_btn ){
		gtk_widget_set_sensitive( priv->reconciliation_btn,
				has_account && ofo_account_is_reconciliable( account ));
	}
}
