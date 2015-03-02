/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"

#include "ui/my-editable-date.h"

typedef struct {
	guint date_format;
	guint max_length;					/* entry max length */
}
	sDateFormat;

static const sDateFormat st_formats[] = {
		{ MY_DATE_DMMM, 11 },			/* d mmm yyyy */
		{ MY_DATE_MMYY,  9 },			/* mmm yyyy */
		{ MY_DATE_DMYY, 10 },			/* dd/mm/yyyy */
		{ MY_DATE_SQL,  10 },			/* yyyy-mm-dd */
		{ MY_DATE_YYMD,  8 },			/* yyyymmdd */
		{ 0 }
};

/*
 * data attached to each implementor object
 */
typedef struct {
	GDate              date;
	const sDateFormat *format;
	gboolean           valid;			/* whether the string parses to a valid date */
	gboolean           setting_text;
	GtkWidget         *label;
	myDateFormat       label_format;
	gboolean           mandatory;
}
	sEditableDate;

#define DEFAULT_ENTRY_FORMAT            MY_DATE_DMYY
#define DEFAULT_MANDATORY               TRUE

#define EDITABLE_DATE_DATA              "my-editable-date-data"

static const gboolean st_debug          = FALSE;
#define DEBUG                           if( st_debug ) g_debug

static gint           st_year           = -1;

static void               on_editable_finalized( sEditableDate *data, GObject *was_the_editable );
static sEditableDate     *get_editable_date_data( GtkEditable *editable );
static const sDateFormat *get_date_format( guint date_format );
static void               on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data );
static gchar             *on_text_inserted_dmyy( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data );
static void               insert_char_at_pos( GtkEditable *editable, gint pos, gchar c, sEditableDate *data );
static void               on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableDate *data );
static void               on_changed( GtkEditable *editable, sEditableDate *data );
static gboolean           on_key_pressed( GtkWidget *widget, GdkEventKey *event, sEditableDate *data );
static void               try_for_completion( sEditableDate *data, GtkEntry *entry );
static gboolean           on_focus_in( GtkWidget *entry, GdkEvent *event, sEditableDate *data );
static gboolean           on_focus_out( GtkWidget *entry, GdkEvent *event, sEditableDate *data );
/*static gchar         *editable_date_get_string( GtkEditable *editable, sEditableDate **pdata );*/
static void               editable_date_render( GtkEditable *editable );

/**
 * my_editable_date_init:
 * @editable: the #GtkEditable object
 *
 * Initialize the GtkEditable to enter a date. Is supposed to be
 * called each time the edition is started.
 */
void
my_editable_date_init( GtkEditable *editable )
{
	static const gchar *thisfn = "my_editable_date_init";
	sEditableDate *data;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) editable, G_OBJECT_TYPE_NAME( editable ));

	data = get_editable_date_data( editable );

	g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_finalized, data );
}

static void
on_editable_finalized( sEditableDate *data, GObject *finalized_editable )
{
	static const gchar *thisfn = "my_editable_date_on_weak_notify";

	g_debug( "%s: data=%p, finalized_editable=%p",
			thisfn, ( void * ) data, ( void * ) finalized_editable );

	g_free( data );
}

static sEditableDate *
get_editable_date_data( GtkEditable *editable )
{
	sEditableDate *data;

	data = ( sEditableDate * ) g_object_get_data( G_OBJECT( editable ), EDITABLE_DATE_DATA );

	if( !data ){
		data = g_new0( sEditableDate, 1 );
		g_object_set_data( G_OBJECT( editable ), EDITABLE_DATE_DATA, data );

		data->setting_text = FALSE;
		my_editable_date_set_format( editable, -1 );
		data->mandatory = DEFAULT_MANDATORY;

		g_signal_connect(
				G_OBJECT( editable ), "insert-text", G_CALLBACK( on_text_inserted ), data );
		g_signal_connect(
				G_OBJECT( editable ), "delete-text", G_CALLBACK( on_text_deleted ), data );
		g_signal_connect(
				G_OBJECT( editable ), "changed", G_CALLBACK( on_changed ), data );

		if( GTK_IS_WIDGET( editable )){
			g_signal_connect(
					G_OBJECT( editable ), "focus-in-event", G_CALLBACK( on_focus_in ), data );
			g_signal_connect(
					G_OBJECT( editable ), "focus-out-event", G_CALLBACK( on_focus_out ), data );
			g_signal_connect(
					G_OBJECT( editable ), "key-press-event", G_CALLBACK( on_key_pressed ), data );
		}
	}

	return( data );
}

