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

#ifndef __OPENBOOK_API_OFO_ENTRY_H__
#define __OPENBOOK_API_OFO_ENTRY_H__

/**
 * SECTION: ofo_entry
 * @short_description: #ofoEntry class definition.
 * @include: openbook/ofo-entry.h
 *
 * This file defines the #ofoEntry public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-concil-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_ENTRY                ( ofo_entry_get_type())
#define OFO_ENTRY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ENTRY, ofoEntry ))
#define OFO_ENTRY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ENTRY, ofoEntryClass ))
#define OFO_IS_ENTRY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ENTRY ))
#define OFO_IS_ENTRY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ENTRY ))
#define OFO_ENTRY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ENTRY, ofoEntryClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoEntry;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoEntryClass;

/**
 * ofeEntryStatus:
 *
 * @ENT_STATUS_PAST: status attached to the entries forwarded from a past
 *  exercice; these entries are not imputed on accounts nor ledgers;
 *  these entries have been validated during their exercice.
 *
 * @ENT_STATUS_ROUGH:
 * @ENT_STATUS_VALIDATED:
 * @ENT_STATUS_DELETED:
 *
 * @ENT_STATUS_FUTURE: status attached to the entries imported or created
 *  for a future exercice; these entries are imputed on the corresponding
 *  solde of accounts and ledgers. They are considered as rough.
 */
typedef enum {
	ENT_STATUS_PAST = 1,
	ENT_STATUS_ROUGH,
	ENT_STATUS_VALIDATED,
	ENT_STATUS_DELETED,
	ENT_STATUS_FUTURE
}
	ofeEntryStatus;

/**
 * ofeEntryRule:
 *
 * @ENT_RULE_NORMAL: the normal entry.
 *
 * @ENT_RULE_FORWARD: an entry created when opening the exercice to get
 *  the carried forward soldes on the accounts.
 *
 * @ENT_RULE_CLOSE: an entry created when closing the exercice to solde
 *  the accounts.
 */
typedef enum {
	ENT_RULE_NORMAL = 1,
	ENT_RULE_FORWARD,
	ENT_RULE_CLOSE
}
	ofeEntryRule;

/* data max length */
#define ENT_LABEL_MAX_LENGTH          256

GType           ofo_entry_get_type                   ( void ) G_GNUC_CONST;

ofoEntry       *ofo_entry_new                        ( ofaIGetter *getter );

void            ofo_entry_dump                       ( const ofoEntry *entry );

GList          *ofo_entry_get_dataset_account_balance( ofaIGetter *getter,
															const gchar *from_account, const gchar *to_account,
															const GDate *from_date, const GDate *to_date );

GList          *ofo_entry_get_dataset_ledger_balance ( ofaIGetter *getter,
															const gchar *ledger,
															const GDate *from_date, const GDate *to_date );

GList          *ofo_entry_get_dataset_for_print_by_account
                                                     ( ofaIGetter *getter,
															const gchar *from_account, const gchar *to_account,
															const GDate *from_date, const GDate *to_date );

GList          *ofo_entry_get_dataset_for_print_by_ledger
                                                     ( ofaIGetter *getter,
															const GSList *mnemos,
															const GDate *from_date, const GDate *to_date );

GList          *ofo_entry_get_dataset_for_print_reconcil
                                                     ( ofaIGetter *getter,
                                                    		 const gchar *account, const GDate *date );

GList          *ofo_entry_get_dataset_for_exercice_by_status
                                                     ( ofaIGetter *getter, ofeEntryStatus status );

GList          *ofo_entry_get_dataset                ( ofaIGetter *getter );
ofxCounter      ofo_entry_get_count                  ( ofaIGetter *getter );
ofoEntry       *ofo_entry_get_by_number              ( ofaIGetter *getter, ofxCounter number );
void            ofo_entry_get_settlement_by_number   ( ofaIGetter *getter, ofxCounter number, gchar **user, GTimeVal *stamp );

#define         ofo_entry_free_dataset( L )          g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

gboolean        ofo_entry_use_account                ( ofaIGetter *getter, const gchar *account );
gboolean        ofo_entry_use_ledger                 ( ofaIGetter *getter, const gchar *ledger );

