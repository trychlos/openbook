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

#ifndef __OFA_ADMIN_CREDENTIALS_PIECE_H__
#define __OFA_ADMIN_CREDENTIALS_PIECE_H__

/**
 * SECTION: ofa_admin_credentials_piece
 * @short_description: #ofaAdminCredentialsPiece class definition.
 * @include: core/ofa-admin-credentials-piece.h
 *
 * Let the user enter dossier administrative account and password when
 * defining a new dossier, so we do not check here whether the entered
 * credentials are actually registered into the dossier database.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_ADMIN_CREDENTIALS_PIECE                ( ofa_admin_credentials_piece_get_type())
#define OFA_ADMIN_CREDENTIALS_PIECE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ADMIN_CREDENTIALS_PIECE, ofaAdminCredentialsPiece ))
#define OFA_ADMIN_CREDENTIALS_PIECE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ADMIN_CREDENTIALS_PIECE, ofaAdminCredentialsPieceClass ))
#define OFA_IS_ADMIN_CREDENTIALS_PIECE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ADMIN_CREDENTIALS_PIECE ))
#define OFA_IS_ADMIN_CREDENTIALS_PIECE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ADMIN_CREDENTIALS_PIECE ))
#define OFA_ADMIN_CREDENTIALS_PIECE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ADMIN_CREDENTIALS_PIECE, ofaAdminCredentialsPieceClass ))

typedef struct _ofaAdminCredentialsPiecePrivate         ofaAdminCredentialsPiecePrivate;

typedef struct {
	/*< public members >*/
	GObject                          parent;

	/*< private members >*/
	ofaAdminCredentialsPiecePrivate *priv;
}
	ofaAdminCredentialsPiece;

typedef struct {
	/*< public members >*/
	GObjectClass                     parent;
}
	ofaAdminCredentialsPieceClass;

GType                     ofa_admin_credentials_piece_get_type ( void ) G_GNUC_CONST;

ofaAdminCredentialsPiece *ofa_admin_credentials_piece_new      ( void );

void                      ofa_admin_credentials_piece_attach_to( ofaAdminCredentialsPiece *piece,
																			GtkContainer *parent );

gboolean                  ofa_admin_credentials_piece_is_valid ( const ofaAdminCredentialsPiece *piece );

G_END_DECLS

#endif /* __OFA_ADMIN_CREDENTIALS_PIECE_H__ */
