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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/ofa-core.h"

static const gchar *st_authors[] = {
	"Pierre Wieser <pwieser@trychlos.org>",
	NULL
};

/**
 * ofa_core_get_copyright:
 *
 * Returns: the copyright string for the application.
 */
const gchar *
ofa_core_get_copyright( void )
{
	return( _( "Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)" ));
}

/**
 * ofa_core_get_authors:
 *
 * Returns: the list of authors for the application.
 */
const gchar **
ofa_core_get_authors( void )
{
	return( st_authors );
}
