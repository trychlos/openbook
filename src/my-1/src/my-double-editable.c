/*
 * Open Firm Accounting
 * A double-editable accounting application for freelances.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-double-editable.h"
#include "my/my-utils.h"

/*
 * data attached to each implementor object
 * (typically a GtkEditable, or anything which implements the GtkEditable
 *  interface)
 */
typedef struct {

	/* configuration
	 */
	guint    decimals;
	gunichar thousand_sep;
	gunichar decimal_sep;
	gboolean accept_sign;
	gboolean accept_dot;
	gboolean accept_comma;

	/* amount
	 */
	gdouble  amount;

	/* run
	 */
	gboolean has_decimal;
	gboolean setting_text;
	GList   *cbs;
}
	sEditable;

typedef struct {
	GCallback cb;
	void     *user_data;
}
	sCB;

#define DEFAULT_DECIMALS                 2
#define DEFAULT_ACCEPT_SIGN              TRUE
#define DOUBLE_EDITABLE_DATA            "my-double-editable-data"

static void       double_editable_init( GtkEditable *editable );
static void       on_editable_finalized( sEditable *data, GObject *was_the_editable );
static sEditable *get_editable_amount_data( GtkEditable *editable );
static void       on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditable *data );
static void       on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditable *data );
static void       on_changed( GtkEditable *editable, sEditable *data );
static gboolean   on_focus_in( GtkWidget *editable, GdkEvent *event, sEditable *data );
static gboolean   on_focus_out( GtkWidget *editable, GdkEvent *event, sEditable *data );
static gchar     *editable_amount_get_localized_string( GtkEditable *editable, sEditable **pdata );
static void       editable_amount_render( GtkEditable *editable, const gchar *string, sEditable *data );

/**
 * my_double_editable_init:
 * @editable: the #GtkEditable object
 *
 * Initialize the GtkEditable to enter an amount. Is supposed to be
 * called each time the edition is started.
 */
void
my_double_editable_init( GtkEditable *editable )
{
	static const gchar *thisfn = "my_double_editable_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) editable, G_OBJECT_TYPE_NAME( editable ));

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	double_editable_init( editable );
}

/**
 * my_double_editable_init_ex:
 * @editable: the #GtkEditable object
 * @thousand_sep: thousand separator.
 * @decimal_sep: decimal separator.
 * @accept_dot: whether we accept a dot as the decimal separator on entry.
 * @accept_comma: whether we accept a comma as the decimal separator on entry.
 * @decimals: decimals count.
 *
 * Initialize the GtkEditable to enter an amount. Is supposed to be
 * called each time the edition is started.
 */
void
my_double_editable_init_ex( GtkEditable *editable,
		gunichar thousand_sep, gunichar decimal_sep, gboolean accept_dot, gboolean accept_comma, gint decimals )
{
	static const gchar *thisfn = "my_double_editable_init_ex";

	g_debug( "%s: self=%p (%s), thousand_sep=%c, decimal_sep=%c, accept_dot=%s, accept_comma=%s, decimals=%d",
			thisfn, ( void * ) editable, G_OBJECT_TYPE_NAME( editable ),
			thousand_sep, decimal_sep, accept_dot ? "True":"False", accept_comma ? "True":"False", decimals );

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	double_editable_init( editable );
	my_double_editable_set_thousand_sep( editable, thousand_sep );
	my_double_editable_set_decimal_sep( editable, decimal_sep );
	my_double_editable_set_accept_dot( editable, accept_dot );
	my_double_editable_set_accept_comma( editable, accept_comma );
	my_double_editable_set_decimals( editable, decimals );
}

/*
 * When starting to edit an amount:
 * - remove thousand separator (space)
 * - replace decimal comma with a dot
 */
static void
double_editable_init( GtkEditable *editable )
{
	sEditable *data;

	data = get_editable_amount_data( editable );

	g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_finalized, data );
}

