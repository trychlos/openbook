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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-utils.h"

/* priv instance data
 */
struct _myDatePrivate {
	gboolean     dispose_has_run;
	GDate        date;
};

G_DEFINE_TYPE( myDate, my_date, G_TYPE_OBJECT )

static gboolean date_parse_ddmmyyyy_string( myDate *date, const gchar *str );
static gboolean parse_ddmmyyyy_string( GDate *date, const gchar *str );
static gboolean date_parse_sql_string( myDate *date, const gchar *sql_string );
static gboolean parse_sql_string( GDate *date, const gchar *sql_string );
static gint     date_cmp( const GDate *a, const GDate *b, gboolean infinite_is_past );
static gchar   *date_to_str( const GDate *date, myDateFormat format );

static void     on_date_entry_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gpointer position, gpointer user_data );
static gchar   *date_entry_insert_text_ddmm( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position );
static void     on_date_entry_changed( GtkEditable *editable, gpointer user_data );
/*static void     date_entry_set_label( GtkEditable *editable, const gchar *str );*/

static void
my_date_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_date_finalize";
	myDatePrivate *priv;

	priv = MY_DATE( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_DATE( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_parent_class )->finalize( instance );
}

static void
my_date_dispose( GObject *instance )
{
	myDatePrivate *priv;

	g_return_if_fail( instance && MY_IS_DATE( instance ));

	priv = MY_DATE( instance )->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_parent_class )->dispose( instance );
}

static void
my_date_init( myDate *self )
{
	static const gchar *thisfn = "my_date_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( myDatePrivate, 1 );

	g_date_clear( &self->private->date, 1 );
}

static void
my_date_class_init( myDateClass *klass )
{
	static const gchar *thisfn = "my_date_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = my_date_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_date_finalize;
}

/**
 * my_date_new:
 *
 * Returns: a newly allocated #myDate object.
 * The date is cleared.
 */
myDate *
my_date_new( void )
{
	myDate *date;

	date = g_object_new( MY_TYPE_DATE, NULL );

	return( date );
}

/**
 * my_date_new_from_date:
 *
 * Returns: a newly allocated #myDate object, initialized with the
 * specified date.
 */
myDate *
my_date_new_from_date( const myDate *date )
{
	myDate *new_date;

	new_date = g_object_new( MY_TYPE_DATE, NULL );
	my_date_set_from_date( new_date, date );

	return( new_date );
}

/**
 * my_date_new_from_sql:
 * @sql_string: a pointer to a SQL string 'yyyy-mm-dd', or NULL;
 *  the SQL string may be zero '0000-00-00' or a valid date.
 *
 * Returns: a newly allocated #myDate object.
 *
 * The date is set if the @sql_string can be parsed to a valid
 *  'yyyy-mm-dd' date.
 * It is left cleared else.
 */
myDate *
my_date_new_from_sql( const gchar *sql_string )
{
	myDate *date;

	date = my_date_new();

	date_parse_sql_string( date, sql_string );

	return( date );
}

/**
 * my_date_new_from_str:
 * @str: the string to be parsed.
 * @format: the expected format.
 *
 * Returns: a newly allocated #myDate object.
 * The date is cleared if the @str was NULL or cannot be parsed to a
 * valid date according to the specified @format.
 */
myDate *
my_date_new_from_str( const gchar *str, myDateFormat format )
{
	myDate *date;

	date = my_date_new();

	switch( format ){

		case MY_DATE_DMMM:
			break;

		case MY_DATE_DMYY:
			date_parse_ddmmyyyy_string( date, str );
			break;

		case MY_DATE_SQL:
			date_parse_sql_string( date, str );
			break;

		case MY_DATE_YYMD:
			break;
	}

	return( date );
}

/*
 * Returns TRUE if the string parses to a valid 'dd/mm/yyyy' date
 */
static gboolean
date_parse_ddmmyyyy_string( myDate *date, const gchar *str )
{
	return( parse_ddmmyyyy_string( &date->private->date, str ));
}

