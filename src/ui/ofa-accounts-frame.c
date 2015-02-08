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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-accounts-book.h"
#include "ui/ofa-accounts-frame.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountsFramePrivate {
	gboolean         dispose_has_run;

	GtkGrid         *grid;
	ofaMainWindow   *main_window;
	gint             buttons;

	ofaAccountsBook *book;
	ofaButtonsBox   *box;

	GtkWidget       *update_btn;
	GtkWidget       *delete_btn;
	GtkWidget       *view_entries_btn;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaAccountsFrame, ofa_accounts_frame, GTK_TYPE_BIN )

static void setup_top_grid( ofaAccountsFrame *frame );
static void on_new_clicked( GtkButton *button, ofaAccountsFrame *frame );
static void on_properties_clicked( GtkButton *button, ofaAccountsFrame *frame );
static void on_delete_clicked( GtkButton *button, ofaAccountsFrame *frame );
static void on_view_entries_clicked( GtkButton *button, ofaAccountsFrame *frame );
static void on_book_selection_changed( ofaAccountsBook *book, const gchar *number, ofaAccountsFrame *frame );
static void on_book_selection_activated( ofaAccountsBook *book, const gchar *number, ofaAccountsFrame *frame );
static void update_buttons_sensitivity( ofaAccountsFrame *self, const gchar *number );

static void
accounts_frame_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_frame_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_FRAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_frame_parent_class )->finalize( instance );
}

static void
accounts_frame_dispose( GObject *instance )
{
	ofaAccountsFramePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_FRAME( instance ));

	priv = ( OFA_ACCOUNTS_FRAME( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_frame_parent_class )->dispose( instance );
}

static void
ofa_accounts_frame_init( ofaAccountsFrame *self )
{
	static const gchar *thisfn = "ofa_accounts_frame_init";

	g_return_if_fail( self && OFA_IS_ACCOUNTS_FRAME( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNTS_FRAME, ofaAccountsFramePrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_accounts_frame_class_init( ofaAccountsFrameClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_frame_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_frame_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_frame_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountsFramePrivate ));

	/**
	 * ofaAccountsFrame::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountsFrame *frame,
	 * 						const gchar    *number,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_ACCOUNTS_FRAME,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaAccountsFrame::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account number.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountsFrame *frame,
	 * 						const gchar    *number,
	 * 						gpointer        user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_ACCOUNTS_FRAME,
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
 * ofa_accounts_frame_new:
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
 * | | |   (cf. ofaAccountsBook class)               |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaAccountsFrame *
ofa_accounts_frame_new( void  )
{
	ofaAccountsFrame *self;

	self = g_object_new( OFA_TYPE_ACCOUNTS_FRAME, NULL );

	setup_top_grid( self );

	return( self );
}

/*
 * create the top grid which contains the accounts notebook and the
 * buttons, and attach it to our #GtkBin
 */
static void
setup_top_grid( ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;
	GtkWidget *grid;

	priv = frame->priv;

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( frame ), grid );

	priv->grid = GTK_GRID( grid );
	gtk_widget_set_margin_left( grid, 4 );
	gtk_widget_set_margin_bottom( grid, 4 );

	/* create the accounts notebook
	 */
	priv->book = ofa_accounts_book_new();
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->book ), 0, 0, 1, 1 );

	g_signal_connect(
			G_OBJECT( priv->book ), "changed", G_CALLBACK( on_book_selection_changed ), frame );
	g_signal_connect(
			G_OBJECT( priv->book ), "activated", G_CALLBACK( on_book_selection_activated ), frame );

	gtk_widget_show_all( GTK_WIDGET( frame ));
}

