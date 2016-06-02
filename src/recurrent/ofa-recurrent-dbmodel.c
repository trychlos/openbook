/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-iprogress.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-ope-template.h"

#include "ofa-recurrent-dbmodel.h"
#include "ofo-recurrent-gen.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* a dedicated structure to hold needed datas
 */
typedef struct {

	/* initialization
	 */
	const ofaIDBModel   *instance;
	ofaHub              *hub;
	const ofaIDBConnect *connect;
	myIProgress         *window;

	/* progression
	 */
	gulong               total;
	gulong               current;
}
	sUpdate;

/* the functions which update the DB model
 */
static gboolean dbmodel_to_v1( sUpdate *update_data, guint version );
static gulong   count_v1( sUpdate *update_data );
static gboolean dbmodel_to_v2( sUpdate *update_data, guint version );
static gulong   count_v2( sUpdate *update_data );
static gboolean dbmodel_to_v3( sUpdate *update_data, guint version );
static gulong   count_v3( sUpdate *update_data );

typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( sUpdate *update_data, guint version );
	gulong   ( *fncount )( sUpdate *update_data );
}
	sMigration;

static sMigration st_migrates[] = {
		{ 1, dbmodel_to_v1, count_v1 },
		{ 2, dbmodel_to_v2, count_v2 },
		{ 3, dbmodel_to_v3, count_v3 },
		{ 0 }
};

#define MARGIN_LEFT                     20

static guint      idbmodel_get_interface_version( void );
static guint      idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint      idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static gboolean   idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *window );
static gboolean   upgrade_to( sUpdate *update_data, sMigration *smig );
static gboolean   exec_query( sUpdate *update_data, const gchar *query );
static gboolean   version_begin( sUpdate *update_data, gint version );
static gboolean   version_end( sUpdate *update_data, gint version );
static gulong     idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress );
static gulong     check_model( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress );
static gulong     check_run( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress );

/*
 * #ofaIDBModel interface setup
 */
void
ofa_recurrent_dbmodel_iface_init( ofaIDBModelInterface *iface )
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
idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *window )
{
	sUpdate *update_data;
	guint i, cur_version, last_version;
	GtkWidget *label;
	gchar *str;
	gboolean ok;

	ok = TRUE;
	update_data = g_new0( sUpdate, 1 );
	update_data->instance = instance;
	update_data->hub = hub;
	update_data->connect = ofa_hub_get_connect( hub );
	update_data->window = window;

	cur_version = idbmodel_get_current_version( instance, update_data->connect );
	last_version = idbmodel_get_last_version( instance, update_data->connect );

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
				if( !upgrade_to( update_data, &st_migrates[i] )){
					str = g_strdup_printf(
								_( "Unable to upgrade current Recurrent DB model to v %d" ),
								st_migrates[i].ver_target );
					label = gtk_label_new( str );
					g_free( str );
					my_utils_widget_set_margins( label, 0, 0, 2*MARGIN_LEFT, 0 );
					my_utils_widget_set_style( label, "labelerror" );
					gtk_label_set_xalign( GTK_LABEL( label ), 0 );
					my_iprogress_start_progress( update_data->window, update_data->instance, label, FALSE );
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
		my_iprogress_start_progress( update_data->window, update_data->instance, label, FALSE );
	}

	return( ok );
}

/*
 * upgrade the DB model to the specified version
 */
static gboolean
upgrade_to( sUpdate *update_data, sMigration *smig )
{
	gboolean ok;
	GtkWidget *label;
	gchar *str;

	str = g_strdup_printf( _( "Upgrading to v %d :" ), smig->ver_target );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_valign( label, GTK_ALIGN_END );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	my_iprogress_start_progress( update_data->window, update_data->instance, label, TRUE );

	update_data->total = smig->fncount( update_data )+3;	/* counting version_begin+version_end */
	update_data->current = 0;

	ok = version_begin( update_data, smig->ver_target ) &&
			smig->fnquery( update_data, smig->ver_target ) &&
			version_end( update_data, smig->ver_target );

	return( ok );
}

static gboolean
exec_query( sUpdate *update_data, const gchar *query )
{
	gboolean ok;

	my_iprogress_set_text( update_data->window, update_data->instance, query );

	ok = ofa_idbconnect_query( update_data->connect, query, TRUE );

	update_data->current += 1;
	my_iprogress_pulse( update_data->window, update_data->instance, update_data->current, update_data->total );

	return( ok );
}

