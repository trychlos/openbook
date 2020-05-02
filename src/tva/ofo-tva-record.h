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

#ifndef __OFO_TVA_RECORD_H__
#define __OFO_TVA_RECORD_H__

/**
 * SECTION: ofo_tva_record
 * @short_description: #ofoTVARecord class definition.
 * @include: tva/ofo-tva-form.h
 *
 * This file defines the #ofoTVARecord class behavior.
 *
 * An #ofoTVARecord describes a VAT declaration.
 * A VAT declaration is created from a form, and is assigned a begin
 * and an end date for this declaration. The declaration end date is
 * required in order the declaration be recordable.
 * Both begin and end dates are needed for the declaration be able to
 * be computed and validated.
 *
 * Dynamic of the status
 * ---------------------
 *
 * [creation] -> NO -> [user validation] -> USER
 *                  -> [period closing]  -> PCLOSE
 *
 * On creation, the declaration is given the status NO: all operations
 * may be freely executed.
 *
 * The declaration may be computed as often that the user wants to.
 * The status does not change when computing.
 *
 * When the user is satisfied with his declaration, he should:
 * - record his VAT declaration on government web site (impots.gouv.fr),
 *   maybe sending a paiment at that time;
 * - generate accounting operations.
 *
 * When the VAT declaration has been recorded on the DGI site, and the
 * paiement sent, then there is no sense to recompute or modify the
 * declaration in Openbook.
 *
 * The user should so "validate" his declaration, either manually
 * (status USER) or by closing the period (status PCLOSE).
 *
 * Accounting operations
 * ---------------------
 *
 * Accounting operations may be generated at any time, either while
 * computing or before or after having declared the VAT to the DGI,
 * or even after having validated the declaration:
 *
 * - accounting operations may be generated only once, and corresponding
 *   entries default to be created with Rough status
 *
 * - when the user validate his VAT declararation, entries which may
 *   have been generated, are validated too
 *
 * - while entries are not validated (and so the VAT declaration itself
 *   is not validated), then the generated operations may be freely
 *   cancelled, and regenerated, maybe after another computing
 *
 * - accounting operations can be generated after having validated the
 *   VAT declaration; corresponding entries are thus immediately set to
 *   Validated status.
 *
 * Properties update, computing
 * ----------------------------
 *
 * Computing the VAT declaration and updating the properties are up to
 * the user as long as the VAT declaration is in NO status and accounting
 * operations have not been generated.
 *
 * Once the VAT declaration has been validated, only notes can be updated.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

#include "tva/ofo-tva-form.h"

G_BEGIN_DECLS

#define OFO_TYPE_TVA_RECORD                ( ofo_tva_record_get_type())
#define OFO_TVA_RECORD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TVA_RECORD, ofoTVARecord ))
#define OFO_TVA_RECORD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TVA_RECORD, ofoTVARecordClass ))
#define OFO_IS_TVA_RECORD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TVA_RECORD ))
#define OFO_IS_TVA_RECORD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TVA_RECORD ))
#define OFO_TVA_RECORD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TVA_RECORD, ofoTVARecordClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoTVARecord;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoTVARecordClass;

/**
 * ofeVatStatus:
 *
 * @VAT_STATUS_NO: the VAT declaration has not been yet validated.
 * @VAT_STATUS_USER: the VAT declaration has been validated by the user.
 * @VAT_STATUS_PCLOSE: the VAT declaration has been automatically
 *  validated on period closing.
 *
 * Validation status of the VAT declaration.
 */
typedef enum {
	VAT_STATUS_NO = 1,
	VAT_STATUS_USER,
	VAT_STATUS_PCLOSE
}
	ofeVatStatus;

GType             ofo_tva_record_get_type                 ( void ) G_GNUC_CONST;

GList            *ofo_tva_record_get_dataset              ( ofaIGetter *getter );

GDate            *ofo_tva_record_get_last_end             ( ofaIGetter *getter, const gchar *mnemo, GDate * );
ofoTVARecord     *ofo_tva_record_get_by_key               ( ofaIGetter *getter, const gchar *mnemo, const GDate *candidate_end );
ofoTVARecord     *ofo_tva_record_get_overlap              ( ofaIGetter *getter, const gchar *mnemo, const GDate *candidate_begin, const GDate *candidate_end );

ofoTVARecord     *ofo_tva_record_new                      ( ofoTVAForm *form );

void              ofo_tva_record_dump                     ( const ofoTVARecord *record );

