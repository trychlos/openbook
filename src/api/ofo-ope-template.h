/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFO_OPE_TEMPLATE_H__
#define __OPENBOOK_API_OFO_OPE_TEMPLATE_H__

/**
 * SECTION: ofoopetemplate
 * @title: ofoOpeTemplate
 * @short_description: #ofoOpeTemplate class definition.
 * @include: openbook/ofo-ope-template.h
 *
 * This file defines the #ofoOpeTemplate class behavior.
 *
 * An #ofoOpeTemplate is a template for an operation. It embeds two
 * types of datas:
 * - the template intrinsic datas:
 *   . mnemonic and label
 *   . notes
 *   . user and timestamp of last update
 *   . a comment on each detail line
 * - the datas used to generate entries:
 *   . ledger
 *   . piece reference
 *   . entry label
 *   . account
 *   . debit and credit amounts.
 *
 * Each entry-generation-data may be locked in the template: it will
 * be used as a mandatory, non-modifiable (though possibly not fixed,
 * see formula below) value for the generated entries.
 * If not locked, the user may choose to modify this data in the
 * operation, thus eventually generating different entries.
 *
 * Formulas may be used to automatically compute the value of some
 * fields, based on the value of other fields.
 * Formulas are mainly a substituting system, outputing an new string
 * after having replaced tokens by their value.
 * Tokens are identified by a percent character '%', followed by an
 * uppercase letter. Unrecognized tokens are just reconducted as-is to
 * the output string.
 * Tokens are case sensitive.
 *
 * Tokens may be:
 *
 * 1/ a reference to another field of the operation;
 *    these tokens are single-uppercase-letter, immediately followed
 *    by the number of the row, counted from 1:
 *    - 'Ai': account number
 *    - 'Li': label
 *    - 'Di': debit
 *    - 'Ci': credit
 *    E.g.: the '%A1' string (without the quotes) will be substituted
 *          with the account number from row n° 1.
 *
 * 2/ a reference to a global field of the operation
 *    - 'OPMN':    operation template mnemo
 *    - 'OPLA':    operation template label
 *    - 'LEMN':    ledger mnemo
 *    - 'LELA':    ledger label
 *    - 'REF':     the operation piece reference
 *    - 'DOPE':    operation date (format from user preferences)
 *    - 'DOMY':    operation date as mmm yyyy
 *    - 'DEFFECT': effect date (format from user preferences)
 *    - 'SOLDE':   the solde of the entries debit and credit to balance
 *                 the operation
 *    - 'IDEM':    the content of the same field from the previous row
 *
 *    Other keywords are searched as rate mnemonics (as a convenient
 *    shortcut to RATE() function).
 *
 * 3/ a function, arguments being passed between parenthesis:
 *    - 'ACLA()': returns account label
 *    - 'ACCU()': returns account currency
 *    - 'EVAL()': evaluate the result of the operation
 *                may be omitted when in an amount entry
 *    - 'RATE()': evaluates the rate value at the operation date
 *                may be omitted, only using %<rate> if the
 *                'rate' mnemonic is not ambiguous
 *    - 'ACCL()': the closing account for the currency of the specified
 *                account number
 *    The opening parenthesis must immediately follow the function name.
 *    E.g.:
 *    - the '%ACLA(%A1)' (without the quotes) will be substituted with
 *      the label of the account whose number is found in row 1
 *    - the '%EVAL( %D1 + %D2 ) will be susbtituted with the sum of
 *      debits from row 1 and 2
 *      Caution: the '%D1 + %D2' string will be only substituted with
 *        the string '<debit_1> + <debit_2', which is probably not what
 *        you want (unless you might want explicite a calculation)
 *    - the '%RATE( TVAN )' is the same that '%TVAN'
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_OPE_TEMPLATE                ( ofo_ope_template_get_type())
#define OFO_OPE_TEMPLATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplate ))
#define OFO_OPE_TEMPLATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplateClass ))
#define OFO_IS_OPE_TEMPLATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_OPE_TEMPLATE ))
#define OFO_IS_OPE_TEMPLATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_OPE_TEMPLATE ))
#define OFO_OPE_TEMPLATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplateClass ))

#if 0
typedef struct _ofoOpeTemplate               ofoOpeTemplate;
typedef struct _ofoOpeTemplatePrivate        ofoOpeTemplatePrivate;
#endif

struct _ofoOpeTemplate {
	/*< public members >*/
	ofoBase                parent;

	/*< private members >*/
	ofoOpeTemplatePrivate *priv;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass           parent;
}
	ofoOpeTemplateClass;

