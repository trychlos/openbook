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

#include "api/my-utils.h"

#include "ui/my-editable-amount.h"

/*
 * data attached to each implementor object
 */
typedef struct {
	guint    decimals;
	gdouble  amount;
	gboolean has_decimal;
	gboolean setting_text;
}
	sEditableAmount;

#define DEFAULT_DECIMALS          2
#define EDITABLE_AMOUNT_DATA      "my-editable-amount-data"

static void             on_editable_weak_notify( sEditableAmount *data, GObject *was_the_editable );
static sEditableAmount *get_editable_amount_data( GtkEditable *editable );
static void             on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableAmount *data );
static void             on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableAmount *data );
static void             on_changed( GtkEditable *editable, sEditableAmount *data );
static gchar           *editable_amount_get_string( GtkEditable *editable, sEditableAmount **pdata );

/**
 * my_editable_amount_init:
 * @editable: the #GtkEditable object
 *
 * Initialize the GtkEditable to enter an amount. Is supposed to be
 * called each time the edition is started on this renderer for this
 * row.
 *
 * When starting to edit an amount:
 * - remove thousand separator (space)
 * - replace decimal comma with a dot
 *
 */
void
my_editable_amount_init( GtkEditable *editable )
{
	static const gchar *thisfn = "my_editable_amount_init";
	sEditableAmount *data;
	GtkEntry *entry;
	gchar *text;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) editable, G_OBJECT_TYPE_NAME( editable ));

	if( GTK_IS_ENTRY( editable )){
		gtk_entry_set_alignment( GTK_ENTRY( editable ), 1 );
	}

	data = get_editable_amount_data( editable );

	if( GTK_IS_ENTRY( editable )){
		entry = GTK_ENTRY( editable );
		text = my_utils_double_undecorate( gtk_entry_get_text( entry ));
		gtk_entry_set_text( entry, text );
		data->has_decimal = ( g_strstr_len( text, -1, "." ) != NULL );
		g_free( text );
	}

	g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_weak_notify, data );
}

static void
on_editable_weak_notify( sEditableAmount *data, GObject *was_the_editable )
{
	static const gchar *thisfn = "my_editable_amount_on_weak_notify";

	g_debug( "%s: data=%p, the_editable_was=%p",
			thisfn, ( void * ) data, ( void * ) was_the_editable );

	g_free( data );
}

static sEditableAmount *
get_editable_amount_data( GtkEditable *editable )
{
	sEditableAmount *data;

	data = ( sEditableAmount * ) g_object_get_data( G_OBJECT( editable ), EDITABLE_AMOUNT_DATA );

	if( !data ){
		data = g_new0( sEditableAmount, 1 );
		g_object_set_data( G_OBJECT( editable ), EDITABLE_AMOUNT_DATA, data );
		data->decimals = DEFAULT_DECIMALS;
		data->amount = 0;

		g_signal_connect(
				G_OBJECT( editable ), "insert-text", G_CALLBACK( on_text_inserted ), data );
		g_signal_connect(
				G_OBJECT( editable ), "delete-text", G_CALLBACK( on_text_deleted ), data );
		g_signal_connect(
				G_OBJECT( editable ), "changed", G_CALLBACK( on_changed ), data );
	}

	return( data );
}

static void
on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditableAmount *data )
{
	gint i;
	gboolean is_ok;
	gunichar ithchar;
	gchar *ithpos;
	gint decimal_dot;

	is_ok = TRUE;
	decimal_dot = 0;

	if( !data->setting_text ){
		for( i=0 ; i<new_text_length && is_ok ; ++i ){
			ithpos = g_utf8_offset_to_pointer( new_text, i );
			ithchar = g_utf8_get_char( ithpos );
			if( g_unichar_isdigit( ithchar )){
				continue;
			}
			if( data->has_decimal ){
				is_ok = FALSE;
				continue;
			}
			if( ithchar == '.' && decimal_dot == 0 ){
				decimal_dot += 1;
				continue;
			}
			is_ok = FALSE;
		}
	}

	if( is_ok ){
		if( decimal_dot > 0 ){
			data->has_decimal = TRUE;
		}
		g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_inserted, data );
		gtk_editable_insert_text( editable, new_text, new_text_length, position );
		g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_inserted, data );
	}

	g_signal_stop_emission_by_name( editable, "insert-text" );
}

static void
on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditableAmount *data )
{
	gchar *text;

	text = gtk_editable_get_chars( editable, start_pos, end_pos );
	if( g_strstr_len( text, -1, "." ) != NULL ){
		data->has_decimal = FALSE;
	}
	g_free( text );

	g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_deleted, data );
	gtk_editable_delete_text( editable, start_pos, end_pos );
	g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_deleted, data );

	g_signal_stop_emission_by_name( editable, "delete-text" );
}

static void
on_changed( GtkEditable *editable, sEditableAmount *data )
{
	gchar *text1, *text2;

	if( !data->setting_text ){
		text1 = gtk_editable_get_chars( editable, 0, -1 );
		text2 = my_utils_double_undecorate( text1 );

		data->amount = g_strtod( text2, NULL );

		g_free( text2 );
		g_free( text1 );

	} else {
		data->setting_text = FALSE;
	}
}

/**
 * my_editable_amount_get_decimals:
 * @editable: this #GtkEditable instance.
 *
 * Returns the current decimals count.
 */
guint
my_editable_amount_get_decimals( GtkEditable *editable )
{
	sEditableAmount *data;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), 0 );

	data = get_editable_amount_data( editable );

	return( data->decimals );
}

/**
 * my_editable_amount_set_decimals:
 * @editable: this #GtkEditable instance.
 * @decimals: the decimals count to be set.
 *
 * Set the current decimals count.
 */
void
my_editable_amount_set_decimals( GtkEditable *editable, gint decimals )
{
	sEditableAmount *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	if( decimals == -1 ){
		data->decimals = DEFAULT_DECIMALS;
	} else {
		data->decimals = decimals;
	}
}

/**
 * my_editable_amount_get_amount:
 * @editable: this #GtkEditable instance.
 *
 * Returns the current amount after interpretation.
 */
gdouble
my_editable_amount_get_amount( GtkEditable *editable )
{
	sEditableAmount *data;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), 0 );

	data = get_editable_amount_data( editable );

	return( data->amount );
}

/**
 * my_editable_amount_get_string:
 * @editable: this #GtkEditable instance.
 *
 * Returns the localized representation of the current amount as a
 * newly allocated string which should be g_free() by the caller.
 */
gchar *
my_editable_amount_get_string( GtkEditable *editable )
{
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	text = editable_amount_get_string( editable, NULL );

	return( text );
}

static gchar *
editable_amount_get_string( GtkEditable *editable, sEditableAmount **pdata )
{
	sEditableAmount *data;
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	data = get_editable_amount_data( editable );

	text = g_strdup_printf( "%'.*lf", data->decimals, data->amount );

	if( pdata ){
		*pdata = data;
	}

	return( text );
}

/**
 * my_editable_amount_render_string:
 * @editable: this #GtkEditable instance.
 *
 * Displays the localized representation of the current amount.
 * Should be called when the edition finishes.
 */
void
my_editable_amount_render_string( GtkEditable *editable )
{
	sEditableAmount *data;
	gchar *text;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	if( GTK_IS_ENTRY( editable )){
		text = editable_amount_get_string( editable, &data );
		data->setting_text = TRUE;
		gtk_entry_set_text( GTK_ENTRY( editable ), text );
		g_free( text );
	}
}
