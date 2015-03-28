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

#ifndef __OPENBOOK_API_OFO_LEDGER_H__
#define __OPENBOOK_API_OFO_LEDGER_H__

/**
 * SECTION: ofo_ledger
 * @short_description: #ofoLedger class definition.
 * @include: openbook/ofo-ledger.h
 *
 * This file defines the #ofoLedger class public API.
 */

#include "api/ofa-box.h"
#include "api/ofo-dossier-def.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

/* a fake ledger under which we reclass the entry models which are
 * attached to an unfound ledger
 */
#define UNKNOWN_LEDGER_MNEMO                 "__xx__"
#define UNKNOWN_LEDGER_LABEL                 _( "Unclassed" )

void            ofo_ledger_connect_handlers  ( const ofoDossier *dossier );

GList          *ofo_ledger_get_dataset       ( ofoDossier *dossier );
ofoLedger      *ofo_ledger_get_by_mnemo      ( ofoDossier *dossier, const gchar *mnemo );
gboolean        ofo_ledger_use_currency      ( ofoDossier *dossier, const gchar *currency );

ofoLedger      *ofo_ledger_new               ( void );

const gchar    *ofo_ledger_get_mnemo         ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_label         ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_notes         ( const ofoLedger *ledger );
const gchar    *ofo_ledger_get_upd_user      ( const ofoLedger *ledger );
const GTimeVal *ofo_ledger_get_upd_stamp     ( const ofoLedger *ledger );
const GDate    *ofo_ledger_get_last_close    ( const ofoLedger *ledger );
GDate          *ofo_ledger_get_last_entry    ( const ofoLedger *ledger, ofoDossier *dossier );

GList          *ofo_ledger_get_currencies    ( const ofoLedger *ledger );

ofxAmount       ofo_ledger_get_val_debit     ( const ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_val_credit    ( const ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_rough_debit   ( const ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_rough_credit  ( const ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_debit   ( const ofoLedger *ledger, const gchar *currency );
ofxAmount       ofo_ledger_get_futur_credit  ( const ofoLedger *ledger, const gchar *currency );

GDate          *ofo_ledger_get_max_last_close( GDate *date, const ofoDossier *dossier );
gboolean        ofo_ledger_has_entries       ( const ofoLedger *ledger, ofoDossier *dossier );
gboolean        ofo_ledger_is_deletable      ( const ofoLedger *ledger, ofoDossier *dossier );
gboolean        ofo_ledger_is_valid          ( const gchar *mnemo, const gchar *label );

void            ofo_ledger_set_mnemo         ( ofoLedger *ledger, const gchar *number );
void            ofo_ledger_set_label         ( ofoLedger *ledger, const gchar *label );
void            ofo_ledger_set_notes         ( ofoLedger *ledger, const gchar *notes );

void            ofo_ledger_set_val_debit     ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_val_credit    ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_rough_debit   ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_rough_credit  ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_debit   ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );
void            ofo_ledger_set_futur_credit  ( ofoLedger *ledger, ofxAmount amount, const gchar *currency );

gboolean        ofo_ledger_close             ( ofoLedger *ledger, ofoDossier *dossier, const GDate *closing );

gboolean        ofo_ledger_insert            ( ofoLedger *ledger, ofoDossier *dossier );
gboolean        ofo_ledger_update            ( ofoLedger *ledger, ofoDossier *dossier, const gchar *prev_mnemo );
gboolean        ofo_ledger_update_balance    ( ofoLedger *ledger, ofoDossier *dossier, const gchar *currency );
gboolean        ofo_ledger_delete            ( ofoLedger *ledger, ofoDossier *dossier );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_LEDGER_H__ */
