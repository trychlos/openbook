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

#ifndef __OFA_DOSSIER_NEW_PIECE_H__
#define __OFA_DOSSIER_NEW_PIECE_H__

/**
 * SECTION: ofa_dossier_new_piece
 * @short_description: #ofaDossierNewPiece class definition.
 * @include: core/ofa-dossier-new-piece.h
 *
 * Let the user define a new dossier, selecting the DBMS provider and
 * its connection properties, registering it in the settings.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW_PIECE                ( ofa_dossier_new_piece_get_type())
#define OFA_DOSSIER_NEW_PIECE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW_PIECE, ofaDossierNewPiece ))
#define OFA_DOSSIER_NEW_PIECE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW_PIECE, ofaDossierNewPieceClass ))
#define OFA_IS_DOSSIER_NEW_PIECE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW_PIECE ))
#define OFA_IS_DOSSIER_NEW_PIECE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW_PIECE ))
#define OFA_DOSSIER_NEW_PIECE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW_PIECE, ofaDossierNewPieceClass ))

typedef struct _ofaDossierNewPiecePrivate         ofaDossierNewPiecePrivate;

typedef struct {
	/*< public members >*/
	GObject                    parent;

	/*< private members >*/
	ofaDossierNewPiecePrivate *priv;
}
	ofaDossierNewPiece;

typedef struct {
	/*< public members >*/
	GObjectClass               parent;
}
	ofaDossierNewPieceClass;

GType               ofa_dossier_new_piece_get_type       ( void ) G_GNUC_CONST;

ofaDossierNewPiece *ofa_dossier_new_piece_new            ( void );

void                ofa_dossier_new_piece_attach_to      ( ofaDossierNewPiece *piece,
																			GtkContainer *parent,
																			GtkSizeGroup *group );

void                ofa_dossier_new_piece_set_frame      ( ofaDossierNewPiece *piece,
																			gboolean visible );

void                ofa_dossier_new_piece_set_provider   ( ofaDossierNewPiece *piece,
																			const gchar *provider );

gboolean            ofa_dossier_new_piece_is_valid       ( const ofaDossierNewPiece *piece );

gboolean            ofa_dossier_new_piece_apply          ( const ofaDossierNewPiece *piece );

void                ofa_dossier_new_piece_get_dname      ( const ofaDossierNewPiece *piece,
																			gchar **dname );

void                ofa_dossier_new_piece_get_database   ( const ofaDossierNewPiece *piece,
																			gchar **database );

void                ofa_dossier_new_piece_get_credentials( const ofaDossierNewPiece *piece,
																			gchar **account,
																			gchar **password );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_PIECE_H__ */