static const sDateFormat *
get_date_format( guint date_format )
{
	gint i;

	for( i=0 ; st_formats[i].date_format ; ++i ){
		if( st_formats[i].date_format == date_format ){
			return( &st_formats[i] );
		}
	}

	return( NULL );
}

/**
 * my_editable_date_set_format:
 * @editable: this #GtkEditable instance.
 * @format: the allowed format for the date; set to -1 to restore the
 *  default value.
 *
 * Set up the current date format.
 */
void
my_editable_date_set_format( GtkEditable *editable, myDateFormat format )
{
	sEditableDate *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( format == -1 || ( format >= MY_DATE_FIRST && format < MY_DATE_LAST ));

	data = get_editable_date_data( editable );

	if( format == -1 ){
		data->format = get_date_format( DEFAULT_ENTRY_FORMAT );
	} else {
		data->format = get_date_format( format );
	}

	if( GTK_IS_ENTRY( editable )){
		gtk_entry_set_max_length( GTK_ENTRY( editable ), data->format->max_length );
	}
}

static void
on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data )
{
	static const gchar *thisfn = "my_editable_date_on_text_inserted";
	gchar *text;

	DEBUG( "%s: editable=%p, new_text=%s, new_text_length=%u, position=%d",
			thisfn, ( void * ) editable, new_text, new_text_length, *position );

	text = NULL;

	if( data->setting_text ){
		DEBUG( "%s: setting_text=%s", thisfn, "True" );
		text = g_strndup( new_text, new_text_length );

	} else {
		switch( data->format->date_format ){
			case MY_DATE_DMYY:
				text = on_text_inserted_dmyy( editable, new_text, new_text_length, ( gint * ) position, data );
				break;
			default:
				g_warning( "%s: unhandled format %u", thisfn, data->format->date_format );
				break;
		}
	}

	if( text && g_utf8_strlen( text, -1 )){
		g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_inserted, data );
		gtk_editable_insert_text( editable, text, g_utf8_strlen( text, -1 ), position );
		g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_inserted, data );
	}

	g_free( text );
	g_signal_stop_emission_by_name( editable, "insert-text" );
}

/*
 * inserting a text at pos:
 * - we maintain in 'str_result' an image of the final result,
 * - each char from 'new_text' is checked against its future position in the
 *   final result
 * - we simultaneously build in 'str_insert' the string to be inserted
 */
