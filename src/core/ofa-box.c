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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-preferences.h"

/**
 * ofsBoxData:
 *
 * Our boxed elementary data
 */
struct _ofsBoxData {
	const ofsBoxDef *def;
	gboolean         is_null;
	union {
		ofxAmount  amount;
		ofxCounter counter;
		gint       integer;
		GDate      date;
		gchar     *string;
		GTimeVal   timestamp;
	};
};

static void   box_dump_def( const ofsBoxDef *def );
static gchar *get_csv_name( const ofsBoxDef *def );
static gchar *compute_csv_name( const gchar *dbms_name );
static void   set_decimal_point( gchar *str, gchar decimal_sep );

/*
 * box_new:
 * @def: the field definition.
 */
static ofsBoxData *
box_new( const ofsBoxDef *def )
{
	ofsBoxData *box;

	box = g_new0( ofsBoxData, 1 );
	box->def = def;
	box->is_null = TRUE;

	return( box );
}

/*
 * OFA_TYPE_AMOUNT
 * ofxAmount
 */
static gpointer
amount_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_AMOUNT, 0 );

	return( AMOUNT_TO_GPOINTER( box->amount ));
}

static void
amount_set( ofsBoxData *box, gconstpointer value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_AMOUNT );

	box->is_null = FALSE;
	box->amount = GPOINTER_TO_AMOUNT( value );
}

static ofsBoxData *
amount_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_AMOUNT, NULL );

	if( my_strlen( str )){
		box->is_null = FALSE;
		box->amount = ( ofxAmount ) my_double_set_from_sql( str );
	}

	return( box );
}

static gchar *
amount_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_AMOUNT, NULL );

	if( box->amount || !box->def->csv_zero_as_empty ){
		str = my_double_to_sql( box->amount );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

/*
 * OFA_TYPE_COUNTER
 * ofxCounter
 */
static ofxCounter
counter_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, 0 );

	return( box->counter );
}

static void
counter_set( ofsBoxData *box, ofxCounter value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_COUNTER );

	box->is_null = FALSE;
	box->counter = value;
}

static ofsBoxData *
counter_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, NULL );

	if( my_strlen( str )){
		box->is_null = FALSE;
		box->counter = atol( str );
	}

	return( box );
}

static gchar *
counter_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, NULL );

	if( box->counter || !box->def->csv_zero_as_empty ){
		str = g_strdup_printf( "%ld", box->counter );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

/*
 * OFA_TYPE_INTEGER
 */
static gint
int_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_INTEGER, 0 );

	return( box->integer );
}

static void
int_set( ofsBoxData *box, gint value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_INTEGER );

	box->is_null = FALSE;
	box->integer = value;
}

static ofsBoxData *
int_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_INTEGER, NULL );

	if( my_strlen( str )){
		box->is_null = FALSE;
		box->integer = atoi( str );
	}

	return( box );
}

static gchar *
int_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_INTEGER, NULL );

	if( box->counter || !box->def->csv_zero_as_empty ){
		str = g_strdup_printf( "%d", box->integer );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

/*
 * OFA_TYPE_DATE
 */
static const GDate *
date_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_DATE, NULL );

	return(( const GDate * ) &box->date );
}

static void
date_set( ofsBoxData *box, const GDate *value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_DATE );

	if( value && my_date_is_valid( value )){
		box->is_null = FALSE;
		memcpy( &box->date, value, sizeof( GDate ));
	} else {
		box->is_null = TRUE;
		my_date_clear( &box->date );
	}
}

static ofsBoxData *
date_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_DATE, NULL );
	my_date_clear( &box->date );

	if( my_strlen( str )){
		box->is_null = FALSE;
		my_date_set_from_sql( &box->date, str );
	}

	return( box );
}

static gchar *
date_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_DATE, NULL );

	if( my_date_is_valid( &box->date )){
		str = my_date_to_str( &box->date, MY_DATE_SQL );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

/*
 * OFA_TYPE_STRING
 */
static const gchar *
string_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	/*g_debug( "string_get: is_null=%s, value=%s", box->is_null ? "True":"False", box->string );*/
	return(( const gchar * ) box->string );
}

static void
string_set( ofsBoxData *box, const gchar *value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_STRING );

	g_free( box->string );
	/* debug */
	if( 0 ){
		g_debug( "string_set: value=%s, len=%ld", value, my_strlen( value ));
	}

	if( my_strlen( value )){
		box->is_null = FALSE;
		box->string = g_strdup( value );

	} else {
		box->is_null = TRUE;
		box->string = NULL;
	}
}

