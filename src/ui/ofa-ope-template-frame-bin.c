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
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-ope-template-frame-bin.h"

/* private instance data
 */
struct _ofaOpeTemplateFrameBinPrivate {
	gboolean               dispose_has_run;

	const ofaMainWindow   *main_window;
	ofaHub                *hub;
	gboolean               is_current;
	GtkGrid               *grid;
	gint                   buttons;

	ofaOpeTemplateBookBin *book;
	ofaButtonsBox         *box;

	GtkWidget             *new_btn;
	GtkWidget             *update_btn;
	GtkWidget             *duplicate_btn;
	GtkWidget             *delete_btn;
	GtkWidget             *guided_input_btn;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	CLOSED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaOpeTemplateFrameBin, ofa_ope_template_frame_bin, GTK_TYPE_BIN )

static void setup_bin( ofaOpeTemplateFrameBin *bin );
static void on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin );
static void on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin );
static void on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin );
static void on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin );
static void on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin );
static void on_book_selection_changed( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *bin );
static void on_book_selection_activated( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *bin );
static void update_buttons_sensitivity( ofaOpeTemplateFrameBin *bin, const gchar *mnemo );
static void on_frame_closed( ofaOpeTemplateFrameBin *bin, void *empty );

static void
ope_templates_frame_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->finalize( instance );
}

static void
ope_templates_frame_dispose( GObject *instance )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	priv = ( OFA_OPE_TEMPLATE_FRAME_BIN( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_ope_template_frame_bin_init( ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_init";

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_FRAME_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBinPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_ope_template_frame_bin_class_init( ofaOpeTemplateFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_templates_frame_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_templates_frame_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplateFrameBinPrivate ));

	/**
	 * ofaOpeTemplateFrameBin::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplateFrameBin::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplateFrameBin::closed:
	 *
	 * This signal is sent on the #ofaOpeTemplateFrameBin when the book is
	 * about to be closed.
	 *
	 * The #ofaOpeTemplateBookBin takes advantage of this signal to save
	 * its own settings.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *store,
	 * 						gpointer            user_data );
	 */
	st_signals[ CLOSED ] = g_signal_new_class_handler(
				"ofa-closed",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_ACTION,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_ope_template_frame_bin_new:
 * @main_window: the #ofaMainWindow main window of the application.
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
 * | | |   (cf. ofaOpeTemplateBookBin class)           |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaOpeTemplateFrameBin *
ofa_ope_template_frame_bin_new( const ofaMainWindow *main_window  )
{
	ofaOpeTemplateFrameBin *bin;

	bin = g_object_new( OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, NULL );

	bin->priv->main_window = main_window;

	setup_bin( bin );

	g_signal_connect( bin, "ofa-closed", G_CALLBACK( on_frame_closed ), NULL );

	return( bin );
}

static void
setup_bin( ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkApplication *application;
	ofoDossier *dossier;
	GtkWidget *grid;

	priv = bin->priv;

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	grid = gtk_grid_new();
	my_utils_widget_set_margin( grid, 0, 4, 4, 0 );
	gtk_container_add( GTK_CONTAINER( bin ), grid );
	priv->grid = GTK_GRID( grid );

	/* create the operation template notebook
	 */
	priv->book = ofa_ope_template_book_bin_new( priv->main_window );
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->book ), 0, 0, 1, 1 );

	g_signal_connect(
			G_OBJECT( priv->book ), "ofa-changed", G_CALLBACK( on_book_selection_changed ), bin );
	g_signal_connect(
			G_OBJECT( priv->book ), "ofa-activated", G_CALLBACK( on_book_selection_activated ), bin );
}

static void
on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, TEMPLATE_BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, TEMPLATE_BUTTON_PROPERTIES );
}

static void
on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, TEMPLATE_BUTTON_DUPLICATE );
}

static void
on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, TEMPLATE_BUTTON_DELETE );
}

static void
on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = bin->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, TEMPLATE_BUTTON_GUIDED_INPUT );
}

/**
 * ofa_ope_template_frame_bin_set_buttons:
 */
void
ofa_ope_template_frame_bin_set_buttons( ofaOpeTemplateFrameBin *bin, gboolean guided_input )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->box = ofa_buttons_box_new();
		gtk_grid_attach( priv->grid, GTK_WIDGET( priv->box ), 1, 0, 1, 1 );

		ofa_buttons_box_add_spacer( priv->box );		/* notebook label */
		ofa_buttons_box_add_spacer( priv->box );		/* treeview header */

		priv->new_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), bin );
		gtk_widget_set_sensitive( priv->new_btn, priv->is_current );

		priv->update_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_PROPERTIES, G_CALLBACK( on_properties_clicked ), bin );

		priv->duplicate_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, _( "D_uplicate" ), G_CALLBACK( on_duplicate_clicked ), bin );

		priv->delete_btn =
				ofa_buttons_box_add_button_with_mnemonic(
						priv->box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), bin );

		if( guided_input ){
			ofa_buttons_box_add_spacer( priv->box );
			priv->guided_input_btn =
					ofa_buttons_box_add_button_with_mnemonic(
							priv->box, _( "_Guided input..." ), G_CALLBACK( on_guided_input_clicked ), bin );
		}
	}
}

/**
 * ofa_ope_template_frame_bin_get_book:
 * @bin:
 *
 * Returns: the embedded #ofaOpeTemplateBookBin book.
 */
ofaOpeTemplateBookBin *
ofa_ope_template_frame_bin_get_book( ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofaOpeTemplateBookBin *book;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = bin->priv;
	book = NULL;

	if( !priv->dispose_has_run ){

		book = priv->book;
	}

	return( book );
}

static void
on_book_selection_changed( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *bin )
{
	update_buttons_sensitivity( bin, mnemo );
	g_signal_emit_by_name( bin, "ofa-changed", mnemo );
}

static void
on_book_selection_activated( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *bin )
{
	g_signal_emit_by_name( bin, "ofa-activated", mnemo );
}

static void
update_buttons_sensitivity( ofaOpeTemplateFrameBin *bin, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	gboolean has_ope;

	priv = bin->priv;
	has_ope = FALSE;

	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( priv->hub, mnemo );
		has_ope = ( ope && OFO_IS_OPE_TEMPLATE( ope ));
	}

	gtk_widget_set_sensitive( priv->update_btn,
			has_ope );

	gtk_widget_set_sensitive( priv->duplicate_btn,
			priv->is_current && has_ope );

	gtk_widget_set_sensitive( priv->delete_btn,
			priv->is_current && has_ope && ofo_ope_template_is_deletable( ope ));

	if( priv->guided_input_btn ){
		gtk_widget_set_sensitive( priv->guided_input_btn,
				priv->is_current && has_ope );
	}
}

static void
on_frame_closed( ofaOpeTemplateFrameBin *bin, void *empty )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_on_frame_closed";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, empty=%p", thisfn, ( void * ) bin, ( void * ) empty );

	priv = bin->priv;

	g_signal_emit_by_name( priv->book, "ofa-closed" );
}
