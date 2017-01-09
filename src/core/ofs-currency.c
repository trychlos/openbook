/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
	gdouble diff;

	digits = ofo_currency_get_digits( currency->currency );
	diff = currency->debit - currency->credit;

	return( my_double_is_zero( diff, digits ));
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
	gboolean debit_is_zero, credit_is_zero;

	digits = ofo_currency_get_digits( currency->currency );
	debit_is_zero = my_double_is_zero( currency->debit, digits );
	credit_is_zero = my_double_is_zero( currency->credit, digits );

	return( debit_is_zero && credit_is_zero );
}

/**
 * ofs_currency_cmp:
 * @a:
 * @b:
 *
 * The two #ofsCurrency are first compared by currency code.
 * If @a and @b both use the same currency, then debits are compared.
 * If debits are the same, then credits are compared.
 *
 * Returns: -1 if @a < @b, 1 if @a > @b, 0 if @a == @b.
 */
gint
ofs_currency_cmp( const ofsCurrency *a, const ofsCurrency *b )
{
	gint cmp, digits;
	gboolean is_zero;
	gdouble diff;

	cmp = my_collate( ofo_currency_get_code( a->currency ), ofo_currency_get_code( b->currency ));
	if( cmp == 0 ){
		digits = ofo_currency_get_digits( a->currency );
		diff = a->debit - b->debit;
		is_zero = my_double_is_zero( diff, digits );
		if( !is_zero ){
			cmp = ( a->debit < b->debit ) ? -1 : 1;
		} else {
			diff = a->credit - b->credit;
			is_zero = my_double_is_zero( diff, digits );
			if( !is_zero ){
				cmp = ( a->credit < b->credit ) ? -1 : 1;
			} else {
				cmp = 0;
			}
		}
	}

	return( cmp );
}

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

static void
currency_free( ofsCurrency *cur )
{
	g_object_unref( cur->currency );
	g_free( cur );
}
