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

#ifndef __MY_BUTTONS_BOX_H__
#define __MY_BUTTONS_BOX_H__

/**
 * SECTION: my_buttons_box
 * @short_description: #myButtonsBox class definition.
 * @include: ui/my-buttons-box.h
 *
 * This class handles the buttons box that many #ofaPage pages display
 * on the right of their view.
 *
 * +------------------------------------------------------------------+
 * | GtkGrid created by the main window,                              |
 * |  top child of the 'main' notebook's page for this theme          |
 * |+------------------------------------------------+---------------+|
 * || left=0, top=0                                  | left=1        ||
 * ||                                                |               ||
 * ||  the view for this theme                       |  buttons box  ||
 * ||                                                |               ||
 * |+------------------------------------------------+---------------+|
 * +------------------------------------------------------------------+
 *
 * The box takes care of allocating a top spacer at the top of the box,
 * before the first button.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_BUTTONS_BOX                ( my_buttons_box_get_type())
#define MY_BUTTONS_BOX( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_BUTTONS_BOX, myButtonsBox ))
#define MY_BUTTONS_BOX_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_BUTTONS_BOX, myButtonsBoxClass ))
#define MY_IS_BUTTONS_BOX( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_BUTTONS_BOX ))
#define MY_IS_BUTTONS_BOX_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_BUTTONS_BOX ))
#define MY_BUTTONS_BOX_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_BUTTONS_BOX, myButtonsBoxClass ))

typedef struct _myButtonsBoxPrivate        myButtonsBoxPrivate;

typedef struct {
	/*< public members >*/
	GtkButtonBox         parent;

	/*< private members >*/
	myButtonsBoxPrivate *priv;
}
	myButtonsBox;

typedef struct {
	/*< public members >*/
	GtkButtonBoxClass parent;
}
	myButtonsBoxClass;

/**
 * #myButtonsBox class provides typical used identifiers.
 *
 * The user may use other identifiers, starting with BUTTONS_BOX_LAST+1.
 */
enum {
	BUTTONS_BOX_FIRST = 1,
	BUTTONS_BOX_NEW,
	BUTTONS_BOX_PROPERTIES,
	BUTTONS_BOX_DELETE,
	BUTTONS_BOX_IMPORT,
	BUTTONS_BOX_EXPORT,
	BUTTONS_BOX_PRINT,
	BUTTONS_BOX_LAST
};

GType         my_buttons_box_get_type         ( void ) G_GNUC_CONST;

myButtonsBox *my_buttons_box_new              ( void );

void          my_buttons_box_pack_button      ( myButtonsBox *box, GtkWidget *button,
														gboolean sensitive,
														GCallback callback, void *user_data );

GtkWidget    *my_buttons_box_pack_button_by_id( myButtonsBox *box, guint id,
														gboolean sensitive,
														GCallback callback, void *user_data );

void          my_buttons_box_inc_top_spacer   ( myButtonsBox *box );

void          my_buttons_box_add_spacer       ( myButtonsBox *box );

G_END_DECLS

#endif /* __MY_BUTTONS_BOX_H__ */
