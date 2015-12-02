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

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-settings.h"
#include "api/my-utils.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-ifile-meta.h"
#include "api/ofa-ifile-period.h"

#include "ofa-mysql.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

static guint           idbprovider_get_interface_version( const ofaIDBProvider *instance );
static ofaIFileMeta   *idbprovider_load_meta( const ofaIDBProvider *instance, ofaIFileMeta **meta, const gchar *dossier_name, mySettings *settings, const gchar *group );
static GList          *meta_load_periods( ofaIFileMeta *meta, mySettings *settings, const gchar *group );
static ofaMySQLPeriod *meta_find_period( ofaMySQLPeriod *period, GList *list );
static ofaIDBConnect  *idbprovider_connect_dossier( const ofaIDBProvider *instance, ofaIFileMeta *meta, ofaIFilePeriod *period, const gchar *account, const gchar *password, gchar **msg );
static ofaIDBConnect  *idbprovider_connect_server( const ofaIDBProvider *instance, ofaIFileMeta *meta, const gchar *account, const gchar *password, gchar **msg );
static void            set_message( gchar **dest, const gchar *message );

/*
 * #ofaIDBProvider interface setup
 */
void
ofa_mysql_idbprovider_iface_init( ofaIDBProviderInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbprovider_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbprovider_get_interface_version;
	iface->get_provider_name = ofa_mysql_idbprovider_get_provider_name;
	iface->load_meta = idbprovider_load_meta;
	iface->connect_dossier = idbprovider_connect_dossier;
	iface->connect_server = idbprovider_connect_server;
}

/*
 * the version of the #ofaIDBProvider interface implemented by the module
 */
static guint
idbprovider_get_interface_version( const ofaIDBProvider *instance )
{
	return( 1 );
}

/*
 * the provider name identifier
 */
const gchar *
ofa_mysql_idbprovider_get_provider_name( const ofaIDBProvider *instance )
{
	return( "MySQL" );
}

/*
 * instanciates or reuses a new ofaMySQLMeta object which holds dossier
 * meta datas
 */
static ofaIFileMeta *
idbprovider_load_meta( const ofaIDBProvider *instance, ofaIFileMeta **meta,
							const gchar *dossier_name, mySettings *settings, const gchar *group )
{
	GList *periods;

	if( !*meta ){
		*meta = ( ofaIFileMeta * ) ofa_mysql_meta_new( instance, dossier_name, settings, group );
	}

	periods = meta_load_periods( *meta, settings, group );
	ofa_ifile_meta_set_periods( *meta, periods );
	ofa_ifile_meta_free_periods( periods );

	return( *meta );
}

/*
 * returns the list the defined periods, making its best to reuse
 *  existing references
 */
static GList *
meta_load_periods( ofaIFileMeta *meta, mySettings *settings, const gchar *group )
{
	GList *outlist, *prev_list;
	GList *keys, *itk;
	const gchar *cstr;
	ofaMySQLPeriod *new_period, *exist_period, *period;

	keys = my_settings_get_keys( settings, group );
	prev_list = ofa_ifile_meta_get_periods( meta );
	outlist = NULL;

	for( itk=keys ; itk ; itk=itk->next ){
		cstr = ( const gchar * ) itk->data;
		/* define a new period with the settings */
		new_period = ofa_mysql_period_new_from_settings( settings, group, cstr );
		if( new_period ){
			/* search for this period in the previous list */
			exist_period = meta_find_period( new_period, prev_list );
			if( exist_period ){
				period = exist_period;
				g_object_unref( new_period );
			} else {
				period = new_period;
			}
			outlist = g_list_prepend( outlist, period );
		}
	}

	ofa_ifile_meta_free_periods( prev_list );
	my_settings_free_keys( keys );

	return( g_list_reverse( outlist ));
}

static ofaMySQLPeriod *
meta_find_period( ofaMySQLPeriod *period, GList *list )
{
	GList *it;
	ofaMySQLPeriod *current;

	for( it=list ; it ; it=it->next ){
		current = ( ofaMySQLPeriod * ) it->data;
		if( ofa_ifile_period_compare( OFA_IFILE_PERIOD( current ), OFA_IFILE_PERIOD( period )) == 0 ){
			return( g_object_ref( current ));
		}
	}

	return( NULL );
}

static ofaIDBConnect *
idbprovider_connect_dossier( const ofaIDBProvider *instance, ofaIFileMeta *meta, ofaIFilePeriod *period, const gchar *account, const gchar *password, gchar **msg )
{
	ofaMySQLConnect *connect;

	if( !meta ){
		set_message( msg, _( "Unspecified dossier." ));
		return( NULL );
	}
	g_return_val_if_fail( OFA_IS_MYSQL_META( meta ), NULL );
	if( !period ){
		set_message( msg, _( "Unspecified exercice." ));
		return( NULL );
	}
	g_return_val_if_fail( OFA_IS_MYSQL_PERIOD( period ), NULL );
	if( !my_strlen( account )){
		set_message( msg, _( "Unspecified user account." ));
		return( NULL );
	}
	if( !my_strlen( password )){
		set_message( msg, _( "Unspecified user password." ));
		return( NULL );
	}

	connect = ofa_mysql_connect_new_for_meta_period(
					OFA_MYSQL_META( meta ), OFA_MYSQL_PERIOD( period ), account, password, msg );

	return( OFA_IDBCONNECT( connect ));
}

static ofaIDBConnect *
idbprovider_connect_server( const ofaIDBProvider *instance, ofaIFileMeta *meta, const gchar *account, const gchar *password, gchar **msg )
{
	ofaMySQLConnect *connect;

	if( !meta ){
		set_message( msg, _( "Unspecified dossier." ));
		return( NULL );
	}
	g_return_val_if_fail( OFA_IS_MYSQL_META( meta ), NULL );
	if( !my_strlen( account )){
		set_message( msg, _( "Unspecified user account." ));
		return( NULL );
	}
	if( !my_strlen( password )){
		set_message( msg, _( "Unspecified user password." ));
		return( NULL );
	}

	connect = ofa_mysql_connect_new_for_meta_period(
					OFA_MYSQL_META( meta ), NULL, account, password, msg );

	return( OFA_IDBCONNECT( connect ));
}

static void
set_message( gchar **dest, const gchar *message )
{
	if( dest ){
		*dest = g_strdup( message );
	}
}
