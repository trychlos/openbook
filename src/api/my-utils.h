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
 * myStampFormat:
 *
 * @MY_STAMP_YYMDHMS: display the timestamp as 'yyyy-mm-dd hh:mi:ss'
 */
typedef enum {
	MY_STAMP_YYMDHMS = 1,
	MY_STAMP_DMYYHM
}
	myStampFormat;

gchar     *my_utils_quote                  ( const gchar *str );

GTimeVal  *my_utils_stamp_set_now          ( GTimeVal *stamp );
GTimeVal  *my_utils_stamp_set_from_sql     ( GTimeVal *timeval, const gchar *str );
GTimeVal  *my_utils_stamp_set_from_stamp   ( GTimeVal *timeval, const GTimeVal *orig );
gchar     *my_utils_stamp_to_str           ( const GTimeVal *stamp, myStampFormat format );

gchar     *my_utils_export_multi_lines     ( const gchar *str );
gchar     *my_utils_import_multi_lines     ( const gchar *str );

gboolean   my_utils_boolean_from_str       ( const gchar *str );

gchar     *my_utils_str_remove_suffix      ( const gchar *string, const gchar *suffix );
gchar     *my_utils_str_remove_underlines  ( const gchar *string );
gchar     *my_utils_str_replace            ( const gchar *string, const gchar *old, const gchar *new );

GtkWidget *my_utils_builder_load_from_path ( const gchar *path_xml, const gchar *widget_name );

gboolean   my_utils_entry_get_valid        ( GtkEntry *entry );
void       my_utils_entry_set_valid        ( GtkEntry *entry, gboolean valid );

GtkBuildable *my_utils_container_get_buildable_by_name( GtkContainer *container, const gchar *name );
GtkWidget    *my_utils_container_get_child_by_name    ( GtkContainer *container, const gchar *name );
GtkWidget    *my_utils_container_get_child_by_type    ( GtkContainer *container, GType type );

GObject   *my_utils_init_notes             ( GtkContainer *container,
													const gchar *widget_name,
													const gchar *notes );

#define    my_utils_init_notes_ex( C,T ) my_utils_init_notes( GTK_CONTAINER(C),"pn-notes", ofo_ ## T ## _get_notes( priv->T))

#define    my_utils_getback_notes_ex( C,T ) GtkTextView *text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( \
												GTK_CONTAINER( C ), "pn-notes" )); GtkTextBuffer *buffer = gtk_text_view_get_buffer( text ); \
												GtkTextIter start, end; gtk_text_buffer_get_start_iter( buffer, &start ); \
												gtk_text_buffer_get_end_iter( buffer, &end ); gchar *notes = gtk_text_buffer_get_text( \
												buffer, &start, &end, TRUE ); ofo_ ## T ## _set_notes( priv->T, notes ); g_free( notes );

void       my_utils_init_upd_user_stamp    ( GtkContainer *container,
												const gchar *label_name,
												const GTimeVal *stamp,
												const gchar *user );

#define    my_utils_init_upd_user_stamp_ex( C,T ) if( !priv->is_new ){ my_utils_init_upd_user_stamp( \
														GTK_CONTAINER(C), "px-last-update", ofo_ ## T ## _get_upd_stamp( priv->T ), \
														ofo_ ## T ## _get_upd_user( priv->T )); }

gboolean   my_utils_output_stream_new      ( const gchar *filename, GFile **file, GOutputStream **stream );

void       my_utils_pango_layout_ellipsize ( PangoLayout *layout, gint max_width );

void       my_utils_window_restore_position( GtkWindow *window, const gchar *name );
void       my_utils_window_save_position   ( GtkWindow *window, const gchar *name );

gboolean   my_utils_file_exists            ( const gchar *filename );

gchar     *my_utils_filename_from_utf8     ( const gchar *filename );

G_END_DECLS

#endif /* __MY_UTILS_H__ */