GType           ofo_ope_template_get_type                 ( void ) G_GNUC_CONST;

void            ofo_ope_template_connect_to_hub_signaling_system
                                                          ( const ofaHub *hub );

GList          *ofo_ope_template_get_dataset              ( ofaHub *hub );
#define         ofo_ope_template_free_dataset( L )        g_list_free_full(( L ),( GDestroyNotify ) g_object_unref )

ofoOpeTemplate *ofo_ope_template_get_by_mnemo             ( ofaHub *hub, const gchar *mnemo );
gboolean        ofo_ope_template_use_ledger               ( ofaHub *hub, const gchar *ledger );
gboolean        ofo_ope_template_use_rate                 ( ofaHub *hub, const gchar *mnemo );

ofoOpeTemplate *ofo_ope_template_new                      ( void );
ofoOpeTemplate *ofo_ope_template_new_from_template        ( const ofoOpeTemplate *model );

const gchar    *ofo_ope_template_get_mnemo                ( const ofoOpeTemplate *model );
gchar          *ofo_ope_template_get_mnemo_new_from       ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_label                ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_ledger               ( const ofoOpeTemplate *model );
gboolean        ofo_ope_template_get_ledger_locked        ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_ref                  ( const ofoOpeTemplate *model );
gboolean        ofo_ope_template_get_ref_locked           ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_notes                ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_upd_user             ( const ofoOpeTemplate *model );
const GTimeVal *ofo_ope_template_get_upd_stamp            ( const ofoOpeTemplate *model );

gboolean        ofo_ope_template_is_deletable             ( const ofoOpeTemplate *model );
gboolean        ofo_ope_template_is_valid                 ( const gchar *mnemo, const gchar *label, const gchar *ledger );

void            ofo_ope_template_set_mnemo                ( ofoOpeTemplate *model, const gchar *mnemo );
void            ofo_ope_template_set_label                ( ofoOpeTemplate *model, const gchar *label );
void            ofo_ope_template_set_ledger               ( ofoOpeTemplate *model, const gchar *ledger );
void            ofo_ope_template_set_ledger_locked        ( ofoOpeTemplate *model, gboolean ledger_locked );
void            ofo_ope_template_set_ref                  ( ofoOpeTemplate *model, const gchar *ref );
void            ofo_ope_template_set_ref_locked           ( ofoOpeTemplate *model, gboolean ref_locked );
void            ofo_ope_template_set_notes                ( ofoOpeTemplate *model, const gchar *notes );

void            ofo_ope_template_add_detail               ( ofoOpeTemplate *model,
																const gchar *comment,
																const gchar *account, gboolean account_locked,
																const gchar *label, gboolean label_locked,
																const gchar *debit, gboolean debit_locked,
																const gchar *credit, gboolean credit_locked );

void            ofo_ope_template_free_detail_all          ( ofoOpeTemplate *model );

gint            ofo_ope_template_get_detail_count         ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_detail_comment       ( const ofoOpeTemplate *model, gint idx );
const gchar    *ofo_ope_template_get_detail_account       ( const ofoOpeTemplate *model, gint idx );
gboolean        ofo_ope_template_get_detail_account_locked( const ofoOpeTemplate *model, gint idx );
const gchar    *ofo_ope_template_get_detail_label         ( const ofoOpeTemplate *model, gint idx );
gboolean        ofo_ope_template_get_detail_label_locked  ( const ofoOpeTemplate *model, gint idx );
const gchar    *ofo_ope_template_get_detail_debit         ( const ofoOpeTemplate *model, gint idx );
gboolean        ofo_ope_template_get_detail_debit_locked  ( const ofoOpeTemplate *model, gint idx );
const gchar    *ofo_ope_template_get_detail_credit        ( const ofoOpeTemplate *model, gint idx );
gboolean        ofo_ope_template_get_detail_credit_locked ( const ofoOpeTemplate *model, gint idx );

gboolean        ofo_ope_template_insert                   ( ofoOpeTemplate *model, ofaHub *hub );
gboolean        ofo_ope_template_update                   ( ofoOpeTemplate *model, const gchar *prev_mnemo );
gboolean        ofo_ope_template_delete                   ( ofoOpeTemplate *model );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_OPE_TEMPLATE_H__ */
