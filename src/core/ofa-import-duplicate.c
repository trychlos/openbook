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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-import-duplicate.h"

typedef struct {
	ofeImportDuplicate mode;
	const gchar  *label;
}
	sLabels;

/* import_mode labels are listed here in display order
 */
static const sLabels st_labels[] = {
		{ OFA_IDUPLICATE_REPLACE,  N_( "Imported duplicates replace already existing datas" ) },
		{ OFA_IDUPLICATE_IGNORE,   N_( "Imported duplicates are ignored, already existing datas being left unchanged" ) },
		{ OFA_IDUPLICATE_ABORT,    N_( "Count duplicate records as errors" ) },
		{ 0 }
};

/**
 * ofa_import_duplicate_get_label:
 * @mode: the import mode.
 *
 * Returns: the corresponding localized label as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofa_import_duplicate_get_label( ofeImportDuplicate mode )
{
	gint i;
	gchar *str;

	for( i=0 ; st_labels[i].mode ; ++i ){
		if( st_labels[i].mode == mode ){
			return( g_strdup( st_labels[i].label ));
		}
	}

	str = g_strdup_printf( _( "Unknown import mode: %d" ), mode );

	return( str );
}

/**
 * ofa_import_duplicate_enum:
 * @fn:
 * @user_data:
 *
 * Enumerate the known import modes.
 */
void
ofa_import_duplicate_enum( ImportDuplicateEnumCb fn, void *user_data )
{
	gint i;

	for( i=0 ; st_labels[i].mode ; ++i ){
		( *fn )( st_labels[i].mode, st_labels[i].label, user_data );
	}
}
