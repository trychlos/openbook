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

#include "api/my-iwindow.h"
#include "api/my-progress-bar.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"

#include "ofa-tva.h"
#include "ofa-tva-dbmodel.h"
#include "ofo-tva-form.h"
#include "ofo-tva-record.h"

/* a dedicated structure to hold needed datas
 */
typedef struct {

	/* initialization
	 */
	const ofaIDBModel   *instance;
	ofaHub              *hub;
	const ofaIDBConnect *connect;
	myIWindow            *window;

	/* progression bar
	 */
	GtkWidget           *bar;
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
static gboolean dbmodel_to_v4( sUpdate *update_data, guint version );
static gulong   count_v4( sUpdate *update_data );

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
		{ 4, dbmodel_to_v4, count_v4 },
		{ 0 }
};

#define MARGIN_LEFT                     20

static guint      idbmodel_get_interface_version( const ofaIDBModel *instance );
static guint      idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint      idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static void       idbmodel_connect_handlers( const ofaIDBModel *instance, ofaHub *hub );
static gboolean   idbmodel_get_is_deletable( const ofaIDBModel *instance, const ofaHub *hub, const ofoBase *object );
static gboolean   idbmodel_ddl_update( const ofaIDBModel *instance, ofaHub *hub, myIWindow *window );
static gboolean   upgrade_to( sUpdate *update_data, sMigration *smig );
static GtkWidget *add_row( sUpdate *update_data, const gchar *title, gboolean with_bar );
static void       set_bar_progression( sUpdate *update_data );
static gboolean   exec_query( sUpdate *update_data, const gchar *query );
static gboolean   version_begin( sUpdate *update_data, gint version );
static gboolean   version_end( sUpdate *update_data, gint version );

/*
 * #ofaIDBModel interface setup
 */
void
ofa_tva_dbmodel_iface_init( ofaIDBModelInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbmodel_get_interface_version;
	iface->get_current_version = idbmodel_get_current_version;
	iface->get_last_version = idbmodel_get_last_version;
	iface->ddl_update = idbmodel_ddl_update;
	iface->connect_handlers = idbmodel_connect_handlers;
	iface->get_is_deletable = idbmodel_get_is_deletable;
}

/*
 * the version of the #ofaIDBModel interface implemented by the module
 */
static guint
idbmodel_get_interface_version( const ofaIDBModel *instance )
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
	guint last_version, i;

	last_version = 0;

	for( i=0 ; st_migrates[i].ver_target ; ++i ){
		if( last_version < st_migrates[i].ver_target ){
			last_version = st_migrates[i].ver_target;
		}
	}

	return( last_version );
}

static void
idbmodel_connect_handlers( const ofaIDBModel *instance, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_idbmodel_connect_handlers";

	g_debug( "%s: instance=%p, hub=%p", thisfn, ( void * ) instance, ( void * ) hub );

	ofo_tva_form_connect_to_hub_handlers( hub );
	ofo_tva_record_connect_to_hub_handlers( hub );
}

static gboolean
idbmodel_get_is_deletable( const ofaIDBModel *instance, const ofaHub *hub, const ofoBase *object )
{
	return( ofo_tva_form_get_is_deletable( hub, object ) &&
			ofo_tva_record_get_is_deletable( hub, object ));
}

static gboolean
idbmodel_ddl_update( const ofaIDBModel *instance, ofaHub *hub, myIWindow *window )
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

	label = gtk_label_new( _( "Updating VAT DB model" ));
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	ofa_idbmodel_add_row_widget( instance, window, label );

	str = g_strdup_printf( _( "Current version is v %u" ), cur_version );
	label = gtk_label_new( str );
	g_free( str );
	my_utils_widget_set_margins( label, 0, 0, MARGIN_LEFT, 0 );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	ofa_idbmodel_add_row_widget( instance, window, label );

	if( cur_version < last_version ){
		for( i=0 ; st_migrates[i].ver_target && ok ; ++i ){
			if( cur_version < st_migrates[i].ver_target ){
				if( !upgrade_to( update_data, &st_migrates[i] )){
					str = g_strdup_printf(
								_( "Unable to upgrade current VAT DB model to v %d" ),
								st_migrates[i].ver_target );
					label = gtk_label_new( str );
					g_free( str );
					my_utils_widget_set_margins( label, 0, 0, 2*MARGIN_LEFT, 0 );
					my_utils_widget_set_style( label, "labelerror" );
					gtk_label_set_xalign( GTK_LABEL( label ), 0 );
					ofa_idbmodel_add_row_widget( instance, window, label );
					ok = FALSE;
					break;
				}
			}
		}
	} else {
		str = g_strdup_printf( _( "Last version is v %u: up to date" ), last_version );
		label = gtk_label_new( str );
		g_free( str );
		my_utils_widget_set_margins( label, 0, 0, MARGIN_LEFT, 0 );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		ofa_idbmodel_add_row_widget( instance, window, label );
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
	gchar *str;

	str = g_strdup_printf( _( "Upgrading to v %d :" ), smig->ver_target );
	update_data->bar = add_row( update_data, str, TRUE );
	g_free( str );

	update_data->total = smig->fncount( update_data )+3;	/* counting version_begin+version_end */
	update_data->current = 0;

	ok = version_begin( update_data, smig->ver_target ) &&
			smig->fnquery( update_data, smig->ver_target ) &&
			version_end( update_data, smig->ver_target );

	return( ok );
}

