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

static gchar *exercice_get_description( const gchar *dname, const gchar *key );

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
	GSList *keys_list, *it;
	GSList *out_list;
	const gchar *cstr;
	gchar *line;

	out_list = NULL;
	keys_list = ofa_settings_dossier_get_keys( dname );

	for( it=keys_list ; it ; it=it->next ){
		cstr = ( const gchar * ) it->data;
		if( g_str_has_prefix( cstr, SETTINGS_DBMS_DATABASE )){
			line = exercice_get_description( dname, cstr );
			out_list = g_slist_prepend( out_list, line );
		}
	}

	ofa_settings_dossier_free_keys( keys_list );

	return( out_list );
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

/*
 * returned the current exercice description as a semi-colon separated
 * string:
 * - a displayable label
 * - the database name
 * - the begin of exercice yyyy-mm-dd
 * - the end of exercice yyyy-mm-dd
 * - the status
 */
static gchar *
exercice_get_description( const gchar *dname, const gchar *key )
{
	GList *strlist, *it;
	const gchar *sdb, *sbegin;
	gchar *send;
	gchar **array;
	gboolean is_current;
	GDate begin, end;
	gchar *label, *svalue, *sdbegin, *sdend, *status;

	strlist = ofa_settings_dossier_get_string_list( dname, key );

	sdb = sbegin = send = NULL;
	it = strlist;
	sdb = ( const gchar * ) it->data;
	it = it->next;
	if( it ){
		sbegin = ( const gchar * ) it->data;
	}
	if( g_utf8_collate( key, SETTINGS_DBMS_DATABASE )){
		array = g_strsplit( key, "_", -1 );
		send = g_strdup( *(array+1 ));
		g_strfreev( array );
		is_current = FALSE;
	} else {
		it = it ? it->next : NULL;
		if( it ){
			send = g_strdup(( const gchar * ) it->data );
		}
		is_current = TRUE;
	}

	status = g_strdup( is_current ? _( "Current" ) : _( "Archived" ));

	my_date_set_from_str( &begin, sbegin, MY_DATE_YYMD );
	sdbegin = my_date_to_str( &begin, MY_DATE_SQL );

	my_date_set_from_str( &end, send, MY_DATE_YYMD );
	sdend = my_date_to_str( &end, MY_DATE_SQL );

	label = ofa_dossier_misc_get_exercice_label( &begin, &end, is_current );

	svalue = g_strdup_printf( "%s;%s;%s;%s;%s;", label, sdb, sdbegin, sdend, status );

	g_free( label );
	g_free( send );
	g_free( sdbegin );
	g_free( sdend );
	g_free( status );

	ofa_settings_free_string_list( strlist );

	return( svalue );
}

/**
 * ofa_dossier_misc_get_current_dbname:
 * @dname: the name of the dossier
 *
 * Returns the name of the database for the current exercice as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_dossier_misc_get_current_dbname( const gchar *dname )
{
	GList *slist;
	gchar *dbname;

	slist = ofa_settings_dossier_get_string_list( dname, SETTINGS_DBMS_DATABASE );
	dbname = g_strdup( slist ? slist->data : "" );
	ofa_settings_free_string_list( slist );

	return( dbname );
}

/**
 * ofa_dossier_misc_set_current:
 * @dname:
 * @begin:
 * @end:
 *
 * Set the settings with the dates of the current exercice.
 */
void
ofa_dossier_misc_set_current( const gchar *dname, const GDate *begin, const GDate *end )
{
	GList *list, *it;
	gchar *dbname, *sbegin, *send, *str;

	list = ofa_settings_dossier_get_string_list( dname, SETTINGS_DBMS_DATABASE );
	dbname = NULL;
	it = list;
	if( it ){
		dbname = g_strdup(( const gchar * ) it->data );
	}
	ofa_settings_free_string_list( list );

	sbegin = my_date_to_str( begin, MY_DATE_YYMD );
	send = my_date_to_str( end, MY_DATE_YYMD );
	str = g_strdup_printf( "%s;%s;%s;", dbname, sbegin, send );

	ofa_settings_dossier_set_string( dname, SETTINGS_DBMS_DATABASE, str );

	g_free( dbname );
	g_free( str );
	g_free( sbegin );
	g_free( send );
}

/**
 * ofa_dossier_misc_set_new_exercice:
 * @dname:
 * @dbname:
 * @begin:
 * @end:
 *
 * Move the current exercice as an archived one
 * Define a new current exercice with the provided dates
 */
void
ofa_dossier_misc_set_new_exercice( const gchar *dname, const gchar *dbname, const GDate *begin, const GDate *end )
{
	GList *slist, *it;
	const gchar *sdb, *sbegin, *send;
	gchar *key, *content, *sbegin_next, *send_next;

	/* move current exercice to archived */
	slist = ofa_settings_dossier_get_string_list( dname, SETTINGS_DBMS_DATABASE );

	sdb = sbegin = send = NULL;
	it = slist;
	sdb = ( const gchar * ) it->data;
	it = it ? it->next : NULL;
	sbegin = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	send = it ? ( const gchar * ) it->data : NULL;
	g_return_if_fail( sdb && sbegin && send );

	key = g_strdup_printf( "%s_%s", SETTINGS_DBMS_DATABASE, send );
	content = g_strdup_printf( "%s;%s;", sdb, sbegin );

	ofa_settings_dossier_set_string( dname, key, content );

	ofa_settings_free_string_list( slist );
	g_free( key );
	g_free( content );

	/* define new current exercice */
	sbegin_next = my_date_to_str( begin, MY_DATE_YYMD );
	send_next = my_date_to_str( end, MY_DATE_YYMD );
	content = g_strdup_printf( "%s;%s;%s;", dbname, sbegin_next, send_next );

	ofa_settings_dossier_set_string( dname, SETTINGS_DBMS_DATABASE, content );

	g_free( content );
	g_free( sbegin_next );
	g_free( send_next );
}
