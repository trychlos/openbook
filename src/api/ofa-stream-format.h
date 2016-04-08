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
 * A convenience class which manages the format to be used on input/output
 * streams.
 *
 * File formats are named..
 *
 * Import and export assistants may save the used stream format using
 * the data type class name as the user-provided name.
 *
 * File format is serialized in user settings as a semi-colon separated
 * string list:
 * - encoding charmap
 * - date format
 * - decimal separator (ascii code of the char)
 * - field separator (ascii code of the char)
 * - whether the exported file contains headers, or count of the headers
 *   lines in an imported file.
 *
 * The full key name in the user settings is so built from:
 * - the user-provided name, which defaults to 'Default'
 * - the mode associated to the format 'Import' or 'Export'
 * - a 'Format suffix.
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

/*
 * +--------------------------+---------+------------+------------+----------+-------------+---------+-------------+---------+
 * | Content                  | Charmap | DateFormat | DecimalSep | FieldSep | StringDelim | Headers | ThousandSep | EndLine |
 * +--------------------------+---------+------------+------------+----------+-------------+---------+-------------+---------+
 * | application/pdf          |    Y    |     Y      |      Y     |    N     |      N      |    N    |      Y      |    N    |
 * | application/vnd.ms-excel |    Y    |     Y      |      Y     |    N     |      Y      |    N    |      Y      |    Y    |
 * | text/csv                 |    Y    |     Y      |      Y     |    Y     |      Y      |    Y    |      Y      |    Y    |
 * | text/json                |    Y    |     Y      |      Y     |    Y     |      Y      |    Y    |      Y      |    N    |
 * +--------------------------+---------+------------+------------+----------+-------------+---------+-------------+---------+
 */

/**
 * ofeSFMode:
 * The destination of the file format:
 * @OFA_SFMODE_EXPORT: file format for export, with headers indicator
 * @OFA_SFMODE_IMPORT: file format for import, with count of headers
 */
typedef enum {
	OFA_SFMODE_EXPORT = 1,				/* keep this =1 as this is the default */
	OFA_SFMODE_IMPORT
}
	ofeSFMode;

/**
 * Whether this stream format has which indicators.
 */
typedef enum {
	OFA_SFHAS_CHARMAP     = 1 << 0,
	OFA_SFHAS_DATEFMT     = 1 << 1,
	OFA_SFHAS_THOUSANDSEP = 1 << 2,
	OFA_SFHAS_DECIMALSEP  = 1 << 3,
	OFA_SFHAS_FIELDSEP    = 1 << 4,
	OFA_SFHAS_STRDELIM    = 1 << 5,
	OFA_SFHAS_ALL         = 0xffff,
}
	ofeSFHas;

GType            ofa_stream_format_get_type          ( void ) G_GNUC_CONST;

const gchar     *ofa_stream_format_get_default_name  ( void );

ofeSFMode        ofa_stream_format_get_default_mode  ( void );

const gchar     *ofa_stream_format_get_mode_str      ( ofeSFMode mode );
const gchar     *ofa_stream_format_get_mode_localestr( ofeSFMode mode );

gboolean         ofa_stream_format_exists            ( const gchar *name,
															ofeSFMode mode );

ofaStreamFormat *ofa_stream_format_new               ( const gchar *name,
															ofeSFMode mode );

const gchar     *ofa_stream_format_get_name          ( const ofaStreamFormat *settings );
ofeSFMode        ofa_stream_format_get_mode          ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_charmap   ( const ofaStreamFormat *settings );
const gchar     *ofa_stream_format_get_charmap       ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_date      ( const ofaStreamFormat *settings );
myDateFormat     ofa_stream_format_get_date_format   ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_thousand  ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_thousand_sep  ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_decimal   ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_decimal_sep   ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_field     ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_field_sep     ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_has_strdelim  ( const ofaStreamFormat *settings );
gchar            ofa_stream_format_get_string_delim  ( const ofaStreamFormat *settings );

gboolean         ofa_stream_format_get_with_headers  ( const ofaStreamFormat *settings );
gint             ofa_stream_format_get_headers_count ( const ofaStreamFormat *settings );

void             ofa_stream_format_set               ( ofaStreamFormat *settings,
															gboolean has_charmap, const gchar *charmap,
															gboolean has_datefmt, myDateFormat datefmt,
															gboolean has_thousand_sep, gchar thousand_sep,
															gboolean has_decimal_sep, gchar decimal_sep,
															gboolean has_field_sep, gchar field_sep,
															gboolean has_string_delim, gchar string_delim,
															gint count_headers );

void             ofa_stream_format_set_name          ( ofaStreamFormat *settings, const gchar *name );
void             ofa_stream_format_set_mode          ( ofaStreamFormat *settings, ofeSFMode mode );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_STREAM_FORMAT_H__ */