static gboolean
parse_ddmmyyyy_string( GDate *date, const gchar *str )
{
	gboolean valid;
	gchar **tokens, **iter;
	gint dd, mm, yy;

	dd = mm = yy = 0;
	valid = FALSE;
	g_date_clear( date, 1 );

	if( str && g_utf8_strlen( str, -1 )){
		tokens = g_strsplit( str, "/", -1 );
		iter = tokens;
		if( *iter && g_utf8_strlen( *iter, -1 )){
			dd = atoi( *iter );
			iter++;
			if( *iter && g_utf8_strlen( *iter, -1 )){
				mm = atoi( *iter );
				iter++;
				if( *iter && g_utf8_strlen( *iter, -1 )){
					yy = atoi( *iter );
				}
			}
		}
		g_strfreev( tokens );
	}

	if( g_date_valid_dmy( dd, mm, yy )){
		g_date_set_dmy( date, dd, mm, yy );
		valid = TRUE;
	}

	return( valid );
}

/*
 * Returns TRUE if the string parses to a valid 'yyyy-mm-dd' date
 *
 * It appears that g_date_set_parse() is rather locale-independant when
 * dealing with a SQL date.
 */
static gboolean
date_parse_sql_string( myDate *date, const gchar *sql_string )
{
	return( parse_sql_string( &date->private->date, sql_string ));
}

static gboolean
parse_sql_string( GDate *date, const gchar *sql_string )
{
	gboolean valid;

	valid = FALSE;
	g_date_clear( date, 1 );

	if( sql_string &&
			g_utf8_strlen( sql_string, -1 ) &&
			g_utf8_collate( sql_string, "0000-00-00" )){

		g_date_set_parse( date, sql_string );
		valid = g_date_valid( date );
	}

	return( valid );
}

/**
 * my_date_is_valid:
 * @date: [not-null]: the #myDate to be evaluated
 *
 * Returns: %TRUE if the date is valid.
 */
gboolean
my_date_is_valid( const myDate *date )
{
	g_return_val_if_fail( date && MY_IS_DATE( date ), FALSE );

	if( date->private->dispose_has_run ){
		return( FALSE );
	}

	return( g_date_valid( &date->private->date ));
}

/**
 * my_date_compare:
 * @a: the first #myDate to be compared
 * @b: the second #yDate to be compared to @a
 * @infinite_is_past: if %TRUE, then an infinite value (i.e. an invalid
 *  date) is considered lesser than anything, but another infinite value.
 *  Else, an invalid value is considered infinite in the future.
 *
 * Compare the two dates, returning -1, 0 or 1 if @a less than, equal or
 * greater than @b.
 * An invalid date is considered infinite.
 *
 * Returns: -1, 0 or 1.
 */
gint
my_date_compare( const myDate *a, const myDate *b, gboolean infinite_is_past )
{
	g_return_val_if_fail( a && MY_IS_DATE( a ), 0 );
	g_return_val_if_fail( b && MY_IS_DATE( b ), 0 );

	if( !a->private->dispose_has_run && !b->private->dispose_has_run ){

		return( date_cmp( &a->private->date, &b->private->date, infinite_is_past ));
	}

	return( 0 );
}

static gint
date_cmp( const GDate *a, const GDate *b, gboolean infinite_is_past )
{
	if( !a || !g_date_valid( a )){
		if( !b || !g_date_valid( b )){
			/* a and b are unset/infinite: returns equal */
			return( 0 );
		}
		/* a is infinite, but not b */
		return( infinite_is_past ? -1 : 1 );
	}

	if( !b || !g_date_valid( b )){
		/* b is infinite, but not a */
		return( infinite_is_past ? 1 : -1 );
	}

	/* a and b are valid */
	return( g_date_compare( a, b ));
}

/**
 * my_date_set_from_date:
 * @date: [out]: a non-NULL pointer to a #myDate object
 * @orig: [in][allow-none]: the #myDate to be copied
 *
 * Set the @date to the give @orig one.
 * The dest @date is left unchanged if the @orig one doesn't represent
 * a valid date.
 *
 * Returns: %TRUE if @orig represents a valid date.
 */
gboolean
my_date_set_from_date( myDate *date, const myDate *orig )
{
	gboolean valid;

	valid = FALSE;

	g_return_val_if_fail( date && MY_IS_DATE( date ), FALSE );
	g_return_val_if_fail( orig && MY_IS_DATE( orig ), FALSE );

	if( orig->private->dispose_has_run || !my_date_is_valid( orig )){
		return( FALSE );
	}

	if( !date->private->dispose_has_run ){

		memcpy( date->private, orig->private, sizeof( myDatePrivate ));
		valid = TRUE;
	}

	return( valid );
}

