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

#include "ui/my-editable-date.h"

typedef struct {
	guint date_format;
	guint max_length;					/* entry max length */
}
	sDateFormat;

static const sDateFormat st_formats[] = {
		{ MY_DATE_DMMM, 11 },			/* d mmm yyyy */
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
}
	sEditableDate;

#define DEFAULT_ENTRY_FORMAT    MY_DATE_DMYY
#define EDITABLE_DATE_DATA      "my-editable-date-data"

static void               on_editable_weak_notify( sEditableDate *data, GObject *was_the_editable );
static sEditableDate     *get_editable_date_data( GtkEditable *editable );
static const sDateFormat *get_date_format( guint date_format );
static void               on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data );
static gchar             *on_text_inserted_dmyy( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data );
static void               on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableDate *data );
static void               on_changed( GtkEditable *editable, sEditableDate *data );
static gboolean           on_focus_in( GtkWidget *entry, GdkEvent *event, sEditableDate *data );
static gboolean           on_focus_out( GtkWidget *entry, GdkEvent *event, sEditableDate *data );
/*static gchar         *editable_date_get_string( GtkEditable *editable, sEditableDate **pdata );*/

/**
 * my_editable_date_init:
 * @editable: the #GtkEditable object
 *
 * Initialize the GtkEditable to enter an amount. Is supposed to be
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

	g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_weak_notify, data );
}

static void
on_editable_weak_notify( sEditableDate *data, GObject *was_the_editable )
{
	static const gchar *thisfn = "my_editable_date_on_weak_notify";

	g_debug( "%s: data=%p, the_editable_was=%p",
			thisfn, ( void * ) data, ( void * ) was_the_editable );

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
	gchar *text;

	g_debug( "my_editable_date_on_text_inserted: editable=%p, new_text=%s, position=%d",
			( void * ) editable, new_text, *position );

	text = NULL;

	if( data->setting_text ){
		text = g_strndup( new_text, new_text_length );

	} else {
		switch( data->format->date_format ){
			case MY_DATE_DMYY:
				text = on_text_inserted_dmyy( editable, new_text, new_text_length, ( gint * ) position, data );
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
 * - each char is compared to the final result
 * - we simultaneously build the string to be inserted
 */
