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

#include "ui/ofa-buttons-box.h"

/* private instance data
 */
struct _ofaButtonsBoxPrivate {
	gboolean   dispose_has_run;

	/* internals
	 */
	GtkWidget *alignment;
	GtkGrid   *grid;
	gint       rows;
	gint       spacers;
};

#define BUTTON_ID                       "button-id"

/* some styles layout
 */
#define STYLE_ROW_MARGIN                4
#define STYLE_SPACER                    28

G_DEFINE_TYPE( ofaButtonsBox, ofa_buttons_box, G_TYPE_OBJECT )

static void       on_widget_finalized( ofaButtonsBox *box, gpointer finalized_parent );
static GtkWidget *get_top_grid( ofaButtonsBox *box );

static void
buttons_box_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_buttons_box_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BUTTONS_BOX( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_buttons_box_parent_class )->finalize( instance );
}

static void
buttons_box_dispose( GObject *instance )
{
	ofaButtonsBoxPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BUTTONS_BOX( instance ));

	priv = ( OFA_BUTTONS_BOX( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_buttons_box_parent_class )->dispose( instance );
}

static void
ofa_buttons_box_init( ofaButtonsBox *self )
{
	static const gchar *thisfn = "ofa_buttons_box_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BUTTONS_BOX( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BUTTONS_BOX, ofaButtonsBoxPrivate );
}

static void
ofa_buttons_box_class_init( ofaButtonsBoxClass *klass )
{
	static const gchar *thisfn = "ofa_buttons_box_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = buttons_box_dispose;
	G_OBJECT_CLASS( klass )->finalize = buttons_box_finalize;

	g_type_class_add_private( klass, sizeof( ofaButtonsBoxPrivate ));
}

/**
 * ofa_buttons_box_new:
 */
ofaButtonsBox *
ofa_buttons_box_new( void )
{
	ofaButtonsBox *box;

	box = g_object_new( OFA_TYPE_BUTTONS_BOX, NULL );

	return( box );
}

/**
 * ofa_buttons_box_attach_to:
 * @box: this #ofaButtonsBox object.
 * @parent
 *
 * Attach the buttons box to the parent.
 */
void
ofa_buttons_box_attach_to( ofaButtonsBox *box, GtkContainer *parent )
{
	GtkWidget *grid;

	g_return_if_fail( box && OFA_IS_BUTTONS_BOX( box ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	grid = get_top_grid( box );
	gtk_container_add( parent, grid );
	g_object_weak_ref( G_OBJECT( grid ), ( GWeakNotify ) on_widget_finalized, box );

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

static void
on_widget_finalized( ofaButtonsBox *box, gpointer finalized_widget )
{
	static const gchar *thisfn = "ofa_buttons_box_on_widget_finalized";

	g_debug( "%s: box=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) box, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	g_return_if_fail( box );

	g_object_unref( G_OBJECT( box ));
}

static GtkWidget *
get_top_grid( ofaButtonsBox *box )
{
	ofaButtonsBoxPrivate *priv;
	GtkWidget *grid;

	priv = box->priv;

	if( !priv->grid ){

		priv->alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
		gtk_alignment_set_padding( GTK_ALIGNMENT( priv->alignment ), 0, 0, 8, 8 );

		grid = gtk_grid_new();
		gtk_container_add( GTK_CONTAINER( priv->alignment ), grid );
		gtk_grid_set_row_spacing( GTK_GRID( grid ), STYLE_ROW_MARGIN );

		priv->grid = GTK_GRID( grid );
		priv->rows = 0;
		priv->spacers = 0;
	}

	return( priv->alignment );
}

/**
 * ofa_buttons_box_add_spacer:
 * @box: this #ofaButtonsBox object.
 *
 * Adds a spacer at the end of the buttons box.
 */
void
ofa_buttons_box_add_spacer( ofaButtonsBox *box )
{
	ofaButtonsBoxPrivate *priv;

	g_return_if_fail( box && OFA_IS_BUTTONS_BOX( box ));

	priv = box->priv;

	if( !priv->dispose_has_run ){

		get_top_grid( box );
		priv->spacers += 1;
	}
}

/**
 * ofa_buttons_box_add_button:
 * @box: this #ofaButtonsBox object.
 * @button: the button to be packed.
 * @sensitive: whether the button is initially sensitive.
 * @callback: a callback function for the "clicked" signal.
 * @user_data: user data to be provided to the callback function.
 *
 * Packs a button in the specified @box.
 *
 * Returns: the newly created button, or %NULL.
 */
GtkWidget *
ofa_buttons_box_add_button( ofaButtonsBox *box, gint button_id, gboolean sensitive, GCallback cb, void *user_data )
{
	static const gchar *thisfn = "ofa_buttons_box_add_button";
	ofaButtonsBoxPrivate *priv;
	GtkWidget *alignment, *button;

	g_return_val_if_fail( box && OFA_IS_BUTTONS_BOX( box ), NULL );

	priv = box->priv;
	button = NULL;

	if( !priv->dispose_has_run ){

		get_top_grid( box );

		switch( button_id ){
			case BUTTON_NEW:
				button = gtk_button_new_with_mnemonic( _( "_New..." ));
				break;
			case BUTTON_PROPERTIES:
				button = gtk_button_new_with_mnemonic( _( "_Properties..." ));
				break;
			case BUTTON_DUPLICATE:
				button = gtk_button_new_with_mnemonic( _( "_Duplicate" ));
				break;
			case BUTTON_DELETE:
				button = gtk_button_new_with_mnemonic( _( "_Delete" ));
				break;
			case BUTTON_IMPORT:
				button = gtk_button_new_with_mnemonic( _( "_Import..." ));
				break;
			case BUTTON_EXPORT:
				button = gtk_button_new_with_mnemonic( _( "_Export..." ));
				break;
			case BUTTON_PRINT:
				button = gtk_button_new_with_mnemonic( _( "_Print..." ));
				break;
			case BUTTON_VIEW_ENTRIES:
				button = gtk_button_new_with_mnemonic( _( "View _entries..." ));
				break;
			case BUTTON_GUIDED_INPUT:
				button = gtk_button_new_with_mnemonic( _( "_Guided input..." ));
				break;
			default:
				g_warning( "%s: button=%u not implemented", thisfn, button_id );
				break;
		}

		if( button ){
			alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
			gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), priv->spacers*STYLE_SPACER, 0, 0, 0 );
			priv->spacers = 0;

			g_object_set_data( G_OBJECT( button ), BUTTON_ID, GINT_TO_POINTER( button_id ));
			gtk_widget_set_sensitive( button, sensitive );

			if( cb ){
				g_signal_connect( G_OBJECT( button ), "clicked", cb, user_data );
			}

			gtk_container_add( GTK_CONTAINER( alignment ), button );

			gtk_grid_attach( priv->grid, alignment, 0, priv->rows, 1, 1 );
			priv->rows += 1;
		}
	}

	return( button );
}

/**
 * ofa_buttons_box_get_top_widget:
 * @box: this #ofaButtonsBox object.
 *
 * Returns: the top box widget.
 */
GtkWidget *
ofa_buttons_box_get_top_widget( ofaButtonsBox *box )
{
	ofaButtonsBoxPrivate *priv;
	GtkWidget *top;

	g_return_val_if_fail( box && OFA_IS_BUTTONS_BOX( box ), NULL );

	priv = box->priv;
	top = NULL;

	if( !priv->dispose_has_run ){

		top = get_top_grid( box );
	}

	return( top );
}
