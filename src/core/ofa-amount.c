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

#define _GNU_SOURCE
#include <math.h>

#include "my/my-double.h"

#include "api/ofa-amount.h"
#include "api/ofa-preferences.h"
#include "api/ofo-currency.h"

/**
 * ofa_amount_from_str:
 * @str: a localized, decorated, string.
 *
 * Returns: the evaulated amount.
 */
ofxAmount
ofa_amount_from_str( const gchar *str )
{
	ofxAmount amount;

	amount = my_double_set_from_str(
					str,
					g_utf8_get_char( ofa_prefs_amount_thousand_sep()),
					g_utf8_get_char( ofa_prefs_amount_decimal_sep()));

	return( amount );
}

/**
 * ofa_amount_to_str:
 * @amount:
 * @currency: [allow-none]:
 *
 * Returns: the amount as a displayable, localized, decorated newly
 * allocated string, which should be g_free() by the caller.
 */
gchar *
ofa_amount_to_str( ofxAmount amount, ofoCurrency *currency )
{
	gchar *str;
	guint digits;

	digits = currency && OFO_IS_CURRENCY( currency ) ?
					ofo_currency_get_digits( currency ) : CUR_DEFAULT_DIGITS;

	str = my_double_to_str( amount,
					g_utf8_get_char( ofa_prefs_amount_thousand_sep()),
					g_utf8_get_char( ofa_prefs_amount_decimal_sep()), digits );

	return( str );
}

/**
 * ofa_amount_is_zero:
 * @amount:
 * @currency:
 *
 * Returns: %TRUE if the amount is zero (or less than required precision).
 */
gboolean
ofa_amount_is_zero( ofxAmount amount, ofoCurrency *currency )
{
	gdouble precision;
	guint digits;

	digits = currency && OFO_IS_CURRENCY( currency ) ?
					ofo_currency_get_digits( currency ) : CUR_DEFAULT_DIGITS;

	precision = 1.0 / exp10( digits );

	return( fabs( amount ) < precision );
}