/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-buttons-box.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* internals
	 */
	GtkGrid      *grid;
	gint          rows;
	gint          spacers;
}
	ofaButtonsBoxPrivate;

/* some styles layout
 */
#define STYLE_ROW_MARGIN                2
#define STYLE_SPACER                   30

static void setup_top_grid( ofaButtonsBox *box );

G_DEFINE_TYPE_EXTENDED( ofaButtonsBox, ofa_buttons_box, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaButtonsBox ))

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

	priv = ofa_buttons_box_get_instance_private( OFA_BUTTONS_BOX( instance ));

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
	ofaButtonsBoxPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BUTTONS_BOX( self ));

	priv = ofa_buttons_box_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_buttons_box_class_init( ofaButtonsBoxClass *klass )
{
	static const gchar *thisfn = "ofa_buttons_box_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = buttons_box_dispose;
	G_OBJECT_CLASS( klass )->finalize = buttons_box_finalize;
}

/**
 * ofa_buttons_box_new:
 */
ofaButtonsBox *
ofa_buttons_box_new( void )
{
	ofaButtonsBox *box;

	box = g_object_new( OFA_TYPE_BUTTONS_BOX, NULL );

	setup_top_grid( box );

	return( box );
}

static void
setup_top_grid( ofaButtonsBox *box )
{
	ofaButtonsBoxPrivate *priv;
	GtkWidget *top_widget, *grid;

	priv = ofa_buttons_box_get_instance_private( box );

	top_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_container_add( GTK_CONTAINER( box ), top_widget );

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( top_widget ), grid );
	gtk_grid_set_row_spacing( GTK_GRID( grid ), STYLE_ROW_MARGIN );

	priv->grid = GTK_GRID( grid );
	priv->rows = 0;
	priv->spacers = 0;

	gtk_widget_show_all( GTK_WIDGET( box ));
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

	priv = ofa_buttons_box_get_instance_private( box );

	g_return_if_fail( !priv->dispose_has_run );

	priv->spacers += 1;
}

/**
 * ofa_buttons_box_add_button_with_mnemonic:
 * @box: this #ofaButtonsBox object.
 * @mnemonic: the mnemonic to be set on the new button.
 * @callback: [allow-none]: the handler of the "clicked" signal.
 * @user_data: user data to be provided to the @callback function.
 *
 * Packs a button in the specified @box.
 * The new button sensitivity defaults to %FALSE.
 *
 * Returns: the newly created button, or %NULL.
 */
GtkWidget *
ofa_buttons_box_add_button_with_mnemonic( ofaButtonsBox *box, const gchar *mnemonic, GCallback cb, void *user_data )
{
	ofaButtonsBoxPrivate *priv;
	GtkWidget *button;

	g_return_val_if_fail( box && OFA_IS_BUTTONS_BOX( box ), NULL );
	g_return_val_if_fail( my_strlen( mnemonic ), NULL );

	priv = ofa_buttons_box_get_instance_private( box );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	button = gtk_button_new_with_mnemonic( mnemonic );
	ofa_buttons_box_append_button( box, button );

	gtk_widget_set_sensitive( button, FALSE );
	if( cb ){
		g_signal_connect( button, "clicked", cb, user_data );
	}

	return( button );
}

/**
 * ofa_buttons_box_append_button:
 * @box: this #ofaButtonsBox object.
 * @button: the button to append.
 *
 * Packs a button in the specified @box.
 */
void
ofa_buttons_box_append_button( ofaButtonsBox *box, GtkWidget *button )
{
	ofaButtonsBoxPrivate *priv;

	g_return_if_fail( box && OFA_IS_BUTTONS_BOX( box ));
	g_return_if_fail( button && GTK_IS_WIDGET( button ));

	priv = ofa_buttons_box_get_instance_private( box );

	g_return_if_fail( !priv->dispose_has_run );

	my_utils_widget_set_margins( button, priv->spacers*STYLE_SPACER, 0, 0, 0 );
	priv->spacers = 0;

	gtk_grid_attach( priv->grid, button, 0, priv->rows, 1, 1 );
	priv->rows += 1;
}
