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

#ifndef __OFO_TVA_FORM_H__
#define __OFO_TVA_FORM_H__

/**
 * SECTION: ofo_tva_form
 * @short_description: #ofoTVAForm class definition.
 * @include: tva/ofo-tva-form.h
 *
 * This file defines the #ofoTVAForm class behavior.
 *
 * An #ofoTVAForm describes a TVA form.
 *
 * A form here is the exact image of the form asked by the
 * administration. Both bases and taxes are:
 * - either directly taken from an account,
 * - either computed from another field of the form.
 *
 * The language used here is mainly a substituting system where final
 * values are just the content of the fields after having replaced the
 * substitution tokens.
 * A substitution token is identified by a percent character '%',
 * followed by an uppercase letter. Unrecognized tokens are just
 * reconducted as-is to the output string.
 * Tokens are case sensitive.
 *
 * Tokens may be:
 *
 * 1/ a function, arguments being passed between parenthesis:
 *    - %CODE():    returns the line number (counted from 1) which holds
 *                  the code.
 *    - %AMOUNT():  returns the amount of the given line, counted from 1.
 *    - %BASE():    returns the base of the given line, counted from 1.
 *    - %ACCOUNT(begin[;end]): returns the rough+validated balance of the
 *                  entries imputed on the specified account with an
 *                  effect date between the declaration begin and end
 *                  dates (included).
 *                  The account may be specified as begin[;end] to indicate
 *                  that we must consider all acounts between 'begin' and
 *                  'end' (included) identifiers.
 *    - %BALANCE(begin[;end]): returns the current rough+validated balance
 *                  of the account(s).
 *
 * The opening parenthesis must immediately follow the function name.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_TVA_FORM                ( ofo_tva_form_get_type())
#define OFO_TVA_FORM( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TVA_FORM, ofoTVAForm ))
#define OFO_TVA_FORM_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TVA_FORM, ofoTVAFormClass ))
#define OFO_IS_TVA_FORM( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TVA_FORM ))
#define OFO_IS_TVA_FORM_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TVA_FORM ))
#define OFO_TVA_FORM_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TVA_FORM, ofoTVAFormClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoTVAForm;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoTVAFormClass;

GType             ofo_tva_form_get_type                  ( void ) G_GNUC_CONST;

GList            *ofo_tva_form_get_dataset               ( ofaIGetter *getter );

ofoTVAForm       *ofo_tva_form_get_by_mnemo              ( ofaIGetter *getter, const gchar *mnemo );

gboolean          ofo_tva_form_use_ope_template          ( ofaIGetter *getter, const gchar *ope_template );

ofoTVAForm       *ofo_tva_form_new                       ( ofaIGetter *getter );
ofoTVAForm       *ofo_tva_form_new_from_form             ( ofoTVAForm *form );

const gchar      *ofo_tva_form_get_mnemo                 ( const ofoTVAForm *form );
gchar            *ofo_tva_form_get_mnemo_new_from        ( const ofoTVAForm *form );
const gchar      *ofo_tva_form_get_label                 ( const ofoTVAForm *form );
const gchar      *ofo_tva_form_get_cre_user              ( const ofoTVAForm *form );
const myStampVal *ofo_tva_form_get_cre_stamp             ( const ofoTVAForm *form );
gboolean          ofo_tva_form_get_has_correspondence    ( const ofoTVAForm *form );
gboolean          ofo_tva_form_get_is_enabled            ( const ofoTVAForm *form );
const gchar      *ofo_tva_form_get_notes                 ( const ofoTVAForm *form );
const gchar      *ofo_tva_form_get_upd_user              ( const ofoTVAForm *form );
const myStampVal *ofo_tva_form_get_upd_stamp             ( const ofoTVAForm *form );

gboolean          ofo_tva_form_is_deletable              ( const ofoTVAForm *form );
gboolean          ofo_tva_form_is_valid_data             ( const gchar *mnemo, const gchar *label, gchar **msgerr );

gint              ofo_tva_form_compare_id                ( const ofoTVAForm *a, const ofoTVAForm *b );

void              ofo_tva_form_set_mnemo                 ( ofoTVAForm *form, const gchar *mnemo );
void              ofo_tva_form_set_label                 ( ofoTVAForm *form, const gchar *label );
void              ofo_tva_form_set_has_correspondence    ( ofoTVAForm *form, gboolean has_correspondence );
void              ofo_tva_form_set_is_enabled            ( ofoTVAForm *form, gboolean enabled );
void              ofo_tva_form_set_notes                 ( ofoTVAForm *form, const gchar *notes );

guint             ofo_tva_form_boolean_get_count         ( ofoTVAForm *form );
const gchar      *ofo_tva_form_boolean_get_label         ( ofoTVAForm *form, guint idx );

void              ofo_tva_form_boolean_reset             ( ofoTVAForm *form );

void              ofo_tva_form_boolean_add               ( ofoTVAForm *form,
															const gchar *label );

GList            *ofo_tva_form_boolean_get_orphans       ( ofaIGetter *getter );
#define           ofo_tva_form_boolean_free_orphans( L ) ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

guint             ofo_tva_form_detail_get_count          ( ofoTVAForm *form );
const gchar      *ofo_tva_form_detail_get_code           ( ofoTVAForm *form, guint idx );
const gchar      *ofo_tva_form_detail_get_label          ( ofoTVAForm *form, guint idx );
guint             ofo_tva_form_detail_get_level          ( ofoTVAForm *form, guint idx );
gboolean          ofo_tva_form_detail_get_has_base       ( ofoTVAForm *form, guint idx );
const gchar      *ofo_tva_form_detail_get_base           ( ofoTVAForm *form, guint idx );
gboolean          ofo_tva_form_detail_get_has_amount     ( ofoTVAForm *form, guint idx );
const gchar      *ofo_tva_form_detail_get_amount         ( ofoTVAForm *form, guint idx );
gboolean          ofo_tva_form_detail_get_has_template   ( ofoTVAForm *form, guint idx );
const gchar      *ofo_tva_form_detail_get_template       ( ofoTVAForm *form, guint idx );

void              ofo_tva_form_update_ope_template       ( ofoTVAForm *form, const gchar *prev_id, const gchar *new_id );

void              ofo_tva_form_detail_reset              ( ofoTVAForm *form );

void              ofo_tva_form_detail_add                ( ofoTVAForm *form,
																guint level,
																const gchar *code,
																const gchar *label,
																gboolean has_base,
																const gchar *base,
																gboolean has_amount,
																const gchar *amount,
																gboolean has_template,
																const gchar *template );

GList            *ofo_tva_form_detail_get_orphans        ( ofaIGetter *getter );
#define           ofo_tva_form_detail_free_orphans( L )  ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

guint             ofo_tva_form_doc_get_count             ( ofoTVAForm *form );

GList            *ofo_tva_form_doc_get_orphans           ( ofaIGetter *getter );
#define           ofo_tva_form_doc_free_orphans( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean          ofo_tva_form_insert                    ( ofoTVAForm *form );
gboolean          ofo_tva_form_update                    ( ofoTVAForm *form, const gchar *prev_mnemo );
gboolean          ofo_tva_form_delete                    ( ofoTVAForm *form );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_TVA_FORM_H__ */
