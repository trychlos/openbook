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

#ifndef __OFA_DBMS_ROOT_PIECE_H__
#define __OFA_DBMS_ROOT_PIECE_H__

/**
 * SECTION: ofa_dbms_root_piece
 * @short_description: #ofaDBMSRootPiece class definition.
 * @include: core/ofa-dbms-root-piece.h
 *
 * Let the user enter DBMS administrator account and password.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DBMS_ROOT_PIECE                ( ofa_dbms_root_piece_get_type())
#define OFA_DBMS_ROOT_PIECE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBMS_ROOT_PIECE, ofaDBMSRootPiece ))
#define OFA_DBMS_ROOT_PIECE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBMS_ROOT_PIECE, ofaDBMSRootPieceClass ))
#define OFA_IS_DBMS_ROOT_PIECE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBMS_ROOT_PIECE ))
#define OFA_IS_DBMS_ROOT_PIECE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBMS_ROOT_PIECE ))
#define OFA_DBMS_ROOT_PIECE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBMS_ROOT_PIECE, ofaDBMSRootPieceClass ))

typedef struct _ofaDBMSRootPiecePrivate         ofaDBMSRootPiecePrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaDBMSRootPiecePrivate *priv;
}
	ofaDBMSRootPiece;

typedef struct {
	/*< public members >*/
	GObjectClass             parent;
}
	ofaDBMSRootPieceClass;

GType             ofa_dbms_root_piece_get_type       ( void ) G_GNUC_CONST;

ofaDBMSRootPiece *ofa_dbms_root_piece_new            ( void );

void              ofa_dbms_root_piece_attach_to      ( ofaDBMSRootPiece *piece,
																GtkContainer *parent,
																GtkSizeGroup *group );

void              ofa_dbms_root_piece_set_dossier    ( ofaDBMSRootPiece *piece,
																const gchar *dname );

gboolean          ofa_dbms_root_piece_is_valid       ( const ofaDBMSRootPiece *piece );

void              ofa_dbms_root_piece_set_credentials( ofaDBMSRootPiece *piece,
																const gchar *account,
																const gchar *password );

void              ofa_dbms_root_piece_set_valid      ( ofaDBMSRootPiece *piece,
																gboolean valid );

G_END_DECLS

#endif /* __OFA_DBMS_ROOT_PIECE_H__ */
