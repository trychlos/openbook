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

#include <glib/gi18n.h>

#include "my/my-iident.h"
#include "my/my-iprogress.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-igetter.h"
#include "api/ofo-ope-template.h"

#include "ofa-tva-dbmodel.h"
#include "ofo-tva-form.h"
#include "ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* update setup
	 */
	ofaIGetter          *getter;
	const ofaIDBConnect *connect;
	myIProgress         *window;

	/* update progression
	 */
	gulong               total;
	gulong               current;
}
	ofaTvaDBModelPrivate;

#define DBMODEL_CANON_NAME               "VAT"

/* the functions which update the DB model
 */
static gboolean dbmodel_to_v1( ofaTvaDBModel *self, guint version );
static gulong   count_v1( ofaTvaDBModel *self );
static gboolean dbmodel_to_v2( ofaTvaDBModel *self, guint version );
static gulong   count_v2( ofaTvaDBModel *self );
static gboolean dbmodel_to_v3( ofaTvaDBModel *self, guint version );
static gulong   count_v3( ofaTvaDBModel *self );
static gboolean dbmodel_to_v4( ofaTvaDBModel *self, guint version );
static gulong   count_v4( ofaTvaDBModel *self );
static gboolean dbmodel_to_v5( ofaTvaDBModel *self, guint version );
static gulong   count_v5( ofaTvaDBModel *self );
static gboolean dbmodel_to_v6( ofaTvaDBModel *self, guint version );
static gulong   count_v6( ofaTvaDBModel *self );
static gboolean dbmodel_to_v7( ofaTvaDBModel *self, guint version );
static gulong   count_v7( ofaTvaDBModel *self );

typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( ofaTvaDBModel *self, guint version );
	gulong   ( *fncount )( ofaTvaDBModel *self );
}
	sMigration;

static sMigration st_migrates[] = {
		{ 1, dbmodel_to_v1, count_v1 },
		{ 2, dbmodel_to_v2, count_v2 },
		{ 3, dbmodel_to_v3, count_v3 },
		{ 4, dbmodel_to_v4, count_v4 },
		{ 5, dbmodel_to_v5, count_v5 },
		{ 6, dbmodel_to_v6, count_v6 },
		{ 7, dbmodel_to_v7, count_v7 },
		{ 0 }
};

#define MARGIN_LEFT                     20

static void       iident_iface_init( myIIdentInterface *iface );
static gchar     *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar     *iident_get_version( const myIIdent *instance, void *user_data );
static void       idbmodel_iface_init( ofaIDBModelInterface *iface );
static guint      idbmodel_get_interface_version( void );
static guint      idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint      idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint      get_last_version( void );
static gboolean   idbmodel_ddl_update( ofaIDBModel *instance, ofaIGetter *getter, myIProgress *window );
static gboolean   upgrade_to( ofaTvaDBModel *self, sMigration *smig );
static gboolean   exec_query( ofaTvaDBModel *self, const gchar *query );
static gboolean   version_begin( ofaTvaDBModel *self, gint version );
static gboolean   version_end( ofaTvaDBModel *self, gint version );
static gulong     idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );
static gulong     check_forms( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );
static gulong     check_records( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );

G_DEFINE_TYPE_EXTENDED( ofaTvaDBModel, ofa_tva_dbmodel, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaTvaDBModel )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBMODEL, idbmodel_iface_init ))

static void
tva_dbmodel_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DBMODEL( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_dbmodel_parent_class )->finalize( instance );
}

static void
tva_dbmodel_dispose( GObject *instance )
{
	ofaTvaDBModelPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_DBMODEL( instance ));

	priv = ofa_tva_dbmodel_get_instance_private( OFA_TVA_DBMODEL( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_dbmodel_parent_class )->dispose( instance );
}

static void
ofa_tva_dbmodel_init( ofaTvaDBModel *self )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_init";
	ofaTvaDBModelPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DBMODEL( self ));

	priv = ofa_tva_dbmodel_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tva_dbmodel_class_init( ofaTvaDBModelClass *klass )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_dbmodel_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_dbmodel_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( DBMODEL_CANON_NAME ));
}

/*
 * Openbook uses IDBModel MyIIdent interface to pass the current IDBConnect
 *
 * Note that the version number returned here for this plugin must be
 * the last available version number, rather than one read from an opened
 * database.
 */
