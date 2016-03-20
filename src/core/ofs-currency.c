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

#include "my/my-utils.h"

#include "api/ofo-currency.h"
#include "api/ofs-currency.h"

static gint cmp_currency( const ofsCurrency *a, const ofsCurrency *b );
static void currency_dump( ofsCurrency *cur, void *empty );
static void currency_free( ofsCurrency *cur );

/**
 * ofs_currency_add_by_code:
 */
ofsCurrency *
ofs_currency_add_by_code( GList **list, ofaHub *hub, const gchar *currency, gdouble debit, gdouble credit )
{
	ofsCurrency *found;
	ofoCurrency *cur_object;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( currency ), NULL );

	found = ofs_currency_get_by_code( *list, currency );

	if( !found ){
		cur_object = ofo_currency_get_by_code( hub, currency );
		g_return_val_if_fail( cur_object && OFO_IS_CURRENCY( cur_object ), NULL );
		found = g_new0( ofsCurrency, 1 );
		found->currency = g_object_ref( cur_object );
		*list = g_list_insert_sorted( *list, found, ( GCompareFunc ) cmp_currency );
	}

	found->debit += debit;
	found->credit += credit;

	return( found );
}

/**
 * ofs_currency_add_by_object:
 */
ofsCurrency *
ofs_currency_add_by_object( GList **list, ofoCurrency *currency, gdouble debit, gdouble credit )
{
	ofsCurrency *found;

	g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), NULL );

	found = ofs_currency_get_by_code( *list, ofo_currency_get_code( currency ));

	if( !found ){
		found = g_new0( ofsCurrency, 1 );
		found->currency = g_object_ref( currency );
		*list = g_list_insert_sorted( *list, found, ( GCompareFunc ) cmp_currency );
	}

	found->debit += debit;
	found->credit += credit;

	return( found );
}

/**
 * ofs_currency_get_by_code:
 */
ofsCurrency *
ofs_currency_get_by_code( GList *list, const gchar *currency )
{
	GList *it;
	ofsCurrency *it_cur;
	const gchar *it_code;

	g_return_val_if_fail( my_strlen( currency ), NULL );

	for( it=list ; it ; it=it->next ){
		it_cur = ( ofsCurrency * ) it->data;
		g_return_val_if_fail( it_cur && it_cur->currency && OFO_IS_CURRENCY( it_cur->currency ), NULL );
		it_code = ofo_currency_get_code( it_cur->currency );
		if( !my_collate( currency, it_code )){
			return( it_cur );
		}
	}

	return( NULL );
}

static gint
cmp_currency( const ofsCurrency *a, const ofsCurrency *b )
{
	const gchar *a_code, *b_code;

	g_return_val_if_fail( a && a->currency && OFO_IS_CURRENCY( a->currency ), 0 );
	g_return_val_if_fail( b && b->currency && OFO_IS_CURRENCY( b->currency ), 0 );

	a_code = ofo_currency_get_code( a->currency );
	b_code = ofo_currency_get_code( b->currency );

	return( g_utf8_collate( a_code, b_code ));
}

#if 0
static glong
amount_to_long_by_currency( gdouble amount, ofoCurrency *currency )
{
	gint digits;
	gdouble precision;

	digits = ofo_currency_get_digits( currency );
	precision = exp10( digits );

	return( amount_to_long_by_precision( amount, precision ));
}

static glong
amount_to_long_by_precision( gdouble amount, gdouble precision )
{
	gdouble temp;

	temp = amount * precision + 0.5;

	return(( glong ) temp );
}
#endif

/**
 * ofs_currency_is_balanced:
 * @currency:
 *
 * Returns: %TRUE if debit and credit are balanced.
 */
gboolean
ofs_currency_is_balanced( const ofsCurrency *currency )
{
	gint digits;
	gdouble precision, diff;

	digits = ofo_currency_get_digits( currency->currency );
	precision = 1/exp10( digits );

	diff = currency->debit - currency->credit;

	return( fabs( diff ) < precision );
}

/**
 * ofs_currency_is_zero:
 * @currency:
 *
 * Returns: %TRUE if debit and credit are equal to zero.
 */
gboolean
ofs_currency_is_zero( const ofsCurrency *currency )
{
	gint digits;
	gdouble precision;

	digits = ofo_currency_get_digits( currency->currency );
	precision = 1/exp10( digits );

	return( fabs( currency->debit ) < precision && fabs( currency->credit) < precision );
}

#if 0
/**
 * ofs_currency_amount_to_long:
 */
glong
ofs_currency_amount_to_long( const ofsCurrency *currency, gdouble amount )
{
	return( amount_to_long_by_currency( amount, currency->currency ));
}

/**
 * ofs_currency_update_amounts:
 */
void
ofs_currency_update_amounts( ofsCurrency *currency )
{
	gint digits;
	gdouble precision;

	digits = ofo_currency_get_digits( currency->currency );
	precision = exp10( digits );

	currency->debit = currency->ldebit / precision;
	currency->credit = currency->lcredit / precision;
}
#endif

/**
 * ofs_currency_list_dump:
 */
void
ofs_currency_list_dump( GList *list )
{
	g_list_foreach( list, ( GFunc ) currency_dump, NULL );
}

static void
currency_dump( ofsCurrency *cur, void *empty )
{
	g_debug( "  [%p] %s: debit=%.5lf, credit=%.5lf",
			( void * ) cur, ofo_currency_get_code( cur->currency ), cur->debit, cur->credit );
}

/*
 * ofs_currency_free:
 */
static void
currency_free( ofsCurrency *cur )
{
	g_object_unref( cur->currency );
	g_free( cur );
}

/**
 * ofs_currency_list_free:
 */
void
ofs_currency_list_free( GList **list )
{
	if( *list ){
		g_list_free_full( *list, ( GDestroyNotify ) currency_free );
		*list = NULL;
	}
}
