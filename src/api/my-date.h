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

#ifndef __MY_DATE_H__
#define __MY_DATE_H__

/**
 * SECTION: my_date
 * @short_description: #myDate class definition.
 * @include: ui/my-date.h
 *
 * This is a class which handles all dates.
 *
 * Goals are:
 * - be independant of the locale
 * - make sure all the hard stuff is centralized
 * - manage GtkEntry as well as GtkCellRenderer
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_DATE                ( my_date_get_type())
#define MY_DATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DATE, myDate ))
#define MY_DATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DATE, myDateClass ))
#define MY_IS_DATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DATE ))
#define MY_IS_DATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DATE ))
#define MY_DATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DATE, myDateClass ))

typedef struct _myDatePrivate       myDatePrivate;

typedef struct {
	/*< private >*/
	GObject          parent;
	myDatePrivate   *private;
}
	myDate;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	myDateClass;

/**
 * myDateFormat:
 *
 * @MY_DATE_DMMM: display the date as 'd mmm yyyy'
 * @MY_DATE_DDMM: display the date as 'dd/mm/yyyy'
 * MY_DATE_SQL:   display the date as 'yyyy-mm-dd'
 */
typedef enum {
	MY_DATE_DMMM = 1,
	MY_DATE_DDMM,
	MY_DATE_SQL
}
	myDateFormat;

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

GType      my_date_get_type               ( void ) G_GNUC_CONST;

GDate     *my_date_set_from_sql     ( GDate *dest, const gchar *sql_string );
GDate     *my_date_set_from_date    ( GDate *dest, const GDate *src );
gchar     *my_date_to_str           ( const GDate *date, myDateFormat format );
gint       my_date_cmp              ( const GDate *a, const GDate *b, gboolean infinite_is_past );
void       my_date_parse_from_entry ( const myDateParse *parms );
GDate     *my_date_parse_from_str   ( GDate *date, const gchar *text, myDateFormat format );

G_END_DECLS

#endif /* __MY_DATE_H__ */
