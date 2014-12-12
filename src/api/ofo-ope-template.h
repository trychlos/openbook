/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFO_OPE_TEMPLATE_H__
#define __OFO_OPE_TEMPLATE_H__

/**
 * SECTION: ofo_ope_template
 * @short_description: #ofoOpeTemplate class definition.
 * @include: api/ofo-ope-template.h
 *
 * This file defines the #ofoOpeTemplate class behavior.
 */

#include "api/ofo-ope-template-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

void            ofo_ope_template_connect_handlers  ( const ofoDossier *dossier );

GList          *ofo_ope_template_get_dataset       ( ofoDossier *dossier );
ofoOpeTemplate *ofo_ope_template_get_by_mnemo      ( ofoDossier *dossier, const gchar *mnemo );
gboolean        ofo_ope_template_use_ledger        ( ofoDossier *dossier, const gchar *ledger );
gboolean        ofo_ope_template_use_rate          ( ofoDossier *dossier, const gchar *mnemo );

ofoOpeTemplate *ofo_ope_template_new               ( void );
ofoOpeTemplate *ofo_ope_template_new_from_template ( const ofoOpeTemplate *model );

const gchar    *ofo_ope_template_get_mnemo         ( const ofoOpeTemplate *model );
gchar          *ofo_ope_template_get_mnemo_new_from( const ofoOpeTemplate *model, ofoDossier *dossier );
const gchar    *ofo_ope_template_get_label         ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_ledger        ( const ofoOpeTemplate *model );
gboolean        ofo_ope_template_get_ledger_locked ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_notes         ( const ofoOpeTemplate *model );
const gchar    *ofo_ope_template_get_upd_user      ( const ofoOpeTemplate *model );
const GTimeVal *ofo_ope_template_get_upd_stamp     ( const ofoOpeTemplate *model );

gboolean        ofo_ope_template_is_deletable      ( const ofoOpeTemplate *model, ofoDossier *dossier );
gboolean        ofo_ope_template_is_valid          ( ofoDossier *dossier,
															const gchar *mnemo,
															const gchar *label,
															const gchar *ledger );

void            ofo_ope_template_set_mnemo         ( ofoOpeTemplate *model, const gchar *mnemo );
void            ofo_ope_template_set_label         ( ofoOpeTemplate *model, const gchar *label );
void            ofo_ope_template_set_ledger        ( ofoOpeTemplate *model, const gchar *ledger );
void            ofo_ope_template_set_ledger_locked ( ofoOpeTemplate *model, gboolean ledger_locked );
void            ofo_ope_template_set_notes         ( ofoOpeTemplate *model, const gchar *notes );

void            ofo_ope_template_add_detail        ( ofoOpeTemplate *model,
															const gchar *comment,
															const gchar *account, gboolean account_locked,
															const gchar *label, gboolean label_locked,
															const gchar *debit, gboolean debit_locked,
															const gchar *credit, gboolean credit_locked );
void            ofo_ope_template_free_detail_all   ( ofoOpeTemplate *model );

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

gboolean        ofo_ope_template_detail_is_formula        ( const gchar *str );

gboolean        ofo_ope_template_insert            ( ofoOpeTemplate *model, ofoDossier *dossier );
gboolean        ofo_ope_template_update            ( ofoOpeTemplate *model, ofoDossier *dossier, const gchar *prev_mnemo );
gboolean        ofo_ope_template_delete            ( ofoOpeTemplate *model, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_OPE_TEMPLATE_H__ */
