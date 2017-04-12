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
 * File formats are identified by their name and their mode.
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
 *
 * The code which defines a stream format may also define which fields
 * are user-updatable.
 */

#include "my/my-date.h"

#include "api/ofa-igetter-def.h"

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
 *
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
 * ofeSFHas:
 *
 * Whether this stream format has which indicators.
 *
 * This same enumerator is also used to define which fields are user-
 * updatable.
 */
typedef enum {
	OFA_SFHAS_NAME        = 1 << 0,
	OFA_SFHAS_MODE        = 1 << 1,
	OFA_SFHAS_CHARMAP     = 1 << 2,
	OFA_SFHAS_DATEFMT     = 1 << 3,
	OFA_SFHAS_THOUSANDSEP = 1 << 4,
	OFA_SFHAS_DECIMALSEP  = 1 << 5,
	OFA_SFHAS_FIELDSEP    = 1 << 6,
	OFA_SFHAS_STRDELIM    = 1 << 7,
	OFA_SFHAS_HEADERS     = 1 << 8,
	OFA_SFHAS_ALL         = 0xffff,
}
	ofeSFHas;

GType            ofa_stream_format_get_type          ( void ) G_GNUC_CONST;

const gchar     *ofa_stream_format_get_default_name  ( void );

ofeSFMode        ofa_stream_format_get_default_mode  ( void );

const gchar     *ofa_stream_format_get_mode_str      ( ofeSFMode mode );
const gchar     *ofa_stream_format_get_mode_localestr( ofeSFMode mode );

gboolean         ofa_stream_format_exists            ( ofaIGetter *getter,
															const gchar *name,
															ofeSFMode mode );

ofaStreamFormat *ofa_stream_format_new               ( ofaIGetter *getter,
															const gchar *name,
															ofeSFMode mode );

const gchar     *ofa_stream_format_get_name          ( ofaStreamFormat *format );
ofeSFMode        ofa_stream_format_get_mode          ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_charmap   ( ofaStreamFormat *format );
const gchar     *ofa_stream_format_get_charmap       ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_date      ( ofaStreamFormat *format );
myDateFormat     ofa_stream_format_get_date_format   ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_thousand  ( ofaStreamFormat *format );
gchar            ofa_stream_format_get_thousand_sep  ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_decimal   ( ofaStreamFormat *format );
gchar            ofa_stream_format_get_decimal_sep   ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_field     ( ofaStreamFormat *format );
gchar            ofa_stream_format_get_field_sep     ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_has_strdelim  ( ofaStreamFormat *format );
gchar            ofa_stream_format_get_string_delim  ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_with_headers  ( ofaStreamFormat *format );
gint             ofa_stream_format_get_headers_count ( ofaStreamFormat *format );

gboolean         ofa_stream_format_get_updatable     ( ofaStreamFormat *format,
															ofeSFHas field );

void             ofa_stream_format_set_updatable     ( ofaStreamFormat *format,
															ofeSFHas field,
															gboolean updatable );

void             ofa_stream_format_set               ( ofaStreamFormat *format,
															gboolean has_charmap, const gchar *charmap,
															gboolean has_datefmt, myDateFormat datefmt,
															gboolean has_thousand_sep, gchar thousand_sep,
															gboolean has_decimal_sep, gchar decimal_sep,
															gboolean has_field_sep, gchar field_sep,
															gboolean has_string_delim, gchar string_delim,
															gint count_headers );

void             ofa_stream_format_set_name          ( ofaStreamFormat *format, const gchar *name );
void             ofa_stream_format_set_mode          ( ofaStreamFormat *format, ofeSFMode mode );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_STREAM_FORMAT_H__ */
