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

#include <api/my-settings.h>
#include <api/ofa-ifile-meta.h>

#include "ofa-mysql.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"

#define MYSQL_DATABASE_KEY_PREFIX       "mysql-db-"

static guint         idbprovider_get_interface_version( const ofaIDBProvider *instance );
static ofaIFileMeta *idbprovider_get_dossier_meta( const ofaIDBProvider *instance, const gchar *dossier_name, mySettings *settings, const gchar *group );
static GList        *idbprovider_get_dossier_periods( const ofaIDBProvider *instance, mySettings *settings, const gchar *group );

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
 * connect to the specified instance and database
 */
static GList *
idbprovider_get_dossier_periods( const ofaIDBProvider *instance, mySettings *settings, const gchar *group )
{
	GList *outlist;
	GList *keys, *itk;
	const gchar *cstr;

	keys = my_settings_get_keys( settings, group );
	for( itk=keys ; itk ; itk=itk->next ){
		cstr = ( const gchar * ) itk->data;
		if( g_str_has_prefix( cstr, MYSQL_DATABASE_KEY_PREFIX )){

		}
	}
	return( outlist );
}