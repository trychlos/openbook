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
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-ope-template-frame-bin.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaOpeTemplateFrameBinPrivate {
	gboolean             dispose_has_run;

	GtkGrid             *grid;
	ofaMainWindow       *main_window;
	gint                 buttons;

	ofaOpeTemplateBookBin *book;
	ofaButtonsBox       *box;

	GtkWidget           *new_btn;
	GtkWidget           *update_btn;
	GtkWidget           *duplicate_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *guided_input_btn;
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

static void setup_top_grid( ofaOpeTemplateFrameBin *frame );
static void on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame );
static void on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame );
static void on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame );
static void on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame );
static void on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame );
static void on_book_selection_changed( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *frame );
static void on_book_selection_activated( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *frame );
static void update_buttons_sensitivity( ofaOpeTemplateFrameBin *self, const gchar *mnemo );
static void on_frame_closed( ofaOpeTemplateFrameBin *frame, void *empty );

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
ofa_ope_template_frame_bin_new( void  )
{
	ofaOpeTemplateFrameBin *self;

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, NULL );

	setup_top_grid( self );

	g_signal_connect( self, "ofa-closed", G_CALLBACK( on_frame_closed ), NULL );

	return( self );
}

static void
setup_top_grid( ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *grid;

	priv = frame->priv;

	grid = gtk_grid_new();
	my_utils_widget_set_margin_left( grid, 4 );
	gtk_widget_set_margin_bottom( grid, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), grid );
	priv->grid = GTK_GRID( grid );

	/* create the operation template notebook
	 */
	priv->book = ofa_ope_template_book_bin_new();
	gtk_grid_attach( priv->grid, GTK_WIDGET( priv->book ), 0, 0, 1, 1 );

	g_signal_connect(
			G_OBJECT( priv->book ), "ofa-changed", G_CALLBACK( on_book_selection_changed ), frame );
	g_signal_connect(
			G_OBJECT( priv->book ), "ofa-activated", G_CALLBACK( on_book_selection_activated ), frame );

	gtk_widget_show_all( GTK_WIDGET( frame ));
}

/**
 * ofa_ope_template_frame_bin_set_main_window:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
void
ofa_ope_template_frame_bin_set_main_window( ofaOpeTemplateFrameBin *frame, ofaMainWindow *main_window )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATE_FRAME_BIN( frame ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;
		ofa_ope_template_book_bin_set_main_window( priv->book, main_window );
	}
}

static void
on_new_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, BUTTON_PROPERTIES );
}

static void
on_duplicate_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, BUTTON_DUPLICATE );
}

static void
on_delete_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, BUTTON_DELETE );
}

static void
on_guided_input_clicked( GtkButton *button, ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = frame->priv;
	ofa_ope_template_book_bin_button_clicked( priv->book, BUTTON_GUIDED_INPUT );
}

/**
 * ofa_ope_template_frame_bin_set_buttons:
 */
void
ofa_ope_template_frame_bin_set_buttons( ofaOpeTemplateFrameBin *frame, gboolean guided_input )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATE_FRAME_BIN( frame ));

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
		priv->duplicate_btn = ofa_buttons_box_add_button( priv->box,
				BUTTON_DUPLICATE, FALSE, G_CALLBACK( on_duplicate_clicked ), frame );
		priv->delete_btn = ofa_buttons_box_add_button( priv->box,
				BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), frame );

		if( guided_input ){
			ofa_buttons_box_add_spacer( priv->box );
			priv->guided_input_btn = ofa_buttons_box_add_button( priv->box,
					BUTTON_GUIDED_INPUT, FALSE, G_CALLBACK( on_guided_input_clicked ), frame );
		}
	}
}

/**
 * ofa_ope_template_frame_bin_get_book:
 * @frame:
 *
 * Returns: the embedded #ofaOpeTemplateBookBin book.
 */
ofaOpeTemplateBookBin *
ofa_ope_template_frame_bin_get_book( ofaOpeTemplateFrameBin *frame )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofaOpeTemplateBookBin *book;

	g_return_val_if_fail( frame && OFA_IS_OPE_TEMPLATE_FRAME_BIN( frame ), NULL );

	priv = frame->priv;
	book = NULL;

	if( !priv->dispose_has_run ){

		book = priv->book;
	}

	return( book );
}

static void
on_book_selection_changed( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *frame )
{
	update_buttons_sensitivity( frame, mnemo );
	g_signal_emit_by_name( frame, "ofa-changed", mnemo );
}

static void
on_book_selection_activated( ofaOpeTemplateBookBin *book, const gchar *mnemo, ofaOpeTemplateFrameBin *frame )
{
	g_signal_emit_by_name( frame, "ofa-activated", mnemo );
}

static void
update_buttons_sensitivity( ofaOpeTemplateFrameBin *self, const gchar *mnemo )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	gboolean has_ope;
	ofoDossier *dossier;
	gboolean is_current;

	priv = self->priv;

	has_ope = FALSE;
	dossier = ofa_main_window_get_dossier( priv->main_window );
	is_current = ofo_dossier_is_current( dossier );

	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( dossier, mnemo );
		has_ope = ( ope && OFO_IS_OPE_TEMPLATE( ope ));
	}

	gtk_widget_set_sensitive(
				priv->new_btn,
				is_current );

	gtk_widget_set_sensitive(
				priv->update_btn,
				has_ope );

	gtk_widget_set_sensitive(
				priv->duplicate_btn,
				has_ope && is_current );

	gtk_widget_set_sensitive(
				priv->delete_btn,
				has_ope &&
				ofo_ope_template_is_deletable( ope, dossier ));

	if( priv->guided_input_btn ){
		gtk_widget_set_sensitive(
					priv->guided_input_btn,
					has_ope && is_current );
	}
}

static void
on_frame_closed( ofaOpeTemplateFrameBin *frame, void *empty )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_on_frame_closed";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: frame=%p, empty=%p", thisfn, ( void * ) frame, ( void * ) empty );

	priv = frame->priv;

	g_signal_emit_by_name( priv->book, "ofa-closed" );
}