static gboolean
version_begin( sUpdate *update_data, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( update_data,
			"CREATE TABLE IF NOT EXISTS REC_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'Recurrent DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'Recurrent version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO REC_T_VERSION (VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = exec_query( update_data, query );
	g_free( query );

	return( ok );
}

static gboolean
version_end( sUpdate *update_data, gint version )
{
	gchar *query;
	gboolean ok;

	/* we do this only at the end of the DB model udpate
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE REC_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( update_data, query );
	g_free( query );

	return( ok );
}

static gboolean
dbmodel_to_v1( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v1";
	gchar *str;

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"CREATE TABLE IF NOT EXISTS REC_T_GEN ("
			"	REC_ID             INTEGER      NOT NULL UNIQUE        COMMENT 'Unique identifier',"
			"	REC_LAST_RUN       DATE                                COMMENT 'Last recurrent operations generation date')"
			" CHARACTER SET utf8" )){
		return( FALSE );
	}

	str = g_strdup_printf( "INSERT IGNORE INTO REC_T_GEN (REC_ID,REC_LAST_RUN) VALUES (%d,NULL)", RECURRENT_ROW_ID );
	if( !exec_query( update_data, str )){
		return( FALSE );
	}

	/* updated in v2 */
	if( !exec_query( update_data,
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
	if( !exec_query( update_data,
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
count_v1( sUpdate *update_data )
{
	return( 4 );
}

/*
 * display three amounts in the model, letting the user edit them
 */
static gboolean
dbmodel_to_v2( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v2";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"ALTER TABLE REC_T_MODELS "
			"	ADD COLUMN REC_DEF_AMOUNT1    VARCHAR(64)              COMMENT 'Definition of amount n° 1',"
			"	ADD COLUMN REC_DEF_AMOUNT2    VARCHAR(64)              COMMENT 'Definition of amount n° 2',"
			"	ADD COLUMN REC_DEF_AMOUNT3    VARCHAR(64)              COMMENT 'Definition of amount n° 3'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE REC_T_RUN "
			"	ADD COLUMN REC_AMOUNT1        DECIMAL(20,5)            COMMENT 'Amount n° 1',"
			"	ADD COLUMN REC_AMOUNT2        DECIMAL(20,5)            COMMENT 'Amount n° 2',"
			"	ADD COLUMN REC_AMOUNT3        DECIMAL(20,5)            COMMENT 'Amount n° 3'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v2( sUpdate *update_data )
{
	return( 2 );
}

/*
 * Review REC_T_RUN index:
 *   for a same mnemo+date couple, may have several Cancelled, only one
 *   Waiting|Validated - this is controlled by the code.
 */
static gboolean
dbmodel_to_v3( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_recurrent_dbmodel_to_v3";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"ALTER TABLE REC_T_RUN "
			"	DROP PRIMARY KEY" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE REC_T_RUN "
			"	ADD COLUMN REC_NUMSEQ         BIGINT NOT NULL UNIQUE AUTO_INCREMENT COMMENT 'Automatic sequence number'" )){
		return( FALSE );
	}

	return( TRUE );
}

static gulong
count_v3( sUpdate *update_data )
{
	return( 2 );
}

static gulong
idbmodel_check_dbms_integrity( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress )
{
	gulong errs;

	errs = 0;
	errs += check_model( instance, hub, progress );
	errs += check_run( instance, hub, progress );

	return( errs );
}

static gulong
check_model( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress )
{
	gulong errs;
	void *worker;
	GtkWidget *label;
	GList *records, *it;
	gulong count, i;
	ofoRecurrentModel *model;
	gchar *str;
	const gchar *mnemo, *ope_mnemo;
	ofoOpeTemplate *ope_object;

	worker = GUINT_TO_POINTER( OFO_TYPE_RECURRENT_MODEL );

	if( progress ){
		label = gtk_label_new( _( " Check for recurrent models integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_recurrent_model_get_dataset( hub );
	count = g_list_length( records );

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( i=1, it=records ; it ; ++i, it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		mnemo = ofo_recurrent_model_get_mnemo( model );

		/* operation template */
		ope_mnemo = ofo_recurrent_model_get_ope_template( model );
		if( !my_strlen( ope_mnemo )){
			if( progress ){
				str = g_strdup_printf( _( "Recurrent model %s does not have an operation template" ), mnemo );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		} else {
			ope_object = ofo_ope_template_get_by_mnemo( hub, ope_mnemo );
			if( !ope_object || !OFO_IS_OPE_TEMPLATE( ope_object )){
				if( progress ){
					str = g_strdup_printf(
							_( "Recurrent model %s has operation template '%s' which doesn't exist" ), mnemo, ope_mnemo );
					my_iprogress_set_text( progress, worker, str );
					g_free( str );
				}
				errs += 1;
			}
		}

		if( progress ){
			my_iprogress_pulse( progress, worker, i, count );
		}
	}

	/* progress end */
	if( progress ){
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}

static gulong
check_run( const ofaIDBModel *instance, ofaHub *hub, myIProgress *progress )
{
	gulong errs;
	void *worker;
	GtkWidget *label;
	GList *records, *it;
	gulong count, i;
	ofoRecurrentRun *obj;
	gchar *str;
	const gchar *mnemo;
	ofoRecurrentModel *model_object;

	worker = GUINT_TO_POINTER( OFO_TYPE_RECURRENT_RUN );

	if( progress ){
		label = gtk_label_new( _( " Check for recurrent runs integrity " ));
		my_iprogress_start_work( progress, worker, label );
		my_iprogress_start_progress( progress, worker, NULL, TRUE );
	}

	errs = 0;
	records = ofo_recurrent_run_get_dataset( hub );
	count = g_list_length( records );

	if( count == 0 ){
		if( progress ){
			my_iprogress_pulse( progress, worker, 0, 0 );
		}
	}

	for( i=1, it=records ; it ; ++i, it=it->next ){
		obj = OFO_RECURRENT_RUN( it->data );
		mnemo = ofo_recurrent_run_get_mnemo( obj );

		/* recurrent model */
		model_object = ofo_recurrent_model_get_by_mnemo( hub, mnemo );
		if( !model_object || !OFO_IS_RECURRENT_MODEL( model_object )){
			if( progress ){
				str = g_strdup_printf(
						_( "Recurrent model %s doesn't exist" ), mnemo );
				my_iprogress_set_text( progress, worker, str );
				g_free( str );
			}
			errs += 1;
		}

		if( progress ){
			my_iprogress_pulse( progress, worker, i, count );
		}
	}

	/* progress end */
	if( progress ){
		my_iprogress_set_ok( progress, worker, NULL, errs );
	}

	return( errs );
}
