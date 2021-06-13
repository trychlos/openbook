/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-period.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-ope-template.h"

#include "ofa-recurrent-dbmodel.h"
#include "ofo-recurrent-gen.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

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
	ofaRecurrentDBModelPrivate;

#define DBMODEL_CANON_NAME               "REC"

/**
 * Periodicity identifier
 * This is an invariant which identifies the periodicity object.
 * This cannot be fully configurable as the #ofo_rec_period_enum_between()
 * method must know how to deal with each periodicity.
 * Used in v7-v10 models.
 */
#define REC_PERIOD_MONTHLY             "MONTHLY"
#define REC_PERIOD_NEVER               "NEVER"
#define REC_PERIOD_WEEKLY              "WEEKLY"

/* the functions which update the DB model
 */
static gboolean dbmodel_to_v1( ofaRecurrentDBModel *self, guint version );
static gulong   count_v1( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v2( ofaRecurrentDBModel *self, guint version );
static gulong   count_v2( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v3( ofaRecurrentDBModel *self, guint version );
static gulong   count_v3( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v4( ofaRecurrentDBModel *self, guint version );
static gulong   count_v4( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v5( ofaRecurrentDBModel *self, guint version );
static gulong   count_v5( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v6( ofaRecurrentDBModel *self, guint version );
static gulong   count_v6( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v7( ofaRecurrentDBModel *self, guint version );
static gulong   count_v7( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v8( ofaRecurrentDBModel *self, guint version );
static gulong   count_v8( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v9( ofaRecurrentDBModel *self, guint version );
static gulong   count_v9( ofaRecurrentDBModel *self );
static gboolean dbmodel_to_v10( ofaRecurrentDBModel *self, guint version );
static gulong   count_v10( ofaRecurrentDBModel *self );

typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( ofaRecurrentDBModel *self, guint version );
	gulong   ( *fncount )( ofaRecurrentDBModel *self );
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
		{ 8, dbmodel_to_v8, count_v8 },
		{ 9, dbmodel_to_v9, count_v9 },
		{ 10, dbmodel_to_v10, count_v10 },
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
static gboolean   upgrade_to( ofaRecurrentDBModel *self, sMigration *smig );
static gboolean   exec_query( ofaRecurrentDBModel *self, const gchar *query );
static gboolean   version_begin( ofaRecurrentDBModel *self, gint version );
static gboolean   version_end( ofaRecurrentDBModel *self, gint version );
static gulong     idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );
static gulong     check_model( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );
static gulong     check_run( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentDBModel, ofa_recurrent_dbmodel, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaRecurrentDBModel )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBMODEL, idbmodel_iface_init ))

static void
recurrent_dbmodel_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_DBMODEL( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_dbmodel_parent_class )->finalize( instance );
}

static void
recurrent_dbmodel_dispose( GObject *instance )
{
	ofaRecurrentDBModelPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_DBMODEL( instance ));

	priv = ofa_recurrent_dbmodel_get_instance_private( OFA_RECURRENT_DBMODEL( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_dbmodel_parent_class )->dispose( instance );
}

static void
ofa_recurrent_dbmodel_init( ofaRecurrentDBModel *self )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_init";
	ofaRecurrentDBModelPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_DBMODEL( self ));

	priv = ofa_recurrent_dbmodel_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_recurrent_dbmodel_class_init( ofaRecurrentDBModelClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_dbmodel_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_dbmodel_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_iident_iface_init";

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
	return( g_strdup_printf( "%s:%u", DBMODEL_CANON_NAME, get_last_version()));
}

/*
 * #ofaIDBModel interface setup
 */
static void
idbmodel_iface_init( ofaIDBModelInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_iface_init";

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
			"SELECT MAX(VER_NUMBER) FROM REC_T_VERSION WHERE VER_DATE > 0", &vcurrent, FALSE );

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
	ofaRecurrentDBModelPrivate *priv;
	guint i, cur_version, last_version;
	GtkWidget *label;
	gchar *str;
	gboolean ok;
	ofaHub *hub;

	ok = TRUE;
	priv = ofa_recurrent_dbmodel_get_instance_private( OFA_RECURRENT_DBMODEL( instance ));
	priv->getter = getter;
	hub = ofa_igetter_get_hub( getter );
	priv->connect = ofa_hub_get_connect( hub );
	priv->window = window;

	cur_version = idbmodel_get_current_version( instance, priv->connect );
	last_version = idbmodel_get_last_version( instance, priv->connect );

	label = gtk_label_new( _( " Updating Recurrent DB Model " ));
	my_iprogress_start_work( window, instance, label );

	str = g_strdup_printf( _( "Current version is v %u" ), cur_version );
	label = gtk_label_new( str );
	g_free( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	my_iprogress_start_work( window, instance, label );

	if( cur_version < last_version ){
		for( i=0 ; st_migrates[i].ver_target && ok ; ++i ){
			if( cur_version < st_migrates[i].ver_target ){
				if( !upgrade_to( OFA_RECURRENT_DBMODEL( instance ), &st_migrates[i] )){
					str = g_strdup_printf(
								_( "Unable to upgrade current Recurrent DB model to v %d" ),
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
upgrade_to( ofaRecurrentDBModel *self, sMigration *smig )
{
	ofaRecurrentDBModelPrivate *priv;
	gboolean ok;
	GtkWidget *label;
	gchar *str;

	priv = ofa_recurrent_dbmodel_get_instance_private( self );

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
exec_query( ofaRecurrentDBModel *self, const gchar *query )
{
	ofaRecurrentDBModelPrivate *priv;
	gboolean ok;

	priv = ofa_recurrent_dbmodel_get_instance_private( self );

	my_iprogress_set_text( priv->window, self, MY_PROGRESS_NONE, query );

	ok = ofa_idbconnect_query( priv->connect, query, TRUE );

	priv->current += 1;
	my_iprogress_pulse( priv->window, self, priv->current, priv->total );

	return( ok );
}

static gboolean
version_begin( ofaRecurrentDBModel *self, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'Recurrent DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'Recurrent version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO REC_T_VERSION (VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

static gboolean
version_end( ofaRecurrentDBModel *self, gint version )
{
	gchar *query;
	gboolean ok;

	/* we do this only at the end of the DB model udpate
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE REC_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

static gboolean
dbmodel_to_v1( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v1";
	gchar *str;

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* updated in v4 */
	/* altered in v7 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_GEN ("
			"	REC_ID             INTEGER      NOT NULL UNIQUE        COMMENT 'Unique identifier',"
			"	REC_LAST_RUN       DATE                                COMMENT 'Last recurrent operations generation date')"
			" CHARACTER SET utf8" )){
		return( FALSE );
	}

	str = g_strdup_printf( "INSERT IGNORE INTO REC_T_GEN (REC_ID,REC_LAST_RUN) VALUES (%d,NULL)", RECURRENT_ROW_ID );
	if( !exec_query( self, str )){
		return( FALSE );
	}

	/* updated in v2 */
	/* updated in v5 */
	/* altered in v6 */
	/* rec_period_detail modified in v8 */
	/* creation and enabled user and timestamp added in v10 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_MODELS ("
			"	REC_MNEMO          VARCHAR(64)  BINARY NOT NULL UNIQUE COMMENT 'Recurrent operation identifier',"
			"	REC_LABEL          VARCHAR(256)                        COMMENT 'Recurrent operation label',"
			"	REC_OPE_TEMPLATE   VARCHAR(64)                         COMMENT 'Operation template identifier',"
			"	REC_PERIOD         CHAR(1)                             COMMENT 'Periodicity',"
			"	REC_PERIOD_DETAIL  VARCHAR(128)                        COMMENT 'Periodicity detail',"
			"	REC_NOTES          VARCHAR(4096)                       COMMENT 'Notes',"
			"	REC_UPD_USER       VARCHAR(64)                         COMMENT 'User responsible of last update',"
			"	REC_UPD_STAMP      TIMESTAMP                           COMMENT 'Last update timestamp')"
			" CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* updated in v2 */
	/* updated in v3 */
	/* updated in v4 */
	/* creation and status user and timestamp added in v10 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_RUN ("
			"	REC_MNEMO          VARCHAR(64)  BINARY NOT NULL        COMMENT 'Recurrent operation identifier',"
			"	REC_DATE           DATE                NOT NULL        COMMENT 'Operation date',"
			"	REC_STATUS         CHAR(1)                             COMMENT 'Operation status',"
			"	REC_UPD_USER       VARCHAR(64)                         COMMENT 'User responsible of last update',"
			"	REC_UPD_STAMP      TIMESTAMP                           COMMENT 'Last update timestamp',"
			" CONSTRAINT PRIMARY KEY( REC_MNEMO,REC_DATE ))"
			" CHARACTER SET utf8" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v1( ofaRecurrentDBModel *self )
{
	return( 4 );
}

/*
 * display three amounts in the model, letting the user edit them
 */
static gboolean
dbmodel_to_v2( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v2";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	ADD COLUMN REC_DEF_AMOUNT1    VARCHAR(64)              COMMENT 'Definition of amount n° 1',"
			"	ADD COLUMN REC_DEF_AMOUNT2    VARCHAR(64)              COMMENT 'Definition of amount n° 2',"
			"	ADD COLUMN REC_DEF_AMOUNT3    VARCHAR(64)              COMMENT 'Definition of amount n° 3'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE REC_T_RUN "
			"	ADD COLUMN REC_AMOUNT1        DECIMAL(20,5)            COMMENT 'Amount n° 1',"
			"	ADD COLUMN REC_AMOUNT2        DECIMAL(20,5)            COMMENT 'Amount n° 2',"
			"	ADD COLUMN REC_AMOUNT3        DECIMAL(20,5)            COMMENT 'Amount n° 3'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v2( ofaRecurrentDBModel *self )
{
	return( 2 );
}

/*
 * Review REC_T_RUN index:
 *   for a same mnemo+date couple, may have several Cancelled, only one
 *   Waiting|Validated - this is controlled by the code.
 */
static gboolean
dbmodel_to_v3( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v3";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE REC_T_RUN "
			"	DROP PRIMARY KEY" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE REC_T_RUN "
			"	ADD COLUMN REC_NUMSEQ         BIGINT NOT NULL UNIQUE AUTO_INCREMENT COMMENT 'Automatic sequence number'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v3( ofaRecurrentDBModel *self )
{
	return( 2 );
}

/*
 * REC_T_GEN: maintain last NUMSEQ
 */
static gboolean
dbmodel_to_v4( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v4";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE REC_T_GEN "
			"	ADD COLUMN REC_LAST_NUMSEQ    BIGINT                                COMMENT 'Last sequence number'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"UPDATE REC_T_GEN "
			"	SET REC_LAST_NUMSEQ=(SELECT MAX(REC_NUMSEQ) FROM REC_T_RUN)" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v4( ofaRecurrentDBModel *self )
{
	return( 2 );
}

/*
 * REC_T_MODEL: enable/disable the model
 */
static gboolean
dbmodel_to_v5( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v5";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	ADD COLUMN REC_ENABLED        CHAR(1)                               COMMENT 'Whether the model is enabled'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"UPDATE REC_T_MODELS SET REC_ENABLED='Y'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v5( ofaRecurrentDBModel *self )
{
	return( 2 );
}

/*
 * REC_T_PERIODS: configure the periodicity per table
 */
static gboolean
dbmodel_to_v6( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v6";
	ofaRecurrentDBModelPrivate *priv;
	gchar *query;
	gboolean ok;
	const gchar *userid;

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	priv = ofa_recurrent_dbmodel_get_instance_private( self );
	userid = ofa_idbconnect_get_account( priv->connect );

	/* 1 - create Periodicity table */
	/* altered in v7 */
	/* removed in v10 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_PERIODS ("
			"	REC_PER_ID          VARCHAR(16)    BINARY NOT NULL   COMMENT 'Periodicity identifier',"
			"	REC_PER_LABEL       VARCHAR(256)                     COMMENT 'Periodicity label',"
			"	REC_PER_HAVE_DETAIL CHAR(1)                          COMMENT 'Whether have detail',"
			"	REC_PER_ADD_TYPE    CHAR(1)                          COMMENT 'Increment type',"
			"	REC_PER_ADD_COUNT   INTEGER                          COMMENT 'Increment count',"
			"	REC_PER_NOTES       VARCHAR(4096)                    COMMENT 'Notes',"
			"	REC_PER_UPD_USER    VARCHAR(64)                      COMMENT 'Last update user',"
			"	REC_PER_UPD_STAMP   TIMESTAMP                        COMMENT 'Last update timestamp',"
			"	UNIQUE (REC_PER_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 2 - create Periodicity Details table */
	/* altered in v7 */
	/* removed in v10 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_PERIODS_DET ("
			"	REC_PER_ID          VARCHAR(16)    BINARY NOT NULL   COMMENT 'Periodicity identifier',"
			"	REC_PER_DET_ID      VARCHAR(16)                      COMMENT 'Periodicity detail identifier',"
			"	REC_PER_DET_LABEL   VARCHAR(256)                     COMMENT 'Periodicity detail label',"
			"	UNIQUE (REC_PER_ID, REC_PER_DET_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 3 - update Models description */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	MODIFY COLUMN REC_PERIOD         VARCHAR(16)         COMMENT 'Recurrent model periodicity',"
			"	MODIFY COLUMN REC_PERIOD_DETAIL  VARCHAR(16)         COMMENT 'Recurrent model periodicity detail'")){
		return( FALSE );
	}

	/* 4 - initialize periodicity
	 * values are those used in the code for now - they cannot be anything */
	query = g_strdup_printf(
			"INSERT IGNORE INTO REC_T_PERIODS "
			"		(REC_PER_ID,REC_PER_LABEL,REC_PER_HAVE_DETAIL,REC_PER_ADD_TYPE,REC_PER_ADD_COUNT,REC_PER_UPD_USER) "
			"		VALUES "
			"	('0N','Never','N',NULL,NULL,'%s'),"
			"	('3W','Weekly','Y','D',7,'%s'),"
			"	('6M','Monthly','Y','M',1,'%s')", userid, userid, userid );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 5 - initialize periodicity details */
	if( !exec_query( self,
			"INSERT IGNORE INTO REC_T_PERIODS_DET (REC_PER_ID,REC_PER_DET_ID,REC_PER_DET_LABEL) VALUES "
			"	('3W','0MON','Monday'),"
			"	('3W','1TUE','Tuesday'),"
			"	('3W','2WED','Wednesday'),"
			"	('3W','3THU','Thursday'),"
			"	('3W','4FRI','Friday'),"
			"	('3W','5SAT','Saturday'),"
			"	('3W','6SUN','Sunday'),"
			"	('6M','01','1'),"
			"	('6M','02','2'),"
			"	('6M','03','3'),"
			"	('6M','04','4'),"
			"	('6M','05','5'),"
			"	('6M','06','6'),"
			"	('6M','07','7'),"
			"	('6M','08','8'),"
			"	('6M','09','9'),"
			"	('6M','10','10'),"
			"	('6M','11','11'),"
			"	('6M','12','12'),"
			"	('6M','13','13'),"
			"	('6M','14','14'),"
			"	('6M','15','15'),"
			"	('6M','16','16'),"
			"	('6M','17','17'),"
			"	('6M','18','18'),"
			"	('6M','19','19'),"
			"	('6M','20','20'),"
			"	('6M','21','21'),"
			"	('6M','22','22'),"
			"	('6M','23','23'),"
			"	('6M','24','24'),"
			"	('6M','25','25'),"
			"	('6M','26','26'),"
			"	('6M','27','27'),"
			"	('6M','28','28'),"
			"	('6M','29','29'),"
			"	('6M','30','30'),"
			"	('6M','31','31')" )){
		return( FALSE );
	}

	/* 6 - update current models periodicity */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD='0N' WHERE REC_PERIOD='N'" )){
		return( FALSE );
	}

	/* 7 - update current models periodicity */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD='3W' WHERE REC_PERIOD='W'" )){
		return( FALSE );
	}

	/* 8 - update current models periodicity */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
		"		SET REC_PERIOD='6M' WHERE REC_PERIOD='M'" )){
		return( FALSE );
	}

	/* 9 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='0MON' WHERE REC_PERIOD_DETAIL='MON'" )){
		return( FALSE );
	}

	/* 10 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='1TUE' WHERE REC_PERIOD_DETAIL='TUE'" )){
		return( FALSE );
	}

	/* 11 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='2WED' WHERE REC_PERIOD_DETAIL='WED'" )){
		return( FALSE );
	}

	/* 12 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='3THU' WHERE REC_PERIOD_DETAIL='THU'" )){
		return( FALSE );
	}

	/* 13 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='4FRI' WHERE REC_PERIOD_DETAIL='FRI'" )){
		return( FALSE );
	}

	/* 14 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='5SAT' WHERE REC_PERIOD_DETAIL='SAT'" )){
		return( FALSE );
	}

	/* 15 - update current models periodicity weekly details */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL='6SUN' WHERE REC_PERIOD_DETAIL='SUN'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v6( ofaRecurrentDBModel *self )
{
	return( 15 );
}

/*
 * REC_T_PERIODS: have numeric identifiers
 */
static gboolean
dbmodel_to_v7( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v7";
	gchar *query;
	gboolean ok;

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* 1 - update GEN table */
	if( !exec_query( self,
			"ALTER TABLE REC_T_GEN "
			"	ADD    COLUMN REC_LAST_PER_DET_ID      BIGINT DEFAULT 0 COMMENT 'Last periodicity detail identifier'" )){
		return( FALSE );
	}

	/* 2 - update Periodicity table
	 *     REC_PER_DETAILS_COUNT removed in v9 */
	if( !exec_query( self,
			"ALTER TABLE REC_T_PERIODS "
			"	DROP   COLUMN REC_PER_HAVE_DETAIL,"
			"	DROP   COLUMN REC_PER_ADD_TYPE,"
			"	DROP   COLUMN REC_PER_ADD_COUNT,"
			"	ADD    COLUMN REC_PER_ORDER            INTEGER           COMMENT 'Periodicity display order',"
			"	ADD    COLUMN REC_PER_DETAILS_COUNT    INTEGER           COMMENT 'Count of detail types'" )){
		return( FALSE );
	}

	/* 3 - update Periodicity Details table */
	if( !exec_query( self,
			"ALTER TABLE REC_T_PERIODS_DET "
			"	CHANGE COLUMN REC_PER_DET_ID REC_PER_DET_ID0 VARCHAR(16),"
			"	ADD    COLUMN REC_PER_DET_ID                 BIGINT  NOT NULL  COMMENT 'Periodicity detail identifier',"
			"	ADD    COLUMN REC_PER_DET_ORDER              INTEGER           COMMENT 'Periodicity detail display order',"
			"	ADD    COLUMN REC_PER_DET_NUMBER             INTEGER           COMMENT 'Periodicity detail type number',"
			"	ADD    COLUMN REC_PER_DET_VALUE              INTEGER           COMMENT 'Periodicity detail value',"
			"	DROP KEY REC_PER_ID" )){
		return( FALSE );
	}

	/* 4 - update Periodicity table */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS SET "
			"	REC_PER_ID='%s',"
			"	REC_PER_ORDER=10,"
			"	REC_PER_DETAILS_COUNT=0 WHERE REC_PER_ID='0N'", REC_PERIOD_NEVER );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 5 - update Periodicity table */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS SET "
			"	REC_PER_ID='%s',"
			"	REC_PER_ORDER=20,"
			"	REC_PER_DETAILS_COUNT=1 WHERE REC_PER_ID='3W'", REC_PERIOD_WEEKLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 6 - update Periodicity table */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS SET "
			"	REC_PER_ID='%s',"
			"	REC_PER_ORDER=30,"
			"	REC_PER_DETAILS_COUNT=1 WHERE REC_PER_ID='6M'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 7 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=1,"
			"	REC_PER_DET_ORDER=0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='0MON'",
			REC_PERIOD_WEEKLY, G_DATE_MONDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 8 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=2,"
			"	REC_PER_DET_ORDER=1,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='1TUE'",
			REC_PERIOD_WEEKLY, G_DATE_TUESDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 9 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=3,"
			"	REC_PER_DET_ORDER=2,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='2WED'",
			REC_PERIOD_WEEKLY, G_DATE_WEDNESDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 10 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=4,"
			"REC_PER_DET_ORDER=3,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='3THU'",
			REC_PERIOD_WEEKLY, G_DATE_THURSDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 11 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=5,"
			"REC_PER_DET_ORDER=4,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='4FRI'",
			REC_PERIOD_WEEKLY, G_DATE_FRIDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 12 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=6,"
			"REC_PER_DET_ORDER=5,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='5SAT'",
			REC_PERIOD_WEEKLY, G_DATE_SATURDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 13 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=7,"
			"REC_PER_DET_ORDER=6,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=%d WHERE REC_PER_DET_ID0='6SUN'",
			REC_PERIOD_WEEKLY, G_DATE_SUNDAY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 14 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=8,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='01'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 15 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=9,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='02'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 16 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=10,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='03'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 17 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=11,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='04'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 18 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=12,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='05'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 19 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=13,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='06'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 20 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=14,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='07'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 21 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=15,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='08'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 22 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=16,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='09'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 23 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=17,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='10'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 24 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=18,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='11'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 25 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=19,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='12'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 26 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=20,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='13'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 27 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=21,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='14'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 28 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=22,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='15'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 29 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=23,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='16'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 30 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=24,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='17'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 31 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=25,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='18'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 32 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=26,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='19'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 33 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=27,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='20'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 34 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=28,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='21'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 35 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=29,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='22'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 36 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=30,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='23'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 37 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=31,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='24'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 38 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=32,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='25'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 39 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=33,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='26'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 40 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=34,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='27'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 41 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=35,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='28'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 42 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=36,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='29'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 43 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=37,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='30'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 44 - update Periodicity details */
	query = g_strdup_printf(
			"UPDATE REC_T_PERIODS_DET "
			"	SET REC_PER_ID='%s',REC_PER_DET_ID=38,"
			"	REC_PER_DET_ORDER=0+REC_PER_DET_ID0,REC_PER_DET_NUMBER=0,REC_PER_DET_VALUE=0+REC_PER_DET_ID0"
			"	WHERE REC_PER_DET_ID0='31'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 45 - update GEN table */
	query = g_strdup_printf( "UPDATE REC_T_GEN SET REC_LAST_PER_DET_ID=38 WHERE REC_ID=%u", RECURRENT_ROW_ID );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 46 - update Periodicity Details table */
	if( !exec_query( self,
			"ALTER TABLE REC_T_PERIODS_DET "
			"	DROP   COLUMN REC_PER_DET_ID0,"
			"	ADD UNIQUE KEY PERID_IX (REC_PER_DET_ID)" )){
		return( FALSE );
	}

	/* 47 - update Recurrent models */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	CHANGE COLUMN REC_PERIOD_DETAIL REC_PERIOD_DET0   VARCHAR(16),"
			"	ADD    COLUMN REC_PERIOD_DETAIL BIGINT  NOT NULL  COMMENT 'Periodicity detail identifier'" )){
		return( FALSE );
	}

	/* 48 - update current models periodicity */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS SET REC_PERIOD='%s' WHERE REC_PERIOD='0N'", REC_PERIOD_NEVER );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 49 - update current models periodicity */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS SET REC_PERIOD='%s' WHERE REC_PERIOD='3W'", REC_PERIOD_WEEKLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 50 - update current models periodicity */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS SET REC_PERIOD='%s' WHERE REC_PERIOD='6M'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 51 - update current models periodicity weekly details */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL=LEFT(REC_PERIOD_DET0,1)-1 WHERE REC_PERIOD='%s'", REC_PERIOD_WEEKLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 52 - update current models periodicity monthly details */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS "
			"	SET REC_PERIOD_DETAIL=REC_PERIOD_DET0+7 WHERE REC_PERIOD='%s'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* 55 - update current models */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	DROP COLUMN REC_PERIOD_DET0" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v7( ofaRecurrentDBModel *self )
{
	return( 53 );
}

/*
 * Define documents index
 */
static gboolean
dbmodel_to_v8( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v8";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* 1. create Models documents index */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_MODELS_DOC ("
			"	REC_MNEMO           VARCHAR(64) BINARY NOT NULL      COMMENT 'Recurrent model identifier',"
			"	REC_DOC_ID          BIGINT             NOT NULL      COMMENT 'Document identifier',"
			"	UNIQUE (REC_MNEMO,REC_DOC_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 2. create Run documents index */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_RUN_DOC ("
			"	REC_NUMSEQ          BIGINT             NOT NULL      COMMENT 'Recurrent run identifier',"
			"	REC_DOC_ID          BIGINT             NOT NULL      COMMENT 'Document identifier',"
			"	UNIQUE (REC_NUMSEQ,REC_DOC_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 3. create Periodicity documents index */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS REC_T_PERIODS_DOC ("
			"	REC_PER_ID          VARCHAR(16) BINARY NOT NULL      COMMENT 'Periodicity identifier',"
			"	REC_DOC_ID          BIGINT             NOT NULL      COMMENT 'Document identifier',"
			"	UNIQUE (REC_PER_ID,REC_DOC_ID)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* 4. modifiy rec_period_detail */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	MODIFY COLUMN REC_PERIOD_DETAIL BIGINT               COMMENT 'Periodicity detail identifier'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v8( ofaRecurrentDBModel *self )
{
	return( 4 );
}

/*
 * Remove REC_PER_DETAILS_COUNT column
 */
static gboolean
dbmodel_to_v9( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v9";

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* 1. create Models documents index */
	if( !exec_query( self,
			"ALTER TABLE REC_T_PERIODS "
			"	DROP   COLUMN REC_PER_DETAILS_COUNT" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v9( ofaRecurrentDBModel *self )
{
	return( 1 );
}

/*
 * - Disable TIMESTAMP auto-update by adding 'DEFAULT 0' option
 *   cf. https://mariadb.com/kb/en/library/timestamp/
 * - Add creation, status, amounts audit trace
 */
static gboolean
dbmodel_to_v10( ofaRecurrentDBModel *self, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v10";
	gchar *query;
	gboolean ok;

	g_debug( "%s: self=%p, version=%u", thisfn, ( void * ) self, version );

	/* 1 */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	ADD    COLUMN REC_CRE_USER      VARCHAR(64)  NOT NULL    COMMENT 'Creation user',"
			"	ADD    COLUMN REC_CRE_STAMP     TIMESTAMP    DEFAULT 0   COMMENT 'Creation timestamp',"
			"	ADD    COLUMN REC_END           DATE                     COMMENT 'End of model usage',"
			"	ADD    COLUMN REC_PERIOD_ID     CHAR(1)      DEFAULT 'U' COMMENT 'Periodicity identifier',"
			"	ADD    COLUMN REC_PERIOD_N      INTEGER      DEFAULT 1   COMMENT 'Periodicity count',"
			"	ADD    COLUMN REC_PERIOD_DET    VARCHAR(256)             COMMENT 'Periodicity details',"
			"	MODIFY COLUMN REC_UPD_STAMP     TIMESTAMP    DEFAULT 0   COMMENT 'Properties last update timestamp'" )){
		return( FALSE );
	}

	/* 2 */
	if( !exec_query( self,
			"ALTER TABLE REC_T_RUN "
			"	MODIFY COLUMN REC_STATUS        CHAR(1)      NOT NULL    COMMENT 'Operation status',"
			"	ADD    COLUMN REC_LABEL         VARCHAR(256) NOT NULL    COMMENT 'Model label',"
			"	ADD    COLUMN REC_OPE_TEMPLATE  VARCHAR(64)  NOT NULL    COMMENT 'Operation template',"
			"	ADD    COLUMN REC_PERIOD_ID     CHAR(1)      NOT NULL    COMMENT 'Periodicity identifier',"
			"	ADD    COLUMN REC_PERIOD_N      INTEGER      DEFAULT 1   COMMENT 'Periodicity count',"
			"	ADD    COLUMN REC_PERIOD_DET    VARCHAR(256)             COMMENT 'Periodicity details',"
			"	ADD    COLUMN REC_END           DATE                     COMMENT 'Creation user',"
			"	ADD    COLUMN REC_CRE_USER      VARCHAR(64)  NOT NULL    COMMENT 'Creation user',"
			"	ADD    COLUMN REC_CRE_STAMP     TIMESTAMP    DEFAULT 0   COMMENT 'Creation timestamp',"
			"	CHANGE COLUMN REC_UPD_USER "
			"                     REC_STA_USER      VARCHAR(64)  NOT NULL    COMMENT 'Status last update user',"
			"	CHANGE COLUMN REC_UPD_STAMP "
			"                     REC_STA_STAMP     TIMESTAMP    DEFAULT 0   COMMENT 'Status last update timestamp',"
			"	ADD    COLUMN REC_EDI_USER      VARCHAR(64)  NOT NULL    COMMENT 'Editable amount last update user',"
			"	ADD    COLUMN REC_EDI_STAMP     TIMESTAMP    DEFAULT 0   COMMENT 'Editable amount last update timestamp'" )){
		return( FALSE );
	}

	/* 3 */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS SET REC_PERIOD_ID='M' WHERE REC_PERIOD='%s'", REC_PERIOD_MONTHLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok){
		return( FALSE );
	}

	/* 4 */
	query = g_strdup_printf(
			"UPDATE REC_T_MODELS SET REC_PERIOD_ID='W' WHERE REC_PERIOD='%s'", REC_PERIOD_WEEKLY );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok){
		return( FALSE );
	}

	/* 5 */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS SET REC_PERIOD_N=1 WHERE REC_ENABLED='Y'" )){
		return( FALSE );
	}

	/* 6 */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS SET REC_PERIOD_DET="
			"   (SELECT REC_PER_DET_VALUE FROM REC_T_PERIODS_DET WHERE REC_PER_DET_ID=REC_PERIOD_DETAIL) "
			"	WHERE REC_ENABLED='Y'" )){
		return( FALSE );
	}

	/* 7 */
	if( !exec_query( self,
			"UPDATE REC_T_MODELS SET "
			"	REC_CRE_USER=REC_UPD_USER,"
			"	REC_CRE_STAMP=REC_UPD_STAMP" )){
		return( FALSE );
	}

	/* 8 */
	if( !exec_query( self,
			"UPDATE REC_T_RUN a SET "
			"	REC_LABEL=(SELECT REC_LABEL FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_OPE_TEMPLATE=(SELECT REC_OPE_TEMPLATE FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_PERIOD_ID=(SELECT REC_PERIOD_ID FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_PERIOD_N=(SELECT REC_PERIOD_N FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_PERIOD_DET=(SELECT REC_PERIOD_DET FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_END=(SELECT REC_END FROM REC_T_MODELS b WHERE a.REC_MNEMO=b.REC_MNEMO),"
			"	REC_CRE_USER=REC_STA_USER,"
			"	REC_CRE_STAMP=REC_STA_STAMP,"
			"	REC_EDI_USER=REC_STA_USER,"
			"	REC_EDI_STAMP=REC_STA_STAMP" )){
		return( FALSE );
	}

	/* 9 */
	if( !exec_query( self, "DROP TABLE REC_T_PERIODS " )){
		return( FALSE );
	}

	/* 10 */
	if( !exec_query( self, "DROP TABLE REC_T_PERIODS_DET " )){
		return( FALSE );
	}

	/* 11 */
	if( !exec_query( self, "DELETE FROM OFA_T_DOCS WHERE DOC_ID=(SELECT REC_DOC_ID FROM REC_T_PERIODS_DOC)" )){
		return( FALSE );
	}

	/* 12 */
	if( !exec_query( self, "DROP TABLE REC_T_PERIODS_DOC " )){
		return( FALSE );
	}

	/* 13 */
	if( !exec_query( self,
			"ALTER TABLE REC_T_GEN "
			"	DROP   COLUMN REC_LAST_PER_DET_ID" )){
		return( FALSE );
	}

	/* 14 */
	if( !exec_query( self,
			"ALTER TABLE REC_T_MODELS "
			"	DROP   COLUMN REC_PERIOD,"
			"	DROP   COLUMN REC_PERIOD_DETAIL" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v10( ofaRecurrentDBModel *self )
{
	return( 14 );
}

static gulong
idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	gulong errs;

	errs = 0;
	errs += check_model( instance, getter, progress );
	errs += check_run( instance, getter, progress );

	return( errs );
}

/*
 * Check recurrent models
 * Even disabled ones must be checked.
 */
static gulong
check_model( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	static gchar *thisfn = "ofa_recurrent_dbmodel_idbmodel_check_integrity_check_model";
	gulong errs, moderrs;
	void *worker;
	GtkWidget *label;
	GList *records, *it, *orphans, *ito;
	gulong count, i;
	ofoRecurrentModel *model;
	gchar *str, *str2;
	const gchar *mnemo, *ope_mnemo;
	ofoOpeTemplate *ope_object;
	ofxCounter docid;
	gboolean all_messages;
	myPeriod *period;

	g_debug( "%s: instance=%p (%s), getter=%p (%s), progress=%p (%s)",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) getter, G_OBJECT_TYPE_NAME( getter ),
			( void * ) progress, G_OBJECT_TYPE_NAME( progress ));

	worker = GUINT_TO_POINTER( OFO_TYPE_RECURRENT_MODEL );
	all_messages = ofa_prefs_check_integrity_get_display_all( getter );

	if( progress ){
		label = gtk_label_new( _( " Check for recurrent models integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_recurrent_model_get_dataset( getter );
	count = 1 + 3*g_list_length( records );
	i = 0;

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( it=records ; it ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		mnemo = ofo_recurrent_model_get_mnemo( model );
		moderrs = 0;

		/* operation template */
		ope_mnemo = ofo_recurrent_model_get_ope_template( model );
		if( !my_strlen( ope_mnemo )){
			if( progress ){
				str = g_strdup_printf( _( "Recurrent model %s does not have an operation template" ), mnemo );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
			moderrs += 1;
		} else {
			ope_object = ofo_ope_template_get_by_mnemo( getter, ope_mnemo );
			if( !ope_object || !OFO_IS_OPE_TEMPLATE( ope_object )){
				if( progress ){
					str = g_strdup_printf(
							_( "Recurrent model %s has operation template '%s' which doesn't exist" ), mnemo, ope_mnemo );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				moderrs += 1;
			}
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* periodicity
		 * may be unset for a disabled model */
		period = ofo_recurrent_model_get_period( model );
		if( !period || my_period_is_empty( period )){
			if( !ofo_recurrent_model_get_enabled( model )){
				if( progress ){
					str = g_strdup_printf(
							_( "Recurrent model %s has empty periodicity" ), mnemo );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				moderrs += 1;
			}
		} else if( !my_period_is_valid( period, &str )){
			str2 = g_strdup_printf( _( "%s for recurrent model %s" ), str, mnemo );
			my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str2 );
			g_free( str );
			g_free( str2 );
			errs += 1;
			moderrs += 1;
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* check for referenced documents which actually do not exist */
		orphans = ofa_idoc_get_orphans( OFA_IDOC( model ));
		if( g_list_length( orphans ) > 0 ){
			for( ito=orphans ; ito ; ito=ito->next ){
				docid = ( ofxCounter ) ito->data;
				if( progress ){
					str = g_strdup_printf( _( "Found orphan document(s) with DocId %lu" ), docid );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				moderrs += 1;
			}
		}
		ofa_idoc_free_orphans( orphans );
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		if( moderrs == 0 && progress && all_messages ){
			str = g_strdup_printf( _( "Recurrent model %s does not exhibit any error: OK" ), mnemo );
			my_iprogress_set_text( progress, worker, MY_PROGRESS_NORMAL, str );
			g_free( str );
		}
	}

	/* check that all documents have a model parent */
	orphans = ofo_recurrent_model_doc_get_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan document(s) with RecMnemo %s" ), ( const gchar * ) it->data );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
		}
	} else if( all_messages ){
		my_iprogress_set_text( progress, worker, MY_PROGRESS_NORMAL, _( "No orphan recurrent model document found: OK" ));
	}
	ofo_recurrent_model_doc_free_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* progress end */
	if( progress ){
		if( all_messages ){
			my_iprogress_set_text( progress, worker, MY_PROGRESS_NONE, "" );
		}
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}

static gulong
check_run( const ofaIDBModel *instance, ofaIGetter *getter, myIProgress *progress )
{
	static gchar *thisfn = "ofa_recurrent_dbmodel_idbmodel_check_integrity_check_run";
	gulong errs, runerrs;
	void *worker;
	GtkWidget *label;
	GList *records, *it, *orphans, *ito;
	gulong count, i;
	ofoRecurrentRun *obj;
	gchar *str, *str2;
	const gchar *mnemo, *ope_mnemo;
	ofoRecurrentModel *model_object;
	ofoOpeTemplate *ope_object;
	ofxCounter numseq, docid;
	gboolean all_messages;
	myPeriod *period;

	g_debug( "%s: instance=%p (%s), getter=%p (%s), progress=%p (%s)",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) getter, G_OBJECT_TYPE_NAME( getter ),
			( void * ) progress, G_OBJECT_TYPE_NAME( progress ));

	worker = GUINT_TO_POINTER( OFO_TYPE_RECURRENT_RUN );
	all_messages = ofa_prefs_check_integrity_get_display_all( getter );

	if( progress ){
		label = gtk_label_new( _( " Check for recurrent runs integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_recurrent_run_get_dataset( getter );
	count = 1 + 4*g_list_length( records );
	i = 0;

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( it=records ; it ; it=it->next ){
		obj = OFO_RECURRENT_RUN( it->data );
		numseq = ofo_recurrent_run_get_numseq( obj );
		runerrs = 0;
		model_object = NULL;

		/* recurrent model is mandatory */
		mnemo = ofo_recurrent_run_get_mnemo( obj );
		if( !my_strlen( mnemo )){
			if( progress ){
				str = g_strdup_printf( _( "Recurrent run %lu does not have a model mnemonic" ), numseq );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
			runerrs += 1;
		} else {
			model_object = ofo_recurrent_model_get_by_mnemo( getter, mnemo );
			if( !model_object || !OFO_IS_RECURRENT_MODEL( model_object )){
				if( progress ){
					str = g_strdup_printf(
							_( "Recurrent run %lu has model %s which doesn't exist" ), numseq, mnemo );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				runerrs += 1;
			}
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* operation template */
		ope_mnemo = ofo_recurrent_run_get_ope_template( obj );
		if( !my_strlen( ope_mnemo )){
			if( progress ){
				str = g_strdup_printf( _( "Recurrent model %s does not have an operation template" ), mnemo );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
			runerrs += 1;
		} else {
			ope_object = ofo_ope_template_get_by_mnemo( getter, ope_mnemo );
			if( !ope_object || !OFO_IS_OPE_TEMPLATE( ope_object )){
				if( progress ){
					str = g_strdup_printf(
							_( "Recurrent run %s has operation template '%s' which doesn't exist" ), mnemo, ope_mnemo );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				runerrs += 1;
			}
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* periodicity */
		period = ofo_recurrent_run_get_period( obj );
		if( !period || my_period_is_empty( period )){
			if( progress ){
				str = g_strdup_printf(
						_( "Recurrent run %s has invalid periodicity" ), mnemo );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
			runerrs += 1;

		} else if( !my_period_is_valid( period, &str )){
			str2 = g_strdup_printf( _( "%s for recurrent run %lu" ), str, numseq );
			my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str2 );
			g_free( str );
			g_free( str2 );
			errs += 1;
			runerrs += 1;
		}
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		/* check for referenced documents which actually do not exist */
		orphans = ofa_idoc_get_orphans( OFA_IDOC( obj ));
		if( g_list_length( orphans ) > 0 ){
			for( ito=orphans ; ito ; ito=ito->next ){
				docid = ( ofxCounter ) ito->data;
				if( progress ){
					str = g_strdup_printf( _( "Found orphan document(s) with DocId %lu" ), docid );
					my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
					g_free( str );
				}
				errs += 1;
				runerrs += 1;
			}
		}
		ofa_idoc_free_orphans( orphans );
		if( progress ){
			my_iprogress_pulse( progress, worker, ++i, count );
		}

		if( runerrs == 0 && progress && all_messages ){
			str = g_strdup_printf( _( "Recurrent run %lu does not exhibit any error: OK" ), numseq );
			my_iprogress_set_text( progress, worker, MY_PROGRESS_NORMAL, str );
			g_free( str );
		}
	}

	/* check that all documents have a model parent */
	orphans = ofo_recurrent_run_get_doc_orphans( getter );
	if( g_list_length( orphans ) > 0 ){
		for( it=orphans ; it ; it=it->next ){
			if( progress ){
				str = g_strdup_printf( _( "Found orphan document(s) with RecNumseq %lu" ), ( ofxCounter ) it->data );
				my_iprogress_set_text( progress, worker, MY_PROGRESS_ERROR, str );
				g_free( str );
			}
			errs += 1;
		}
	} else if( all_messages ){
		my_iprogress_set_text( progress, worker, MY_PROGRESS_NORMAL, _( "No orphan recurrent run document found: OK" ));
	}
	ofo_recurrent_run_free_doc_orphans( orphans );
	if( progress ){
		my_iprogress_pulse( progress, worker, ++i, count );
	}

	/* progress end */
	if( progress ){
		if( all_messages ){
			my_iprogress_set_text( progress, worker, MY_PROGRESS_NONE, "" );
		}
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}
