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

#ifndef __OPENBOOK_API_OFO_BAT_H__
#define __OPENBOOK_API_OFO_BAT_H__

/**
 * SECTION: ofobat
 * @title: ofoBat
 * @short_description: #ofoBat class definition.
 * @include: openbook/ofo-bat.h
 *
 * This file defines the #ofoBat class public API: these are the tables
 * which contain the imported bank account transaction lines.
 *
 * Import
 * ------
 * Due to the particular nature of this class, import is driven one BAT
 * at a time, that is each import operation only concerns one BAT file.
 *
 * As other classes, ofoBat class expects to be provided with a GSList
 * of lines, each line being itself a GSList of fields. These lists are
 * provided by ofaIImporter implementations.
 *
 * Two type of lines must be provided, in this order:
 *
 * Bat:
 * - line type = 1
 * - id
 * - source uri
 * - importer label
 * - rib
 * - currency
 * - beginning date
 * - beginning solde
 * - beginning solde set (Y|N)
 * - ending date
 * - ending solde
 * - ending solde set (Y|N)
 *
 * BatLine:
 * - line type = 2
 * - id
 * - operation date
 * - effect date
 * - reference
 * - label
 * - amount (<0 if expense)
 * - currency
 *
 * Datas are expected to be provided only if present in the source
 * (though, obviously, at least a date, a label and an amount should
 *  be present in the detail lines).
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-bat-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_BAT                ( ofo_bat_get_type())
#define OFO_BAT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_BAT, ofoBat ))
#define OFO_BAT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_BAT, ofoBatClass ))
#define OFO_IS_BAT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_BAT ))
#define OFO_IS_BAT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_BAT ))
#define OFO_BAT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_BAT, ofoBatClass ))

#if 0
typedef struct _ofoBat              ofoBat;
#endif

struct _ofoBat {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoBatClass;

/**
 * ofsImportedBat:
 *
 * Some datas returned after import.
 *
 * The #ofoBat implementation of the #ofaIImportable interface makes
 * use of the ofsImporterParms::importable_data pointer to hold this
 * data structure, and return informations to the caller.
 */
typedef struct {
	ofxCounter bat_id;
}
	ofsImportedBat;

GType             ofo_bat_get_type                   ( void ) G_GNUC_CONST;

GList            *ofo_bat_get_dataset                ( ofaIGetter *getter );

ofoBat           *ofo_bat_get_by_id                  ( ofaIGetter *getter, ofxCounter id );

ofoBat           *ofo_bat_new                        ( ofaIGetter *getter );

ofxCounter        ofo_bat_get_id                     ( ofoBat *bat );
const gchar      *ofo_bat_get_uri                    ( ofoBat *bat );
const gchar      *ofo_bat_get_format                 ( ofoBat *bat );
const GDate      *ofo_bat_get_begin_date             ( ofoBat *bat );
ofxAmount         ofo_bat_get_begin_solde            ( ofoBat *bat );
gboolean          ofo_bat_get_begin_solde_set        ( ofoBat *bat );
const GDate      *ofo_bat_get_end_date               ( ofoBat *bat );
ofxAmount         ofo_bat_get_end_solde              ( ofoBat *bat );
gboolean          ofo_bat_get_end_solde_set          ( ofoBat *bat );
const gchar      *ofo_bat_get_rib                    ( ofoBat *bat );
const gchar      *ofo_bat_get_currency               ( ofoBat *bat );
const gchar      *ofo_bat_get_cre_user               ( ofoBat *bat );
const myStampVal *ofo_bat_get_cre_stamp              ( ofoBat *bat );
const gchar      *ofo_bat_get_notes                  ( ofoBat *bat );
const gchar      *ofo_bat_get_upd_user               ( ofoBat *bat );
const myStampVal *ofo_bat_get_upd_stamp              ( ofoBat *bat );
const gchar      *ofo_bat_get_account                ( ofoBat *bat );
const gchar      *ofo_bat_get_acc_user               ( ofoBat *bat );
const myStampVal *ofo_bat_get_acc_stamp              ( ofoBat *bat );

gboolean          ofo_bat_exists                     ( ofaIGetter *getter, const gchar *rib, const GDate *begin, const GDate *end );
gboolean          ofo_bat_is_deletable               ( ofoBat *bat );
gint              ofo_bat_get_lines_count            ( ofoBat *bat );
gint              ofo_bat_get_used_count             ( ofoBat *bat );

void              ofo_bat_set_uri                    ( ofoBat *bat, const gchar *uri );
void              ofo_bat_set_format                 ( ofoBat *bat, const gchar *format );
void              ofo_bat_set_begin_date             ( ofoBat *bat, const GDate *date );
void              ofo_bat_set_begin_solde            ( ofoBat *bat, ofxAmount solde );
void              ofo_bat_set_begin_solde_set        ( ofoBat *bat, gboolean set );
void              ofo_bat_set_end_date               ( ofoBat *bat, const GDate *date );
void              ofo_bat_set_end_solde              ( ofoBat *bat, ofxAmount solde );
void              ofo_bat_set_end_solde_set          ( ofoBat *bat, gboolean set );
void              ofo_bat_set_rib                    ( ofoBat *bat, const gchar *rib );
void              ofo_bat_set_currency               ( ofoBat *bat, const gchar *currency );

guint             ofo_bat_doc_get_count              ( ofoBat *bat );

GList            *ofo_bat_doc_get_orphans            ( ofaIGetter *getter );
#define           ofo_bat_doc_free_orphans( L )      ( g_list_free( L ))

gboolean          ofo_bat_insert                     ( ofoBat *bat );
gboolean          ofo_bat_update_notes               ( ofoBat *bat, const gchar *notes );
gboolean          ofo_bat_update_account             ( ofoBat *bat, const gchar *account );
gboolean          ofo_bat_delete                     ( ofoBat *bat );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_BAT_H__ */
