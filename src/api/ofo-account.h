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

#ifndef __OPENBOOK_API_OFO_ACCOUNT_H__
#define __OPENBOOK_API_OFO_ACCOUNT_H__

/**
 * SECTION: ofoaccount
 * @title: ofoAccount
 * @short_description: #ofoAccount class definition.
 * @include: openbook/ofo-account.h
 *
 * This file defines the #ofoAccount class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-account-def.h"
#include "api/ofo-account-v34-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_ACCOUNT                ( ofo_account_get_type())
#define OFO_ACCOUNT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ACCOUNT, ofoAccount ))
#define OFO_ACCOUNT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ACCOUNT, ofoAccountClass ))
#define OFO_IS_ACCOUNT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ACCOUNT ))
#define OFO_IS_ACCOUNT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ACCOUNT ))
#define OFO_ACCOUNT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ACCOUNT, ofoAccountClass ))

#if 0
typedef struct _ofoAccount              ofoAccount;
#endif

struct _ofoAccount {
	/*< public members >*/
	ofoAccountv34      parent;
};

typedef struct {
	/*< public members >*/
	ofoAccountv34Class parent;
}
	ofoAccountClass;

/**
 * ofeAccountType:
 *
 * This is a type attached to the archived account balance.
 *
 * @ACC_TYPE_OPEN: the balance of the account when opening the exercice.
 * @ACC_TYPE_NORMAL: a balance archived during the exercice.
 */
typedef enum {
	ACC_TYPE_OPEN = 1,
	ACC_TYPE_NORMAL
}
	ofeAccountType;

/* data max length */
#define ACC_NUMBER_WIDTH                10
#define ACC_NUMBER_MAX_LENGTH           64

GType             ofo_account_get_type                  ( void ) G_GNUC_CONST;

GList            *ofo_account_get_dataset               ( ofaIGetter *getter );
GList            *ofo_account_get_dataset_for_solde     ( ofaIGetter *getter );
#define           ofo_account_free_dataset( L )         ( g_list_free_full(( L ),( GDestroyNotify ) g_object_unref ))

ofoAccount       *ofo_account_get_by_number             ( ofaIGetter *getter, const gchar *number );

ofoAccount       *ofo_account_new                       ( ofaIGetter *getter );

gint              ofo_account_get_class                 ( const ofoAccount *account );
const gchar      *ofo_account_get_number                ( const ofoAccount *account );
const gchar      *ofo_account_get_cre_user              ( const ofoAccount *account );
const myStampVal *ofo_account_get_cre_stamp             ( const ofoAccount *account );
const gchar      *ofo_account_get_label                 ( const ofoAccount *account );
const gchar      *ofo_account_get_currency              ( const ofoAccount *account );
gboolean          ofo_account_is_root                   ( const ofoAccount *account );
gboolean          ofo_account_is_settleable             ( const ofoAccount *account );
gboolean          ofo_account_get_keep_unsettled        ( const ofoAccount *account );
gboolean          ofo_account_is_reconciliable          ( const ofoAccount *account );
gboolean          ofo_account_get_keep_unreconciliated  ( const ofoAccount *account );
gboolean          ofo_account_is_forwardable            ( const ofoAccount *account );
gboolean          ofo_account_is_closed                 ( const ofoAccount *account );
const gchar      *ofo_account_get_notes                 ( const ofoAccount *account );
const gchar      *ofo_account_get_upd_user              ( const ofoAccount *account );
const myStampVal *ofo_account_get_upd_stamp             ( const ofoAccount *account );
ofxAmount         ofo_account_get_open_debit            ( ofoAccount *account );
ofxAmount         ofo_account_get_open_credit           ( ofoAccount *account );
ofxAmount         ofo_account_get_current_rough_debit   ( const ofoAccount *account );
ofxAmount         ofo_account_get_current_rough_credit  ( const ofoAccount *account );
ofxAmount         ofo_account_get_current_val_debit     ( const ofoAccount *account );
ofxAmount         ofo_account_get_current_val_credit    ( const ofoAccount *account );
ofxAmount         ofo_account_get_futur_rough_debit     ( const ofoAccount *account );
ofxAmount         ofo_account_get_futur_rough_credit    ( const ofoAccount *account );
ofxAmount         ofo_account_get_futur_val_debit       ( const ofoAccount *account );
ofxAmount         ofo_account_get_futur_val_credit      ( const ofoAccount *account );
ofxAmount         ofo_account_get_solde_at_date         ( ofoAccount *account, const GDate *date, GDate *deffect, ofxAmount *debit, ofxAmount *credit );

