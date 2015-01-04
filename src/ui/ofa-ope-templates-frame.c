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

#include <glib/gi18n.h>

#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-ope-templates-book.h"
#include "ui/ofa-ope-templates-frame.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaOpeTemplatesFramePrivate {
	gboolean             dispose_has_run;

	GtkGrid             *grid;
	ofaMainWindow       *main_window;
	gint                 buttons;

	ofaOpeTemplatesBook *book;
	ofaButtonsBox       *box;

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

G_DEFINE_TYPE( ofaOpeTemplatesFrame, ofa_ope_templates_frame, G_TYPE_OBJECT )

static GtkWidget *get_top_grid( ofaOpeTemplatesFrame *frame );
static void       on_new_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame );
static void       on_properties_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame );
static void       on_duplicate_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame );
static void       on_delete_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame );
static void       on_guided_input_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame );
static void       on_book_selection_changed( ofaOpeTemplatesBook *book, const gchar *mnemo, ofaOpeTemplatesFrame *frame );
static void       on_book_selection_activated( ofaOpeTemplatesBook *book, const gchar *mnemo, ofaOpeTemplatesFrame *frame );
static void       update_buttons_sensitivity( ofaOpeTemplatesFrame *self, const gchar *mnemo );
static void       on_frame_closed( ofaOpeTemplatesFrame *frame, void *empty );

static void
ope_templates_frame_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_templates_frame_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_FRAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_frame_parent_class )->finalize( instance );
}

static void
ope_templates_frame_dispose( GObject *instance )
{
	ofaOpeTemplatesFramePrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_FRAME( instance ));

	priv = ( OFA_OPE_TEMPLATES_FRAME( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_frame_parent_class )->dispose( instance );
}

static void
ofa_ope_templates_frame_init( ofaOpeTemplatesFrame *self )
{
	static const gchar *thisfn = "ofa_ope_templates_frame_init";

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATES_FRAME( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATES_FRAME, ofaOpeTemplatesFramePrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_ope_templates_frame_class_init( ofaOpeTemplatesFrameClass *klass )
{
	static const gchar *thisfn = "ofa_ope_templates_frame_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_templates_frame_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_templates_frame_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplatesFramePrivate ));

	/**
	 * ofaOpeTemplatesFrame::changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected operation template mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesFrame *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_OPE_TEMPLATES_FRAME,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplatesFrame::activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected account mnemo.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesFrame *frame,
	 * 						const gchar        *mnemo,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"activated",
				OFA_TYPE_OPE_TEMPLATES_FRAME,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

	/**
	 * ofaOpeTemplatesFrame::closed:
	 *
	 * This signal is sent on the #ofaOpeTemplatesFrame when the book is
	 * about to be closed.
	 *
	 * The #ofaOpeTemplatesBook takes advantage of this signal to save
	 * its own settings.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplatesFrame *store,
	 * 						gpointer            user_data );
	 */
	st_signals[ CLOSED ] = g_signal_new_class_handler(
				"closed",
				OFA_TYPE_OPE_TEMPLATES_FRAME,
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
 * ofa_ope_templates_frame_new:
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
 * | | |   (cf. ofaOpeTemplatesBook class)           |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaOpeTemplatesFrame *
ofa_ope_templates_frame_new( void  )
{
	ofaOpeTemplatesFrame *self;

	self = g_object_new( OFA_TYPE_OPE_TEMPLATES_FRAME, NULL );

	get_top_grid( self );

	g_signal_connect( self, "closed", G_CALLBACK( on_frame_closed ), NULL );

	return( self );
}

/**
 * ofa_ope_templates_frame_attach_to:
 *
 * Attachs the created content to the specified parent.
 */
void
ofa_ope_templates_frame_attach_to( ofaOpeTemplatesFrame *frame, GtkContainer *parent )
{
	ofaOpeTemplatesFramePrivate *priv;
	GtkWidget *grid;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		grid = get_top_grid( frame );
		gtk_container_add( parent, grid );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

/**
 * ofa_ope_templates_frame_set_main_window:
 *
 * Returns the top focusable widget, here the treeview of the current
 * page.
 */
void
ofa_ope_templates_frame_set_main_window( ofaOpeTemplatesFrame *frame, ofaMainWindow *main_window )
{
	ofaOpeTemplatesFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;

		ofa_ope_templates_book_set_main_window( priv->book, main_window );
	}
}

static GtkWidget *
get_top_grid( ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;
	GtkWidget *grid, *alignment;

	priv = frame->priv;

	if( !priv->grid ){

		grid = gtk_grid_new();

		priv->grid = GTK_GRID( grid );
		gtk_widget_set_margin_left( grid, 4 );
		gtk_widget_set_margin_bottom( grid, 4 );

		/* create the operation template notebook
		 */
		alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
		gtk_grid_attach( priv->grid, alignment, 0, 0, 1, 1 );

		priv->book = ofa_ope_templates_book_new();

		ofa_ope_templates_book_attach_to( priv->book, GTK_CONTAINER( alignment ));

		g_signal_connect(
				G_OBJECT( priv->book ), "changed", G_CALLBACK( on_book_selection_changed ), frame );
		g_signal_connect(
				G_OBJECT( priv->book ), "activated", G_CALLBACK( on_book_selection_activated ), frame );
	}

	return( GTK_WIDGET( priv->grid ));
}

static void
on_new_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	priv = frame->priv;
	ofa_ope_templates_book_button_clicked( priv->book, BUTTON_NEW );
}

static void
on_properties_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	priv = frame->priv;
	ofa_ope_templates_book_button_clicked( priv->book, BUTTON_PROPERTIES );
}

static void
on_duplicate_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	priv = frame->priv;
	ofa_ope_templates_book_button_clicked( priv->book, BUTTON_DUPLICATE );
}

static void
on_delete_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	priv = frame->priv;
	ofa_ope_templates_book_button_clicked( priv->book, BUTTON_DELETE );
}