static gchar *
on_text_inserted_dmyy( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data )
{
	static const gchar *thisfn = "my_editable_date_on_text_inserted_dmyy";
	static const int st_days[] = { 31,29,31,30,31,30,31,31,30,31,30,31 };
	gint i, pos;
	gchar *ithpos;
	gunichar ithchar;
	gboolean ok;
	gchar *str;
	GString *str_result, *str_insert;
	gchar **components;
	gint day, month, new_num, len_day;
	gboolean have_year, shortcut_millenary, done, have_two_slashes;
	gint ival;

	shortcut_millenary = TRUE;
	have_two_slashes = FALSE;

	/* 'str_result' is initialized with the initial content of the
	 * GtkEditable, will be incremented with each validated char */
	str = gtk_editable_get_chars( editable, 0, -1 );
	str_result = g_string_new( str );
	g_free( str );

	/* 'str_insert' is built with each validated char, plus maybe
	 * automated chars (e;g. '/' slash separator */
	str_insert = g_string_new( "" );

	/* each char of the inserted text is tested at its future position
	 * we may have to insert more characters in order to have a good
	 * representation of the date:
	 * - a '0' as the dizain of the day or the month,
	 * - a '/' before month or year
	 * - the century of the year if shortcut is allowed
	 * 'pos' is the position in 'str_full' into which the next char will
	 * be inserted */
	for( ok=TRUE, i=0, pos=*position ; ok && i<new_text_length ; ++i ){

		/* split the components of the current date
		 * at this moment, there should not be any error (but the date
		 * may be incomplete) */
		day = 0;
		month = 0;
		len_day = 0;
		have_year = FALSE;
		components = NULL;
		if( g_utf8_strlen( str_result->str, -1 )){
			components = g_strsplit( str_result->str, "/", -1 );
			if( *components ){
				if( g_utf8_strlen( *components, -1 )){
					day = atoi( *components );
					len_day = g_utf8_strlen( *components, -1 );
				}
				if( *( components+1 )){
					if( g_utf8_strlen( *( components+1 ), -1 )){
						month = atoi( *( components+1 ));
					}
					if( *( components+2 )){
						have_two_slashes = TRUE;
						if( g_utf8_strlen( *( components+2 ), -1 )){
							have_year = TRUE;
						}
						if( *( components+3 )){
							g_warning( "%s: current date has more than three components", thisfn );
							ok = FALSE;
							continue;
						}
					}
				}
			}
		}

		ithpos = g_utf8_offset_to_pointer( new_text, i );
		ithchar = g_utf8_get_char( ithpos );

		DEBUG( "%s: i=%u, char='%c', pos=%u, position=%u, day=%u, month=%u, have_year=%s",
				thisfn, i, ithchar, pos, *position, day, month, have_year ? "True":"False" );

		if( g_unichar_isdigit( ithchar )){
			ival = g_unichar_digit_value( ithchar );

			/* are we entering a day ?
			 * we must have 'd' to be able to insert a digit
			 *  (because 'dd' is automatically followed by a slash)
			 * pos may be 0 or 1 */
			if( pos <= 1 ){
				if( pos == 0 && day > 0 ){
					new_num = 10*ival + day;
				} else {
					new_num = 10*day + ival;
				}
				if( new_num <= 31 &&
						( !month || ( month >= 1 && month <= 12 && new_num <= st_days[month-1] ))){
					if( pos == 0 && new_num >= 4 && new_num < 10 ){
						str_insert = g_string_append_c( str_insert, '0' );
						str_result = g_string_insert_c( str_result, pos, '0' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					}
					str_insert = g_string_append_c( str_insert, ithchar );
					str_result = g_string_insert_c( str_result, pos, ithchar );
					pos += 1;
					DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
							thisfn, str_insert->str, *position, str_result->str, pos );
					if( pos == 2 && str_result->str[pos] != '/' && ( i == new_text_length-1 || new_text[i+1] != '/' )){
						str_insert = g_string_append_c( str_insert, '/' );
						str_result = g_string_insert_c( str_result, pos, '/' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					}
				} else {
					ok = FALSE;
					continue;
				}

			/* are we entering a month ?
			 * we must have something like d or dd or dd/m or dd/m/yyyy
			 * in order to be able to insert a digit */
			} else if( pos >= 2 && pos <= 4 ){
				if( pos == 2 ){
					if( str_result->str[pos] != '/' && !have_two_slashes ){
						str_insert = g_string_append_c( str_insert, '/' );
						str_result = g_string_insert_c( str_result, pos, '/' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					} else {
						ok = FALSE;
						continue;
					}
				}
				if( pos == 3 && month > 0 ){
					new_num = 10*ival + month;
				} else {
					new_num = 10*month + ival;
				}
				if( new_num >= 1 && new_num <= 12 && day <= st_days[new_num-1] ){
					if( pos == 3 && new_num >= 2 && new_num < 10 ){
						str_insert = g_string_append_c( str_insert, '0' );
						str_result = g_string_insert_c( str_result, pos, '0' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					}
					str_insert = g_string_append_c( str_insert, ithchar );
					str_result = g_string_insert_c( str_result, pos, ithchar );
					pos += 1;
					DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
							thisfn, str_insert->str, *position, str_result->str, pos );
					if( pos == 5 && str_result->str[pos] != '/' &&
							( i == new_text_length-1 || new_text[i+1] != '/' ) &&
							!have_two_slashes ){
						str_insert = g_string_append_c( str_insert, '/' );
						str_result = g_string_insert_c( str_result, pos, '/' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					}
				} else {
					ok = FALSE;
					continue;
				}

			/* are we entering a year ?
			 * we are willing to have some sort of shortcut
			 * we have dd/mm/yyyy */
			} else if( pos >= 5 && pos <= 9 ){
				if( pos == 5 ){
					if( str_result->str[pos] != '/' && !have_two_slashes ){
						str_insert = g_string_append_c( str_insert, '/' );
						str_result = g_string_insert_c( str_result, pos, '/' );
						pos += 1;
						DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
								thisfn, str_insert->str, *position, str_result->str, pos );
					} else {
						ok = FALSE;
						continue;
					}
				}
				if( pos == 6 && !have_year && shortcut_millenary ){
					if( ival <= 5 ){
						str_insert = g_string_append( str_insert, "20" );
						str_result = g_string_insert( str_result, pos, "20" );
					} else {
						str_insert = g_string_append( str_insert, "19" );
						str_result = g_string_insert( str_result, pos, "19" );
					}
					pos += 2;
					DEBUG( "%s: str_insert=%s, position=%u, str_result=%s, pos=%u",
							thisfn, str_insert->str, *position, str_result->str, pos );
				}
				str_insert = g_string_append_c( str_insert, ithchar );
				str_result = g_string_insert_c( str_result, pos, ithchar );
				pos += 1;
			} else {
				ok = FALSE;
				continue;
			}

		/* are we entering a separator ?
		 * only slashes are accepted
		 * unable to enter again a separator if we already have our
		 * three components */
		} else if( ithchar == '/' && !have_two_slashes && !have_year ){

			done = FALSE;

			if( pos == 1 ){
				insert_char_at_pos( editable, 0, '0', data );
				str_result = g_string_insert_c( str_result, 0, '0' );
				pos += 1;
				*position += 1;
			}
			if( pos == 2 ){
				str_insert = g_string_append_c( str_insert, ithchar );
				str_result = g_string_insert_c( str_result, pos, ithchar );
				pos += 1;
				done = TRUE;
			}
			if( pos == 3 && !done ){
				if( len_day < 2 ){
					insert_char_at_pos( editable, 0, '0', data );
					str_result = g_string_insert_c( str_result, 0, '0' );
					*position += 1;
				} else {
					str_insert = g_string_append_c( str_insert, ithchar );
					str_result = g_string_insert_c( str_result, pos, ithchar );
					done = TRUE;
				}
				pos += 1;
			}
			if( pos == 4 && !done ){
				if( len_day < 2 ){
					insert_char_at_pos( editable, 0, '0', data );
					str_result = g_string_insert_c( str_result, 0, '0' );
				} else {
					insert_char_at_pos( editable, 3, '0', data );
					str_result = g_string_insert_c( str_result, 3, '0' );
				}
				*position += 1;
				pos += 1;
			}
			if( pos == 5 && !done ){
				str_insert = g_string_append_c( str_insert, ithchar );
				str_result = g_string_insert_c( str_result, pos, ithchar );
				pos += 1;
				done = TRUE;
			}
			if( !done ){
				ok = FALSE;
				continue;
			}

		/* neither a digit nor a separator */
		} else {
			ok = FALSE;
			continue;
		}

		g_strfreev( components );
	}

	g_string_free( str_result, TRUE );

	DEBUG( "%s: ok=%s, str_insert=%s", thisfn, ok ? "True":"False", str_insert->str );
	str = g_string_free( str_insert, FALSE );
	if( !ok ){
		g_free( str );
		str = NULL;
	}

	return( str );
}

/*
 * insert the given @c char in the GtkEditable at given @pos position
 * no signal is triggered
 */
static void
insert_char_at_pos( GtkEditable *editable, gint pos, gchar c, sEditableDate *data )
{
	gchar *text;

	text = g_strdup_printf( "%c", c );

	data->setting_text = TRUE;

	g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_inserted, data );
	gtk_editable_insert_text( editable, text, g_utf8_strlen( text, -1 ), &pos );
	g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_inserted, data );

	g_free( text );
}