static ofsBoxData *
string_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	if( my_strlen( str )){
		box->is_null = FALSE;
		box->string = g_strdup( str );
	}

	return( box );
}

static gchar *
string_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *temp, *str, *regexp;
	gchar field_sep, str_delim;

	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	if( box->string ){
		field_sep = ofa_stream_format_get_field_sep( format );
		str_delim = ofa_stream_format_get_string_delim( format );
		regexp = g_strdup_printf( "[\"\n\r%c]", field_sep );
		temp = my_utils_quote_regexp( box->string, regexp );
		if( str_delim ){
			str = g_strdup_printf( "%c%s%c", str_delim, temp, str_delim );
		} else {
			str = g_strdup( temp );
		}
		g_free( temp );
		g_free( regexp );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

static void
string_free( ofsBoxData *box )
{
	g_return_if_fail( box->def->type == OFA_TYPE_STRING );

	/*g_debug( "ofa_box_string_free: box=%p", ( void * ) box );*/
	g_free( box->string );
	g_free( box );
}

/*
 * OFA_TYPE_TIMESTAMP
 */
static const GTimeVal *
timestamp_get( const ofsBoxData *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( !box->is_null ){
		return(( const GTimeVal * ) &box->timestamp );
	}

	return( NULL );
}

static void
timestamp_set( ofsBoxData *box, const GTimeVal *value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_TIMESTAMP );

	if( value ){
		box->is_null = FALSE;
		memcpy( &box->timestamp, value, sizeof( GTimeVal ));

	} else {
		box->is_null = TRUE;
	}
}

static ofsBoxData *
timestamp_new_from_dbms( const ofsBoxDef *def, const gchar *str )
{
	ofsBoxData *box;

	box = box_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( my_strlen( str )){
		box->is_null = FALSE;
		my_stamp_set_from_sql( &box->timestamp, str );
	}

	return( box );
}

static gchar *
timestamp_to_string( const ofsBoxData *box, ofaStreamFormat *format )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( box->is_null ){
		str = g_strdup( "" );
	} else {
		str = my_stamp_to_str( &box->timestamp, MY_STAMP_YYMDHMS );
		if( !str ){
			str = g_strdup( "" );
		}
	}

	return( str );
}

typedef gconstpointer ( *GetFn )       ( gconstpointer box );
typedef void          ( *SetFn )       ( gpointer box, gconstpointer value );
typedef gpointer      ( *FromDBMSFn )  ( const ofsBoxDef *def, const gchar *source );
typedef gchar       * ( *ToDBMSFn )    ( gconstpointer box );
typedef gpointer      ( *FromStringFn )( const ofsBoxDef *def, const gchar *source );
typedef gchar       * ( *ToStringFn )  ( gconstpointer box, ofaStreamFormat *format );
typedef void          ( *FreeFn )      ( gpointer box );

typedef struct {
	gint         type;
	GetFn        get_fn;
	SetFn        set_fn;
	FromDBMSFn   from_dbms_fn;
	ToDBMSFn     to_dbms_fn;
	FromStringFn from_string_fn;
	ToStringFn   to_string_fn;
	FreeFn       free_fn;
}
	sBoxHelpers;

static const sBoxHelpers st_box_helpers[] = {
		{ OFA_TYPE_AMOUNT,
				( GetFn )        amount_get,
				( SetFn )        amount_set,
				( FromDBMSFn )   amount_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   amount_to_string,
				( FreeFn )       g_free },
		{ OFA_TYPE_COUNTER,
				( GetFn )        counter_get,
				( SetFn )        counter_set,
				( FromDBMSFn )   counter_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   counter_to_string,
				( FreeFn )       g_free },
		{ OFA_TYPE_INTEGER,
				( GetFn )        int_get,
				( SetFn )        int_set,
				( FromDBMSFn )   int_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   int_to_string,
				( FreeFn )       g_free },
		{ OFA_TYPE_DATE,
				( GetFn )        date_get,
				( SetFn )        date_set,
				( FromDBMSFn )   date_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   date_to_string,
				( FreeFn )       g_free },
		{ OFA_TYPE_STRING,
				( GetFn )        string_get,
				( SetFn )        string_set,
				( FromDBMSFn )   string_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   string_to_string,
				( FreeFn )       string_free },
		{ OFA_TYPE_TIMESTAMP,
				( GetFn )        timestamp_get,
				( SetFn )        timestamp_set,
				( FromDBMSFn )   timestamp_new_from_dbms,
				( ToDBMSFn )     NULL,
				( FromStringFn ) NULL,
				( ToStringFn )   timestamp_to_string,
				( FreeFn )       g_free },
		{ 0 }
};

/**
 * ofa_box_register_types:
 */
