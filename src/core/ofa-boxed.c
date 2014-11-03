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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-boxed.h"

/*
 * our boxed type
 */
typedef struct {
	const ofsBoxedDef *def;
	gboolean           is_null;
	union {
		gdouble  amount;
		gint     counter;
		GDate    date;
		gchar   *string;
		GTimeVal timestamp;
	};
}
	sBoxed;

static sBoxed *
boxed_new( const ofsBoxedDef *def )
{
	sBoxed *box;

	box = g_new0( sBoxed, 1 );
	box->def = def;
	box->is_null = TRUE;

	return( box );
}

static sBoxed *
amount_new_from_dbms_str( const ofsBoxedDef *def, const gchar *str )
{
	sBoxed *box;

	box = boxed_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_AMOUNT, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		box->is_null = FALSE;
		box->amount = my_double_set_from_sql( str );
	}

	return( box );
}

static gchar *
amount_to_csv_str( const sBoxed *box )
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

static gpointer
amount_get_fn( const sBoxed *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_AMOUNT, 0 );

	return( GDOUBLE_TO_POINTER( box->amount ));
}

static void
amount_set_fn( sBoxed *box, gdouble value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_AMOUNT );

	box->is_null = FALSE;
	box->amount = value;
}

static sBoxed *
counter_new_from_dbms_str( const ofsBoxedDef *def, const gchar *str )
{
	sBoxed *box;

	box = boxed_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		box->is_null = FALSE;
		box->counter = atoi( str );
	}

	return( box );
}

static gchar *
counter_to_csv_str( const sBoxed *box )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, NULL );

	if( box->counter || !box->def->csv_zero_as_empty ){
		str = g_strdup_printf( "%d", box->counter );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

static gint
counter_get_fn( const sBoxed *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_COUNTER, 0 );

	return( box->counter );
}

static void
counter_set_fn( sBoxed *box, gint value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_COUNTER );

	box->is_null = FALSE;
	box->counter = value;
}

static sBoxed *
date_new_from_dbms_str( const ofsBoxedDef *def, const gchar *str )
{
	sBoxed *box;

	box = boxed_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_DATE, NULL );
	my_date_clear( &box->date );

	if( str && g_utf8_strlen( str, -1 )){
		box->is_null = FALSE;
		my_date_set_from_sql( &box->date, str );
		g_debug( "date_new_from_dbms_str: date=%s", my_date_to_str( &box->date, MY_DATE_DMYY ));
	}

	return( box );
}

static gchar *
date_to_csv_str( const sBoxed *box )
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

static const GDate *
date_get_fn( const sBoxed *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_DATE, NULL );

	return(( const GDate * ) &box->date );
}

static void
date_set_fn( sBoxed *box, const GDate *value )
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

static sBoxed *
string_new_from_dbms_str( const ofsBoxedDef *def, const gchar *str )
{
	sBoxed *box;

	box = boxed_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		box->is_null = FALSE;
		box->string = g_strdup( str );
	}

	return( box );
}

static void
string_free( sBoxed *box )
{
	g_return_if_fail( box->def->type == OFA_TYPE_STRING );

	/*g_debug( "ofa_boxed_string_free: box=%p", ( void * ) box );*/
	g_free( box->string );
	g_free( box );
}

static gchar *
string_to_csv_str( const sBoxed *box )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	if( box->string ){
		str = g_strdup( box->string );
	} else {
		str = g_strdup( "" );
	}

	return( str );
}

static const gchar *
string_get_fn( const sBoxed *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_STRING, NULL );

	return(( const gchar * ) box->string );
}

static void
string_set_fn( sBoxed *box, const gchar *value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_STRING );

	g_free( box->string );

	if( value && g_utf8_strlen( value, -1 )){
		box->is_null = FALSE;
		box->string = g_strdup( value );

	} else {
		box->is_null = TRUE;
		box->string = NULL;
	}
}

static sBoxed *
timestamp_new_from_dbms_str( const ofsBoxedDef *def, const gchar *str )
{
	sBoxed *box;

	box = boxed_new( def );
	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		box->is_null = FALSE;
		my_utils_stamp_set_from_sql( &box->timestamp, str );
	}

	return( box );
}

static gchar *
timestamp_to_csv_str( const sBoxed *box )
{
	gchar *str;

	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( box->is_null ){
		str = g_strdup( "" );
	} else {
		str = my_utils_stamp_to_str( &box->timestamp, MY_STAMP_YYMDHMS );
	}

	return( str );
}

static const GTimeVal *
timestamp_get_fn( const sBoxed *box )
{
	g_return_val_if_fail( box->def->type == OFA_TYPE_TIMESTAMP, NULL );

	if( !box->is_null ){
		return(( const GTimeVal * ) &box->timestamp );
	}

	return( NULL );
}

static void
timestamp_set_fn( sBoxed *box, const GTimeVal *value )
{
	g_return_if_fail( box->def->type == OFA_TYPE_TIMESTAMP );

	if( value ){
		box->is_null = FALSE;
		memcpy( &box->timestamp, value, sizeof( GTimeVal ));

	} else {
		box->is_null = TRUE;
	}
}

