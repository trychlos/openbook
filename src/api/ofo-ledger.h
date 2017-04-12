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

#ifndef __OPENBOOK_API_OFO_LEDGER_H__
#define __OPENBOOK_API_OFO_LEDGER_H__

/**
 * SECTION: ofoledger
 * @title: ofoLedger
 * @short_description: #ofoLedger class definition.
 * @include: openbook/ofo-ledger.h
 *
 * This file defines the #ofoLedger class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_LEDGER                ( ofo_ledger_get_type())
#define OFO_LEDGER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_LEDGER, ofoLedger ))
#define OFO_LEDGER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_LEDGER, ofoLedgerClass ))
#define OFO_IS_LEDGER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_LEDGER ))
#define OFO_IS_LEDGER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_LEDGER ))
#define OFO_LEDGER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_LEDGER, ofoLedgerClass ))

#if 0
typedef struct _ofoLedger              ofoLedger;
#endif

struct _ofoLedger {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoLedgerClass;

/* a fake ledger under which we reclass the entry models which are
 * attached to an unfound ledger
 */
#define UNKNOWN_LEDGER_MNEMO            "__xx__"
#define UNKNOWN_LEDGER_LABEL            _( "Unclassed" )

GType           ofo_ledger_get_type                   ( void ) G_GNUC_CONST;

GList          *ofo_ledger_get_dataset                ( ofaIGetter *getter );
#define         ofo_ledger_free_dataset( L )          ( g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref ))

ofoLedger      *ofo_ledger_get_by_mnemo               ( ofaIGetter *getter, const gchar *mnemo );

ofoLedger      *ofo_ledger_new                        ( ofaIGetter *getter );

const gchar    *ofo_ledger_get_mnemo                  ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_label                  ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_notes                  ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_upd_user               ( const ofoLedger *ledger );
const GTimeVal *ofo_ledger_get_upd_stamp              ( const ofoLedger *ledger );
const GDate    *ofo_ledger_get_last_close             ( const ofoLedger *ledger );

GDate          *ofo_ledger_get_last_entry             ( const ofoLedger *ledger, GDate *date );
GDate          *ofo_ledger_get_max_last_close         ( ofaIGetter *getter, GDate *date );
gboolean        ofo_ledger_has_entries                ( const ofoLedger *ledger );
gboolean        ofo_ledger_is_deletable               ( const ofoLedger *ledger );
gboolean        ofo_ledger_is_valid_data              ( const gchar *mnemo, const gchar *label, gchar **msgerr );

void            ofo_ledger_set_mnemo                  ( ofoLedger *ledger, const gchar *number );
void            ofo_ledger_set_label                  ( ofoLedger *ledger, const gchar *label );
void            ofo_ledger_set_notes                  ( ofoLedger *ledger, const gchar *notes );

GList          *ofo_ledger_currency_get_list          ( ofoLedger *ledger );
void            ofo_ledger_currency_update_code       ( ofoLedger *ledger, const gchar *prev_id, const gchar *new_id );

ofxAmount       ofo_ledger_get_current_rough_debit    ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_current_rough_credit   ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_current_val_debit      ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_current_val_credit     ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_rough_debit      ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_rough_credit     ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_val_debit        ( ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_val_credit       ( ofoLedger *ledger, const gchar *currency );

void            ofo_ledger_set_current_rough_debit    ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_current_rough_credit   ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_current_val_debit      ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_current_val_credit     ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_rough_debit      ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_rough_credit     ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_val_debit        ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_val_credit       ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );

GList          *ofo_ledger_currency_get_orphans       ( ofaIGetter *getter );
#define         ofo_ledger_currency_free_orphans( L ) ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean        ofo_ledger_archive_balances           ( ofoLedger *ledger, const GDate *date );

guint           ofo_ledger_archive_get_count          ( ofoLedger *ledger );
const gchar    *ofo_ledger_archive_get_currency       ( ofoLedger *ledger, guint idx );
const GDate    *ofo_ledger_archive_get_date           ( ofoLedger *ledger, guint idx );
ofxAmount       ofo_ledger_archive_get_debit          ( ofoLedger *ledger, const gchar *currency, const GDate *date );
ofxAmount       ofo_ledger_archive_get_credit         ( ofoLedger *ledger, const gchar *currency, const GDate *date );

GList          *ofo_ledger_archive_get_orphans        ( ofaIGetter *getter );
#define         ofo_ledger_archive_free_orphans( L )  ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

guint           ofo_ledger_doc_get_count              ( ofoLedger *ledger );

GList          *ofo_ledger_doc_get_orphans            ( ofaIGetter *getter );
#define         ofo_ledger_doc_free_orphans( L )      ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean        ofo_ledger_close                      ( ofoLedger *ledger, const GDate *closing );

gboolean        ofo_ledger_insert                     ( ofoLedger *ledger );
gboolean        ofo_ledger_update                     ( ofoLedger *ledger, const gchar *prev_mnemo );
gboolean        ofo_ledger_update_balance             ( ofoLedger *ledger, const gchar *currency );
gboolean        ofo_ledger_delete                     ( ofoLedger *ledger );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_LEDGER_H__ */
