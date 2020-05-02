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

#ifndef __MY_API_MY_STAMP_H__
#define __MY_API_MY_STAMP_H__

/**
 * SECTION: my_stamp
 * @short_description: Miscellaneous utilities for timestamp management
 * @include: my/my-stamp.h
 */

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * myStampVal:
 *
 * An opaque data structure which holds a timestamp.
 */
typedef struct _myStampVal myStampVal;

/**
 * myStampFormat:
 *
 * @MY_STAMP_YYMDHMS: display as 'yyyy-mm-dd hh:mi:ss' (SQL-like format)
 * @MY_STAMP_DMYYHM:  display as 'dd/mm/yyyy hh:mi'.
 * @MY_STAMP_YYMD: display as 'yyyymmdd'
 *
 * MAINTAINER_NOTE: only add a new format at the end of the list as the
 * format number is stored as a user settings
 * + update my-stamp.c
 */
typedef enum {
	MY_STAMP_YYMDHMS = 1,
	MY_STAMP_DMYYHM,
	MY_STAMP_YYMD
}
	myStampFormat;

myStampVal *my_stamp_new           ( void );
myStampVal *my_stamp_new_now       ( void );
myStampVal *my_stamp_new_from_sql  ( const gchar *str );
myStampVal *my_stamp_new_from_stamp( const myStampVal *stamp );
myStampVal *my_stamp_new_from_str  ( const gchar *str, myStampFormat format );

myStampVal *my_stamp_set_now       ( myStampVal *stamp );

gint        my_stamp_compare       ( const myStampVal *a, const myStampVal *b );
gint64      my_stamp_diff_us       ( const myStampVal *a, const myStampVal *b );

time_t      my_stamp_get_seconds   ( const myStampVal *stamp );
gulong      my_stamp_get_usecs     ( const myStampVal *stamp );

myStampVal *my_stamp_set_from_sql  ( myStampVal *stamp, const gchar *str );
myStampVal *my_stamp_set_from_stamp( myStampVal *stamp, const myStampVal *orig );
myStampVal *my_stamp_set_from_str  ( myStampVal *stamp, const gchar *str, myStampFormat format );

gchar      *my_stamp_to_str        ( const myStampVal *stamp, myStampFormat format );

void        my_stamp_free          ( myStampVal *stamp );

G_END_DECLS

#endif /* __MY_API_MY_STAMP_H__ */