typedef gpointer      ( *NewFromDBMSStrFn )( const ofsBoxedDef *def, const gchar *str );
typedef void          ( *FreeFn )          ( gpointer box );
typedef gchar       * ( *ExportToCSVStrFn )( gconstpointer box );
typedef gconstpointer ( *GetFn )           ( gconstpointer box );
typedef void          ( *SetFn )           ( gpointer box, gconstpointer value );

typedef struct {
	gint             id;
	NewFromDBMSStrFn new_from_dbms_fn;
	FreeFn           free_fn;
	ExportToCSVStrFn to_csv_str_fn;
	GetFn            get_fn;
	SetFn            set_fn;
}
	sBoxedHelpers;

static const sBoxedHelpers st_boxed_helpers[] = {
		{ OFA_TYPE_AMOUNT,
				( NewFromDBMSStrFn ) amount_new_from_dbms_str,
				( FreeFn )           g_free,
				( ExportToCSVStrFn ) amount_to_csv_str,
				( GetFn )            amount_get_fn,
				( SetFn )            amount_set_fn },
		{ OFA_TYPE_COUNTER,
				( NewFromDBMSStrFn ) counter_new_from_dbms_str,
				( FreeFn )           g_free,
				( ExportToCSVStrFn ) counter_to_csv_str,
				( GetFn )            counter_get_fn,
				( SetFn )            counter_set_fn },
		{ OFA_TYPE_DATE,
				( NewFromDBMSStrFn ) date_new_from_dbms_str,
				( FreeFn )           g_free,
				( ExportToCSVStrFn ) date_to_csv_str,
				( GetFn )            date_get_fn,
				( SetFn )            date_set_fn },
		{ OFA_TYPE_STRING,
				( NewFromDBMSStrFn ) string_new_from_dbms_str,
				( FreeFn )           string_free,
				( ExportToCSVStrFn ) string_to_csv_str,
				( GetFn )            string_get_fn,
				( SetFn )            string_set_fn },
		{ OFA_TYPE_TIMESTAMP,
				( NewFromDBMSStrFn ) timestamp_new_from_dbms_str,
				( FreeFn )           g_free,
				( ExportToCSVStrFn ) timestamp_to_csv_str,
				( GetFn )            timestamp_get_fn,
				( SetFn )            timestamp_set_fn },
		{ 0 }
};

/**
 * ofa_boxed_register_types:
 *
 * Register our GBoxed derived types which will hold all our elementary
 * data.
 */
void
ofa_boxed_register_types( void )
{
	static const gchar *thisfn = "ofa_boxed_register_types";

	g_debug( "%s: sizeof gpointer=%lu", thisfn, sizeof( gpointer ));
	g_debug( "%s: sizeof gdouble=%lu", thisfn, sizeof( gdouble ));
}

/*
 * returns the sBoxedHelpers structure for the specified @type
 */
static const sBoxedHelpers *
boxed_get_helper_for_type( eBoxedType type )
{
	const sBoxedHelpers *ihelper;

	ihelper = st_boxed_helpers;
	while( ihelper->id ){
		if( ihelper->id == type ){
			return( ihelper );
		}
		ihelper++;
	}
	return( NULL );
}

/*
 * ofa_boxed_get_dbms_columns:
 * @defs: the definition of ofaBoxed elementary data of the object
 *
 * Returns the list of DBMS columns, as a newly allocated string which
 * should be g_free() by the caller.
 *
 * The returned string is suitable for a DBMS selection.
 */