void
ofa_box_register_types( void )
{
	static const gchar *thisfn = "ofa_box_register_types";

	g_debug( "%s: sizeof gpointer=%lu", thisfn, sizeof( gpointer ));
	g_debug( "%s: sizeof gdouble=%lu", thisfn, sizeof( gdouble ));
}

/*
 * returns the sBoxHelpers structure for the specified @type
 */
static const sBoxHelpers *
box_get_helper_for_type( ofeBoxType type )
{
	static const gchar *thisfn = "ofa_box_get_helper_for_type";
	const sBoxHelpers *ihelper;

	ihelper = st_box_helpers;
	while( ihelper->type ){
		if( ihelper->type == type ){
			return( ihelper );
		}
		ihelper++;
	}
	g_warning( "%s: no helper for type=%u", thisfn, type );
	return( NULL );
}

static void
box_dump_def( const ofsBoxDef *def )
{
	g_debug( "           id=%d", def->id );
	g_debug( "         dbms=%s", def->dbms );
	g_debug( "          csv=%s", def->csv );
	g_debug( "         type=%u", def->type );
	g_debug( "       import=%s", def->import ? "True":"False" );
	g_debug( "zero_as_empty=%s", def->csv_zero_as_empty ? "True":"False" );
}

/**
 * ofa_box_init_fields_list:
 * @defs: the definition of ofaBox elementary data of the object
 *
 * Returns: the list of fields for the object.
 *  Fields are allocated in the same order than the definitions,
 *  and are empty.
 */
GList *
ofa_box_init_fields_list( const ofsBoxDef *defs )
{
	GList *fields_list;
	const ofsBoxDef *idef;

	fields_list = NULL;

	if( defs ){
		idef = defs;
		while( idef->id ){
			fields_list = g_list_prepend( fields_list, box_new( idef ));
			idef++;
		}
	}

	return( g_list_reverse( fields_list ));
}

/**
 * ofa_box_dump_fields_list:
 * @fname: the name of a function to be printed as a prefix.
 * @list: the list of fields.
 */
void
ofa_box_dump_fields_list( const gchar *fname, const GList *fields )
{
	const GList *it;
	ofsBoxData *box;
	const sBoxHelpers *helper;
	gchar *key, *value;

	for( it=fields ; it ; it=it->next ){
		box = ( ofsBoxData * ) it->data;
		if( 0 ){
			box_dump_def( box->def );
		}
		helper = box_get_helper_for_type( box->def->type );
		key = get_csv_name( box->def );
		value = helper->to_string_fn( box, '\0' );
		g_debug( "%s: %s=%s", fname, key, value );
		g_free( key );
		g_free( value );
	}
}

/**
 * ofa_box_dbms_get_columns_list:
 * @defs: the definition of ofaBox elementary data of the object
 *
 * Returns the list of DBMS columns, as a newly allocated string which
 * should be g_free() by the caller.
 *
 * The returned string is suitable for a DBMS selection.
 */
gchar *
ofa_box_dbms_get_columns_list( const ofsBoxDef *defs )
{
	GString *query;
	const ofsBoxDef *idef;

	query = g_string_new( "" );

	if( defs ){
		idef = defs;
		while( idef->id ){
			if( my_strlen( idef->dbms )){
				if( query->len ){
					query = g_string_append_c( query, ',' );
				}
				g_string_append_printf( query, "%s", idef->dbms );
			}
			idef++;
		}
	}

	return( g_string_free( query, FALSE ));
}

/**
 * ofa_box_dbms_parse_result:
 * @defs: the definition of ofaBox elementary data of the object
 * @row: a row of the DBMS result to be parsed.
 *
 * Returns a newly allocated GList which contains ofaBox-derived
 * elementary data. All data are allocated, though some may be just
 * initialized to NULL values.
 */
GList *
ofa_box_dbms_parse_result( const ofsBoxDef *defs, GSList *row )
{
	GList *fields_list;
	GSList *icol;
	const ofsBoxDef *idef;
	const sBoxHelpers *ihelper;
	gboolean first;

	fields_list = NULL;
	first = TRUE;

	if( row && defs ){
		idef = defs;
		while( idef->id ){
			ihelper = box_get_helper_for_type( idef->type );
			g_return_val_if_fail( ihelper, NULL );
			icol = ( GSList * )( first ? row->data : icol->next );
			fields_list = g_list_prepend( fields_list, ihelper->from_dbms_fn( idef, icol->data ));
			idef++;
			first = FALSE;
		}
	}

	return( g_list_reverse( fields_list ));
}

