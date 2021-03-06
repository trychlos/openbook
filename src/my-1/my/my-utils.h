/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __MY_API_MY_UTILS_H__
#define __MY_API_MY_UTILS_H__

/**
 * SECTION: my_utils
 * @short_description: Miscellaneous utilities
 * @include: my/my-utils.h
 */

#include <gtk/gtk.h>

#include <my/my-isettings.h>
#include <my/my-stamp.h>

G_BEGIN_DECLS

gchar        *my_casefold                             ( const gchar *str );
gint          my_collate                              ( const gchar *a, const gchar *b );
glong         my_strlen                               ( const gchar *str );

void          my_utils_dump_gslist                    ( const GSList *list );
void          my_utils_dump_gslist_str                ( const GSList *list );

gchar        *my_utils_quote_sql                      ( const gchar *str );
gchar        *my_utils_quote_single                   ( const gchar *str );
gchar        *my_utils_quote_regexp                   ( const gchar *str, const gchar *regexp );
gchar        *my_utils_unquote_regexp                 ( const gchar *str, const gchar *regexp );
gchar        *my_utils_subst_long_dash                ( const gchar *str );

gchar        *my_utils_export_multi_lines             ( const gchar *str );
gchar        *my_utils_import_multi_lines             ( const gchar *str );

gboolean      my_utils_boolean_from_str               ( const gchar *str );

gchar        *my_utils_char_replace                   ( const gchar *string, gchar old_ch, gchar new_ch );

gboolean      my_utils_str_in_list                    ( const gchar *str, const GList *list );
gchar        *my_utils_str_first_word                 ( const gchar *string );
gchar        *my_utils_str_funny_capitalized          ( const gchar *string );
gchar        *my_utils_str_remove_str_delim           ( const gchar *string, gchar fieldsep, gchar strdelim );
gchar        *my_utils_str_remove_suffix              ( const gchar *string, const gchar *suffix );
gchar        *my_utils_str_remove_underlines          ( const gchar *string );
gchar        *my_utils_str_replace                    ( const gchar *string, const gchar *old, const gchar *new );
gchar        *my_utils_str_from_uint_list             ( GList *uint_list, const gchar *sep );
GList        *my_utils_str_to_uint_list               ( const gchar *string, const gchar *sep );

GtkWidget    *my_utils_builder_load_from_path         ( const gchar *path_xml, const gchar *widget_name );
GtkWidget    *my_utils_builder_load_from_resource     ( const gchar *resource, const gchar *widget_name );

void          my_utils_msg_dialog                     ( GtkWindow *parent, GtkMessageType type, const gchar *msg );
gboolean      my_utils_dialog_question                ( GtkWindow *parent, const gchar *msg, const gchar *ok_text );

GtkWidget    *my_utils_container_get_child_by_name    ( GtkContainer *container, const gchar *name );
GtkWidget    *my_utils_container_get_child_by_type    ( GtkContainer *container, GType type );
GtkWidget    *my_utils_container_attach_from_resource ( GtkContainer *container, const gchar *resource, const gchar *window, const gchar *widget );
GtkWidget    *my_utils_container_attach_from_ui       ( GtkContainer *container, const gchar *ui, const gchar *window, const gchar *widget );
GtkWidget    *my_utils_container_attach_from_window   ( GtkContainer *container, GtkWindow *window, const gchar *widget );
void          my_utils_container_set_editable         ( GtkContainer *container, gboolean editable );
void          my_utils_container_dump                 ( GtkContainer *container );

GtkWidget    *my_utils_container_notes_setup_full     ( GtkContainer *container,
																const gchar *widget_name,
																const gchar *content,
																gboolean editable );

void          my_utils_container_notes_setup_ex       ( GtkTextView *textview,
																const gchar *content,
																gboolean editable );

#define       my_utils_container_notes_init( C,T )    my_utils_container_notes_setup_full( \
																GTK_CONTAINER(C),"pn-notes", \
																ofo_ ## T ## _get_notes( priv->T ), TRUE )

