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
 *
 */

#ifndef __OFS_BAT_H__
#define __OFS_BAT_H__

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
 * ofsBat:
 * This structure is used when importing a bank account transaction
 * list (BAT).
 * All data members are to be set on output (no input data here).
 * Though most data are optional, the importer MUST set the version
 * number of the structure to the version it is using.
 */
typedef struct {
	gint     version;
	gchar   *uri;
	gchar   *format;
	GDate    begin;
	GDate    end;
	gchar   *rib;
	gchar   *currency;
	gdouble  begin_solde;				/* <0 if bank debit (so account credit) */
	gboolean begin_solde_set;
	gdouble  end_solde;
	gboolean end_solde_set;
	GList   *details;
}
	ofsBat;

typedef struct {
							/* bourso    bourso       lcl        lcl */
							/* excel95 excel2002 excel_tabulated pdf */
							/* ------- --------- --------------- --- */
	gint    version;		/*   X         X           X          X  */
	GDate   dope;			/*   X         X                      X  */
	GDate   deffect;		/*   X         X           X          X  */
	gchar  *ref;			/*                         X             */
	gchar  *label;			/*   X         X           X          X  */
	gdouble amount;			/*   X         X           X          X  */
	gchar  *currency;		/*   X         X                         */
}
	ofsBatDetail;

#define OFS_BAT_LAST_VERSION            1
#define OFS_BAT_DETAIL_LAST_VERSION     1

void ofs_bat_dump( const ofsBat *bat );

void ofs_bat_free( ofsBat *bat );

G_END_DECLS

#endif /* __OFS_BAT_H__ */