const gchar      *ofo_tva_record_get_mnemo                ( const ofoTVARecord *record );
const GDate      *ofo_tva_record_get_end                  ( const ofoTVARecord *record );
gboolean          ofo_tva_record_get_has_correspondence   ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_cre_user             ( const ofoTVARecord *record );
const myStampVal *ofo_tva_record_get_cre_stamp            ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_label                ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_correspondence       ( const ofoTVARecord *record );
const GDate      *ofo_tva_record_get_begin                ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_notes                ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_upd_user             ( const ofoTVARecord *record );
const myStampVal *ofo_tva_record_get_upd_stamp            ( const ofoTVARecord *record );
const GDate      *ofo_tva_record_get_dope                 ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_ope_user             ( const ofoTVARecord *record );
const myStampVal *ofo_tva_record_get_ope_stamp            ( const ofoTVARecord *record );
ofeVatStatus      ofo_tva_record_get_status               ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_status_get_dbms          ( ofeVatStatus valid );
const gchar      *ofo_tva_record_status_get_abr           ( ofeVatStatus valid );
const gchar      *ofo_tva_record_status_get_label         ( ofeVatStatus valid );
const GDate      *ofo_tva_record_get_sta_closing          ( const ofoTVARecord *record );
const gchar      *ofo_tva_record_get_sta_user             ( const ofoTVARecord *record );
const myStampVal *ofo_tva_record_get_sta_stamp            ( const ofoTVARecord *record );

GList            *ofo_tva_record_get_accounting_opes      ( ofoTVARecord *record );
void              ofo_tva_record_delete_accounting_entries( ofoTVARecord *record, GList *opes );

guint             ofo_tva_record_boolean_get_count        ( ofoTVARecord *record );
const gchar      *ofo_tva_record_boolean_get_label        ( ofoTVARecord *record, guint idx );
gboolean          ofo_tva_record_boolean_get_true         ( ofoTVARecord *record, guint idx );

guint             ofo_tva_record_detail_get_count         ( ofoTVARecord *record );
const gchar      *ofo_tva_record_detail_get_code          ( ofoTVARecord *record, guint idx );
const gchar      *ofo_tva_record_detail_get_label         ( ofoTVARecord *record, guint idx );
guint             ofo_tva_record_detail_get_level         ( ofoTVARecord *record, guint idx );
gboolean          ofo_tva_record_detail_get_has_base      ( ofoTVARecord *record, guint idx );
const gchar      *ofo_tva_record_detail_get_base_formula  ( ofoTVARecord *record, guint idx );
ofxAmount         ofo_tva_record_detail_get_base          ( ofoTVARecord *record, guint idx );
gboolean          ofo_tva_record_detail_get_has_amount    ( ofoTVARecord *record, guint idx );
const gchar      *ofo_tva_record_detail_get_amount_formula( ofoTVARecord *record, guint idx );
ofxAmount         ofo_tva_record_detail_get_amount        ( ofoTVARecord *record, guint idx );
gboolean          ofo_tva_record_detail_get_has_template  ( ofoTVARecord *record, guint idx );
const gchar      *ofo_tva_record_detail_get_template      ( ofoTVARecord *record, guint idx );
ofxCounter        ofo_tva_record_detail_get_ope_number    ( ofoTVARecord *record, guint idx );

gboolean          ofo_tva_record_is_deletable             ( const ofoTVARecord *record, GList *gen_opes );
gboolean          ofo_tva_record_is_valid_data            ( const gchar *mnemo, const gchar *label, const GDate *begin, const GDate *end, gchar **msgerr );

gint              ofo_tva_record_compare_by_key           ( const ofoTVARecord *record, const gchar *mnemo, const GDate *end );

void              ofo_tva_record_set_end                  ( ofoTVARecord *record, const GDate *date );
void              ofo_tva_record_set_label                ( ofoTVARecord *record, const gchar *label );
void              ofo_tva_record_set_correspondence       ( ofoTVARecord *record, const gchar *correspondence );
void              ofo_tva_record_set_begin                ( ofoTVARecord *record, const GDate *date );
void              ofo_tva_record_set_notes                ( ofoTVARecord *record, const gchar *notes );

void              ofo_tva_record_boolean_set_true         ( ofoTVARecord *record, guint idx, gboolean is_true );

void              ofo_tva_record_detail_set_base          ( ofoTVARecord *record, guint idx, ofxAmount base );
void              ofo_tva_record_detail_set_amount        ( ofoTVARecord *record, guint idx, ofxAmount amount );
void              ofo_tva_record_detail_set_ope_number    ( ofoTVARecord *record, guint idx, ofxCounter number );

GList            *ofo_tva_record_get_bool_orphans         ( ofaIGetter *getter );
#define           ofo_tva_record_free_bool_orphans( L )   ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

GList            *ofo_tva_record_get_det_orphans          ( ofaIGetter *getter );
#define           ofo_tva_record_free_det_orphans( L )    ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

GList            *ofo_tva_record_get_doc_orphans          ( ofaIGetter *getter );
#define           ofo_tva_record_free_doc_orphans( L )    ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean          ofo_tva_record_validate                 ( ofoTVARecord *record, ofeVatStatus status, const GDate *closing );
guint             ofo_tva_record_validate_all             ( ofaIGetter *getter, const GDate *closing );

gboolean          ofo_tva_record_insert                   ( ofoTVARecord *record );

gboolean          ofo_tva_record_update                   ( ofoTVARecord *record );
gboolean          ofo_tva_record_update_notes             ( ofoTVARecord *record );
gboolean          ofo_tva_record_update_dope              ( ofoTVARecord *record, const GDate *date );

gboolean          ofo_tva_record_delete                   ( ofoTVARecord *record );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_TVA_RECORD_H__ */
