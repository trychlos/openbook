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

#include "api/ofa-idbconnect.h"

#include "ofa-tva.h"
#include "ofa-tva-dbcustomer.h"

static guint        idbcustomer_get_interface_version( const ofaIDBCustomer *instance );
static const gchar *idbcustomer_get_name( const ofaIDBCustomer *instance );
static gboolean     idbcustomer_needs_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect );
static gboolean     idbcustomer_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect );
static guint        version_get_current( const ofaIDBConnect *connect );
static guint        version_get_last( void );
static gboolean     version_begin( const ofaIDBConnect *connect, gint version );
static gboolean     version_end( const ofaIDBConnect *connect, gint version );
static gboolean     dbmodel_to_v1( const ofaIDBConnect *connect, guint version );
static gboolean     dbmodel_to_v2( const ofaIDBConnect *connect, guint version );

/* the functions which update the DB model
 */
typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( const ofaIDBConnect *connect, guint version );
}
	sMigration;

static sMigration st_migrates[] = {
		{ 1, dbmodel_to_v1 },
		{ 2, dbmodel_to_v2 },
		{ 0 }
};

/*
 * #ofaIDBCustomer interface setup
 */
void
ofa_tva_dbcustomer_iface_init( ofaIDBCustomerInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbcustomer_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbcustomer_get_interface_version;
	iface->get_name = idbcustomer_get_name;
	iface->needs_ddl_update = idbcustomer_needs_ddl_update;
	iface->ddl_update = idbcustomer_ddl_update;
}

/*
 * the version of the #ofaIDBCustomer interface implemented by the module
 */
static guint
idbcustomer_get_interface_version( const ofaIDBCustomer *instance )
{
	return( 1 );
}

static const gchar *
idbcustomer_get_name( const ofaIDBCustomer *instance )
{
	return( "TVA" );
}

static gboolean
idbcustomer_needs_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	guint vcurrent, vlast;

	vcurrent = version_get_current( connect );
	vlast = version_get_last();

	return( vcurrent < vlast );
}

static gboolean
idbcustomer_ddl_update( const ofaIDBCustomer *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_tva_dbcustomer_idbcustomer_ddl_update";
	guint i, vcurrent;
	gboolean ok;

	ok = TRUE;
	vcurrent = version_get_current( connect );

	for( i=0 ; st_migrates[i].ver_target && ok ; ++i ){
		if( vcurrent < st_migrates[i].ver_target ){
			ok = version_begin( connect, st_migrates[i].ver_target ) &&
					st_migrates[i].fnquery( connect, st_migrates[i].ver_target ) &&
					version_end( connect, st_migrates[i].ver_target );
			if( !ok ){
				g_warning( "%s: current DBMS model is version %d, unable to update it to v %d",
						thisfn, vcurrent, st_migrates[i].ver_target );
			}
		}
	}

	return( ok );
}

static guint
version_get_current( const ofaIDBConnect *connect )
{
	gint vcurrent;

	vcurrent = 0;
	ofa_idbconnect_query_int( connect,
			"SELECT MAX(VER_NUMBER) FROM TVA_T_VERSION WHERE VER_DATE > 0", &vcurrent, FALSE );

	return(( guint ) vcurrent );
}

static guint
version_get_last( void )
{
	guint last_version, i;

	last_version = 0;

	for( i=0 ; st_migrates[i].ver_target ; ++i ){
		if( last_version < st_migrates[i].ver_target ){
			last_version = st_migrates[i].ver_target;
		}
	}

	return( last_version );
}

static gboolean
version_begin( const ofaIDBConnect *connect, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !ofa_idbconnect_query( connect,
			"CREATE TABLE IF NOT EXISTS TVA_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'TVA DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'TVA update timestamp')", TRUE )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO TVA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gboolean
version_end( const ofaIDBConnect *connect, gint version )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"UPDATE TVA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gboolean
dbmodel_to_v1( const ofaIDBConnect *connect, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbcustomer_dbmodel_to_v1";

	g_debug( "%s: connect=%p, version=%u", thisfn, ( void * ) connect, version );

	if( !ofa_idbconnect_query( connect,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL UNIQUE COMMENT 'Form mnemonic',"
			"	TFO_LABEL          VARCHAR(80)                  COMMENT 'Form label',"
			"	TFO_NOTES          VARCHAR(4096)                COMMENT 'Notes',"
			"	TFO_UPD_USER       VARCHAR(20)                  COMMENT 'User responsible of last update',"
			"	TFO_UPD_STAMP      TIMESTAMP                    COMMENT 'Last update timestamp')", TRUE )){
		return( FALSE );
	}

	if( !ofa_idbconnect_query( connect,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS_DET ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_DET_ROW        INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_DET_CODE       VARCHAR(10)                  COMMENT 'Form line code',"
			"	TFO_DET_LABEL      VARCHAR(80)                  COMMENT 'Form line label',"
			"	TFO_DET_HAS_AMOUNT CHAR(1)                      COMMENT 'whether the form line has an amount',"
			"	TFO_DET_AMOUNT     VARCHAR(80)                  COMMENT 'Line amount computing rule',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_DET_ROW))", TRUE )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * dbmodel_to_v2:
 * -> set TFO_DET_LABEL to varchar(192)
 * -> add has Mention Expresse
 * -> add mention expresse
 * -> add has paiement par imputation
 * -> add paiement par imputation
 * -> add has correspondance
 * -> add cadre for correspondance
 * -> add detail has base (some rows have two columns: base and taxe)
 * -> add detail base
 * -> add has declaration néant
 * -> add declaration néant
 */
static gboolean
dbmodel_to_v2( const ofaIDBConnect *connect, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbcustomer_dbmodel_to_v2";

	g_debug( "%s: connect=%p, version=%u", thisfn, ( void * ) connect, version );

	if( !ofa_idbconnect_query( connect,
			"ALTER TABLE TVA_T_FORMS "
			"	ADD    COLUMN TFO_HAS_CORRESPONDENCE CHAR(1)       COMMENT 'Whether this form has a correspondence frame'", TRUE )){
		return( FALSE );
	}

	if( !ofa_idbconnect_query( connect,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_DET_LABEL          VARCHAR(192) COMMENT 'Form line label',"
			"	ADD    COLUMN TFO_DET_HAS_BASE       CHAR(1)      COMMENT 'Whether detail line has a base amount',"
			"	ADD    COLUMN TFO_DET_BASE           VARCHAR(80)  COMMENT 'Detail base'", TRUE )){
		return( FALSE );
	}

	if( !ofa_idbconnect_query( connect,
			"UPDATE TVA_T_FORMS_DET SET TFO_DET_HAS_BASE='N'", TRUE )){
		return( FALSE );
	}

	if( !ofa_idbconnect_query( connect,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS_BOOL ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_BOOL_ROW       INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_BOOL_LABEL     VARCHAR(80)                  COMMENT 'Form line label',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_BOOL_ROW))", TRUE )){
		return( FALSE );
	}

	return( TRUE );
}