static gchar *
on_text_inserted_dmyy( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableDate *data )
{
	static const int st_days[] = { 31,29,31,30,31,30,31,31,30,31,30,31 };
	gchar *str;
	GString *strfull, *current;
	GString *text;
	gint i;
	gchar *ithpos;
	gunichar ithchar;
	gint pos;
	gint ival;
	gboolean ok;
	gint newnum, prevday, prevmonth;
	gboolean havemonth, haveyear;
	gchar **components;
	gboolean shortcut_millenary;

	/* strfull is supposed to be empty, or like 'dd/mm/yyyy' minus one
	 * char because we must be able to insert one
	 * it will be completed with newly inserted chars as we are checking
	 * them from inserted new_text */
	str = gtk_editable_get_chars( editable, 0, -1 );
	strfull = g_string_new( str );
	g_free( str );

	/* text is the returned string: this is the entered new_text, maybe
	 * plus a trailing slash */
	text = g_string_new( "" );

	ok = TRUE;
	shortcut_millenary = TRUE;
	pos = *position;

	for( i=0 ; ok && i<new_text_length ; ++i, ++pos ){
		ithpos = g_utf8_offset_to_pointer( new_text, i );
		ithchar = g_utf8_get_char( ithpos );
		ok = FALSE;

		components = NULL;
		prevday = 0;
		prevmonth = 0;
		havemonth = FALSE;
		haveyear = FALSE;
		if( g_utf8_strlen( strfull->str, -1 )){
			components = g_strsplit( strfull->str, "/", -1 );
			if( *components && g_utf8_strlen( *components, -1 )){
				prevday = atoi( *components );
				if( *( components+1 ) && g_utf8_strlen( *( components+1 ), -1 )){
					havemonth = TRUE;
					prevmonth = atoi( *( components+1 ));
					if( *( components+2 ) && g_utf8_strlen( *( components+2 ), -1 )){
						haveyear = TRUE;
					}
				}
			}
		}
		g_debug( "on_text_inserted_dmyy: prevday=%u, prevmonth=%u, havemonth=%s, haveyear=%s",
				prevday, prevmonth, havemonth ? "True":"False", haveyear ? "True":"False" );

		/* only allowed chars are digits and slashes */
		if( g_unichar_isdigit( ithchar )){
			ival = g_unichar_digit_value( ithchar );

			/* are we entering a day ?
			 * we must have d to be able to insert a digit
			 *  (because 'dd' is automatically followed by a slash)
			 * pos may be 0 or 1 */
			if( pos <= 1 ){
				if( pos == 0 && prevday > 0 ){
					newnum = 10*ival + prevday;
				} else {
					newnum = 10*prevday + ival;
				}
				if( newnum >= 1 && newnum <= 31 &&
						( !havemonth ||
								( prevmonth >= 1 && prevmonth <= 12 && newnum <= st_days[prevmonth-1] ))){
					current = g_string_new( "" );
					if( pos == 0 && ival >= 4 ){
						current = g_string_append_c( current, '0' );
					}
					current = g_string_append_c( current, ithchar );
					if(( pos == 0 && ival >= 4 ) || ( pos == 1 && strfull->str[2] != '/' )){
						current = g_string_append_c( current, '/' );
					}
					text = g_string_append( text, current->str );
					strfull = g_string_assign( strfull, current->str );
					if( havemonth ){
						strfull = g_string_append( strfull, *( components+1 ));
						if( haveyear ){
							strfull = g_string_append_c( strfull, '/' );
							strfull = g_string_append( strfull, *( components+2 ));
						}
					}
					g_string_free( current, TRUE );
					ok = TRUE;
				}

			/* are we entering a month ?
			 * we must have something like dd/m or dd/m/yyyy in order
			 * to be able to insert a digit
			 * pos is 3 or 4 */
			} else if( pos == 3 || pos == 4 ){
				if( pos == 3 && prevmonth > 0 ){
					newnum = 10*ival + prevmonth;
				} else {
					newnum = 10*prevmonth + ival;
				}
				if( newnum >= 1 && newnum <= 12 && prevday <= st_days[newnum-1] ){
					current = g_string_new( "" );
					if( pos == 3 && ival >= 2 ){
						current = g_string_append_c( current, '0' );
					}
					current = g_string_append_c( current, ithchar );
					if(( pos == 3 && ival >= 2 ) || ( pos == 4 && strfull->str[5] != '/' )){
						current = g_string_append_c( current, '/' );
					}
					text = g_string_append( text, current->str );
					g_string_printf( strfull, "%s/%s/", *components, current->str );
					if( haveyear ){
						strfull = g_string_append( strfull, *( components+2 ));
					}
					g_string_free( current, TRUE );
					ok = TRUE;
				}

			/* are we entering a year ?
			 * we are willing to have some sort of shortcut
			 * we have dd/mm/yyyy
			 * pos is 6 to 9
			 */
			} else if( pos >= 6 && pos <= 9 ){
				current = g_string_new( "" );
				if( pos == 6 && !haveyear && shortcut_millenary ){
					if( ival <= 5 ){
						current = g_string_append( current, "20" );
					} else {
						current = g_string_append( current, "19" );
					}
				}
				current = g_string_append_c( current, ithchar );
				text = g_string_append( text, current->str );
				g_string_printf( strfull, "%s/%s/%s", *components, *( components+1 ), current->str );
				g_string_free( current, TRUE );
				ok = TRUE;
			}

		/* are we entering a separator ?
		 * only slashes are accepted */
		} else if( ithchar == '/' ){

			if( pos == 1 ){
				*position -= 1;
				current = g_string_new( "" );
				g_string_printf( current, "%2.2d/", prevday );
				text = g_string_assign( text, current->str );
				strfull = g_string_assign( strfull, current->str );
				if( havemonth ){
					strfull = g_string_append( strfull, *( components+1 ));
					if( haveyear ){
						strfull = g_string_append_c( strfull, '/' );
						strfull = g_string_append( strfull, *( components+2 ));
					}
				}
				g_string_free( current, TRUE );
				ok = TRUE;

			} else if( pos == 2 ){
				text = g_string_append_c( text, '/' );
				g_string_printf( strfull, "%2.2d/", prevday );
				if( havemonth ){
					strfull = g_string_append( strfull, *( components+1 ));
					if( haveyear ){
						strfull = g_string_append_c( strfull, '/' );
						strfull = g_string_append( strfull, *( components+2 ));
					}
				}
				ok = TRUE;

			} else if( pos == 4 ){
				*position -= 1;
				current = g_string_new( "" );
				g_string_printf( current, "%2.2d/", prevmonth );
				text = g_string_assign( text, current->str );
				g_string_printf( strfull, "%2.2d/%2.2d/", prevday, prevmonth );
				if( haveyear ){
					strfull = g_string_append( strfull, *( components+2 ));
				}
				g_string_free( current, TRUE );
				ok = TRUE;

			} else if( pos == 5 ){
				text = g_string_append_c( text, '/' );
				g_string_printf( strfull, "%2.2d/", prevmonth );
				if( haveyear ){
					strfull = g_string_append( strfull, *( components+2 ));
				}
				ok = TRUE;
			}
		}

		g_strfreev( components );
	}

	g_string_free( strfull, TRUE );

	str = g_string_free( text, FALSE );
	if( !ok ){
		g_free( str );
		str = NULL;
	}

	return( str );
}

