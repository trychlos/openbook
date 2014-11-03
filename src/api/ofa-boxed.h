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

#ifndef __OFA_BOXED_H__
#define __OFA_BOXED_H__

#include "api/ofo-sgbd-def.h"

/**
 * SECTION: ofa_boxed
 * @short_description: Definition of GBoxed-derived types
 * @include: api/ofa-boxed.h
 */

G_BEGIN_DECLS

typedef enum {
	OFA_TYPE_AMOUNT = 1,
	OFA_TYPE_COUNTER,
	OFA_TYPE_DATE,
	OFA_TYPE_STRING,
	OFA_TYPE_TIMESTAMP,
}
	eBoxedType;

/**
 * ofsBoxed:
 * @id: the identifier of the elementary data
 *  must be greater than zero
 *  is unique inside the object
 * @dbms: the name of the field in the DBMS table
 *  may be NULL if the field is not read from nor stored into the DBMS
 *  (i.e. is only dynamically computed)
 *  not localized
 * @csv: the name of the column when exporting
 *  default to the dbms column name, funny capitalized, without the
 *  underscores
 *  not localized
 * @type: the identifier of the #ofaBoxed type
 * @import: whether the data is importable (all data are exported)
 * @csv_zero_as_empty: whether a zero counter or amount should be
 *  exported as an empty string
 *
 * This structure is to be defined by each #ofoBase derived object
 * which would wish banalize its dataset. Each elementary data is
 * described here
 */
typedef struct {
	gint         id;
	const gchar *dbms;
	const gchar *csv;
	eBoxedType   type;
	gboolean     import;
	gboolean     csv_zero_as_empty;
}
	ofsBoxedDef;

/**
 * OFA_BOXED_DBMS:
 * A macro which simultaneously defines the identifier of the ofaBoxed
 * and the name of the DBMS field
 */
#define OFA_BOXED_DBMS(N)               (N), "" #N ""

/**
 * OFA_BOXED_CSV:
 * A macro which adds to the previous definition those of the csv
 * column name
 */
#define OFA_BOXED_CSV(N)                (N), "" #N "", NULL

#define GPOINTER_TO_DOUBLE(P)           ((gdouble)(glong)(P))
#define GDOUBLE_TO_POINTER(D)           ((gpointer)(glong)(D))

void          ofa_boxed_register_types       ( void );

gchar        *ofa_boxed_get_dbms_columns     ( const ofsBoxedDef *boxed );

GList        *ofa_boxed_parse_dbms_result    ( const ofsBoxedDef *boxed, GSList *row );

gchar        *ofa_boxed_get_csv_header       ( const ofsBoxedDef *boxed, gchar field_sep );

gchar        *ofa_boxed_get_csv_line         ( const GList *fields_list, gchar field_sep, gchar decimal_sep );

gconstpointer ofa_boxed_get_value            ( const GList *fields_list, gint id );

#define       ofa_boxed_get_amount(F,I)      (GPOINTER_TO_DOUBLE(ofa_boxed_get_value((F),(I))))
#define       ofa_boxed_get_counter(F,I)     (GPOINTER_TO_INT(ofa_boxed_get_value((F),(I))))
#define       ofa_boxed_get_date(F,I)        ((const GDate *)ofa_boxed_get_value((F),(I)))
#define       ofa_boxed_get_string(F,I)      ((const gchar *)ofa_boxed_get_value((F),(I)))
#define       ofa_boxed_get_timestamp(F,I)   ((const GTimeVal *)ofa_boxed_get_value((F),(I)))

void          ofa_boxed_set_value            ( const GList *fields_list, gint id, gconstpointer value );

#define       ofa_boxed_set_amount(F,I,V)    (ofa_boxed_set_value((F),(I),GDOUBLE_TO_POINTER(V)))
#define       ofa_boxed_set_counter(F,I,V)   (ofa_boxed_set_value((F),(I),GINT_TO_POINTER(V)))
#define       ofa_boxed_set_date(F,I,V)      (ofa_boxed_set_value((F),(I),(const GDate *)(V)))
#define       ofa_boxed_set_string(F,I,V)    (ofa_boxed_set_value((F),(I),(const gchar *)(V)))
#define       ofa_boxed_set_timestamp(F,I,V) (ofa_boxed_set_value((F),(I),(const GTimeVal *)(V)))

void          ofa_boxed_free_fields_list     ( GList *fields_list );

G_END_DECLS

#endif /* __OFA_BOXED_H__ */