gboolean          ofo_account_is_deletable              ( const ofoAccount *account );
gboolean          ofo_account_is_valid_data             ( const gchar *number, const gchar *label, const gchar *devise, gboolean root, gchar **msgerr );
gint              ofo_account_get_class_from_number     ( const gchar *number );
gint              ofo_account_get_level_from_number     ( const gchar *number );
gboolean          ofo_account_has_children              ( const ofoAccount *account );
GList            *ofo_account_get_children              ( const ofoAccount *account );
gboolean          ofo_account_is_child_of               ( const ofoAccount *account, const gchar *candidate );
gboolean          ofo_account_is_allowed                ( const ofoAccount *account, gint allowed );
const gchar      *ofo_account_get_balance_type_dbms     ( ofeAccountType type );
const gchar      *ofo_account_get_balance_type_short    ( ofeAccountType type );

void              ofo_account_set_number                ( ofoAccount *account, const gchar *number );
void              ofo_account_set_label                 ( ofoAccount *account, const gchar *label );
void              ofo_account_set_currency              ( ofoAccount *account, const gchar *devise );
void              ofo_account_set_root                  ( ofoAccount *account, gboolean root );
void              ofo_account_set_settleable            ( ofoAccount *account, gboolean settleable );
void              ofo_account_set_keep_unsettled        ( ofoAccount *account, gboolean keep );
void              ofo_account_set_reconciliable         ( ofoAccount *account, gboolean reconciliable );
void              ofo_account_set_keep_unreconciliated  ( ofoAccount *account, gboolean keep );
void              ofo_account_set_forwardable           ( ofoAccount *account, gboolean forwardable );
void              ofo_account_set_closed                ( ofoAccount *account, gboolean closed );
void              ofo_account_set_notes                 ( ofoAccount *account, const gchar *notes );
void              ofo_account_set_current_rough_debit   ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_current_rough_credit  ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_current_val_debit     ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_current_val_credit    ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_futur_rough_debit     ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_futur_rough_credit    ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_futur_val_debit       ( ofoAccount *account, ofxAmount amount );
void              ofo_account_set_futur_val_credit      ( ofoAccount *account, ofxAmount amount );

gboolean          ofo_account_archive_openings          ( ofaIGetter *getter, const GDate *exe_begin );
gboolean          ofo_account_archive_balances          ( ofoAccount *account, const GDate *date );

guint             ofo_account_archive_get_count         ( ofoAccount *account );
const GDate      *ofo_account_archive_get_date          ( ofoAccount *account, guint idx );
ofeAccountType    ofo_account_archive_get_type          ( ofoAccount *account, guint idx );
ofxAmount         ofo_account_archive_get_debit         ( ofoAccount *account, guint idx );
ofxAmount         ofo_account_archive_get_credit        ( ofoAccount *account, guint idx );

GList            *ofo_account_archive_get_orphans       ( ofaIGetter *getter );
#define           ofo_account_archive_free_orphans( L ) ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

guint             ofo_account_doc_get_count             ( ofoAccount *account );

GList            *ofo_account_doc_get_orphans           ( ofaIGetter *getter );
#define           ofo_account_doc_free_orphans( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean          ofo_account_insert                    ( ofoAccount *account );
gboolean          ofo_account_update                    ( ofoAccount *account, const gchar *prev_number );
gboolean          ofo_account_update_amounts            ( ofoAccount *account );
gboolean          ofo_account_delete                    ( ofoAccount *account );
gboolean          ofo_account_delete_with_children      ( ofoAccount *account );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ACCOUNT_H__ */
