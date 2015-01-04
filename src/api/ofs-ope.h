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

#ifndef __OFS_OPE_H__
#define __OFS_OPE_H__

/**
 * SECTION: ofs_ope
 * @short_description: #ofsOpe structure definition.
 * @include: api/ofs-ope.h
 *
 * This is used as an entry for operation templates work:
 * an ope + an ope template = n entries (if %TRUE)
 */

#include "api/ofo-dossier-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

/**
 * ofsOpe:
 *
 * @dope_user_set: only for consistency as Openbook doesn't provide any
 *  default for operation date
 *
 * @detail: detail lines must be in the same order than in the operation
 *  template.
 */
typedef struct {
	gchar   *ope_template;
	gchar   *ledger;
	gboolean ledger_user_set;
	GDate    dope;
	gboolean dope_user_set;
	GDate    deffect;
	gboolean deffect_user_set;
	GList   *detail;
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
	gchar   *ref;
	gboolean ref_user_set;
	gchar   *account;
	gboolean account_user_set;
	gchar   *label;
	gboolean label_user_set;
	gdouble  debit;
	gboolean debit_user_set;
	gdouble  credit;
	gboolean credit_user_set;
}
	ofsOpeDetail;


/**
 * There are column identifiers in the UI grid view
 */
enum {
	OPE_COL_RANG = 0,
	OPE_COL_REF,
	OPE_COL_ACCOUNT,
	OPE_COL_ACCOUNT_SELECT,
	OPE_COL_LABEL,
	OPE_COL_DEBIT,
	OPE_COL_CREDIT,
	OPE_COL_CURRENCY,
	OPE_N_COLUMNS
};

ofsOpe  *ofs_ope_new             ( const ofoOpeTemplate *template );

void     ofs_ope_compute_formulas( ofsOpe *ope,
										ofoDossier *dossier,
										const ofoOpeTemplate *template );

gboolean ofs_ope_is_valid        ( ofsOpe *ope,
										ofoDossier *dossier,
										gchar **message,
										GList **currencies );

GList   *ofs_ope_generate_entries( ofsOpe *ope,
										ofoDossier *dossier );

void     ofs_ope_free            ( ofsOpe *ope );

G_END_DECLS

#endif /* __OFS_OPE_H__ */