static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup_printf( "DBMS:%u", get_last_version()));
}

/*
 * #ofaIDBModel interface setup
 */
static void
idbmodel_iface_init( ofaIDBModelInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbmodel_get_interface_version;
	iface->get_current_version = idbmodel_get_current_version;
	iface->get_last_version = idbmodel_get_last_version;
	iface->ddl_update = idbmodel_ddl_update;
	iface->check_dbms_integrity = idbmodel_check_dbms_integrity;
}

/*
 * the version of the #ofaIDBModel interface implemented by the module
 */
static guint
idbmodel_get_interface_version( void )
{
	return( 1 );
}

static guint
idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	gint vcurrent;

	vcurrent = 0;
	ofa_idbconnect_query_int( connect,
			"SELECT MAX(VER_NUMBER) FROM TVA_T_VERSION WHERE VER_DATE > 0", &vcurrent, FALSE );

	return(( guint ) vcurrent );
}

static guint
idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	return( get_last_version());
}

static guint
get_last_version( void )
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
idbmodel_ddl_update( ofaIDBModel *instance, ofaIGetter *getter, myIProgress *window )
{
	ofaTvaDBModelPrivate *priv;
	guint i, cur_version, last_version;
	GtkWidget *label;
	gchar *str;
	gboolean ok;
	ofaHub *hub;

	ok = TRUE;
	priv = ofa_tva_dbmodel_get_instance_private( OFA_TVA_DBMODEL( instance ));
	priv->getter = getter;
	hub = ofa_igetter_get_hub( getter );
	priv->connect = ofa_hub_get_connect( hub );
	priv->window = window;

	cur_version = idbmodel_get_current_version( instance, priv->connect );
	last_version = idbmodel_get_last_version( instance, priv->connect );

	label = gtk_label_new( _( " Updating VAT DB Model " ));
	my_iprogress_start_work( window, instance, label );

	str = g_strdup_printf( _( "Current version is v %u" ), cur_version );
	label = gtk_label_new( str );
	g_free( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	my_iprogress_start_work( window, instance, label );

	if( cur_version < last_version ){
		for( i=0 ; st_migrates[i].ver_target && ok ; ++i ){
			if( cur_version < st_migrates[i].ver_target ){
				if( !upgrade_to( OFA_TVA_DBMODEL( instance ), &st_migrates[i] )){
					str = g_strdup_printf(
								_( "Unable to upgrade current VAT DB model to v %d" ),
								st_migrates[i].ver_target );
					label = gtk_label_new( str );
					g_free( str );
					my_utils_widget_set_margins( label, 0, 0, 2*MARGIN_LEFT, 0 );
					my_style_add( label, "labelerror" );
					gtk_label_set_xalign( GTK_LABEL( label ), 0 );
					my_iprogress_start_progress( priv->window, instance, label, FALSE );
					ok = FALSE;
					break;
				}
			}
		}
	} else {
		str = g_strdup_printf( _( "Last version is v %u : up to date" ), last_version );
		label = gtk_label_new( str );
		g_free( str );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		my_iprogress_start_progress( priv->window, instance, label, FALSE );
	}

	return( ok );
}

/*
 * upgrade the DB model to the specified version
 */
static gboolean
upgrade_to( ofaTvaDBModel *self, sMigration *smig )
{
	ofaTvaDBModelPrivate *priv;
	gboolean ok;
	GtkWidget *label;
	gchar *str;

	priv = ofa_tva_dbmodel_get_instance_private( self );

	str = g_strdup_printf( _( "Upgrading to v %d :" ), smig->ver_target );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_valign( label, GTK_ALIGN_END );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	my_iprogress_start_progress( priv->window, self, label, TRUE );

	priv->total = smig->fncount( self )+3;	/* counting version_begin+version_end */
	priv->current = 0;

	ok = version_begin( self, smig->ver_target ) &&
			smig->fnquery( self, smig->ver_target ) &&
			version_end( self, smig->ver_target );

	my_iprogress_set_ok( priv->window, self, NULL, ok ? 0 : 1 );

	return( ok );
}

static gboolean
exec_query( ofaTvaDBModel *self, const gchar *query )
{
	ofaTvaDBModelPrivate *priv;
	gboolean ok;

	priv = ofa_tva_dbmodel_get_instance_private( self );

	my_iprogress_set_text( priv->window, self, query );

	ok = ofa_idbconnect_query( priv->connect, query, TRUE );

	priv->current += 1;
	my_iprogress_pulse( priv->window, self, priv->current, priv->total );

	return( ok );
}

static gboolean
version_begin( ofaTvaDBModel *self, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'VAT DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'VAT version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO TVA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

static gboolean
version_end( ofaTvaDBModel *self, gint version )
{
	gchar *query;
	gboolean ok;

	/* we do this only at the end of the DB model udpate
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE TVA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

static gboolean
dbmodel_to_v1( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v1";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL UNIQUE COMMENT 'Form mnemonic',"
			"	TFO_LABEL          VARCHAR(80)                  COMMENT 'Form label',"
			"	TFO_NOTES          VARCHAR(4096)                COMMENT 'Notes',"
			"	TFO_UPD_USER       VARCHAR(20)                  COMMENT 'User responsible of last update',"
			"	TFO_UPD_STAMP      TIMESTAMP                    COMMENT 'Last update timestamp')" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS_DET ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_DET_ROW        INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_DET_CODE       VARCHAR(10)                  COMMENT 'Form line code',"
			"	TFO_DET_LABEL      VARCHAR(80)                  COMMENT 'Form line label',"
			"	TFO_DET_HAS_AMOUNT CHAR(1)                      COMMENT 'whether the form line has an amount',"
			"	TFO_DET_AMOUNT     VARCHAR(80)                  COMMENT 'Line amount computing rule',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_DET_ROW))" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v1( ofaTvaDBModel *self )
{
	return( 2 );
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
dbmodel_to_v2( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v2";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS "
			"	ADD    COLUMN TFO_HAS_CORRESPONDENCE CHAR(1)       COMMENT 'Whether this form has a correspondence frame'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_DET_LABEL          VARCHAR(192) COMMENT 'Form line label',"
			"	ADD    COLUMN TFO_DET_HAS_BASE       CHAR(1)      COMMENT 'Whether detail line has a base amount',"
			"	ADD    COLUMN TFO_DET_BASE           VARCHAR(80)  COMMENT 'Detail base',"
			"	ADD    COLUMN TFO_DET_LEVEL          INTEGER      COMMENT 'Detail line level'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS_BOOL ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_BOOL_ROW       INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_BOOL_LABEL     VARCHAR(192)                 COMMENT 'Form line label',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_BOOL_ROW))" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v2( ofaTvaDBModel *self )
{
	return( 3 );
}

/*
 * records the declaration
 */
static gboolean
dbmodel_to_v3( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v3";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_RECORDS ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_LABEL          VARCHAR(80)                  COMMENT 'Form label',"
			"	TFO_HAS_CORRESPONDENCE CHAR(1)                  COMMENT 'Whether this form has a correspondence frame',"
			"	TFO_NOTES          VARCHAR(4096)                COMMENT 'Notes',"
			"	TFO_VALIDATED      CHAR(1)      DEFAULT 'N'     COMMENT 'Whether this declaration is validated',"
			"	TFO_BEGIN          DATE                         COMMENT 'Declaration period begin',"
			"	TFO_END            DATE         NOT NULL        COMMENT 'Declaration period end',"
			"	TFO_UPD_USER       VARCHAR(20)                  COMMENT 'User responsible of last update',"
			"	TFO_UPD_STAMP      TIMESTAMP                    COMMENT 'Last update timestamp',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_END))" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_RECORDS_DET ("
			"	TFO_MNEMO           VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_END             DATE         NOT NULL        COMMENT 'Declaration period end',"
			"	TFO_DET_ROW         INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_DET_LEVEL       INTEGER                      COMMENT 'Detail line level',"
			"	TFO_DET_CODE        VARCHAR(10)                  COMMENT 'Form line code',"
			"	TFO_DET_LABEL       VARCHAR(192)                 COMMENT 'Form line label',"
			"	TFO_DET_HAS_BASE    CHAR(1)                      COMMENT 'Whether detail line has a base amount',"
			"	TFO_DET_BASE_RULE   VARCHAR(80)                  COMMENT 'Detail base computing rule',"
			"	TFO_DET_BASE        DECIMAL(20,5)                COMMENT 'Detail base',"
			"	TFO_DET_HAS_AMOUNT  CHAR(1)                      COMMENT 'whether the form line has an amount',"
			"	TFO_DET_AMOUNT_RULE VARCHAR(80)                  COMMENT 'Line amount computing rule',"
			"	TFO_DET_AMOUNT      DECIMAL(20,5)                COMMENT 'Line amount',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_END,TFO_DET_ROW))" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_RECORDS_BOOL ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL        COMMENT 'Form mnemonic',"
			"	TFO_END            DATE         NOT NULL        COMMENT 'Declaration period end',"
			"	TFO_BOOL_ROW       INTEGER      NOT NULL        COMMENT 'Form line number',"
			"	TFO_BOOL_LABEL     VARCHAR(192)                 COMMENT 'Form line label',"
			"	TFO_BOOL_TRUE      CHAR(1)                      COMMENT 'Whether this boolean is set',"
			"	CONSTRAINT PRIMARY KEY (TFO_MNEMO,TFO_END,TFO_BOOL_ROW))" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v3( ofaTvaDBModel *self )
{
	return( 3 );
}

/*
 * resize identifiers and labels
 */
static gboolean
dbmodel_to_v4( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v4";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_LABEL           VARCHAR(256)                          COMMENT 'Form label',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_BOOL_LABEL      VARCHAR(256)                          COMMENT 'Form line label'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_DET_CODE        VARCHAR(64)                           COMMENT 'Detail line code',"
			"	MODIFY COLUMN TFO_DET_LABEL       VARCHAR(256)                          COMMENT 'Detail line label',"
			"	MODIFY COLUMN TFO_DET_BASE        VARCHAR(128)                          COMMENT 'Detail base computing rule',"
			"	MODIFY COLUMN TFO_DET_AMOUNT      VARCHAR(128)                          COMMENT 'Detail amount computing rule'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_LABEL           VARCHAR(256)                          COMMENT 'Form label',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_BOOL_LABEL      VARCHAR(256)                          COMMENT 'Form line label'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_DET "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_DET_CODE        VARCHAR(64)                           COMMENT 'Detail line code',"
			"	MODIFY COLUMN TFO_DET_LABEL       VARCHAR(256)                          COMMENT 'Detail line label',"
			"	MODIFY COLUMN TFO_DET_BASE_RULE   VARCHAR(128)                          COMMENT 'Detail base computing rule',"
			"	MODIFY COLUMN TFO_DET_AMOUNT_RULE VARCHAR(128)                          COMMENT 'Detail amount computing rule'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v4( ofaTvaDBModel *self )
{
	return( 6 );
}

/*
 * resize rules
 */
static gboolean
dbmodel_to_v5( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v5";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_DET_CODE        VARCHAR(64)                           COMMENT 'Detail line code',"
			"	MODIFY COLUMN TFO_DET_BASE        VARCHAR(256)                          COMMENT 'Detail base computing rule',"
			"	MODIFY COLUMN TFO_DET_AMOUNT      VARCHAR(256)                          COMMENT 'Detail amount computing rule'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_DET "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_DET_CODE        VARCHAR(64)                           COMMENT 'Detail line code',"
			"	MODIFY COLUMN TFO_DET_BASE_RULE   VARCHAR(256)                          COMMENT 'Detail base computing rule',"
			"	MODIFY COLUMN TFO_DET_AMOUNT_RULE VARCHAR(256)                          COMMENT 'Detail amount computing rule'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v5( ofaTvaDBModel *self )
{
	return( 6 );
}

/*
 * Define operation template rules
 */
static gboolean
dbmodel_to_v6( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v6";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	ADD    COLUMN TFO_DET_HAS_TEMPLATE CHAR(1)                              COMMENT 'Has operation template',"
			"	ADD    COLUMN TFO_DET_TEMPLATE     VARCHAR(64)                          COMMENT 'Operation template'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS "
			"	DROP   COLUMN TFO_LABEL,"
			"	DROP   COLUMN TFO_HAS_CORRESPONDENCE,"
			"	ADD    COLUMN TFO_CORRESPONDENCE   VARCHAR(4096)                        COMMENT 'Correspondence',"
			"	ADD    COLUMN TFO_DOPE             DATE                                 COMMENT 'Validation operation date'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_BOOL "
			"	DROP   COLUMN TFO_BOOL_LABEL" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE TVA_T_RECORDS_DET "
			"	DROP   COLUMN TFO_DET_LEVEL,"
			"	DROP   COLUMN TFO_DET_CODE,"
			"	DROP   COLUMN TFO_DET_LABEL,"
			"	DROP   COLUMN TFO_DET_HAS_BASE,"
			"	DROP   COLUMN TFO_DET_BASE_RULE,"
			"	DROP   COLUMN TFO_DET_HAS_AMOUNT,"
			"	DROP   COLUMN TFO_DET_AMOUNT_RULE,"
			"	ADD    COLUMN TFO_DET_OPE_NUMBER   BIGINT                               COMMENT 'Generated operation number'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v6( ofaTvaDBModel *self )
{
	return( 4 );
}

/*
 * Define Documents index
 */
static gboolean
dbmodel_to_v7( ofaTvaDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v7";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* 1. create Records documents index */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_RECORDS_DOC ("
			"	TFO_MNEMO           VARCHAR(64) BINARY NOT NULL      COMMENT 'VAT record identifier',"
			"	TFO_END             DATE               NOT NULL      COMMENT 'VAT record date',"
			"	TFO_DOC_ID          BIGINT             NOT NULL      COMMENT 'Document identifier',"
			"	UNIQUE (TFO_MNEMO,TFO_END,TFO_DOC_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 2. create Form documents index */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS_DOC ("
			"	TFO_MNEMO           VARCHAR(64) BINARY NOT NULL      COMMENT 'VAT form identifier',"
			"	TFO_DOC_ID          BIGINT             NOT NULL      COMMENT 'Document identifier',"
			"	UNIQUE (TFO_MNEMO,TFO_DOC_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v7( ofaTvaDBModel *self )
{
	return( 2 );
}

/*
 * Cannot fully check VAT forms integrity without interpreting the
 * computing rules.
 * Should at least check for operation template(s)..
 */
static gulong
idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	gulong errs;

	errs = 0;
	errs += check_forms( instance, getter, progress );
	errs += check_records( instance, getter, progress );

	return( errs );
}

static gulong
check_forms( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	gulong errs, objerrs;
	void *worker;
	GtkWidget *label;
	GList *records, *it, *orphans, *ito;
	gulong count, i;
	gchar *str;
	const gchar *mnemo, *template;
	ofoTVAForm *form_obj;
	ofxCounter docid;
	guint det_count, idet;
	gboolean has_template;
	ofoOpeTemplate *template_obj;

	worker = GUINT_TO_POINTER( OFO_TYPE_TVA_FORM );

	if( progress ){
		label = gtk_label_new( _( " Check for VAT forms integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_tva_form_get_dataset( getter );
	count = 3 + 2*g_list_length( records );
	i = 0;

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( it=records ; it ; it=it->next ){
		form_obj = OFO_TVA_FORM( it->data );
		mnemo = ofo_tva_form_get_mnemo( form_obj );
		det_count = ofo_tva_form_detail_get_count( form_obj );
		objerrs = 0;

		/* check for operation template in detail lines */
		for( idet=0 ; idet<det_count ; ++idet ){
			has_template = ofo_tva_form_detail_get_has_template( form_obj, idet );
			template = ofo_tva_form_detail_get_template( form_obj, idet );

			if( !has_template && my_strlen( template )){
				if( progress ){
					str = g_strdup_printf(
							_( "VAT form %s, detail %u, is said to not have template, but template %s is set" ),
							mnemo, idet, template );
					my_iprogress_set_text( progress, worker, str );
					g_free( str );
				}
				errs += 1;
				objerrs += 1;

			} else if( has_template && !my_strlen( template )){
				if( progress ){
					str = g_strdup_printf(
							_( "VAT form %s, detail %u, is said to have template, but template is not set" ),
							mnemo, idet );
					my_iprogress_set_text( progress, worker, str );
					g_free( str );
				}
				errs += 1;
				objerrs += 1;

			} else if( has_template && my_strlen( template )){
				template_obj = ofo_ope_template_get_by_mnemo( getter, template );
				if( !template_obj || !OFO_IS_OPE_TEMPLATE( template_obj )){
					if( progress ){
						str = g_strdup_printf(
								_( "VAT form %s, detail %u, has operation template '%s' which doesn't exist" ),
								mnemo, idet, template );
						my_iprogress_set_text( progress, worker, str );
						g_free( str );
					}
					errs += 1;
					objerrs += 1;
				}
			}
			if( progress ){
				my_iprogress_pulse( progress, worker, ++i, count );
			}

			/* unable to check for accounts and rates without evaluating
			 * the formulas */
		}

		/* check for referenced documents which actually do not exist */
		orphans = ofa_idoc_get_orphans( OFA_IDOC( form_obj ));
		if( g_list_length( orphans ) > 0 ){
			for( ito=orphans ; ito ; ito=ito->next ){
				docid = ( ofxCounter ) ito->data;
				if( progress ){
					str = g_strdup_printf( _( "Found orphan document(s) with DocId %lu" ), docid );
					my_iprogress_set_text( progress, worker, str );
					g_free( str );
				}
				errs += 1;
				objerrs += 1;
			}
		}
		ofa_idoc_free_orphans( orphans );
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		if( objerrs == 0 && progress ){
			str = g_strdup_printf( _( "VAT form %s does not exhibit any error: OK" ), mnemo );
			my_iprogress_set_text( progress, worker, str );
			g_free( str );
		}
	}

	/* check that all booleans have a form parent */
	orphans = ofo_tva_form_get_bool_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan boolean(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT form boolean found: OK" ));
	}
	ofo_tva_form_free_bool_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* check that all details have a form parent */
	orphans = ofo_tva_form_get_det_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan detail(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT form found: OK" ));
	}
	ofo_tva_form_free_det_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* check that all documents have a form parent */
	orphans = ofo_tva_form_get_doc_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan document(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT form document found: OK" ));
	}
	ofo_tva_form_free_doc_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* progress end */
	if( progress ){
		my_iprogress_set_text( progress, worker, "" );
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}

static gulong
check_records( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	gulong errs, objerrs;
	void *worker;
	GtkWidget *label;
	GList *records, *it, *orphans, *ito;
	gulong count, i;
	gchar *str, *sdate;
	const gchar *mnemo;
	ofoTVARecord *record;
	ofoTVAForm *form_obj;
	ofxCounter docid;

	worker = GUINT_TO_POINTER( OFO_TYPE_TVA_FORM );

	if( progress ){
		label = gtk_label_new( _( " Check for VAT records integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_tva_record_get_dataset( getter );
	count = 3 + 2*g_list_length( records );
	i = 0;

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( it=records ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		mnemo = ofo_tva_record_get_mnemo( record );
		sdate = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
		objerrs = 0;

		/* check that form exists */
		form_obj = ofo_tva_form_get_by_mnemo( getter, mnemo );
		if( !form_obj || !OFO_IS_TVA_FORM( form_obj )){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan VAT record(s) with TfoMnemo %s" ), mnemo );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
			objerrs += 1;
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* check for referenced documents which actually do not exist */
		orphans = ofa_idoc_get_orphans( OFA_IDOC( record ));
		if( g_list_length( orphans ) > 0 ){
			for( ito=orphans ; ito ; ito=ito->next ){
				docid = ( ofxCounter ) ito->data;
				if( progress ){
					str = g_strdup_printf( _( "Found orphan document(s) with DocId %lu" ), docid );
					my_iprogress_set_text( progress, worker, str );
					g_free( str );
				}
				errs += 1;
				objerrs += 1;
			}
		}
		ofa_idoc_free_orphans( orphans );
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		if( objerrs == 0 && progress ){
			str = g_strdup_printf( _( "VAT record %s-%s does not exhibit any error: OK" ), mnemo, sdate );
			my_iprogress_set_text( progress, worker, str );
			g_free( str );
		}

		g_free( sdate );
	}

	/* check that all booleans have a record parent */
	orphans = ofo_tva_record_get_bool_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan boolean(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT record boolean found: OK" ));
	}
	ofo_tva_record_free_bool_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* check that all details have a record parent */
	orphans = ofo_tva_record_get_det_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan detail(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT record found: OK" ));
	}
	ofo_tva_record_free_det_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* check that all documents have a record parent */
	orphans = ofo_tva_record_get_doc_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan document(s) with TfoMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}
	} else {
		my_iprogress_set_text( progress, worker, _( "No orphan VAT record document found: OK" ));
	}
	ofo_tva_record_free_doc_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* progress end */
	if( progress ){
		my_iprogress_set_text( progress, worker, "" );
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}
