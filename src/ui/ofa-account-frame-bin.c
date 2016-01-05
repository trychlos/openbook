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

#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-ihubber.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-chart-bin.h"
#include "ui/ofa-account-frame-bin.h"

/* private instance data
 */
struct _ofaAccountFrameBinPrivate {
	gboolean             dispose_has_run;

	const ofaMainWindow *main_window;
	ofoDossier          *dossier;
	ofaHub              *hub;
	gboolean             is_current;	/* whether the dossier is current */
	GtkGrid             *grid;
	gint                 buttons;

	ofaAccountChartBin  *account_chart;
	ofaButtonsBox       *box;

	GtkWidget           *new_btn;
	GtkWidget           *update_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *view_entries_btn;
	GtkWidget           *settlement_btn;
	GtkWidget           *reconciliation_btn;
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

static void setup_bin( ofaAccountFrameBin *bin );
static void on_new_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_properties_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_delete_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *bin );
static void on_book_selection_changed( ofaAccountChartBin *book, const gchar *account_id, ofaAccountFrameBin *bin );
static void on_book_selection_activated( ofaAccountChartBin *book, const gchar *account_id, ofaAccountFrameBin *bin );
static void update_buttons_sensitivity( ofaAccountFrameBin *bin, const gchar *account_id );

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
	 * 						const gchar      *account_id,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
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
	 * 						const gchar      *account_id,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
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
 * | | |   (cf. ofaAccountChartBin class)            |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountFrameBin *
ofa_account_frame_bin_new( const ofaMainWindow *main_window  )
{
	ofaAccountFrameBin *bin;

	bin = g_object_new( OFA_TYPE_ACCOUNT_FRAME_BIN, NULL );

	bin->priv->main_window = main_window;

	setup_bin( bin );

	return( bin );
}

/*
 * create the top grid which contains the accounts notebook and the
 * buttons, and attach it to our #GtkBin
 */
static void
setup_bin( ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;
	GtkWidget *grid;
	ofoDossier *dossier;
	GtkApplication *application;

	priv = bin->priv;

	dossier = ofa_main_window_get_dossier( priv->main_window );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->dossier = dossier;
	priv->is_current = ofo_dossier_is_current( dossier );

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));
	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( bin ), grid );

	priv->grid = GTK_GRID( grid );
	my_utils_widget_set_margin_left( grid, 4 );
	gtk_widget_set_margin_bottom( grid, 4 );

	/* create the accounts notebook
	 */
	priv->account_chart = ofa_account_chart_bin_new( priv->main_window );
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->account_chart ), 0, 0, 1, 1 );

	g_signal_connect(
			G_OBJECT( priv->account_chart ), "changed", G_CALLBACK( on_book_selection_changed ), bin );
	g_signal_connect(
			G_OBJECT( priv->account_chart ), "activated", G_CALLBACK( on_book_selection_activated ), bin );

	ofa_account_chart_bin_expand_all( priv->account_chart );
}

static void
on_new_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_PROPERTIES );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_DELETE );
}

static void
on_view_entries_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_VIEW_ENTRIES );
}

static void
on_settlement_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_SETTLEMENT );
}

static void
on_reconciliation_clicked( GtkButton *button, ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_account_chart_bin_button_clicked( priv->account_chart, ACCOUNT_BUTTON_RECONCILIATION );
}

/**
 * ofa_account_frame_bin_set_buttons:
 * @bin: this #ofaAccountFrameBin instance.
 * @view_entries: whether the 'View entries...' button should be displayed.
 * @settlement: whether the 'Settlement...' button should be displayed.
 * @reconciliation: whether the 'Reconciliation...' button should be displayed.
 *
 * Display the required buttons.
 */
void
ofa_account_frame_bin_set_buttons( ofaAccountFrameBin *bin,
				gboolean view_entries, gboolean settlement, gboolean reconciliation )
{
	ofaAccountFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->box = ofa_buttons_box_new();
		gtk_grid_attach( priv->grid, GTK_WIDGET( priv->box ), 1, 0, 1, 1 );

		ofa_buttons_box_add_spacer( priv->box );		/* notebook label */
		ofa_buttons_box_add_spacer( priv->box );		/* treeview header */

		priv->new_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_NEW,
						G_CALLBACK( on_new_clicked ), bin );
		gtk_widget_set_sensitive( priv->new_btn, priv->is_current );

		priv->update_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_PROPERTIES,
						G_CALLBACK( on_properties_clicked ), bin );

		priv->delete_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_DELETE,
						G_CALLBACK( on_delete_clicked ), bin );

		if( view_entries ){
			ofa_buttons_box_add_spacer( priv->box );
			priv->view_entries_btn =
					ofa_buttons_box_add_button_with_mnemonic(
							priv->box, BUTTON_VIEW_ENTRIES,
							G_CALLBACK( on_view_entries_clicked ), bin );
		}
		if( settlement ){
			priv->settlement_btn =
					ofa_buttons_box_add_button_with_mnemonic(
							priv->box, BUTTON_SETTLEMENT,
							G_CALLBACK( on_settlement_clicked ), bin );
		}
		if( reconciliation ){
			priv->reconciliation_btn =
					ofa_buttons_box_add_button_with_mnemonic(
							priv->box, BUTTON_RECONCILIATION,
							G_CALLBACK( on_reconciliation_clicked ), bin );
		}
	}
}

/**
 * ofa_account_frame_bin_get_chart:
 * @bin:
 *
 * Returns: the #ofaAccountChartBin book.
 */
ofaAccountChartBin *
ofa_account_frame_bin_get_chart( const ofaAccountFrameBin *bin )
{
	ofaAccountFrameBinPrivate *priv;
	ofaAccountChartBin *book;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_FRAME_BIN( bin ), NULL );

	priv = bin->priv;
	book = NULL;

	if( !priv->dispose_has_run ){

		book = priv->account_chart;
	}

	return( book );
}

static void
on_book_selection_changed( ofaAccountChartBin *book, const gchar *account_id, ofaAccountFrameBin *bin )
{
	update_buttons_sensitivity( bin, account_id );
	g_signal_emit_by_name( bin, "ofa-changed", account_id );
}

static void
on_book_selection_activated( ofaAccountChartBin *book, const gchar *account_id, ofaAccountFrameBin *bin )
{
	g_signal_emit_by_name( bin, "ofa-activated", account_id );
}

static void
update_buttons_sensitivity( ofaAccountFrameBin *bin, const gchar *account_id )
{
	ofaAccountFrameBinPrivate *priv;
	ofoAccount *account;
	gboolean has_account;

	priv = bin->priv;
	account = NULL;
	has_account = FALSE;

	if( account_id ){
		account = ofo_account_get_by_number( priv->hub, account_id );
		has_account = ( account && OFO_IS_ACCOUNT( account ));
	}

	gtk_widget_set_sensitive( priv->update_btn,
			has_account );

	gtk_widget_set_sensitive( priv->delete_btn,
			priv->is_current && has_account && ofo_account_is_deletable( account ));

	if( priv->view_entries_btn ){
		gtk_widget_set_sensitive( priv->view_entries_btn,
				has_account && !ofo_account_is_root( account ));
	}

	if( priv->settlement_btn ){
		gtk_widget_set_sensitive( priv->settlement_btn,
				priv->is_current && has_account && ofo_account_is_settleable( account ));
	}

	if( priv->reconciliation_btn ){
		gtk_widget_set_sensitive( priv->reconciliation_btn,
				priv->is_current && has_account && ofo_account_is_reconciliable( account ));
	}
}