static void
on_editable_finalized( sEditable *data, GObject *finalized_editable )
{
	static const gchar *thisfn = "my_double_editable_on_editable_finalized";

	g_debug( "%s: data=%p, finalized_editable=%p",
			thisfn, ( void * ) data, ( void * ) finalized_editable );

	g_list_free_full( data->cbs, ( GDestroyNotify ) g_free );

	g_free( data );
}

static sEditable *
get_editable_amount_data( GtkEditable *editable )
{
	sEditable *data;

	data = ( sEditable * ) g_object_get_data( G_OBJECT( editable ), DOUBLE_EDITABLE_DATA );

	if( !data ){
		data = g_new0( sEditable, 1 );
		g_object_set_data( G_OBJECT( editable ), DOUBLE_EDITABLE_DATA, data );

		data->accept_sign = DEFAULT_ACCEPT_SIGN;
		data->amount = 0;
		data->setting_text = FALSE;
		my_double_editable_set_decimals( editable, -1 );

		g_signal_connect( editable, "insert-text", G_CALLBACK( on_text_inserted ), data );
		g_signal_connect( editable, "delete-text", G_CALLBACK( on_text_deleted ), data );
		g_signal_connect( editable, "changed", G_CALLBACK( on_changed ), data );

		my_double_editable_set_changed_cb( editable, G_CALLBACK( on_changed ), data );

		if( GTK_IS_WIDGET( editable )){
			g_signal_connect( editable, "focus-in-event", G_CALLBACK( on_focus_in ), data );
			g_signal_connect( editable, "focus-out-event", G_CALLBACK( on_focus_out ), data );
		}

		if( GTK_IS_ENTRY( editable )){
			gtk_entry_set_alignment( GTK_ENTRY( editable ), 1 );
		}
	}

	return( data );
}

static void
on_text_inserted( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, sEditable *data )
{
	gint i;
	gboolean is_ok;
	gunichar ithchar;
	gchar *ithpos;

	is_ok = TRUE;

	if( !data->setting_text ){
		for( i=0 ; i<new_text_length && is_ok ; ++i ){
			ithpos = g_utf8_offset_to_pointer( new_text, i );
			ithchar = g_utf8_get_char( ithpos );

			/* a sign in first position */
			if( ithchar == '+' || ithchar == '-' ){
				if( *position+i == 0 && data->accept_sign ){
					continue;
				} else {
					is_ok = FALSE;
				}
			}

			/* a digit */
			if( g_unichar_isdigit( ithchar )){
				continue;
			}

			/* a decimal separator */
			if( data->has_decimal ){
				is_ok = FALSE;
				continue;
			}
			if( ithchar == '.' && data->accept_dot ){
				data->has_decimal = TRUE;
				continue;
			}
			if( ithchar == ',' && data->accept_comma ){
				data->has_decimal = TRUE;
				continue;
			}

			/* else, refuse the input character */
			is_ok = FALSE;
		}
	}

	if( is_ok ){
		g_signal_handlers_block_by_func( editable, ( gpointer ) on_text_inserted, data );
		gtk_editable_insert_text( editable, new_text, new_text_length, position );
		g_signal_handlers_unblock_by_func( editable, ( gpointer ) on_text_inserted, data );
	}

	g_signal_stop_emission_by_name( editable, "insert-text" );
}

static void
on_text_deleted( GtkEditable *editable, gint start_pos, gint end_pos, sEditable *data )
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
on_changed( GtkEditable *editable, sEditable *data )
{
	gchar *text1;

	if( !data->setting_text ){
		text1 = gtk_editable_get_chars( editable, 0, -1 );
		data->amount = my_double_set_from_str( text1, data->thousand_sep, data->decimal_sep );
		g_free( text1 );

	} else {
		data->setting_text = FALSE;
	}
}

/*
 * Render a C string when focusing into the #myEditableAmount
 * this doesn't trigger the 'changed' signal on the GtkEditable
 *
 * Returns :
 *  TRUE to stop other handlers from being invoked for the event.
 *  FALSE to propagate the event further.
 */
