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

#ifndef __OFA_FILE_FORMAT_PIECE_H__
#define __OFA_FILE_FORMAT_PIECE_H__

/**
 * SECTION: ofa_file_format_piece
 * @short_description: #ofaFileFormatPiece class definition.
 * @include: ui/ofa-export-settings-prefs.h
 *
 * A convenience class which let the user manages its own export
 * settings. It is to be used as a piece of user preferences.
 */

#include "core/ofa-file-format.h"
#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_FILE_FORMAT_PIECE                ( ofa_file_format_piece_get_type())
#define OFA_FILE_FORMAT_PIECE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_FILE_FORMAT_PIECE, ofaFileFormatPiece ))
#define OFA_FILE_FORMAT_PIECE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_FILE_FORMAT_PIECE, ofaFileFormatPieceClass ))
#define OFA_IS_FILE_FORMAT_PIECE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_FILE_FORMAT_PIECE ))
#define OFA_IS_FILE_FORMAT_PIECE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_FILE_FORMAT_PIECE ))
#define OFA_FILE_FORMAT_PIECE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_FILE_FORMAT_PIECE, ofaFileFormatPieceClass ))

typedef struct _ofaFileFormatPiecePrivate         ofaFileFormatPiecePrivate;

typedef struct {
	/*< public members >*/
	GObject                    parent;

	/*< private members >*/
	ofaFileFormatPiecePrivate *priv;
}
	ofaFileFormatPiece;

typedef struct {
	/*< public members >*/
	GObjectClass               parent;
}
	ofaFileFormatPieceClass;

GType               ofa_file_format_piece_get_type       ( void ) G_GNUC_CONST;

ofaFileFormatPiece *ofa_file_format_piece_new            ( const gchar *prefs_name );

void                ofa_file_format_piece_attach_to      ( ofaFileFormatPiece *settings,
																		GtkContainer *parent );

void                ofa_file_format_piece_display        ( ofaFileFormatPiece *settings );

gboolean            ofa_file_format_piece_is_validable   ( ofaFileFormatPiece *settings );

gboolean            ofa_file_format_piece_apply          ( ofaFileFormatPiece *settings );

ofaFileFormat      *ofa_file_format_piece_get_file_format( ofaFileFormatPiece *settings );

G_END_DECLS

#endif /* __OFA_FILE_FORMAT_PIECE_H__ */
