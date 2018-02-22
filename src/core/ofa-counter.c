/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-double.h"

#include "api/ofa-counter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"

/**
 * ofa_counter_to_str:
 * @counter: the counter to be displayed.
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the counter as a displayable, localized, decorated newly
 * allocated string, which should be g_free() by the caller.
 */
gchar *
ofa_counter_to_str( ofxCounter counter, ofaIGetter *getter )
{
	gchar *str;

	str = my_bigint_to_str( counter, g_utf8_get_char( ofa_prefs_amount_get_thousand_sep( getter )));

	return( str );
}