/**
 * ofa_box_csv_get_header:
 * @boxed: the definition of ofaBox elementary data of the object
 * @format: the #ofaStreamFormat configuration settings.
 *
 * Returns the header of a CSV-type export, with a semi-colon separator,
 * as a newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_box_csv_get_header( const ofsBoxDef *defs, ofaStreamFormat *format )
{
	GString *header;
	const ofsBoxDef *idef;
	gchar *name;
	gchar field_sep, str_delim;

	header = g_string_new( "" );
	field_sep = ofa_stream_format_get_field_sep( format );
	str_delim = ofa_stream_format_get_string_delim( format );

	if( defs ){
		idef = defs;
		while( idef->id ){
			name = get_csv_name( idef );
			if( header->len ){
				header = g_string_append_c( header, field_sep );
			}
			if( str_delim ){
				g_string_append_printf( header, "%c%s%c", str_delim, name, str_delim );
			} else {
				header = g_string_append( header, name );
			}
			g_free( name );
			idef++;
		}
	}

	return( g_string_free( header, FALSE ));
}

static gchar *
get_csv_name( const ofsBoxDef *def )
{
	gchar *name;

	if( my_strlen( def->csv )){
		name = g_strdup( def->csv );

	} else if( my_strlen( def->dbms )){
		name = compute_csv_name( def->dbms );

	} else {
		g_warning( "ofa_box_get_csv_name: empty DBMS name for id=%u", def->id );
		name = g_strdup( "" );
	}

	return( name );
}

/*
 * the CSV column name defaults to be computed from the DBMS column
 * name, by transforming to funny capitalized and removing underscores
 */
static gchar *
compute_csv_name( const gchar *dbms_name )
{
	GString *csv_name;
	gchar **tokens, **iter;
	gchar *first, *first_case, *others, *others_case;

	csv_name = g_string_new( "" );
	tokens = g_strsplit( dbms_name, "_", -1 );
	iter = tokens;
	while( *iter ){
		first = g_utf8_substring( *iter, 0, 1 );
		first_case = g_utf8_strup( first, -1 );
		others = g_utf8_substring( *iter, 1, my_strlen( *iter ));
		others_case = g_utf8_strdown( others, -1 );
		g_string_append_printf( csv_name, "%s%s", first_case, others_case );
		g_free( first );
		g_free( first_case );
		g_free( others );
		g_free( others_case );
		iter++;
	}
	g_strfreev( tokens );

	return( g_string_free( csv_name, FALSE ));
}

/**
 * ofa_box_csv_get_line:
 * @fields_list: the list of elementary datas of the record
 * @format: the #ofaStreamFormat configuration settings.
 *
 * Returns the line of a CSV-type export, with requested configuration,
 * as a newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_box_csv_get_line( const GList *fields_list, ofaStreamFormat *format )
{
	return( ofa_box_csv_get_line_ex( fields_list, format, NULL, NULL ));
}

/**
 * ofa_box_csv_get_line_ex:
 * @fields_list:
 * @format: the #ofaStreamFormat configuration settings.
 * @cb:
 * @user_data:
 */
gchar *
ofa_box_csv_get_line_ex( const GList *fields_list, ofaStreamFormat *format, CSVExportFunc cb, void *user_data )
{
	GString *line;
	const GList *it;
	ofsBoxData *box_data;
	const ofsBoxDef *def;
	const sBoxHelpers *ihelper;
	gchar *str, *str2;
	gchar decimal_sep, field_sep;

	line = g_string_new( "" );
	decimal_sep = ofa_stream_format_get_decimal_sep( format );
	field_sep = ofa_stream_format_get_field_sep( format );

	for( it=fields_list ; it ; it=it->next ){

		box_data = ( ofsBoxData * ) it->data;
		g_return_val_if_fail( box_data, NULL );

		def = box_data->def;
		g_return_val_if_fail( def, NULL );

		ihelper = box_get_helper_for_type( def->type );
		g_return_val_if_fail( ihelper, NULL );

		str = ihelper->to_string_fn( it->data, format );
		if( ihelper->type == OFA_TYPE_AMOUNT ){
			set_decimal_point( str, decimal_sep );
		}

		if( cb ){
			str2 = cb( box_data, format, str, user_data );
		} else {
			str2 = g_strdup( str );
		}

		if( line->len ){
			line = g_string_append_c( line, field_sep );
		}

		g_string_append_printf( line, "%s", str2 );

		g_free( str2 );
		g_free( str );
	}

	return( g_string_free( line, FALSE ));
}

/*
 * change the dot which marks the decimal point in sql-exported amounts
 * to the desired decimal separator
 */
