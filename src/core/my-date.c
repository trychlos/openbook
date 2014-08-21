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

#include "api/my-date.h"
#include "api/my-utils.h"

/* private instance data
 */
struct _myDatePrivate {
	gboolean dispose_has_run;
};

G_DEFINE_TYPE( myDate, my_date, G_TYPE_OBJECT )

static void    on_date_entry_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gpointer position, gpointer user_data );
static gchar  *date_entry_insert_text_ddmm( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position );
static void    on_date_entry_changed( GtkEditable *editable, gpointer user_data );
static void    date_entry_set_label( GtkEditable *editable, const gchar *str );
static void    date_entry_parse_ddmm( GDate *date, const gchar *text );

static void
my_date_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_date_finalize";
	myDate *self;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && MY_IS_DATE( instance ));

	self = MY_DATE( instance );

	/* free data members here */
	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_parent_class )->finalize( instance );
}

static void
my_date_dispose( GObject *instance )
{
	myDate *self;

	g_return_if_fail( instance && MY_IS_DATE( instance ));

	self = MY_DATE( instance );

	if( !self->private->dispose_has_run ){

		self->private->dispose_has_run = TRUE;

		/* unref member objects here */

	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_date_parent_class )->dispose( instance );
}

static void
my_date_init( myDate *self )
{
	static const gchar *thisfn = "my_date_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( myDatePrivate, 1 );

	self->private->dispose_has_run = FALSE;
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
my_date_set_from_date( GDate *dest, const GDate *src)
{
	g_return_val_if_fail( dest, NULL );

	g_date_clear( dest, 1 );

	if( src && g_date_valid( src )){
		*dest = *src;
	}

	return( dest );
}

/**
 * my_date_to_str:
 *
 * Returns the date with the requested format, suitable for display or
 * SQL insertion, in a newly allocated string that the user must
 * g_free(), or a new empty string if the date is invalid.
 */
gchar *
my_date_to_str( const GDate *date, myDateFormat format )
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
			case MY_DATE_DDMM:
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
		}
	}

	return( str );
}

/**
 * my_date_cmp:
 * @infinite_is_past: if %TRUE, then an infinite value (i.e. an invalid
 *  date) is considered lesser than anything, but another infinite value.
 *  Else, an invalid value is considered infinite in the future.
 *
 * Compare the two dates, returning -1, 0 or 1 if a less than, equal or
 * greater than b.
 * An invalid date is considered infinite.
 *
 * Returns: -1, 0 or 1.
 */
gint
my_date_cmp( const GDate *a, const GDate *b, gboolean infinite_is_past )
{
	if( !a || !g_date_valid( a )){
		if( !b || !g_date_valid( b)){
			/* a and b are infinite: returns equal */
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
		str = my_date_to_str( parms->date, parms->entry_format );
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

	if( entry_format == MY_DATE_DDMM ){
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
	myDateFormat entry_format, label_format;
	GDate temp_date;
	GDate *date;
	gchar *str;
	myDateCheckCb pfnCheck;
	gpointer user_data_cb;

	entry_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-entry" ));

	my_date_parse_from_str(
			&temp_date, gtk_entry_get_text( GTK_ENTRY( editable )), entry_format );

	pfnCheck = g_object_get_data( G_OBJECT( editable ), "date-check-cb" );
	user_data_cb = g_object_get_data( G_OBJECT( editable ), "date-user-data" );

	if( g_date_valid( &temp_date ) &&
			( !pfnCheck || ( *pfnCheck )( &temp_date, user_data_cb ))){

		label_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( editable ), "date-format-label" ));
		str = my_date_to_str( &temp_date, label_format );
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
}

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

/**
 * my_date_parse_from_str:
 * @date: a non-NULL pointer to a GDate structure
 * @test: [allow-none]:
 */
GDate *
my_date_parse_from_str( GDate *date, const gchar *text, myDateFormat format )
{
	g_date_clear( date, 1 );

	if( format == MY_DATE_DDMM ){
		date_entry_parse_ddmm( date, text );
	}

	return( date );
}

static void
date_entry_parse_ddmm( GDate *date, const gchar *text )
{
	gchar **tokens, **iter;
	gint dd, mm, yy;

	dd = mm = yy = 0;
	if( text && g_utf8_strlen( text, -1 )){
		tokens = g_strsplit( text, "/", -1 );
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
	}
}