/*
 * if with_bar, then add a progress bar in column 1
 * else add a label
 */
static GtkWidget *
add_row( sUpdate *update_data, const gchar *title, gboolean with_bar )
{
	GtkWidget *grid, *label;
	myProgressBar *bar;

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );

	label = gtk_label_new( title );
	my_utils_widget_set_margins( label, 0, 0, MARGIN_LEFT, 0 );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, 0, 1, 1 );

	bar = NULL;

	if( with_bar ){
		bar = my_progress_bar_new();
		gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( bar ), 1, 0, 1, 1 );

	} else {
		label = gtk_label_new( NULL );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 0, 1, 1 );
	}

	ofa_idbmodel_add_row_widget( update_data->instance, update_data->window, grid );

	return( with_bar ? GTK_WIDGET( bar ) : label );
}

static void
set_bar_progression( sUpdate *update_data )
{
	gdouble progress;
	gchar *text;

	if( update_data->total > 0 ){
		progress = ( gdouble ) update_data->current / ( gdouble ) update_data->total;
		g_signal_emit_by_name( update_data->bar, "ofa-double", progress );
	}
	if( 0 ){
		text = g_strdup_printf( "%lu/%lu", update_data->current, update_data->total );
		g_signal_emit_by_name( update_data->bar, "ofa-text", text );
		g_free( text );
	}
}

static gboolean
exec_query( sUpdate *update_data, const gchar *query )
{
	gboolean ok;

	ofa_idbmodel_add_text( update_data->instance, update_data->window, query );
	ok = ofa_idbconnect_query( update_data->connect, query, TRUE );
	update_data->current += 1;
	set_bar_progression( update_data );

	return( ok );
}

static gboolean
version_begin( sUpdate *update_data, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( update_data,
			"CREATE TABLE IF NOT EXISTS TVA_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'VAT DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'VAT version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO TVA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
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
			"UPDATE TVA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( update_data, query );
	g_free( query );

	return( ok );
}

static gboolean
dbmodel_to_v1( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v1";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"CREATE TABLE IF NOT EXISTS TVA_T_FORMS ("
			"	TFO_MNEMO          VARCHAR(10)  NOT NULL UNIQUE COMMENT 'Form mnemonic',"
			"	TFO_LABEL          VARCHAR(80)                  COMMENT 'Form label',"
			"	TFO_NOTES          VARCHAR(4096)                COMMENT 'Notes',"
			"	TFO_UPD_USER       VARCHAR(20)                  COMMENT 'User responsible of last update',"
			"	TFO_UPD_STAMP      TIMESTAMP                    COMMENT 'Last update timestamp')" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
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
count_v1( sUpdate *update_data )
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
dbmodel_to_v2( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v2";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_FORMS "
			"	ADD    COLUMN TFO_HAS_CORRESPONDENCE CHAR(1)       COMMENT 'Whether this form has a correspondence frame'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_DET_LABEL          VARCHAR(192) COMMENT 'Form line label',"
			"	ADD    COLUMN TFO_DET_HAS_BASE       CHAR(1)      COMMENT 'Whether detail line has a base amount',"
			"	ADD    COLUMN TFO_DET_BASE           VARCHAR(80)  COMMENT 'Detail base',"
			"	ADD    COLUMN TFO_DET_LEVEL          INTEGER      COMMENT 'Detail line level'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
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
count_v2( sUpdate *update_data )
{
	return( 3 );
}

/*
 * records the declaration
 */
static gboolean
dbmodel_to_v3( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v3";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
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

	if( !exec_query( update_data,
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

	if( !exec_query( update_data,
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
count_v3( sUpdate *update_data )
{
	return( 3 );
}

/*
 * resize identifiers and labels
 */
static gboolean
dbmodel_to_v4( sUpdate *update_data, guint version )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_to_v4";

	g_debug( "%s: update_data=%p, version=%u", thisfn, ( void * ) update_data, version );

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_FORMS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_LABEL           VARCHAR(256)                          COMMENT 'Form label',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_FORMS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_BOOL_LABEL      VARCHAR(256)                          COMMENT 'Form line label'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_FORMS_DET "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_DET_CODE        VARCHAR(64)                           COMMENT 'Detail line code',"
			"	MODIFY COLUMN TFO_DET_LABEL       VARCHAR(256)                          COMMENT 'Detail line label',"
			"	MODIFY COLUMN TFO_DET_BASE        VARCHAR(128)                          COMMENT 'Detail base computing rule',"
			"	MODIFY COLUMN TFO_DET_AMOUNT      VARCHAR(128)                          COMMENT 'Detail amount computing rule'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_RECORDS "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_LABEL           VARCHAR(256)                          COMMENT 'Form label',"
			"	MODIFY COLUMN TFO_UPD_USER        VARCHAR(64)                           COMMENT 'User responsible of last update'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
			"ALTER TABLE TVA_T_RECORDS_BOOL "
			"	MODIFY COLUMN TFO_MNEMO           VARCHAR(64)  BINARY NOT NULL          COMMENT 'Form identifier',"
			"	MODIFY COLUMN TFO_BOOL_LABEL      VARCHAR(256)                          COMMENT 'Form line label'" )){
		return( FALSE );
	}

	if( !exec_query( update_data,
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
count_v4( sUpdate *update_data )
{
	return( 6 );
}