static void
on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableDate *data )
{
	DEBUG( "my_editable_date_on_text_deleted: editable=%p, start=%d, end=%d",
			( void * ) editable, start_pos, end_pos );

	g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_deleted, data );
	gtk_editable_delete_text( editable, start_pos, end_pos );
	g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_deleted, data );

	g_signal_stop_emission_by_name( editable, "delete-text" );
}

static void
on_changed( GtkEditable *editable, sEditableDate *data )
{
	static const gchar *thisfn = "my_editable_date_on_changed";
	gchar *text, *markup;
	gint len_text;

	if( !data->setting_text ){
		text = gtk_editable_get_chars( editable, 0, -1 );
		len_text = g_utf8_strlen( text, -1 );
		my_date_set_from_str( &data->date, text, data->format->date_format );
		data->valid = my_date_is_valid( &data->date );
		DEBUG( "%s: editable=%p, data=%p, text='%s', valid=%s",
				thisfn, ( void * ) editable, ( void * ) data, text, data->valid ? "True":"False" );
		g_free( text );

		if( data->label ){
			g_return_if_fail( GTK_IS_LABEL( data->label ));
			g_return_if_fail( data->label_format >= MY_DATE_FIRST && data->label_format < MY_DATE_LAST );
			if( data->valid ){
				text = my_date_to_str( &data->date, data->label_format );
			} else if( len_text || data->mandatory ){
				text = g_strdup( _( "invalid date" ));
			} else {
				text = g_strdup( "" );
			}
			markup = g_markup_printf_escaped( "<span fgcolor=\"#666666\" style=\"italic\">%s</span>", text );
			g_free( text );
			gtk_label_set_markup( GTK_LABEL( data->label), markup );
			g_free( markup );
		}

	} else {
		DEBUG( "%s: editable=%p, data=%p: set 'setting_text' to False", thisfn, ( void * ) editable, ( void * ) data );
		data->setting_text = FALSE;
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * We automatically open a selection dialog box for the account if we
 * are leaving the field with a Tab key while it is invalid
 *
 * Note that if we decide to open the selection dialog box, then the
 * Gtk toolkit will complain as we return too late from this function
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, sEditableDate *data )
{
	if( event->state == 0 && event->keyval == GDK_KEY_Tab ){
		try_for_completion( data, GTK_ENTRY( widget ));
	}

	return( FALSE );
}

static void
try_for_completion( sEditableDate *data, GtkEntry *entry )
{
	const gchar *cstr;
	GDate date;

	if( data->valid ){
		st_year = g_date_get_year( &data->date );

	} else {
		cstr = gtk_entry_get_text( entry );
		my_date_set_from_str_ex( &date, cstr, data->format->date_format, &st_year );
		if( my_date_is_valid( &date )){
			my_editable_date_set_date( GTK_EDITABLE( entry ), &date );
		}
	}
}

/*
 * Returns :
 *  TRUE to stop other handlers from being invoked for the event.
 *  FALSE to propagate the event further.
 */
static gboolean
on_focus_in( GtkWidget *entry, GdkEvent *event, sEditableDate *data )
{
	g_return_val_if_fail( GTK_IS_EDITABLE( entry ), TRUE );

	return( FALSE );
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEvent *event, sEditableDate *data )
{
	g_return_val_if_fail( GTK_IS_EDITABLE( entry ), TRUE );

	return( FALSE );
}

/**
 * my_editable_date_set_date:
 * @editable: this #GtkEditable instance.
 * @date: the date to be set
 *
 * Set up the current date.
 */
void
my_editable_date_set_date( GtkEditable *editable, const GDate *date )
{
	sEditableDate *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_date_data( editable );

	my_date_set_from_date( &data->date, date );

	/* render the date, triggering a 'changed' signal */
	editable_date_render( editable );
	on_changed( editable, data );
}

/**
 * my_editable_date_set_label:
 * @editable: this #GtkEditable instance.
 * @label: a #GtkWidget which will be updated with a representation of
 *  the current date at each change.
 * @format: the format of the representation
 *
 * When a @label and a @format are set, then the entered date will be
 * displayed with the specified @format into the specified @label, as
 * the user enters the date in the main #GtkEditable.
 */
void
my_editable_date_set_label( GtkEditable *editable, GtkWidget *label, myDateFormat format )
{
	sEditableDate *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( label && GTK_IS_LABEL ( label ));
	g_return_if_fail( format >= MY_DATE_FIRST && format < MY_DATE_LAST );

	data = get_editable_date_data( editable );

	data->label = label;
	data->label_format = format;
}

/**
 * my_editable_date_set_mandatory:
 * @editable: this #GtkEditable instance.
 * @mandatory: whether the date is mandatory, i.e. also invalid when
 *  empty.
 */
void
my_editable_date_set_mandatory( GtkEditable *editable, gboolean mandatory )
{
	sEditableDate *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_date_data( editable );

	data->mandatory = mandatory;
}

/**
 * my_editable_date_get_date:
 * @editable: this #GtkEditable instance.
 * @valid: [out]: whether the current date is valid or not
 *
 * A pointer to the current date. This pointer is read-only and should
 * not be cleared nor modified by the caller.
 */
const GDate *
my_editable_date_get_date( GtkEditable *editable, gboolean *valid )
{
	sEditableDate *data;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	data = get_editable_date_data( editable );

	if( valid ){
		*valid = data->valid;
	}

	return(( const GDate * ) &data->date );
}

/*
 * my_editable_date_render:
 * @editable: this #GtkEditable instance.
 *
 * Displays the representation of the current date.
 * Should be called when the edition finishes.
 *
 * An invalid date is just rendered as an empty string.
 */
static void
editable_date_render( GtkEditable *editable )
{
	sEditableDate *data;
	gchar *text;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	if( GTK_IS_ENTRY( editable )){
		data = get_editable_date_data( editable );
		text = my_date_to_str( &data->date, data->format->date_format );
		data->setting_text = TRUE;
		gtk_entry_set_text( GTK_ENTRY( editable ), text );
		data->setting_text = FALSE;
		g_free( text );
	}
}

/**
 * my_editable_date_is_empty:
 * @editable: this #GtkEditable instance.
 *
 * Returns: %TRUE if the #GtkEditable is empty.
 */
gboolean
my_editable_date_is_empty( GtkEditable *editable )
{
	gchar *text;
	gboolean empty;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	text = gtk_editable_get_chars( editable, 0, -1 );
	empty = g_utf8_strlen( text, -1 ) == 0;
	g_free( text );

	return( empty );
}