/**
 * ofa_accounts_frame_set_main_window:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
void
ofa_accounts_frame_set_main_window( ofaAccountsFrame *frame, ofaMainWindow *main_window )
{
	ofaAccountsFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		ofa_accounts_book_set_main_window( priv->book, main_window );
		ofa_accounts_book_expand_all( priv->book );
	}
}

static void
on_new_clicked( GtkButton *button, ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	priv = frame->priv;
	ofa_accounts_book_button_clicked( priv->book, BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	priv = frame->priv;
	ofa_accounts_book_button_clicked( priv->book, BUTTON_PROPERTIES );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	priv = frame->priv;
	ofa_accounts_book_button_clicked( priv->book, BUTTON_DELETE );
}

static void
on_view_entries_clicked( GtkButton *button, ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	priv = frame->priv;
	ofa_accounts_book_button_clicked( priv->book, BUTTON_VIEW_ENTRIES );
}

/**
 * ofa_accounts_frame_set_buttons:
 */
void
ofa_accounts_frame_set_buttons( ofaAccountsFrame *frame, gboolean view_entries )
{
	ofaAccountsFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->box = ofa_buttons_box_new();
		gtk_grid_attach( priv->grid, GTK_WIDGET( priv->box ), 1, 0, 1, 1 );

		ofa_buttons_box_add_spacer( priv->box );		/* notebook label */
		ofa_buttons_box_add_spacer( priv->box );		/* treeview header */
		ofa_buttons_box_add_button( priv->box,
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
	}
}

/**
 * ofa_accounts_frame_get_selected:
 * @frame:
 *
 * Returns: the currently selected account number, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_accounts_frame_get_selected( ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;
	gchar *account;

	g_return_val_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ), NULL );

	priv = frame->priv;
	account = NULL;

	if( !priv->dispose_has_run ){

		account = ofa_accounts_book_get_selected( priv->book );
	}

	return( account );
}

/**
 * ofa_accounts_frame_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_accounts_frame_set_selected( ofaAccountsFrame *frame, const gchar *number )
{
	ofaAccountsFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		ofa_accounts_book_set_selected( priv->book, number );
	}
}

/**
 * ofa_accounts_frame_toggle_collapse:
 *
 * Expand/Collapse the tree if a current selection has children
 */
void
ofa_accounts_frame_toggle_collapse( ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		ofa_accounts_book_toggle_collapse( priv->book );
	}
}

/**
 * ofa_accounts_frame_get_top_focusable_widget:
 *
 * Returns: top focusable widget.
 */
GtkWidget *
ofa_accounts_frame_get_top_focusable_widget( const ofaAccountsFrame *frame )
{
	ofaAccountsFramePrivate *priv;

	g_return_val_if_fail( frame && OFA_IS_ACCOUNTS_FRAME( frame ), NULL );

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		return( ofa_accounts_book_get_top_focusable_widget( priv->book ));
	}

	return( NULL );
}

static void
on_book_selection_changed( ofaAccountsBook *book, const gchar *number, ofaAccountsFrame *frame )
{
	update_buttons_sensitivity( frame, number );
	g_signal_emit_by_name( frame, "changed", number );
}

static void
on_book_selection_activated( ofaAccountsBook *book, const gchar *number, ofaAccountsFrame *frame )
{
	g_signal_emit_by_name( frame, "activated", number );
}

static void
update_buttons_sensitivity( ofaAccountsFrame *self, const gchar *number )
{
	ofaAccountsFramePrivate *priv;
	ofoDossier *dossier;
	ofoAccount *account;
	gboolean has_account;

	priv = self->priv;
	has_account = FALSE;
	dossier = ofa_main_window_get_dossier( priv->main_window );

	if( number ){
		account = ofo_account_get_by_number( dossier, number );
		has_account = ( account && OFO_IS_ACCOUNT( account ));
	}

	gtk_widget_set_sensitive( priv->update_btn,
				has_account );

	gtk_widget_set_sensitive( priv->delete_btn,
				has_account && ofo_account_is_deletable( account, dossier ));

	if( priv->view_entries_btn ){
		gtk_widget_set_sensitive( priv->view_entries_btn,
				has_account && !ofo_account_is_root( account ));
	}
}
