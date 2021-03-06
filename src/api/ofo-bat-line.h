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

#ifndef __OPENBOOK_API_OFO_BAT_LINE_H__
#define __OPENBOOK_API_OFO_BAT_LINE_H__

/**
 * SECTION: ofo_bat_line
 * @short_description: #ofoBatLine class definition.
 * @include: openbook/ofo-bat-line.h
 *
 * This file defines the #ofoBatLine class public API: these are the
 * imported bank account transaction lines.
 *
 * Amount is signed
 * ----------------
 * Amount is less than zero for expenses, greater than zero for incomes.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_BAT_LINE                ( ofo_bat_line_get_type())
#define OFO_BAT_LINE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_BAT_LINE, ofoBatLine ))
#define OFO_BAT_LINE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_BAT_LINE, ofoBatLineClass ))
#define OFO_IS_BAT_LINE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_BAT_LINE ))
#define OFO_IS_BAT_LINE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_BAT_LINE ))
#define OFO_BAT_LINE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_BAT_LINE, ofoBatLineClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoBatLine;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoBatLineClass;

GType        ofo_bat_line_get_type                      ( void ) G_GNUC_CONST;

GList       *ofo_bat_line_get_dataset                   ( ofaIGetter *getter, ofxCounter bat_id );
#define      ofo_bat_line_free_dataset(L)               g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

GList       *ofo_bat_line_get_orphans                   ( ofaIGetter *getter );
#define      ofo_bat_line_free_orphans(L)               g_list_free_full(( L ), ( GDestroyNotify ) g_free )

GList       *ofo_bat_line_get_dataset_for_print_reconcil( ofaIGetter *getter, const gchar *account_id, const GDate *date, ofxCounter *batid );

ofxCounter   ofo_bat_line_get_bat_id_from_bat_line_id   ( ofaIGetter *getter, ofxCounter line_id );

ofoBatLine  *ofo_bat_line_new                           ( ofaIGetter *getter );

ofxCounter   ofo_bat_line_get_bat_id                    ( ofoBatLine *batline );
ofxCounter   ofo_bat_line_get_line_id                   ( ofoBatLine *batline );
const GDate *ofo_bat_line_get_deffect                   ( ofoBatLine *batline );
const GDate *ofo_bat_line_get_dope                      ( ofoBatLine *batline );
const gchar *ofo_bat_line_get_ref                       ( ofoBatLine *batline );
const gchar *ofo_bat_line_get_label                     ( ofoBatLine *batline );
const gchar *ofo_bat_line_get_currency                  ( ofoBatLine *batline );
ofxAmount    ofo_bat_line_get_amount                    ( ofoBatLine *batline );

void         ofo_bat_line_set_bat_id                    ( ofoBatLine *batline, ofxCounter bat_id );
void         ofo_bat_line_set_deffect                   ( ofoBatLine *batline, const GDate *date );
void         ofo_bat_line_set_dope                      ( ofoBatLine *batline, const GDate *date );
void         ofo_bat_line_set_ref                       ( ofoBatLine *batline, const gchar *ref );
void         ofo_bat_line_set_label                     ( ofoBatLine *batline, const gchar *label );
void         ofo_bat_line_set_currency                  ( ofoBatLine *batline, const gchar *currency );
void         ofo_bat_line_set_amount                    ( ofoBatLine *batline, ofxAmount montant );

gboolean     ofo_bat_line_insert                        ( ofoBatLine *batline );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_BAT_LINE_H__ */
