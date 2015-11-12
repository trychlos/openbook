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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "ui/ofa-reconcil-columns.h"

typedef struct {
	gint         col_id;
	const gchar *label;
	gboolean     def_visible;
}
	sItem;

/* items are defined from left to right in the order of the creation of
 * their respective column for the treeview
 */
static sItem st_items[] = {
		{ COL_DOPE,     N_( "_Operation date" ),          TRUE },
		{ COL_LEDGER,   N_( "_Ledger identifier" ),       TRUE },
		{ COL_PIECE,    N_( "Piece _reference" ),         FALSE },
		{ COL_IDCONCIL, N_( "_Conciliation identifier" ), FALSE },
		{ -1 },
};

static sItem *id_to_item( gint id );

/**
 * ofa_reconcil_columns_get_label:
 * @col_id: the column identifier.
 *
 * Returns: the localized label for the column.
 */
const gchar *
ofa_reconcil_columns_get_label( guint col_id )
{
	sItem *item;

	item = id_to_item( col_id );
	if( item ){
		return( gettext( item->label ));
	}

	return( NULL );
}

/**
 * ofa_reconcil_columns_get_def_visible:
 * @col_id: the column identifier.
 *
 * Returns: whether the column defaults to be displayed.
 * Returns: %TRUE if the column is not defined here.
 */
gboolean
ofa_reconcil_columns_get_def_visible( guint col_id )
{
	sItem *item;

	item = id_to_item( col_id );
	if( item ){
		return( item->def_visible );
	}

	return( TRUE );
}

/*
 * Returns: the sItem struct which holds the specified column identifier
 */
static sItem *
id_to_item( gint id )
{
	for( gint i=0 ; st_items[i].col_id >= 0 ; ++i ){
		if( st_items[i].col_id == id ){
			return( &st_items[i] );
		}
	}
	return( NULL );
}
