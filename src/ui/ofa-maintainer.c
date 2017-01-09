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

#include "api/ofa-hub.h"

#include "ui/ofa-application.h"
#include "ui/ofa-maintainer.h"

/**
 * ofa_maintainer_run_by_application:
 */
void
ofa_maintainer_run_by_application( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_maintainer_run_by_application";

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

#if 0
	gchar *cstr = "GRANT ALL PRIVILEGES ON `ofat`.* TO 'ofat'@'localhost' WITH GRANT OPTION";
	gchar *prev_dbname = "ofat";
	gchar *dbname = "ofat_3";
	GRegex *regex;
	gchar *str = g_strdup_printf( " `%s`\\.\\* ", prev_dbname );
	g_debug( "%s: str='%s'", thisfn, str );
	regex = g_regex_new( str, 0, 0, NULL );
	g_free( str );
	/*str = g_strdup_printf( "\\1%s", dbname );*/
	str = g_strdup_printf( " `%s`.* ", dbname );
	g_debug( "%s: str=%s", thisfn, str );
	if( g_regex_match( regex, cstr, 0, NULL )){
		gchar *query = g_regex_replace( regex, cstr, -1, 0, str, 0, NULL );
		g_debug( "%s: cstr=%s", thisfn, cstr );
		g_debug( "%s: query=%s", thisfn, query );
	}

	/* test formula engine */
	if( 0 ){
		ofa_formula_test( priv->hub );
	}

	/* generate 100 pseudo random integers */
	guint i;
	gchar *key;

	for( i=1 ; i<100 ; ++i ){
		key = g_strdup_printf( "%8x", g_random_int());
		g_debug( "%s: i=%u, key=%s", thisfn, i, key );
		g_free( key );
	}
#endif
}
