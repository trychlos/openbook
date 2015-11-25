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

static guint          idbprovider_get_interface_version( const ofaIDBProvider *instance );
static ofaIFileMeta  *idbprovider_get_dossier_meta( const ofaIDBProvider *instance, const gchar *dossier_name, mySettings *settings, const gchar *group );
static GList         *idbprovider_get_dossier_periods( const ofaIDBProvider *instance, const ofaIFileMeta *meta );
static ofaIDBConnect *idbprovider_connect_dossier( const ofaIDBProvider *instance, ofaIFileMeta *meta, ofaIFilePeriod *period, const gchar *account, const gchar *password, gchar **msg );
static void           set_message( gchar **dest, const gchar *message );

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
	iface->get_dossier_meta = idbprovider_get_dossier_meta;
	iface->get_dossier_periods = idbprovider_get_dossier_periods;
	iface->connect_dossier = idbprovider_connect_dossier;
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
 * instanciates a new ofaMySQLMeta object which holds dossier meta datas
 */
static ofaIFileMeta *
idbprovider_get_dossier_meta( const ofaIDBProvider *instance, const gchar *dossier_name, mySettings *settings, const gchar *group )
{
	ofaMySQLMeta *meta;

	meta = ofa_mysql_meta_new( instance, dossier_name, settings, group );

	return( OFA_IFILE_META( meta ));
}

/*
 * list the defined periods
 */
static GList *
idbprovider_get_dossier_periods( const ofaIDBProvider *instance, const ofaIFileMeta *meta )
{
	GList *outlist;
	mySettings *settings;
	const gchar *group;
	GList *keys, *itk;
	const gchar *cstr;
	ofaMySQLPeriod *period;

	settings = ofa_ifile_meta_get_settings( meta );
	group = ofa_ifile_meta_get_group_name( meta );
	keys = my_settings_get_keys( settings, group );
	outlist = NULL;

	for( itk=keys ; itk ; itk=itk->next ){
		cstr = ( const gchar * ) itk->data;
		period = ofa_mysql_period_new( instance, settings, group, cstr );
		if( period ){
			outlist = g_list_prepend( outlist, period );
		}
	}

	my_settings_free_keys( keys );

	return( g_list_reverse( outlist ));
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

	connect = ofa_mysql_connect_new(
					OFA_MYSQL_META( meta ), OFA_MYSQL_PERIOD( period ), account, password, msg );

	return( OFA_IDBCONNECT( connect ));
}

static void
set_message( gchar **dest, const gchar *message )
{
	if( dest ){
		*dest = g_strdup( message );
	}
}
