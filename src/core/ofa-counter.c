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

#include "my/my-double.h"

#include "api/ofa-counter.h"
#include "api/ofa-preferences.h"

/**
 * ofa_counter_to_str:
 * @counter:
 *
 * Returns: the counter as a displayable, localized, decorated newly
 * allocated string, which should be g_free() by the caller.
 */
gchar *
ofa_counter_to_str( ofxCounter counter )
{
	gchar *str;

	str = my_bigint_to_str( counter, g_utf8_get_char( ofa_prefs_amount_thousand_sep()));

	return( str );
}
