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
 * SECTION: utils
 * @title: Misc
 * @short_description: Miscellaneous Utilities
 * @include: ui/my-utils.h
 */

#include <glib.h>

G_BEGIN_DECLS

/* boolean manipulation
 */
gboolean my_utils_boolean_from_string   ( const gchar *string );

/* some functions to get or set GSList list of strings
 */
void    my_utils_slist_dump             ( const gchar *prefix, GSList *list );
void    my_utils_slist_free             ( GSList *slist );
GSList *my_utils_slist_duplicate        ( GSList *slist );
GSList *my_utils_slist_from_split       ( const gchar *text, const gchar *separator );
GSList *my_utils_slist_from_array       ( const gchar **str_array );

/* GList list of int
 */
GList  *my_utils_intlist_duplicate      ( GList *list );
GList  *my_utils_intlist_from_split     ( const gchar *text, const gchar *separator );
gchar  *my_utils_intlist_to_string      ( GList *list, gchar *separator );

/* file management
 */
void    my_utils_file_list_perms        ( const gchar *path, const gchar *message );

G_END_DECLS

#endif /* __MY_UTILS_H__ */
