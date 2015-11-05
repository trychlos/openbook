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

#ifndef __OFA_ENTRY_COLUMNS_H__
#define __OFA_ENTRY_COLUMNS_H__

/**
 * SECTION: ofa_entry_page
 * @short_description: #ofaEntryPage class definition.
 * @include: ui/ofa-entry-page-columns.h
 */

G_BEGIN_DECLS

/* columns in the entries store
 * must declared before the private data in order to be able to
 * dimension the renderers array
 */
enum {
	ENT_COL_DOPE = 0,
	ENT_COL_DEFF,
	ENT_COL_NUMBER,						/* entry number */
	ENT_COL_REF,
	ENT_COL_LEDGER,
	ENT_COL_ACCOUNT,
	ENT_COL_LABEL,
	ENT_COL_SETTLE,
	ENT_COL_DRECONCIL,
	ENT_COL_DEBIT,
	ENT_COL_CREDIT,
	ENT_COL_CURRENCY,
	ENT_COL_STATUS,
				/*  below columns are not visible */
	ENT_COL_OBJECT,
	ENT_COL_MSGERR,
	ENT_COL_MSGWARN,
	ENT_COL_DOPE_SET,					/* operation date set by the user */
	ENT_COL_DEFF_SET,					/* effect date set by the user */
	ENT_COL_CURRENCY_SET,				/* currency set by the user */
	ENT_N_COLUMNS
};

const gchar *ofa_entry_columns_get_label      ( guint col_id );

gboolean     ofa_entry_columns_get_def_visible( guint col_id );

G_END_DECLS

#endif /* __OFA_ENTRY_COLUMNS_H__ */
