/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __MY_API_MY_CHAR_H__
#define __MY_API_MY_CHAR_H__

/**
 * SECTION: my_char
 * @short_description: Character management.
 * @include: my/my-char.h
 */

#include <glib.h>

G_BEGIN_DECLS

#define MY_CHAR_COMMA                   ','
#define MY_CHAR_DOT                     '.'
#define MY_CHAR_DQUOTE                  '"'
#define MY_CHAR_PIPE                    '|'
#define MY_CHAR_SCOLON                  ';'
#define MY_CHAR_SPACE                   ' '
#define MY_CHAR_TAB                     '\t'
#define MY_CHAR_ZERO                    '\0'

const gchar *my_char_get_label( gunichar ch );

G_END_DECLS

#endif /* __MY_API_MY_CHAR_H__ */