ofxCounter      ofo_entry_get_number                 ( const ofoEntry *entry );
const gchar    *ofo_entry_get_label                  ( const ofoEntry *entry );
const GDate    *ofo_entry_get_deffect                ( const ofoEntry *entry );
const GDate    *ofo_entry_get_dope                   ( const ofoEntry *entry );
const gchar    *ofo_entry_get_ref                    ( const ofoEntry *entry );
const gchar    *ofo_entry_get_account                ( const ofoEntry *entry );
const gchar    *ofo_entry_get_currency               ( const ofoEntry *entry );
const gchar    *ofo_entry_get_ledger                 ( const ofoEntry *entry );
const gchar    *ofo_entry_get_ope_template           ( const ofoEntry *entry );
ofxAmount       ofo_entry_get_debit                  ( const ofoEntry *entry );
ofxAmount       ofo_entry_get_credit                 ( const ofoEntry *entry );
ofeEntryStatus  ofo_entry_get_status                 ( const ofoEntry *entry );
const gchar    *ofo_entry_status_get_dbms            ( ofeEntryStatus status );
const gchar    *ofo_entry_status_get_abr             ( ofeEntryStatus status );
const gchar    *ofo_entry_status_get_label           ( ofeEntryStatus status );
ofeEntryRule    ofo_entry_get_rule                   ( const ofoEntry *entry );
const gchar    *ofo_entry_rule_get_dbms              ( ofeEntryRule rule );
const gchar    *ofo_entry_rule_get_abr               ( ofeEntryRule rule );
const gchar    *ofo_entry_rule_get_label             ( ofeEntryRule rule );
const gchar    *ofo_entry_get_rule_label             ( const ofoEntry *entry );
ofxCounter      ofo_entry_get_ope_number             ( const ofoEntry *entry );
ofxCounter      ofo_entry_get_settlement_number      ( const ofoEntry *entry );
const gchar    *ofo_entry_get_settlement_user        ( const ofoEntry *entry );
const GTimeVal *ofo_entry_get_settlement_stamp       ( const ofoEntry *entry );
ofxCounter      ofo_entry_get_tiers                  ( const ofoEntry *entry );
const gchar    *ofo_entry_get_notes                  ( const ofoEntry *entry );
const gchar    *ofo_entry_get_upd_user               ( const ofoEntry *entry );
const GTimeVal *ofo_entry_get_upd_stamp              ( const ofoEntry *entry );

gint            ofo_entry_get_exe_changed_count      ( ofaIGetter *getter,
															const GDate *prev_begin, const GDate *prev_end,
															const GDate *new_begin, const GDate *new_end );

GSList         *ofo_entry_get_currencies             ( ofaIGetter *getter );
#define         ofo_entry_free_currencies( L )       g_slist_free_full(( L ), ( GDestroyNotify ) g_free )

gboolean        ofo_entry_is_editable                ( const ofoEntry *entry );

void            ofo_entry_set_label                  ( ofoEntry *entry, const gchar *label );
void            ofo_entry_set_deffect                ( ofoEntry *entry, const GDate *date );
void            ofo_entry_set_dope                   ( ofoEntry *entry, const GDate *date );
void            ofo_entry_set_ref                    ( ofoEntry *entry, const gchar *ref );
void            ofo_entry_set_account                ( ofoEntry *entry, const gchar *number );
void            ofo_entry_set_currency               ( ofoEntry *entry, const gchar *currency );
void            ofo_entry_set_ledger                 ( ofoEntry *entry, const gchar *journal );
void            ofo_entry_set_ope_template           ( ofoEntry *entry, const gchar *model );
void            ofo_entry_set_debit                  ( ofoEntry *entry, ofxAmount amount );
void            ofo_entry_set_credit                 ( ofoEntry *entry, ofxAmount amount );
void            ofo_entry_set_ope_number             ( ofoEntry *entry, ofxCounter counter );
void            ofo_entry_set_settlement_number      ( ofoEntry *entry, ofxCounter counter );
void            ofo_entry_set_rule                   ( ofoEntry *entry, ofeEntryRule rule );
void            ofo_entry_set_tiers                  ( ofoEntry *entry, ofxCounter tiers );
void            ofo_entry_set_notes                  ( ofoEntry *entry, const gchar *notes );

gboolean        ofo_entry_is_valid_data              ( ofaIGetter *getter,
															const GDate *deffect, const GDate *dope,
															const gchar *label,
															const gchar *account, const gchar *currency,
															const gchar *ledger, const gchar *model,
															ofxAmount debit, ofxAmount credit,
															gchar **msgerr );

ofoEntry       *ofo_entry_new_with_data              ( ofaIGetter *getter,
															const GDate *deffect, const GDate *dope,
															const gchar *label, const gchar *ref,
															const gchar *account, const gchar *currency,
															const gchar *ledger, const gchar *model,
															ofxAmount debit, ofxAmount credit );

GList          *ofo_entry_get_doc_orphans            ( ofaIGetter *getter );
#define         ofo_entry_free_doc_orphans( L )      ( g_list_free( L ))

gboolean        ofo_entry_insert                     ( ofoEntry *entry );
gboolean        ofo_entry_update                     ( ofoEntry *entry );
gboolean        ofo_entry_update_settlement          ( ofoEntry *entry, ofxCounter number );
gboolean        ofo_entry_validate                   ( ofoEntry *entry );

gboolean        ofo_entry_validate_by_ledger         ( ofaIGetter *getter,
															const gchar *mnemo, const GDate *deffect );

void            ofo_entry_unsettle_by_number         ( ofaIGetter *getter,
															ofxCounter number );

gboolean        ofo_entry_delete                     ( ofoEntry *entry );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ENTRY_H__ */