/**
 * my_date_set_from_str:
 * @date: [out]: a non-NULL pointer to a #myDate object
 * @text: [in][allow-none]: the string to be parsed
 * @format: the expected format of the string
 *
 * Parse a string which represents a date to a #myDate object.
 * The #myDate object is left unchanged if the string doesn't represent
 * a valid date.
 *
 * Returns: %TRUE if the string represents a valid date.
 */
gboolean
my_date_set_from_str( myDate *date, const gchar *text, myDateFormat format )
{
	gboolean valid;

	valid = FALSE;

	g_return_val_if_fail( date && MY_IS_DATE( date ), FALSE );

	if( !date->private->dispose_has_run ){
		switch( format ){
			case MY_DATE_DMYY:
				valid = parse_ddmmyyyy_string( &date->private->date, text );
				break;
			default:
				break;
		}
	}

	return( valid );
}

/**
 * my_date_to_str:
 * @date: the date to be converted to a string
 * @format: the required format.
 *
 * Returns the date as a newly allocated string with the requested
 * format, suitable for display or SQL insertion, or a new empty string
 * if the date is invalid.
 *
 * It is up to the caller to g_free() the returned string.
 */
gchar *
my_date_to_str( const myDate *date, myDateFormat format )
{
	gchar *str;

	g_return_val_if_fail( date && MY_IS_DATE( date ), NULL );

	str = g_strdup( "" );

	if( !date->private->dispose_has_run ){

		g_free( str );
		str = date_to_str( &date->private->date, format );
	}

	return( str );
}