static void
on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableDate *data )
{
	g_debug( "my_editable_date_on_text_deleted: editable=%p, start=%d, end=%d",
			( void * ) editable, start_pos, end_pos );

	g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_deleted, data );
	gtk_editable_delete_text( editable, start_pos, end_pos );
	g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_deleted, data );

	g_signal_stop_emission_by_name( editable, "delete-text" );
}

static void
on_changed( GtkEditable *editable, sEditableDate *data )
{
	gchar *text;

	g_debug( "my_editable_date_on_changed: editable=%p", ( void * ) editable );

	if( !data->setting_text ){
		text = gtk_editable_get_chars( editable, 0, -1 );
		my_date_set_from_str( &data->date, text, data->format->date_format );
		data->valid = my_date_is_valid( &data->date );
		g_free( text );

	} else {
		data->setting_text = FALSE;
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

	my_editable_date_render( editable );
}

/**
 * my_editable_date_set_label:
 * @editable: this #GtkEditable instance.
 * @label: a #GtkWidget which will be updated with a representation of
 *  the current date at each change.
 * @format: the format of the representation
 */
void
my_editable_date_set_label( GtkEditable *editable, GtkWidget *label, myDateFormat format )
{
	sEditableDate *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_date_data( editable );

	data->label = label;
	data->label_format = format;
}

/**
 * my_editable_date_get_date:
 * @editable: this #GtkEditable instance.
 * @valid: [out]: whether the current date is valid or not
 *
 * A pointer to the current date. This pointer is read-only and should
 * not be cleared nor modified by the caller.
 *
 * The validity pointer is set depending of the currently displayed
 * string (not the currently stored #GDate which is itself only
 * modified when the string parses to a valid date).
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

#if 0
/**
 * my_editable_date_get_string:
 * @editable: this #GtkEditable instance.
 *
 * Returns the localized representation of the current amount as a
 * newly allocated string which should be g_free() by the caller.
 */
gchar *
my_editable_date_get_string( GtkEditable *editable )
{
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	text = editable_date_get_string( editable, NULL );

	return( text );
}

static gchar *
editable_date_get_string( GtkEditable *editable, sEditableDate **pdata )
{
	sEditableDate *data;
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	data = get_editable_date_data( editable );

	text = g_strdup_printf( "%'.*lf", data->decimals, data->amount );

	if( pdata ){
		*pdata = data;
	}

	return( text );
}
#endif

/**
 * my_editable_date_render:
 * @editable: this #GtkEditable instance.
 *
 * Displays the representation of the current date.
 * Should be called when the edition finishes.
 *
 * An invalid date is just rendered as an empty string.
 */
void
my_editable_date_render( GtkEditable *editable )
{
	sEditableDate *data;
	gchar *text;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	if( GTK_IS_ENTRY( editable )){
		data = get_editable_date_data( editable );
		text = my_date_to_str( &data->date, data->format->date_format );
		data->setting_text = TRUE;
		gtk_entry_set_text( GTK_ENTRY( editable ), text );
		g_free( text );
	}
}