static gboolean
on_focus_in( GtkWidget *editable, GdkEvent *event, sEditable *data )
{
	static const gchar *thisfn = "my_double_editable_on_focus_in";
	gchar *text1, *text2;
	GList *it;
	sCB *scb;

	g_debug( "%s: editable=%p, event=%p, data=%p",
			thisfn, ( void * ) editable, ( void * ) event, ( void * ) data );

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), TRUE );

	text1 = gtk_editable_get_chars( GTK_EDITABLE( editable ), 0, -1 );
	text2 = my_double_undecorate( text1, data->thousand_sep, data->decimal_sep );
	data->has_decimal = ( g_strstr_len( text2, -1, "." ) != NULL );

	for( it=data->cbs ; it ; it=it->next ){
		scb = ( sCB * ) it->data;
		g_signal_handlers_block_by_func( editable, scb->cb, scb->user_data );
	}
	editable_amount_render( GTK_EDITABLE( editable ), text2, data );
	for( it=data->cbs ; it ; it=it->next ){
		scb = ( sCB * ) it->data;
		g_signal_handlers_unblock_by_func( editable, scb->cb, scb->user_data );
	}

	g_free( text2 );
	g_free( text1 );

	return( FALSE );
}

/*
 * Render a localized string when focusing out of the EditableofxAmount
 * this doesn't trigger the 'changed' signal on the GtkEditable
 *
 * Returns :
 *  TRUE to stop other handlers from being invoked for the event.
 *  FALSE to propagate the event further.
 */
static gboolean
on_focus_out( GtkWidget *editable, GdkEvent *event, sEditable *data )
{
	static const gchar *thisfn = "my_double_editable_on_focus_out";
	gchar *text;

	g_debug( "%s: editable=%p, event=%p, data=%p",
			thisfn, ( void * ) editable, ( void * ) event, ( void * ) data );

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), TRUE );

	text = editable_amount_get_localized_string( GTK_EDITABLE( editable ), NULL );

	g_signal_handlers_block_by_func( editable, on_changed, data );
	editable_amount_render( GTK_EDITABLE( editable ), text, data );
	g_signal_handlers_unblock_by_func( editable, on_changed, data );

	g_free( text );

	return( FALSE );
}

/**
 * my_double_editable_set_decimals:
 * @editable: this #GtkEditable instance.
 * @decimals: the decimals count to be set ; reset to the default count
 *  of decimals if @decimals is equal to -1.
 *
 * Set the current decimals count.
 */
void
my_double_editable_set_decimals( GtkEditable *editable, gint decimals )
{
	sEditable *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( decimals == -1 || decimals >= 0 );

	data = get_editable_amount_data( editable );

	if( decimals == -1 ){
		data->decimals = DEFAULT_DECIMALS;
	} else {
		data->decimals = decimals;
	}
}

/**
 * my_double_editable_set_thousand_sep:
 * @editable: this #GtkEditable instance.
 * @thousand_sep: the desired thousand separator.
 *
 * Set the desired thousand separator.
 *
 * If not set here, the thousand separator defaults to the current
 * locale one.
 */
void
my_double_editable_set_thousand_sep( GtkEditable *editable, gunichar thousand_sep )
{
	sEditable *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	data->thousand_sep = thousand_sep;
}

/**
 * my_double_editable_set_decimal_sep:
 * @editable: this #GtkEditable instance.
 * @decimal_sep: the desired decimal separator.
 *
 * Set the desired decimal separator.
 *
 * If not set here, the decimal separator defaults to the current
 * locale one.
 */
void
my_double_editable_set_decimal_sep( GtkEditable *editable, gunichar decimal_sep )
{
	sEditable *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	data->decimal_sep = decimal_sep;
}

/**
 * my_double_editable_set_accept_dot:
 * @editable: this #GtkEditable instance.
 * @accept_dot: whether we accept the dot as the decimal separator.
 */
