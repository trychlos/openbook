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

#include <math.h>

#include "api/ofs-currency.h"

static ofsCurrency *currency_get_by_code( GList **list, const gchar *currency );
static gint         cmp_currency( const ofsCurrency *a, const ofsCurrency *b );
static void         currency_dump( ofsCurrency *cur, void *empty );
static void         currency_free( ofsCurrency *cur );

/**
 * ofs_currency_get_by_code:
 */
static ofsCurrency *
currency_get_by_code( GList **list, const gchar *currency )
{
	GList *it;
	ofsCurrency *found;

	found = NULL;
	for( it=*list ; it ; it=it->next ){
		if( !g_utf8_collate((( ofsCurrency * ) it->data )->currency, currency )){
			found = ( ofsCurrency * ) it->data;
			break;
		}
	}

	if( !found ){
		found = g_new0( ofsCurrency, 1 );
		found->currency = g_strdup( currency );
		*list = g_list_insert_sorted( *list, found, ( GCompareFunc ) cmp_currency );
	}

	return( found );
}

static gint
cmp_currency( const ofsCurrency *a, const ofsCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

/**
 * ofs_currency_amount_to_long:
 */
glong
ofs_currency_amount_to_long( const ofsCurrency *currency, gdouble amount )
{
	glong result;
	gdouble dbl;

	dbl = amount * exp10( currency->digits );
	dbl = round( dbl );
	result = ( glong ) dbl;

	return( result );
}

/**
 * ofs_currency_update_amounts:
 */
void
ofs_currency_update_amounts( ofsCurrency *currency )
{
	gdouble precision;

	precision = exp10( currency->digits );

	currency->debit = currency->ldebit / precision;
	currency->credit = currency->lcredit / precision;
}

/**
 * ofs_currency_add_currency:
 */
void
ofs_currency_add_currency( GList **list, const gchar *currency, gdouble debit, gdouble credit )
{
	ofsCurrency *found;

	found = currency_get_by_code( list, currency );
	found->debit += debit;
	found->credit += credit;
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
			( void * ) cur, cur->currency, cur->debit, cur->credit );
}

/*
 * ofs_currency_free:
 */
static void
currency_free( ofsCurrency *cur )
{
	g_free( cur->currency );
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
