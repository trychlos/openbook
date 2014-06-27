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

#ifndef __MY_UTILS_H__
#define __MY_UTILS_H__

/**
 * SECTION: my_utils
 * @short_description: Miscellaneous utilities
 * @include: core/my-utils.h
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * myDateFormat:
 *
 * @MY_DATE_DMMM: display the date as 'dd/mm/yyyy'
 * @MY_DATE_DDMM: display the date as 'd mmm yyyy'
 * MY_DATE_SQL:   display the date as 'yyyy-mm-dd'
 */
typedef enum {
	MY_DATE_DMMM = 1,
	MY_DATE_DDMM,
	MY_DATE_SQL
}
	myDateFormat;

/**
 * myStampFormat:
 *
 * @MY_STAMP_YYMDHMS: display the timestamp as 'yyyy-mm-dd hh:mi:ss'
 */
typedef enum {
	MY_STAMP_YYMDHMS = 1,
}
	myStampFormat;

/**
 * myDateCheckCb:
 *
 * A function which is called to test the validity of the entered date.
 * Only called if the date is already g_date_valid().
 */
typedef gboolean ( *myDateCheckCb )( GDate *, gpointer );

/**
 * myDateParse:
 *
 * @entry:
 * @entry_format:
 * @label: [allow-none]
 * @label_format
 * @date: the destination date is only set when the entered text is a
 *  valid date, and
 * @pfnCheck: [allow-none]
 * @on_changed_cb: [allow-none]
 * @user_data
 *
 * Manage a GtkEntry, parsing the input according to the specified
 * format, updating the provided GDate.
 */
typedef struct {
	GtkWidget    *entry;
	myDateFormat  entry_format;
	GtkWidget    *label;
	myDateFormat  label_format;
	GDate        *date;
	myDateCheckCb pfnCheck;
	GCallback     on_changed_cb;
	gpointer      user_data;
}
	myDateParse;

gchar     *my_utils_quote                 ( const gchar *str );

GDate     *my_utils_date_set_from_sql     ( GDate *dest, const gchar *sql_string );
GDate     *my_utils_date_set_from_date    ( GDate *dest, const GDate *src );
gchar     *my_utils_date_to_str           ( const GDate *date, myDateFormat format );
gint       my_utils_date_cmp              ( const GDate *a, const GDate *b, gboolean infinite_is_past );
void       my_utils_date_parse_from_entry ( const myDateParse *parms );
GDate     *my_utils_date_parse_from_str   ( GDate *date, const gchar *text, myDateFormat format );

GTimeVal  *my_utils_stamp_set_from_sql    ( GTimeVal *timeval, const gchar *str );
gchar     *my_utils_stamp_to_str          ( const GTimeVal *stamp, myStampFormat format );
GTimeVal  *my_utils_stamp_get_now         ( GTimeVal *stamp );

gchar     *my_utils_export_multi_lines    ( const gchar *str );
gchar     *my_utils_import_multi_lines    ( const gchar *str );

gboolean   my_utils_parse_boolean         ( const gchar *str, gboolean *bvar );

gdouble    my_utils_double_set_from_input ( const gchar *sql_string );
gdouble    my_utils_double_set_from_sql   ( const gchar *sql_string );
gchar     *my_utils_sql_from_double       ( gdouble value );

gchar     *my_utils_str_remove_suffix     ( const gchar *string, const gchar *suffix );
gchar     *my_utils_str_replace           ( const gchar *string, const gchar *old, const gchar *new );

GtkWidget *my_utils_builder_load_from_path( const gchar *path_xml, const gchar *widget_name );

gboolean   my_utils_entry_get_valid       ( GtkEntry *entry );
void       my_utils_entry_set_valid       ( GtkEntry *entry, gboolean valid );

GtkWidget *my_utils_container_get_child_by_name( GtkContainer *container, const gchar *name );
GtkWidget *my_utils_container_get_child_by_type( GtkContainer *container, GType type );

void       my_utils_init_notes            ( GtkContainer *container,
													const gchar *widget_name,
													const gchar *notes );

#define    my_utils_init_notes_ex( C,T ) my_utils_init_notes( GTK_CONTAINER(C),"pn-notes", ofo_ ## T ## _get_notes( priv->T))

#define    my_utils_getback_notes_ex( C,T ) GtkTextView *text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( \
												GTK_CONTAINER( C ), "pn-notes" )); GtkTextBuffer *buffer = gtk_text_view_get_buffer( text ); \
												GtkTextIter start, end; gtk_text_buffer_get_start_iter( buffer, &start ); \
												gtk_text_buffer_get_end_iter( buffer, &end ); gchar *notes = gtk_text_buffer_get_text( \
												buffer, &start, &end, TRUE ); ofo_ ## T ## _set_notes( priv->T, notes ); g_free( notes );

void       my_utils_init_maj_user_stamp( GtkContainer *container,
												const gchar *label_name,
												const GTimeVal *stamp,
												const gchar *user );

#define    my_utils_init_maj_user_stamp_ex( C,T ) if( !priv->is_new ){ my_utils_init_maj_user_stamp( \
														GTK_CONTAINER(C), "px-last-update", ofo_ ## T ## _get_maj_stamp( priv->T ), \
														ofo_ ## T ## _get_maj_user( priv->T )); }

gboolean   my_utils_output_stream_new     ( const gchar *uri, GFile **file, GOutputStream **stream );

void       my_utils_pango_layout_ellipsize( PangoLayout *layout, gint max_width );

G_END_DECLS

#endif /* __MY_UTILS_H__ */