static gchar *
date_to_str( const GDate *date, myDateFormat format )
{
	static const gchar *st_month[] = {
			N_( "jan." ),
			N_( "feb." ),
			N_( "mar." ),
			N_( "apr." ),
			N_( "may" ),
			N_( "jun." ),
			N_( "jul." ),
			N_( "aug." ),
			N_( "sept." ),
			N_( "oct." ),
			N_( "nov." ),
			N_( "dec." )
	};

	gchar *str;

	str = g_strdup( "" );

	if( date && g_date_valid( date )){
		switch( format ){

			/* d mmm yyyy - display as a label */
			case MY_DATE_DMMM:
				g_free( str );
				str = g_strdup_printf( "%d %s %4.4d",
						g_date_get_day( date ),
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				break;

			/* dd/mm/yyyy - display for entry */
			case MY_DATE_DMYY:
				g_free( str );
				str = g_strdup_printf( "%2.2d/%2.2d/%4.4d",
						g_date_get_day( date ),
						g_date_get_month( date ),
						g_date_get_year( date ));
				break;

			/* yyyy-mm-dd - suitable for SQL insertion */
			case MY_DATE_SQL:
				g_free( str );
				str = g_strdup_printf( "%4.4d-%2.2d-%2.2d",
						g_date_get_year( date ),
						g_date_get_month( date ),
						g_date_get_day( date ));
				break;

			/* yyyymmdd - for filenames */
			case MY_DATE_YYMD:
				g_free( str );
				str = g_strdup_printf( "%4.4d%2.2d%2.2d",
						g_date_get_year( date ),
						g_date_get_month( date ),
						g_date_get_day( date ));
				break;
		}
	}

	return( str );
}

/**
 * my_date_set_now:
 */
GDate *
my_date_set_now( GDate *date )
{
	GDateTime *dt;
	gint year, month, day;

	g_return_val_if_fail( date, NULL );

	dt = g_date_time_new_now_local();
	g_date_time_get_ymd( dt, &year, &month, &day );
	g_date_time_unref( dt );

	g_date_set_dmy( date, day, month, year );

	return( date );
}

/**
 * my_date_set_from_sql:
 * @dest: must be a pointer to the destination GDate structure.
 * @sql_string: a pointer to a SQL string 'yyyy-mm-dd', or NULL;
 *  the SQL string may be zero '0000-00-00' or a valid date.
 *
 * Parse a SQL string, putting the result in @dest.
 *
 * NB: parsing a 'yyyy-mm-dd' is not locale-sensitive.
 */
GDate *
my_date_set_from_sql( GDate *dest, const gchar *sql_string )
{
	g_return_val_if_fail( dest, NULL );

	g_date_clear( dest, 1 );

	if( sql_string &&
			g_utf8_strlen( sql_string, -1 ) &&
			g_utf8_collate( sql_string, "0000-00-00" )){

		g_date_set_parse( dest, sql_string );
		/*g_debug( "my_date_set_from_sql: dd=%u, mm=%u, yyyy=%u",
					g_date_get_day( dest ), g_date_get_month( dest ), g_date_get_year( dest ));*/
	}

	return( dest );
}

/**
 * my_date_set_from_date:
 * @dest: must be a pointer to the destination GDate structure.
 * @src: a pointer to a source GDate structure, or NULL.
 *
 * Copy one date to another.
 */
GDate *
my_date2_set_from_date( GDate *dest, const GDate *src)
{
	g_return_val_if_fail( dest, NULL );

	g_date_clear( dest, 1 );

	if( src && g_date_valid( src )){
		*dest = *src;
	}

	return( dest );
}

gchar *
my_date2_to_str( const GDate *date, myDateFormat format )
{
	return( date_to_str( date, format ));
}

/**
 * my_date2_from_str:
 */
gboolean
my_date2_from_str( GDate *date, const gchar *text, myDateFormat format )
{
	gboolean valid;

	valid = FALSE;

	switch( format ){
		case MY_DATE_DMYY:
			valid = parse_ddmmyyyy_string( date, text );
			break;
		default:
			break;
	}

	return( valid );
}

/**
 * my_date_parse_from_entry:
 * @entry:
 * @format_entry: ignored for now, only considering input as dd/mm/yyyy
 * @label: [allow-none]:
 * @label_entry:
 * date:
 */
void
my_date_parse_from_entry( const myDateParse *parms )
{
	gchar *str;

	g_return_if_fail( parms->entry && GTK_IS_ENTRY( parms->entry ));
	g_return_if_fail( !parms->label || GTK_IS_LABEL( parms->label ));
	g_return_if_fail( parms->date );

	g_object_set_data( G_OBJECT( parms->entry ), "date-format-entry", GINT_TO_POINTER( parms->entry_format ));
	g_object_set_data( G_OBJECT( parms->entry ), "date-label", parms->label );
	g_object_set_data( G_OBJECT( parms->entry ), "date-format-label", GINT_TO_POINTER( parms->label_format ));
	g_object_set_data( G_OBJECT( parms->entry ), "date-date", parms->date );
	g_object_set_data( G_OBJECT( parms->entry ), "date-check-cb", parms->pfnCheck );
	g_object_set_data( G_OBJECT( parms->entry ), "date-user-data", parms->user_data );

	g_signal_connect( G_OBJECT( parms->entry ), "insert-text", G_CALLBACK( on_date_entry_insert_text ), NULL );
	g_signal_connect( G_OBJECT( parms->entry ), "changed", G_CALLBACK( on_date_entry_changed ), NULL );

	if( parms->on_changed_cb ){
		g_signal_connect( G_OBJECT( parms->entry ), "changed", parms->on_changed_cb, parms->user_data );
	}

	if( g_date_valid( parms->date )){
		str = date_to_str( parms->date, parms->entry_format );
		gtk_entry_set_text( GTK_ENTRY( parms->entry ), str );
		g_free( str );
	}
}

/*
 * if we are typing in the entry, then new_text is only the typed char
 * if we are pasting from the clipboard, then new_text is the whole pasted text
 *
 * accept almost any separator, replacing it with /
 */
static void
on_date_entry_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gpointer position, gpointer user_data )
{
	myDateFormat entry_format;
	gchar *str;

	/*g_debug( "on_date_entry_insert_text: editable=%p, new_text='%s', newt_text_length=%d, position=%p (%d), user_data=%p",
			( void * ) editable, new_text, new_text_length, ( void * ) position, *( gint * ) position, ( void * ) user_data );*/

	str = NULL;
	entry_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-entry" ));

	if( entry_format == MY_DATE_DMYY ){
		str = date_entry_insert_text_ddmm( editable, new_text, new_text_length, ( gint * ) position );
	}

	if( str ){
		g_signal_handlers_block_by_func( editable, ( gpointer ) on_date_entry_insert_text, user_data );
		gtk_editable_insert_text( editable, str, g_utf8_strlen( str, -1 ), position );
		g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_date_entry_insert_text, user_data );
		g_free( str );
	}

	g_signal_stop_emission_by_name( editable, "insert-text" );
}

