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

#ifndef __OFA_FILE_FORMAT_H__
#define __OFA_FILE_FORMAT_H__

/**
 * SECTION: ofa_file_format
 * @short_description: #ofaFileFormat class definition.
 * @include: core/ofa-file-format.h
 *
 * A convenience class which manages the file formats for import and
 * export datas.
 *
 * We are planning to manage two main file formats:
 * 1/ a 'csv'-like format: mode text, line-oriented, with a field
 *    separator
 * 2/ a fixed format, binary mode, where each field has its own fixed
 *    width.
 *
 * File formats may be named by the application (or the user).
 *
 * File format is serialized in settings as a semi-colon separated
 * string list:
 * - name
 * - file format
 * - encoding charmap
 * - date format
 * - decimal separator (ascii code of the char)
 * - field separator (ascii code of the char)
 * - whether the file contains headers
 */

#include "api/my-date.h"

G_BEGIN_DECLS

#define OFA_TYPE_FILE_FORMAT                ( ofa_file_format_get_type())
#define OFA_FILE_FORMAT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_FILE_FORMAT, ofaFileFormat ))
#define OFA_FILE_FORMAT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_FILE_FORMAT, ofaFileFormatClass ))
#define OFA_IS_FILE_FORMAT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_FILE_FORMAT ))
#define OFA_IS_FILE_FORMAT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_FILE_FORMAT ))
#define OFA_FILE_FORMAT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_FILE_FORMAT, ofaFileFormatClass ))

typedef struct _ofaFileFormatPrivate        ofaFileFormatPrivate;

typedef struct {
	/*< public members >*/
	GObject                   parent;

	/*< private members >*/
	ofaFileFormatPrivate *priv;
}
	ofaFileFormat;

typedef struct {
	/*< public members >*/
	GObjectClass              parent;
}
	ofaFileFormatClass;

/**
 * ofaFFmt:
 * The format of the file as a whole.
 * @OFA_FFMT_CSV:   a text file, csv-like format, line-oriented, with
 *                   a field separator
 * @OFA_FFMT_FIXED: a binary mode, fixed-format file (without field
 *                   separator).
 */
typedef enum {
	OFA_FFMT_CSV = 1,					/* keep this =1 as this is the default */
	OFA_FFMT_FIXED
}
	ofaFFmt;

GType          ofa_file_format_get_type       ( void ) G_GNUC_CONST;

ofaFileFormat *ofa_file_format_new            ( const gchar *prefs_name );

ofaFFmt        ofa_file_format_get_ffmt       ( const ofaFileFormat *settings );
const gchar   *ofa_file_format_get_ffmt_str   ( ofaFFmt format );
const gchar   *ofa_file_format_get_charmap    ( const ofaFileFormat *settings );
myDateFormat   ofa_file_format_get_date_format( const ofaFileFormat *settings );
gchar          ofa_file_format_get_decimal_sep( const ofaFileFormat *settings );
gchar          ofa_file_format_get_field_sep  ( const ofaFileFormat *settings );
gboolean       ofa_file_format_has_headers    ( const ofaFileFormat *settings );

void           ofa_file_format_set            ( ofaFileFormat *settings,
														const gchar *name,
														ofaFFmt ffmt,
														const gchar *charmap,
														myDateFormat date_format,
														gchar decimal_sep,
														gchar field_sep,
														gboolean with_headers );

G_END_DECLS

#endif /* __OFA_FILE_FORMAT_H__ */