void
my_double_editable_set_accept_dot( GtkEditable *editable, gboolean accept_dot )
{
	sEditable *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	data->accept_dot = accept_dot;
}

/**
 * my_double_editable_set_accept_comma:
 * @editable: this #GtkEditable instance.
 * @accept_comma: whether we accept the comma as the decimal separator.
 */
void
my_double_editable_set_accept_comma( GtkEditable *editable, gboolean accept_comma )
{
	sEditable *data;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	data->accept_comma = accept_comma;
}

/**
 * my_double_editable_get_amount:
 * @editable: this #GtkEditable instance.
 *
 * Returns the current amount after interpretation.
 */
gdouble
my_double_editable_get_amount( GtkEditable *editable )
{
	sEditable *data;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), 0 );

	data = get_editable_amount_data( editable );

	return( data->amount );
}

/**
 * my_double_editable_set_amount:
 * @editable: this #GtkEditable instance.
 * @amount:
 *
 * Set up the current amount.
 * Render the amount as a localized string, letting the changed signal
 * be triggered on the GtkEditable
 */
void
my_double_editable_set_amount( GtkEditable *editable, gdouble amount )
{
	sEditable *data;
	gchar *text;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	data->amount = amount;

	text = editable_amount_get_localized_string( editable, NULL );
	editable_amount_render( editable, text, data );
	g_free( text );
}

/**
 * my_double_editable_get_string:
 * @editable: this #GtkEditable instance.
 *
 * Returns the localized representation of the current amount as a
 * newly allocated string which should be g_free() by the caller.
 */
gchar *
my_double_editable_get_string( GtkEditable *editable )
{
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	text = editable_amount_get_localized_string( editable, NULL );

	return( text );
}

/**
 * my_double_editable_set_string:
 * @editable: this #GtkEditable instance.
 * @string:
 *
 * Set the amount after string evaluation.
 */
void
my_double_editable_set_string( GtkEditable *editable, const gchar *string )
{
	sEditable *data;
	gdouble amount;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );
	amount = my_double_set_from_str( string, data->thousand_sep, data->decimal_sep );
	amount = my_double_round_to_decimals( amount, data->decimals );
	my_double_editable_set_amount( editable, amount );
}

static gchar *
editable_amount_get_localized_string( GtkEditable *editable, sEditable **pdata )
{
	sEditable *data;
	gchar *text;

	g_return_val_if_fail( editable && GTK_IS_EDITABLE( editable ), NULL );

	data = get_editable_amount_data( editable );

	text = my_double_to_str( data->amount, data->thousand_sep, data->decimal_sep, data->decimals );

	if( pdata ){
		*pdata = data;
	}

	return( text );
}

/*
 * my_double_editable_render:
 * @editable: this #GtkEditable instance.
 *
 * Displays the localized representation of the current amount.
 * Should be called when the edition finishes.
 */
static void
editable_amount_render( GtkEditable *editable, const gchar *string, sEditable *data )
{
	static const gchar *thisfn = "my_double_editable_editable_amount_render";

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	g_debug( "%s: editable=%p, string=%s, data=%p",
			thisfn, ( void * ) editable, string, ( void * ) data );

	if( GTK_IS_ENTRY( editable )){
		data->setting_text = TRUE;
		gtk_entry_set_text( GTK_ENTRY( editable ), string );
		data->setting_text = FALSE;
	}
}

/**
 * my_double_editable_set_changed_cb:
 * @editable: this #GtkEditable instance.
 * @cb: the "changed" callback
 * @user_data: the associated user data
 *
 * Register a "changed" callback.
 *
 * This is used when getting the focus, so that the "changed" callback
 * @cb is kept to be triggered.
 */
void
my_double_editable_set_changed_cb( GtkEditable *editable, GCallback cb, void *user_data )
{
	sEditable *data;
	sCB *scb;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	data = get_editable_amount_data( editable );

	scb = g_new0( sCB, 1 );
	scb->cb = cb;
	scb->user_data = user_data;
	data->cbs = g_list_prepend( data->cbs, scb );
}