static gchar *
date_entry_insert_text_ddmm( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position )
{
	static const int st_days[] = { 31,29,31,30,31,30,31,31,30,31,30,31 };
	gunichar ichar;
	gint ival;
	gint pos;
	gint vnum, month, day;
	gchar *str;

	str = NULL;

	/* typing text in the entry - check for each typed char */
	if( new_text_length == 1 ){
		pos = *position;
		ichar = g_utf8_get_char( new_text );
		if( g_unichar_isdigit( ichar )){
			ival = g_unichar_digit_value( ichar );
			if( pos >= 0 && pos <= 1 ){
				if( pos == 0 ){
					if( ival <= 3 ){
						str = g_strdup( new_text );
					} else {
						str = g_strdup_printf( "0%d/", ival );
					}
				} else if( pos == 1 ){
					vnum = 10 * atoi( gtk_editable_get_chars( editable, 0, 1 )) + ival;
					if( vnum <= 31 ){
						month = atoi( gtk_editable_get_chars( editable, 3, 5 ));
						if( month <= 0 || vnum <= st_days[month-1 ]){
							str = g_strdup( new_text );
						}
					}
				}
			} else if( pos >= 3 && pos <= 4 ){
				if( pos == 3 ){
					if( ival <= 1 ){
						str = g_strdup( new_text );
					} else {
						str = g_strdup_printf( "0%d/", ival );
					}
				} else if( pos == 4 ){
					vnum = 10 * atoi( gtk_editable_get_chars( editable, 3, 4 )) + ival;
					if( vnum <= 12 ){
						str = g_strdup( new_text );
					}
				}
			} else if( pos == 6 ){
				if( ival <= 5 ){
					str = g_strdup_printf( "20%d", ival );
				} else {
					str = g_strdup_printf( "19%d", ival );
				}
			} else if( pos <= 9 ){
				str = g_strdup( new_text );
			}
		} else if( pos == 1 ){
			day = atoi( gtk_editable_get_chars( editable, 0, 1 ));
			if( day > 0 ){
				gtk_editable_delete_text( editable, 0, 1 );
				str = g_strdup_printf( "%2.2u/", day );
				*position = 0;
			}
		} else if( pos == 4 ){
			month = atoi( gtk_editable_get_chars( editable, 3, 4 ));
			if( month > 0 ){
				gtk_editable_delete_text( editable, 3, 4 );
				str = g_strdup_printf( "%2.2u/", month );
				*position = 3;
			}
		} else if( pos == 2  || pos == 5 ){
			str = g_strdup( "/" );
		}
	} else {
		str = g_strdup( new_text );
	}

	return( str );
}

/*
 * this callback is called after the above one, i.e. after the newly
 * entered character has been inserted in the field
 */
static void
on_date_entry_changed( GtkEditable *editable, gpointer user_data )
{
#if 0
	myDateFormat entry_format, label_format;
	GDate temp_date;
	GDate *date;
	gchar *str;
	myDateCheckCb pfnCheck;
	gpointer user_data_cb;

	entry_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-entry" ));

	my_date_set_from_str(
			&temp_date, gtk_entry_get_text( GTK_ENTRY( editable )), entry_format );

	pfnCheck = g_object_get_data( G_OBJECT( editable ), "date-check-cb" );
	user_data_cb = g_object_get_data( G_OBJECT( editable ), "date-user-data" );

	if( g_date_valid( &temp_date ) &&
			( !pfnCheck || ( *pfnCheck )( &temp_date, user_data_cb ))){

		label_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-label" ));
		str = date_to_str( &temp_date, label_format );
		date_entry_set_label( editable, str );
		g_free( str );

		date = g_object_get_data( G_OBJECT( editable ), "date-date" );
		my_date_set_from_date( date, &temp_date );

	} else {
		/* gi18n: invalid date */
		str = g_strdup( _( "invalid" ));
		date_entry_set_label( editable, str );
		g_free( str );
	}
#endif
}

#if 0
static void
date_entry_set_label( GtkEditable *editable, const gchar *str )
{
	GtkWidget *label;
	gchar *markup;

	label = g_object_get_data( G_OBJECT( editable ), "date-label" );

	if( label && GTK_IS_LABEL( label )){
		gtk_widget_set_sensitive( label, FALSE );

		markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", str );
		gtk_label_set_markup( GTK_LABEL( label ), markup );
		g_free( markup );
	}
}
#endif