gchar *
ofa_boxed_get_dbms_columns( const ofsBoxedDef *defs )
{
	GString *query;
	const ofsBoxedDef *idef;

	query = g_string_new( "" );

	if( defs ){
		idef = defs;

		while( idef->id ){
			if( idef->dbms && g_utf8_strlen( idef->dbms, -1 )){
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
 * ofa_boxed_parse_dbms_result:
 * @defs: the definition of ofaBoxed elementary data of the object
 * @row: a row of the DBMS result to be parsed.
 *
 * Returns a newly allocated GList which contains ofaBoxed-derived
 * elementary data. All data are allocated, though some may be just
 * initialized to NULL values.
 */
GList *
ofa_boxed_parse_dbms_result( const ofsBoxedDef *defs, GSList *row )
{
	GList *data;
	GSList *icol;
	const ofsBoxedDef *idef;
	const sBoxedHelpers *ihelper;
	gboolean first;

	data = NULL;
	first = TRUE;

	if( row && defs ){
		idef = defs;
		while( idef->id ){
			ihelper = boxed_get_helper_for_type( idef->type );
			g_return_val_if_fail( ihelper, NULL );
			icol = ( GSList * )( first ? row->data : icol->next );
			data = g_list_prepend( data, ihelper->new_from_dbms_fn( idef, icol->data ));
			idef++;
			first = FALSE;
		}
	}

	return( g_list_reverse( data ));
}

/**
 * ofa_boxed_get_csv_header:
 * @boxed: the definition of ofaBoxed elementary data of the object
 * @field_sep: the field separator (usually a semi-colon)
 *
 * Returns the header of a CSV-type export, with a semi-colon separator,
 * as a newly allocated string which should be g_free() by the caller.
 */
static gchar *get_csv_name( const ofsBoxedDef *def );
static gchar *compute_csv_name( const gchar *dbms_name );

gchar *
ofa_boxed_get_csv_header( const ofsBoxedDef *defs, gchar field_sep )
{
	GString *header;
	const ofsBoxedDef *idef;
	gchar *name;

	header = g_string_new( "" );

	if( defs ){
		idef = defs;
		while( idef->id ){
			name = get_csv_name( idef );
			if( header->len ){
				header = g_string_append_c( header, field_sep );
			}
			g_string_append_printf( header, "%s", name );
			g_free( name );
			idef++;
		}
	}

	return( g_string_free( header, FALSE ));
}

static gchar *
get_csv_name( const ofsBoxedDef *def )
{
	gchar *name;

	if( def->csv && g_utf8_strlen( def->csv, -1 )){
		name = g_strdup( def->csv );
	} else if( def->dbms && g_utf8_strlen( def->dbms, -1 )){
		name = compute_csv_name( def->dbms );
	} else {
		g_warning( "ofa_boxed_get_csv_name: empty DBMS name for id=%u", def->id );
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
		others = g_utf8_substring( *iter, 1, g_utf8_strlen( *iter, -1 ));
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
 * ofa_boxed_get_csv_line:
 * @fields_list: the list of elementary datas of the record
 * @field_sep: the field separator (usually a semi-colon)
 * @decimal_sep: the decimal point separator (usually a dot or a comma)
 *
 * Returns the line of a CSV-type export, with a semi-colon separator,
 * as a newly allocated string which should be g_free() by the caller.
 */
static void set_decimal_point( gchar *str, gchar decimal_sep );

gchar *
ofa_boxed_get_csv_line( const GList *fields_list, gchar field_sep, gchar decimal_sep )
{
	GString *line;
	const GList *it;
	const ofsBoxedDef *def;
	const sBoxedHelpers *ihelper;
	gchar *str;

	line = g_string_new( "" );

	for( it=fields_list ; it ; it=it->next ){

		def = (( sBoxed * ) it->data )->def;
		g_return_val_if_fail( def, NULL );

		ihelper = boxed_get_helper_for_type( def->type );
		g_return_val_if_fail( ihelper, NULL );

		str = ihelper->to_csv_str_fn( it->data );
		if( ihelper->id == OFA_TYPE_AMOUNT ){
			set_decimal_point( str, decimal_sep );
		}

		if( line->len ){
			line = g_string_append_c( line, field_sep );
		}

		g_string_append_printf( line, "%s", str );
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
 * ofa_boxed_get_value:
 * @fields_list: the list of elementary datas of the record
 * @id: the identifier of the searched for elementary data
 *
 * Free the elementary datas of a record.
 */
gconstpointer
ofa_boxed_get_value( const GList *fields_list, gint id )
{
	const GList *it;
	const ofsBoxedDef *def;
	const sBoxedHelpers *ihelper;

	for( it=fields_list ; it ; it=it->next ){
		def = (( sBoxed * ) it->data )->def;
		g_return_val_if_fail( def, NULL );

		if( def->id == id ){
			ihelper = boxed_get_helper_for_type( def->type );
			g_return_val_if_fail( ihelper, NULL );

			return( ihelper->get_fn( it->data ));
		}
	}

	return( NULL );
}

/**
 * ofa_boxed_set_value:
 * @fields_list: the list of elementary datas of the record
 * @id: the identifier of the searched for elementary data
 * @value: the data to be set
 *
 * Set the data into a field.
 */
void
ofa_boxed_set_value( const GList *fields_list, gint id, gconstpointer value )
{
	const GList *it;
	const ofsBoxedDef *def;
	const sBoxedHelpers *ihelper;

	for( it=fields_list ; it ; it=it->next ){
		def = (( sBoxed * ) it->data )->def;
		g_return_if_fail( def );

		if( def->id == id ){
			ihelper = boxed_get_helper_for_type( def->type );
			g_return_if_fail( ihelper );

			ihelper->set_fn( it->data, value );
		}
	}
}

/**
 * ofa_boxed_free_fields_list:
 * @fields_list: the list of elementary datas of the record
 *
 * Free the list of #ofaBoxed elementary datas of a record.
 */
static void boxed_free_data( sBoxed *box );

void
ofa_boxed_free_fields_list( GList *fields_list )
{
	if( fields_list ){
		g_list_free_full( fields_list, ( GDestroyNotify ) boxed_free_data );
	}
}

static void
boxed_free_data( sBoxed *box )
{
	const ofsBoxedDef *def;
	const sBoxedHelpers *ihelper;

	def = (( sBoxed * ) box )->def;
	g_return_if_fail( def );

	ihelper = boxed_get_helper_for_type( def->type );
	g_return_if_fail( ihelper );

	ihelper->free_fn( box );
}
