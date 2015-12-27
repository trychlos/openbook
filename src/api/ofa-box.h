/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OPENBOOK_API_OFA_BOX_H__
#define __OPENBOOK_API_OFA_BOX_H__

#include <glib.h>

/**
 * SECTION: ofa_box
 * @short_description: Definition of GBoxed-derived types
 * @include: openbook/ofa-box.h
 */

G_BEGIN_DECLS

typedef gint64  ofxCounter;
typedef gdouble ofxAmount;

typedef enum {
	OFA_TYPE_AMOUNT = 1,
	OFA_TYPE_COUNTER,
	OFA_TYPE_INTEGER,
	OFA_TYPE_DATE,
	OFA_TYPE_STRING,
	OFA_TYPE_TIMESTAMP,
}
	eBoxType;

/**
 * ofsBoxDef:
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
 * @type: the identifier of the #ofaBox type
 * @import: whether the data is importable (all data are exported)
 * @csv_zero_as_empty: whether a zero counter or amount should be
 *  exported as an empty string (NULL strings and timestamps are
 *  always exported as empty strings)
 *
 * This structure is to be defined by each #ofoBase derived object
 * which would wish banalize its dataset. Each elementary data is
 * described here
 */
typedef struct {
	gint         id;
	const gchar *dbms;
	const gchar *csv;
	eBoxType     type;
	gboolean     import;
	gboolean     csv_zero_as_empty;
}
	ofsBoxDef;

/**
 * OFA_BOX_DBMS:
 * A macro which simultaneously defines the identifier of the ofaBox
 * and the name of the DBMS field
 */
#define OFA_BOX_DBMS(N)                 (N), "" #N ""

/**
 * OFA_BOX_CSV:
 * A macro which adds to the previous definition those of the csv
 * column name
 */
#define OFA_BOX_CSV(N)                  OFA_BOX_DBMS(N), NULL

/**
 * a callback function which may be called on CSV exporting
 */
typedef gchar * ( *CSVExportFunc )( const ofsBoxDef *def, eBoxType type, const gchar *text, void *user_data );

/* because DBMS keeps 5 digits after the decimal dot */
#define PRECISION                       100000
#define GPOINTER_TO_AMOUNT(P)           (((ofxAmount)(glong)(P))/PRECISION)
#define AMOUNT_TO_GPOINTER(A)           ((gpointer)(glong)((A)*PRECISION))

#define GPOINTER_TO_COUNTER(P)          ((ofxCounter)(glong)(P))
#define COUNTER_TO_GPOINTER(D)          ((gpointer)(ofxCounter)(D))

void          ofa_box_register_types       ( void );

GList        *ofa_box_init_fields_list     ( const ofsBoxDef *defs );

void          ofa_box_dump_fields_list     ( const gchar *fname, const GList *fields );

gchar        *ofa_box_get_dbms_columns     ( const ofsBoxDef *defs );

GList        *ofa_box_parse_dbms_result    ( const ofsBoxDef *defs, GSList *row );

gchar        *ofa_box_get_csv_header       ( const ofsBoxDef *defs, gchar field_sep );

gchar        *ofa_box_get_csv_line         ( const GList *fields_list,
													gchar field_sep, gchar decimal_sep );

gchar        *ofa_box_get_csv_line_ex      ( const GList *fields_list,
													gchar field_sep, gchar decimal_sep,
													CSVExportFunc cb, void *user_data );

gconstpointer ofa_box_get_value            ( const GList *fields_list, gint id );

#define       ofa_box_get_amount(F,I)      (GPOINTER_TO_AMOUNT(ofa_box_get_value((F),(I))))
#define       ofa_box_get_counter(F,I)     (GPOINTER_TO_COUNTER(ofa_box_get_value((F),(I))))
#define       ofa_box_get_int(F,I)         (GPOINTER_TO_INT(ofa_box_get_value((F),(I))))
#define       ofa_box_get_date(F,I)        ((const GDate *)ofa_box_get_value((F),(I)))
#define       ofa_box_get_string(F,I)      ((const gchar *)ofa_box_get_value((F),(I)))
#define       ofa_box_get_timestamp(F,I)   ((const GTimeVal *)ofa_box_get_value((F),(I)))

void          ofa_box_set_value            ( const GList *fields_list, gint id, gconstpointer value );

#define       ofa_box_set_amount(F,I,V)    ofa_box_set_value((F),(I),AMOUNT_TO_GPOINTER(V))
#define       ofa_box_set_counter(F,I,V)   ofa_box_set_value((F),(I),COUNTER_TO_GPOINTER(V))
#define       ofa_box_set_int(F,I,V)       ofa_box_set_value((F),(I),GINT_TO_POINTER(V))
#define       ofa_box_set_date(F,I,V)      ofa_box_set_value((F),(I),(const GDate *)(V))
#define       ofa_box_set_string(F,I,V)    ofa_box_set_value((F),(I),(const gchar *)(V))
#define       ofa_box_set_timestamp(F,I,V) ofa_box_set_value((F),(I),(const GTimeVal *)(V))

void          ofa_box_free_fields_list     ( GList *fields_list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_BOX_H__ */
