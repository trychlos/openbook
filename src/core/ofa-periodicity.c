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

#include "api/my-utils.h"
#include "api/ofa-periodicity.h"

typedef struct {
	const gchar *code;
	const gchar *label;
}
	sLabels;

/* periodicity labels are listed here in display order
 */
static const sLabels st_labels[] = {
		{ PER_NEVER,   N_( "Never (disabled)" ) },
		{ PER_WEEKLY,  N_( "Weekly" ) },
		{ PER_MONTHLY, N_( "Monthly" ) },
		{ 0 }
};

/**
 * ofa_periodicity_get_label:
 * @periodicity: the unlocalized periodicity code.
 *
 * Returns: the corresponding localized label as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_periodicity_get_label( const gchar *periodicity )
{
	gint i;
	gchar *str;

	for( i=0 ; st_labels[i].code ; ++i ){
		if( !my_collate( st_labels[i].code, periodicity )){
			return( g_strdup( st_labels[i].label ));
		}
	}

	str = g_strdup_printf( _( "Unknown periodicity code: %s" ), periodicity );

	return( str );
}

/**
 * ofa_periodicity_enum:
 * @fn:
 * @user_data:
 *
 * Enumerate the known periodicities.
 */
void
ofa_periodicity_enum( PeriodicityEnumCb fn, void *user_data )
{
	gint i;

	for( i=0 ; st_labels[i].code ; ++i ){
		( *fn )( st_labels[i].code, st_labels[i].label, user_data );
	}
}
