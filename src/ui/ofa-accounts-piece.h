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

#ifndef __OFA_ACCOUNTS_PIECE_H__
#define __OFA_ACCOUNTS_PIECE_H__

/**
 * SECTION: ofa_accounts_piece
 * @short_description: #ofaAccountsPiece class definition.
 * @include: ui/ofa-accounts-piece.h
 *
 * This is a convenience class which manages both the accounts notebook
 * and the buttons box on the right.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaAccountIStore interface.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_PIECE                ( ofa_accounts_piece_get_type())
#define OFA_ACCOUNTS_PIECE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_PIECE, ofaAccountsPiece ))
#define OFA_ACCOUNTS_PIECE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_PIECE, ofaAccountsPieceClass ))
#define OFA_IS_ACCOUNTS_PIECE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_PIECE ))
#define OFA_IS_ACCOUNTS_PIECE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_PIECE ))
#define OFA_ACCOUNTS_PIECE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_PIECE, ofaAccountsPieceClass ))

typedef struct _ofaAccountsPiecePrivate        ofaAccountsPiecePrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaAccountsPiecePrivate *priv;
}
	ofaAccountsPiece;

typedef struct {
	/*< public members >*/
	GObjectClass             parent;
}
	ofaAccountsPieceClass;

GType             ofa_accounts_piece_get_type                ( void ) G_GNUC_CONST;

ofaAccountsPiece *ofa_accounts_piece_new                     ( void );

void              ofa_accounts_piece_attach_to               ( ofaAccountsPiece *piece,
																		GtkContainer *parent );

void              ofa_accounts_piece_set_main_window         ( ofaAccountsPiece *piece,
																		ofaMainWindow *main_window );

void              ofa_accounts_piece_set_buttons             ( ofaAccountsPiece *piece,
																		gboolean view_entries );

gchar            *ofa_accounts_piece_get_selected            ( ofaAccountsPiece *piece );

void              ofa_accounts_piece_set_selected            ( ofaAccountsPiece *piece,
																	const gchar *number );

GtkWidget        *ofa_accounts_piece_get_top_focusable_widget( const ofaAccountsPiece *piece );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_PIECE_H__ */
