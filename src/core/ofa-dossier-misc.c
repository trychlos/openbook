/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/ofa-dbms.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"

/**
 * ofa_dossier_misc_get_dossiers:
 *
 * Returns: the list of all defined dossiers.
 *
 * Each string of the returned list is a semi-colon separated list of
 * - the dossier name
 * - the DBMS provider name.
 *
 * The returned list should be #ofa_dossier_misc_free_dossiers() by the
 * caller.
 */
GSList *
ofa_dossier_misc_get_dossiers( void )
{
	GSList *slist_in, *it;
	GSList *slist_out;
	gchar *prefix, *dname, *dbms, *out_str;
	gint spfx;
	const gchar *cstr;

	prefix = g_strdup_printf( "%s ", SETTINGS_GROUP_DOSSIER );
	spfx = g_utf8_strlen( prefix, -1 );

	slist_in = ofa_settings_get_groups( SETTINGS_TARGET_DOSSIER );
	slist_out = NULL;

	for( it=slist_in ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		if( g_str_has_prefix( cstr, prefix )){
			dname = g_strstrip( g_strdup( cstr+spfx ));
			dbms = ofa_settings_get_dossier_provider( dname );
			out_str = g_strdup_printf( "%s;%s;", dname, dbms );
			slist_out = g_slist_append( slist_out, out_str );
			g_free( dbms );
			g_free( dname );
		}
	}

	g_free( prefix );
	ofa_settings_free_groups( slist_in );

	return( slist_out );
}

/**
 * ofa_dossier_misc_get_exercices:
 * @dname: the name of the dossier from settings.
 *
 * Returns: the list of known exercices for the dossier.
 *
 * The returned list should be #ofa_dossier_misc_free_exercices() by
 * the caller.
 *
 * Each item of this #GSList is the result of the concatenation of the
 * two strings:
 * - a displayable label
 * - the database name
 * - the exercice begin date as a sql-formatted string yyyy-mm-dd
 * - the exercice end date as a sql-formatted string yyyy-mm-dd
 * - the status of the exercice.
 *
 * The strings are semi-colon separated.
 */
GSList *
ofa_dossier_misc_get_exercices ( const gchar *dname )
{
	ofaDbms *dbms;
	GSList *list;

	dbms = ofa_dbms_new();

	/* only the provider knows how it stores the databases
	 * so ask this to it */
	list = ofa_dbms_get_exercices( dbms, dname );

	g_object_unref( dbms );

	return( list );
}

/**
 * ofa_dossier_misc_get_exercice_label:
 *
 * Returns: the exercice label description as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_dossier_misc_get_exercice_label( const GDate *begin, const GDate *end, gboolean is_current )
{
	GString *svalue;
	gchar *sdate;

	svalue = g_string_new( is_current ? _( "Current exercice" ) : _( "Archived exercice" ));

	if( my_date_is_valid( begin )){
		sdate = my_date_to_str( begin , MY_DATE_DMYY );
		g_string_append_printf( svalue, _( " from %s" ), sdate );
		g_free( sdate );
	}

	if( my_date_is_valid( end )){
		sdate = my_date_to_str( end, MY_DATE_DMYY );
		g_string_append_printf( svalue, _( " to %s" ), sdate );
		g_free( sdate );
	}

	return( g_string_free( svalue, FALSE ));
}
