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
 *
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * How to convert a double to a pointer, and vice-versa
 */
#include <glib.h>
#include <glib/gprintf.h>

int
main( int argc, char *argv[] )
{
	gdouble amount;
	gpointer p2, p3;
	gdouble a2, a3;
	gulong l2, l3;

	g_printf( "sizeof double=%lu\n", sizeof( amount ));
	g_printf( "sizeof pointer=%lu\n", sizeof( p2 ));

#define PRECISION 100000
#define AMOUNT_TO_POINTER(A) ((gpointer)(glong)((A)*PRECISION))
#define POINTER_TO_AMOUNT(P) (((gdouble)(glong)(P))/PRECISION)

	amount = 19.6;
	a2 = amount*100000;
	l2 = ( gulong ) a2;
	p2 = ( gpointer ) l2;

	p3 = p2;
	l3 = ( gulong ) p3;
	a3 = ( gdouble ) l3/100000;

	g_printf( "amount=%.5lf, a3=%.5lf\n", amount, a3 );

	p2 = AMOUNT_TO_POINTER( amount );
	a3 = POINTER_TO_AMOUNT( p2 );

	g_printf( "amount=%.5lf, a3=%.5lf\n", amount, a3 );

#if 0
	/* tests for null string */
	gchar *orig = NULL;
	gchar *dup = g_strdup( orig );	/* it is ok to g_strdup() a NULL string */
	/*g_strstrip( dup );*/				/* NOT OK if null */
#endif

	return 0;
}
