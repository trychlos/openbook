/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_BUTTONS_BOX_H__
#define __OPENBOOK_API_OFA_BUTTONS_BOX_H__

/**
 * SECTION: ofa_buttons_box
 * @short_description: #ofaButtonsBox class definition.
 * @include: openbook/ofa-buttons-box.h
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

#define OFA_TYPE_BUTTONS_BOX                ( ofa_buttons_box_get_type())
#define OFA_BUTTONS_BOX( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BUTTONS_BOX, ofaButtonsBox ))
#define OFA_BUTTONS_BOX_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BUTTONS_BOX, ofaButtonsBoxClass ))
#define OFA_IS_BUTTONS_BOX( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BUTTONS_BOX ))
#define OFA_IS_BUTTONS_BOX_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BUTTONS_BOX ))
#define OFA_BUTTONS_BOX_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BUTTONS_BOX, ofaButtonsBoxClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaButtonsBox;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaButtonsBoxClass;

GType          ofa_buttons_box_get_type                ( void ) G_GNUC_CONST;

ofaButtonsBox *ofa_buttons_box_new                     ( void );

void           ofa_buttons_box_add_spacer              ( ofaButtonsBox *box );

void           ofa_buttons_box_append_button           ( ofaButtonsBox *box,
																GtkWidget *button );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_BUTTONS_BOX_H__ */
