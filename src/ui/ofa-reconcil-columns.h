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

#ifndef __OFA_RECONCIL_COLUMNS_H__
#define __OFA_RECONCIL_COLUMNS_H__

/**
 * SECTION: ofa_reconcil_columns
 * @include: ui/ofa-reconcil-columns.h
 */

G_BEGIN_DECLS

/* column ordering in the listview
 */
enum {
	COL_ACCOUNT,
	COL_DOPE,
	COL_LEDGER,
	COL_PIECE,
	COL_NUMBER,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_IDCONCIL,
	COL_DRECONCIL,
	COL_OBJECT,				/* may be an ofoEntry or an ofoBatLine
	 	 	 	 	 	 	 * as long as it implements ofaIConcil interface */
	N_COLUMNS
};

const gchar *ofa_reconcil_columns_get_label      ( guint col_id );

gboolean     ofa_reconcil_columns_get_def_visible( guint col_id );

G_END_DECLS

#endif /* __OFA_RECONCIL_COLUMNS_H__ */
