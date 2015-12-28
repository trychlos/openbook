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

#ifndef __OFA_BUTTONS_BOX_H__
#define __OFA_BUTTONS_BOX_H__

/**
 * SECTION: ofa_buttons_box
 * @short_description: #ofaButtonsBox class definition.
 * @include: ui/ofa-buttons-box.h
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

typedef struct _ofaButtonsBoxPrivate        ofaButtonsBoxPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                parent;

	/*< private members >*/
	ofaButtonsBoxPrivate *priv;
}
	ofaButtonsBox;

typedef struct {
	/*< public members >*/
	GtkBinClass           parent;
}
	ofaButtonsBoxClass;

/* Some standard button mnemonics are defined here
 *  as an effort to get them homogene
 */
#define BUTTON_DELETE                   _( "_Delete..." )
#define BUTTON_NEW                      _( "_New..." )
#define BUTTON_PROPERTIES               _( "_Properties..." )
#define BUTTON_RECONCILIATION           _( "_Reconciliation" )
#define BUTTON_SETTLEMENT               _( "_Settlement" )
#define BUTTON_VIEW_ENTRIES             _( "View _entries" )

GType          ofa_buttons_box_get_type                ( void ) G_GNUC_CONST;

ofaButtonsBox *ofa_buttons_box_new                     ( void );

void           ofa_buttons_box_add_spacer              ( ofaButtonsBox *box );

GtkWidget     *ofa_buttons_box_add_button_with_mnemonic( ofaButtonsBox *box,
															const gchar *mnemonic,
															GCallback cb,
															void *user_data );

G_END_DECLS

#endif /* __OFA_BUTTONS_BOX_H__ */
