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

#include "ui/my-buttons-box.h"

/* private instance data
 */
struct _myButtonsBoxPrivate {
	gboolean dispose_has_run;

	/* internals
	 */
	gint     top_spacer_height;
	gint     btn_count;
	gboolean previous_was_spacer;
};

#define BUTTON_ID                       "button-id"

/* some styles layout
 */
#define STYLE_MARGIN                    4
#define STYLE_TOP_SPACER               29
#define STYLE_PADDING                   3
#define STYLE_SPACER                   20

G_DEFINE_TYPE( myButtonsBox, my_buttons_box, GTK_TYPE_BUTTON_BOX )

static void
buttons_box_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_buttons_box_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_BUTTONS_BOX( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_buttons_box_parent_class )->finalize( instance );
}

static void
buttons_box_dispose( GObject *instance )
{
	myButtonsBoxPrivate *priv;

	g_return_if_fail( instance && MY_IS_BUTTONS_BOX( instance ));

	priv = ( MY_BUTTONS_BOX( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_buttons_box_parent_class )->dispose( instance );
}

static void
my_buttons_box_init( myButtonsBox *self )
{
	static const gchar *thisfn = "my_buttons_box_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_BUTTONS_BOX( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_BUTTONS_BOX, myButtonsBoxPrivate );
	self->priv->dispose_has_run = FALSE;
	self->priv->top_spacer_height = STYLE_TOP_SPACER;
	self->priv->btn_count = 0;
	self->priv->previous_was_spacer = FALSE;
}

static void
my_buttons_box_class_init( myButtonsBoxClass *klass )
{
	static const gchar *thisfn = "my_buttons_box_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = buttons_box_dispose;
	G_OBJECT_CLASS( klass )->finalize = buttons_box_finalize;

	g_type_class_add_private( klass, sizeof( myButtonsBoxPrivate ));
}

/**
 * my_buttons_box_new:
 */
myButtonsBox *
my_buttons_box_new( void )
{
	myButtonsBox *box;

	box = g_object_new( MY_TYPE_BUTTONS_BOX,
					"orientation", GTK_ORIENTATION_VERTICAL,
					NULL );

	gtk_widget_set_margin_left( GTK_WIDGET( box ), STYLE_MARGIN );
	gtk_widget_set_margin_right( GTK_WIDGET( box ), STYLE_MARGIN );
	gtk_button_box_set_layout( GTK_BUTTON_BOX( box ), GTK_BUTTONBOX_START );

	return( box );
}

/**
 * my_buttons_box_pack_button:
 * @box: this #myButtonsBox object.
 * @button: the button to be packed.
 * @sensitive: whether the button is initially sensitive.
 * @callback: a callback function for the "clicked" signal.
 * @user_data: user data to be provided to the callback function.
 *
 * Packs a button in the specified @box.
 */
void
my_buttons_box_pack_button( myButtonsBox *box, GtkWidget *button, gboolean sensitive, GCallback callback, void *user_data )
{
	myButtonsBoxPrivate *priv;
	GtkWidget *frame;

	g_return_if_fail( box && MY_IS_BUTTONS_BOX( box ));

	priv = box->priv;

	if( !priv->dispose_has_run ){

		gtk_widget_set_sensitive( button, sensitive );

		if( callback ){
			g_signal_connect( G_OBJECT( button ), "clicked", callback, user_data );
		}

		if( priv->btn_count == 0 ){
			gtk_widget_set_margin_top( button, priv->top_spacer_height );

		} else if( !priv->previous_was_spacer ){
			gtk_widget_set_margin_top( button, STYLE_PADDING );

		} else {
			frame = gtk_frame_new( NULL );
			gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );
			gtk_box_pack_start( GTK_BOX( box ), frame, FALSE, FALSE, 0 );
		}

		gtk_box_pack_start( GTK_BOX( box ), button, FALSE, FALSE, 0 );

		priv->btn_count += 1;
		priv->previous_was_spacer = FALSE;
	}
}

/**
 * my_buttons_box_pack_button_by_id:
 * @box: this #myButtonsBox object.
 * @id: the button identifier.
 * @sensitive: whether the button is initially sensitive.
 * @callback: a callback function for the "clicked" signal.
 * @user_data: user data to be provided to the callback function.
 *
 * Packs a pre-identified button in the specified @box.
 */
GtkWidget *
my_buttons_box_pack_button_by_id( myButtonsBox *box, guint id, gboolean sensitive, GCallback callback, void *user_data )
{
	static const gchar *thisfn = "my_buttons_box_pack_button_by_id";
	myButtonsBoxPrivate *priv;
	GtkWidget *button;

	g_return_val_if_fail( box && MY_IS_BUTTONS_BOX( box ), NULL );

	button = NULL;
	priv = box->priv;

	if( !priv->dispose_has_run ){

		switch( id ){
			case BUTTONS_BOX_NEW:
				button = gtk_button_new_with_mnemonic( _( "_New..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			case BUTTONS_BOX_PROPERTIES:
				button = gtk_button_new_with_mnemonic( _( "_Properties..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			case BUTTONS_BOX_DELETE:
				button = gtk_button_new_with_mnemonic( _( "_Delete..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			case BUTTONS_BOX_IMPORT:
				button = gtk_button_new_with_mnemonic( _( "_Import..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			case BUTTONS_BOX_EXPORT:
				button = gtk_button_new_with_mnemonic( _( "_Export..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			case BUTTONS_BOX_PRINT:
				button = gtk_button_new_with_mnemonic( _( "_Print..." ));
				g_object_set_data( G_OBJECT( button ), BUTTON_ID, GUINT_TO_POINTER( id ));
				my_buttons_box_pack_button( box, button, sensitive, callback, user_data );
				break;

			default:
				g_warning( "%s: %u: unknown button identifier", thisfn, id );
				break;
		}
	}

	return( button );
}

/**
 * my_buttons_box_inc_top_spacer:
 * @box: this #myButtonsBox object.
 *
 * Increment the current top spacer height by an another row.
 *
 * A top spacer is a spacer of the height of the headers of a treeview,
 * or of the tabs in a notebook. It is only allowed before any button.
 *
 * It is defined by default to one unit of height. It may be incremented
 * before the first button be defined. After that, this function is
 * without any effect.
 */
void
my_buttons_box_inc_top_spacer( myButtonsBox *box )
{
	myButtonsBoxPrivate *priv;

	g_return_if_fail( box && MY_IS_BUTTONS_BOX( box ));

	priv = box->priv;

	if( !priv->dispose_has_run ){

		if( priv->btn_count == 0 ){
			priv->top_spacer_height += STYLE_TOP_SPACER;
		}
	}
}

/**
 * my_buttons_box_add_spacer:
 * @box: this #myButtonsBox object.
 *
 * Packs a spacer beetween two groups of buttons in the specified @box.
 */
void
my_buttons_box_add_spacer( myButtonsBox *box )
{
	myButtonsBoxPrivate *priv;

	g_return_if_fail( box && MY_IS_BUTTONS_BOX( box ));

	priv = box->priv;

	if( !priv->dispose_has_run ){

		priv->previous_was_spacer = TRUE;
	}
}
