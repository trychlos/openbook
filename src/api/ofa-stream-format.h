/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_STREAM_FORMAT_H__
#define __OPENBOOK_API_OFA_STREAM_FORMAT_H__

/**
 * SECTION: ofa_stream_format
 * @short_description: #ofaStreamFormat class definition.
 * @include: openbook/ofa-stream-format.h
 *
 * A convenience class which manages the data formats for import and
 * export streams.
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
 * - whether the exported file contains headers, or count of the headers
 *   lines in an imported file
 */

#include "my/my-date.h"

G_BEGIN_DECLS

#define OFA_TYPE_STREAM_FORMAT                ( ofa_stream_format_get_type())
#define OFA_STREAM_FORMAT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_STREAM_FORMAT, ofaStreamFormat ))
#define OFA_STREAM_FORMAT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_STREAM_FORMAT, ofaStreamFormatClass ))
#define OFA_IS_STREAM_FORMAT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_STREAM_FORMAT ))
#define OFA_IS_STREAM_FORMAT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_STREAM_FORMAT ))
#define OFA_STREAM_FORMAT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_STREAM_FORMAT, ofaStreamFormatClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaStreamFormat;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaStreamFormatClass;

/**
 * ofeStream:
 * The format of the file as a whole.
 * @OFA_STREAM_CSV:   a text file, csv-like format, line-oriented, with
 *                  a field separator
 * @OFA_STREAM_FIXED: a text or binary mode, fixed-format file (without
 *                  field separator).
 * @OFA_STREAM_OTHER: any other format, whether it is text or binary;
 *                  this format is not handled by Openbook, and must
 *                  be managed by a specialized code (e.g. BAT).
 *
 * This indicator should be provided by ofaIImporter implementations,
 * because it lets the UI display ad-hoc fields.
 */
typedef enum {
	OFA_STREAM_CSV = 1,					/* keep this =1 as this is the default */
	OFA_STREAM_FIXED,
	OFA_STREAM_OTHER
}
	ofeStream;

/*
 * +--------------------------+---------+------------+------------+----------+-------------+---------+
 * | Content                    Charmap   DateFormat   DecimalSep   FieldSep   StringDelim   Headers |
 * +--------------------------+---------+------------+------------+----------+-------------+---------+
 * | application/pdf               Y          Y             Y          N            N           N    |
 * | application/vnd.ms-excel      Y          Y             Y          N            Y           N    |
 * | text/csv                      Y          Y             Y          Y            Y           Y    |
 * +--------------------------+---------+------------+------------+----------+-------------+---------+
 */

/**
 * ofeSMode:
 * The destination of the file format:
 * @OFA_SFMODE_EXPORT: file format for export, with headers indicator
 * @OFA_SFMODE_IMPORT: file format for import, with count of headers
 */
typedef enum {
	OFA_SFMODE_EXPORT = 1,				/* keep this =1 as this is the default */
	OFA_SFMODE_IMPORT
}
	ofeSMode;

GType            ofa_stream_format_get_type         ( void ) G_GNUC_CONST;

ofaStreamFormat *ofa_stream_format_new              ( const gchar *prefs_name );

ofeSMode         ofa_stream_format_get_ffmode       ( const ofaStreamFormat *settings );
ofeStream        ofa_stream_format_get_fftype       ( const ofaStreamFormat *settings );
const gchar     *ofa_stream_format_get_fftype_str   ( ofeStream format );
const gchar     *ofa_stream_format_get_charmap      ( const ofaStreamFormat *settings );
myDateFormat     ofa_stream_format_get_date_format  ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_decimal_sep  ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_field_sep    ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_string_delim ( const ofaStreamFormat *settings );
gint             ofa_stream_format_get_headers_count( const ofaStreamFormat *settings );
gboolean         ofa_stream_format_has_headers      ( const ofaStreamFormat *settings );

void             ofa_stream_format_set              ( ofaStreamFormat *settings,
															const gchar *name,
															ofeStream type,
															ofeSMode mode,
															const gchar *charmap,
															myDateFormat date_format,
															gchar decimal_sep,
															gchar field_sep,
															gchar string_delim,
															gint count_headers );

void             ofa_stream_format_set_mode         ( ofaStreamFormat *settings,
															ofeSMode mode );

void             ofa_stream_format_set_prefs_name   ( ofaStreamFormat *settings,
															const gchar *new_name );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_STREAM_FORMAT_H__ */