static void
set_decimal_point( gchar *str, gchar decimal_sep )
{
	gint i;

	if( decimal_sep != '.' ){
		for( i=0 ; str[i] ; ++i ){
			if( str[i] == '.' ){
				str[i] = decimal_sep;
				break;
			}
		}
	}
}

/**
 * ofa_box_is_set:
 * @fields_list: the list of elementary datas of the record
 * @id: the identifier of the searched for elementary data
 *
 * Returns: %TRUE if the attached value is set.
 */
gboolean
ofa_box_is_set( const GList *fields_list, gint id )
{
	const GList *it;
	ofsBoxData *box_data;

	for( it=fields_list ; it ; it=it->next ){
		box_data = ( ofsBoxData * ) it->data;
		g_return_val_if_fail( box_data, FALSE );
		g_return_val_if_fail( box_data->def, FALSE );
		g_return_val_if_fail( box_data->def->id, FALSE );

		if( box_data->def->id == id ){
			return( !box_data->is_null );
		}
	}

	return( FALSE );
}

/**
 * ofa_box_get_value:
 * @fields_list: the list of elementary datas of the record
 * @id: the identifier of the searched for elementary data
 *
 * Returns: the attached value..
 */
gconstpointer
ofa_box_get_value( const GList *fields_list, gint id )
{
	const GList *it;
	ofsBoxData *box_data;

	for( it=fields_list ; it ; it=it->next ){
		box_data = ( ofsBoxData * ) it->data;
		g_return_val_if_fail( box_data, NULL );
		g_return_val_if_fail( box_data->def, NULL );
		g_return_val_if_fail( box_data->def->id, NULL );

		if( box_data->def->id == id ){
			return( ofa_box_data_get_value( box_data ));
		}
	}

	return( NULL );
}

/**
 * ofa_box_set_value:
 * @fields_list: the list of elementary datas of the record
 * @id: the identifier of the searched for elementary data
 * @value: the data to be set
 *
 * Set the data into a field.
 */
static void
box_set_value( ofsBoxData *data, gconstpointer value )
{
	const sBoxHelpers *ihelper;

	ihelper = box_get_helper_for_type( data->def->type );
	g_return_if_fail( ihelper );
	ihelper->set_fn( data, value );
}

void
ofa_box_set_value( const GList *fields_list, gint id, gconstpointer value )
{
	static const gchar *thisfn = "ofa_box_set_value";
	const GList *it;
	const ofsBoxDef *def;
	gboolean found;

	/*g_debug( "ofa_box_set_value: fields_list=%p, count=%d, id=%d, value=%p",
			( void * ) fields_list, g_list_length(( GList * ) fields_list ), id, value );*/

	for( it=fields_list, found=FALSE ; it ; it=it->next ){
		def = (( ofsBoxData * ) it->data )->def;
		g_return_if_fail( def );

		if( def->id == id ){
			box_set_value( it->data, value );
			found = TRUE;
			break;
		}
	}

	if( !found ){
		g_warning( "%s: data identifier=%d: not found", thisfn, id );
	}
}

/**
 * ofa_box_free_fields_list:
 * @fields_list: the list of elementary datas of the record
 *
 * Free the list of #ofaBox elementary datas of a record.
 */
static void box_free_data( ofsBoxData *box );

void
ofa_box_free_fields_list( GList *fields_list )
{
	if( fields_list ){
		g_list_free_full( fields_list, ( GDestroyNotify ) box_free_data );
	}
}

static void
box_free_data( ofsBoxData *box )
{
	const ofsBoxDef *def;
	const sBoxHelpers *ihelper;

	def = (( ofsBoxData * ) box )->def;
	g_return_if_fail( def );

	ihelper = box_get_helper_for_type( def->type );
	g_return_if_fail( ihelper );

	ihelper->free_fn( box );
}

/**
 * ofa_box_data_get_def:
 * @box: the #ofaBox data as an opaque structure.
 *
 * Returns: the #ofsBoxDef data definition relative to this @box.
 */
const ofsBoxDef *
ofa_box_data_get_def( const ofsBoxData *box )
{
	return( box->def );
}

/**
 * ofa_box_data_get_value:
 * @box: the #ofaBox data as an opaque structure.
 *
 * Returns: the @box value as a pointer which has to be cast.
 */
gconstpointer
ofa_box_data_get_value( const ofsBoxData *box )
{
	const sBoxHelpers *ihelper;

	g_return_val_if_fail( box, NULL );
	g_return_val_if_fail( box->def, NULL );
	g_return_val_if_fail( box->def->type, NULL );

	ihelper = box_get_helper_for_type( box->def->type );
	g_return_val_if_fail( ihelper, NULL );

	return( ihelper->get_fn( box ));
}