static void
on_guided_input_clicked( GtkButton *button, ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	priv = frame->priv;
	ofa_ope_templates_book_button_clicked( priv->book, BUTTON_GUIDED_INPUT );
}

/**
 * ofa_ope_templates_frame_set_buttons:
 */
void
ofa_ope_templates_frame_set_buttons( ofaOpeTemplatesFrame *frame, gboolean guided_input )
{
	ofaOpeTemplatesFramePrivate *priv;
	GtkWidget *alignment;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
		gtk_grid_attach( priv->grid, alignment, 1, 0, 1, 1 );

		priv->box = ofa_buttons_box_new();
		ofa_buttons_box_attach_to( priv->box, GTK_CONTAINER( alignment ));

		ofa_buttons_box_add_spacer( priv->box );		/* notebook label */
		ofa_buttons_box_add_spacer( priv->box );		/* treeview header */
		ofa_buttons_box_add_button( priv->box,
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
 * ofa_ope_templates_frame_get_selected:
 * @frame:
 *
 * Returns: the currently selected account mnemo, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_ope_templates_frame_get_selected( ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;
	gchar *account;

	g_return_val_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ), NULL );

	priv = frame->priv;
	account = NULL;

	if( !priv->dispose_has_run ){

		account = ofa_ope_templates_book_get_selected( priv->book );
	}

	return( account );
}

/**
 * ofa_ope_templates_frame_set_selected:
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_ope_templates_frame_set_selected( ofaOpeTemplatesFrame *frame, const gchar *mnemo )
{
	ofaOpeTemplatesFramePrivate *priv;

	g_return_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ));

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		ofa_ope_templates_book_set_selected( priv->book, mnemo );
	}
}

/**
 * ofa_ope_templates_frame_get_top_focusable_widget:
 *
 * Returns: top focusable widget.
 */
GtkWidget *
ofa_ope_templates_frame_get_top_focusable_widget( const ofaOpeTemplatesFrame *frame )
{
	ofaOpeTemplatesFramePrivate *priv;

	g_return_val_if_fail( frame && OFA_IS_OPE_TEMPLATES_FRAME( frame ), NULL );

	priv = frame->priv;

	if( !priv->dispose_has_run ){

		return( ofa_ope_templates_book_get_top_focusable_widget( priv->book ));
	}

	return( NULL );
}

static void
on_book_selection_changed( ofaOpeTemplatesBook *book, const gchar *mnemo, ofaOpeTemplatesFrame *frame )
{
	update_buttons_sensitivity( frame, mnemo );
	g_signal_emit_by_name( frame, "changed", mnemo );
}

static void
on_book_selection_activated( ofaOpeTemplatesBook *book, const gchar *mnemo, ofaOpeTemplatesFrame *frame )
{
	g_signal_emit_by_name( frame, "activated", mnemo );
}

static void
update_buttons_sensitivity( ofaOpeTemplatesFrame *self, const gchar *mnemo )
{
	ofaOpeTemplatesFramePrivate *priv;
	ofoOpeTemplate *ope;
	gboolean has_ope;
	ofoDossier *dossier;

	priv = self->priv;

	has_ope = FALSE;
	dossier = ofa_main_window_get_dossier( priv->main_window );

	if( mnemo ){
		ope = ofo_ope_template_get_by_mnemo( dossier, mnemo );
		has_ope = ( ope && OFO_IS_OPE_TEMPLATE( ope ));
	}

	gtk_widget_set_sensitive(
				priv->update_btn,
				has_ope );

	gtk_widget_set_sensitive(
				priv->duplicate_btn,
				has_ope );

	gtk_widget_set_sensitive(
				priv->delete_btn,
				has_ope &&
				ofo_ope_template_is_deletable( ope, dossier ));

	if( priv->guided_input_btn ){
		gtk_widget_set_sensitive(
					priv->guided_input_btn,
					has_ope );
	}
}

static void
on_frame_closed( ofaOpeTemplatesFrame *frame, void *empty )
{
	static const gchar *thisfn = "ofa_ope_templates_frame_on_frame_closed";
	ofaOpeTemplatesFramePrivate *priv;

	g_debug( "%s: frame=%p, empty=%p", thisfn, ( void * ) frame, ( void * ) empty );

	priv = frame->priv;

	g_signal_emit_by_name( priv->book, "closed" );
}
