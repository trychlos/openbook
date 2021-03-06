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

#ifndef __OPENBOOK_API_OFS_OPE_H__
#define __OPENBOOK_API_OFS_OPE_H__

/**
 * SECTION: ofs_ope
 * @short_description: #ofsOpe structure definition.
 * @include: openbook/ofs-ope.h
 *
 * This is used as an entry for operation templates work:
 * an ope + an ope template = n entries (if %TRUE)
 */

#include "api/ofa-box.h"
#include "api/ofo-currency-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

/**
 * ofsOpe:
 *
 * @detail: detail lines must be in the same order than in the operation
 *  template.
 */
typedef struct {
	ofoOpeTemplate *ope_template;
	gchar          *ledger;
	gboolean        ledger_user_set;
	GDate           dope;
	gboolean        dope_user_set;
	GDate           deffect;
	gboolean        deffect_user_set;
	gchar          *ref;
	gboolean        ref_user_set;
	GList          *detail;
}
	ofsOpe;

/**
 * ofsOpeDetail:
 *
 * @user_set: when this flag is set, the content of the field is not
 *  overwritten by a formula originating from the operation template;
 *  set it (to %TRUE) if you want the content of the operation field
 *  takes precedence over a possible formula originating from template
 */
typedef struct {
	gchar          *account;
	gboolean        account_user_set;
	gboolean        account_is_valid;
	ofoCurrency    *currency;
	gchar          *label;
	gboolean        label_user_set;
	gboolean        label_is_valid;
	gdouble         debit;
	gboolean        debit_user_set;
	gdouble         credit;
	gboolean        credit_user_set;
	gboolean        amounts_are_valid;
}
	ofsOpeDetail;

/**
 * There are column identifiers in the UI grid view
 */
typedef enum {
	OPE_COL_RANG = 0,
	OPE_COL_ACCOUNT,
	OPE_COL_LABEL,
	OPE_COL_DEBIT,
	OPE_COL_CREDIT,
	OPE_COL_CURRENCY,
	OPE_COL_VALID,
	OPE_N_COLUMNS
}
	ofeOpeColumns;

ofsOpe   *ofs_ope_new                      ( ofoOpeTemplate *template );

void      ofs_ope_apply_template           ( ofsOpe *ope );

gboolean  ofs_ope_is_valid                 ( const ofsOpe *ope,
												gchar **message,
												GList **currencies );

ofxAmount ofs_ope_get_amount               ( const ofsOpe *ope,
												const gchar *cell_def,
												gchar **message );

void      ofs_ope_set_amount               ( ofsOpe *ope,
												const gchar *cell_def,
												ofxAmount amount );

ofxAmount ofs_ope_get_first_non_zero_amount( const ofsOpe *ope );

GList    *ofs_ope_generate_entries         ( const ofsOpe *ope );

void      ofs_ope_dump                     ( const ofsOpe *ope );

void      ofs_ope_free                     ( ofsOpe *ope );

G_END_DECLS

#endif /* __OPENBOOK_API_OFS_OPE_H__ */