#define       my_utils_container_notes_get_ex( W,T )  GtkTextBuffer *buffer = gtk_text_view_get_buffer( W ); \
																GtkTextIter start, end; gtk_text_buffer_get_start_iter( buffer, &start ); \
																gtk_text_buffer_get_end_iter( buffer, &end ); gchar *notes = gtk_text_buffer_get_text( \
																buffer, &start, &end, TRUE ); ofo_ ## T ## _set_notes( priv->T, notes ); g_free( notes );

#define       my_utils_container_notes_get( C,T )     GtkTextView *text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( \
																GTK_CONTAINER( C ), "pn-notes" )); my_utils_container_notes_get_ex( text, T );

void          my_utils_container_updstamp_setup_full  ( GtkContainer *container,
																const gchar *label_name,
																const myStampVal *stamp,
																const gchar *user );

#define       my_utils_container_crestamp_init( C,T ) if( !priv->is_new ){ my_utils_container_updstamp_setup_full( \
																GTK_CONTAINER(C), "px-creation", ofo_ ## T ## _get_cre_stamp( priv->T ), \
																ofo_ ## T ## _get_cre_user( priv->T )); }

#define       my_utils_container_updstamp_init( C,T ) if( !priv->is_new ){ my_utils_container_updstamp_setup_full( \
																GTK_CONTAINER(C), "px-last-update", ofo_ ## T ## _get_upd_stamp( priv->T ), \
																ofo_ ## T ## _get_upd_user( priv->T )); }

GMenuModel   *my_utils_menu_get_menu_model            ( GMenuModel *menu, const gchar *id, gint *pos );

void          my_utils_size_group_add_size_group      ( GtkSizeGroup *target, GtkSizeGroup *source );

GtkWindow    *my_utils_widget_get_toplevel            ( GtkWidget *widget );
void          my_utils_widget_set_editable            ( GtkWidget *widget, gboolean editable );
void          my_utils_widget_set_margins             ( GtkWidget *widget, guint top, guint bottom, guint left, guint right );
void          my_utils_widget_set_margin_left         ( GtkWidget *widget, guint left );
void          my_utils_widget_set_margin_right        ( GtkWidget *widget, guint right );
void          my_utils_widget_set_xalign              ( GtkWidget *widget, gfloat xalign );

gboolean      my_utils_output_stream_new              ( const gchar *uri, GFile **file, GOutputStream **stream );

gboolean      my_utils_input_stream_new               ( const gchar *filename, GFile **file, GInputStream **stream );

void          my_utils_pango_layout_ellipsize         ( PangoLayout *layout, gint max_width );

gboolean      my_utils_window_position_get            ( myISettings *settings, const gchar *name, gint *x, gint *y, gint *width, gint *height );
gboolean      my_utils_window_position_restore        ( GtkWindow *window, myISettings *settings, const gchar *name );
void          my_utils_window_position_save           ( GtkWindow *window, myISettings *settings, const gchar *name );
void          my_utils_window_position_save_pos_only  ( GtkWindow *window, myISettings *settings, const gchar *name );
gboolean      my_utils_window_position_get_has_pos    ( myISettings *settings, const gchar *name );

gboolean      my_utils_file_exists                    ( const gchar *filename );
gboolean      my_utils_file_is_readable               ( const gchar *filename );

gchar        *my_utils_filename_from_utf8             ( const gchar *filename );

gboolean      my_utils_uri_exists                     ( const gchar *uri );
gchar        *my_utils_uri_get_content                ( const gchar *uri, const gchar *from_codeset, guint *errors, gchar **msgerr );
GSList       *my_utils_uri_get_lines                  ( const gchar *uri, const gchar *from_codeset, guint *errors, gchar **msgerr );
gboolean      my_utils_uri_is_dir                     ( const gchar *uri );
gboolean      my_utils_uri_is_readable                ( const gchar *uri );
gchar        *my_utils_uri_get_extension              ( const gchar *uri, gboolean make_lower );

void          my_utils_action_enable                  ( GActionMap *map, GSimpleAction **action, const gchar *name, gboolean enable );

G_END_DECLS

#endif /* __MY_API_MY_UTILS_H__ */
