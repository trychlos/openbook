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
#include <stdlib.h>

#include "my/my-iident.h"
#include "my/my-iprogress.h"
#include "my/my-utils.h"

#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "mysql/ofa-mysql-dbmodel.h"

/* private instance data
 */
struct _ofaMysqlDBModelPrivate {
	gboolean             dispose_has_run;

	/* update setup
	 */
	ofaHub              *hub;
	const ofaIDBConnect *connect;
	myIProgress         *window;

	/* update progression
	 */
	gulong               total;
	gulong               current;
};

#define DBMODEL_CANON_NAME               "CORE"

/* the functions which manage the DB model update
 */

typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( ofaMysqlDBModel *model, gint version );
	gulong   ( *fncount )( ofaMysqlDBModel *model );
}
	sMigration;

/* default imported datas
 *
 * Note that the construct
 *   static const gchar *st_classes = INIT1DIR "/classes-h1.csv";
 * plus a reference to st_classes in the array initializer
 * is refused with the message "initializer element is not constant"
 */
typedef GType ( *fnType )( void );

typedef struct {
	const gchar *label;
	const gchar *table;
	const gchar *filename;
	gint         header_count;
	fnType       typefn;
}
	sImport;

static sImport st_imports[] = {
		{ N_( "Classes" ),
				"OFA_T_CLASSES",       "classes-h1.csv",       1, ofo_class_get_type },
		{ N_( "Currencies" ),
				"OFA_T_CURRENCIES",    "currencies-h1.csv",    1, ofo_currency_get_type },
		{ N_( "Ledgers" ),
				"OFA_T_LEDGERS",       "ledgers-h1.csv",       1, ofo_ledger_get_type },
		{ N_( "Operation templates" ),
				"OFA_T_OPE_TEMPLATES", "ope-templates-h2.csv", 2, ofo_ope_template_get_type },
		{ N_( "Rates" ),
				"OFA_T_RATES",         "rates-h2.csv",         2, ofo_rate_get_type },
		{ 0 }
};

#define MARGIN_LEFT                       20

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void     instance_finalize( GObject *object );
static void     instance_dispose( GObject *object );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     class_init( ofaMysqlDBModelClass *klass );
static void     iident_iface_init( myIIdentInterface *iface );
static gchar   *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar   *iident_get_version( const myIIdent *instance, void *user_data );
static void     idbmodel_iface_init( ofaIDBModelInterface *iface );
static guint    idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint    idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static guint    get_last_version( void );
static gboolean idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *window );
static gboolean upgrade_to( ofaMysqlDBModel *model, sMigration *smig );
static gboolean exec_query( ofaMysqlDBModel *model, const gchar *query );
static gboolean version_begin( ofaMysqlDBModel *model, gint version );
static gboolean version_end( ofaMysqlDBModel *model, gint version );
static gboolean import_utf8_comma_pipe_file( ofaMysqlDBModel *model, sImport *import );
static gint     count_rows( ofaMysqlDBModel *model, const gchar *table );
static gboolean dbmodel_v20( ofaMysqlDBModel *model, gint version );
static gulong   count_v20( ofaMysqlDBModel *model );
static gboolean dbmodel_v21( ofaMysqlDBModel *model, gint version );
static gulong   count_v21( ofaMysqlDBModel *model );
static gboolean dbmodel_v22( ofaMysqlDBModel *model, gint version );
static gulong   count_v22( ofaMysqlDBModel *model );
static gboolean dbmodel_v23( ofaMysqlDBModel *model, gint version );
static gulong   count_v23( ofaMysqlDBModel *model );
static gboolean dbmodel_v24( ofaMysqlDBModel *model, gint version );
static gulong   count_v24( ofaMysqlDBModel *model );
static gboolean dbmodel_v25( ofaMysqlDBModel *model, gint version );
static gulong   count_v25( ofaMysqlDBModel *model );
static gboolean dbmodel_v26( ofaMysqlDBModel *model, gint version );
static gulong   count_v26( ofaMysqlDBModel *model );
static gboolean dbmodel_v27( ofaMysqlDBModel *model, gint version );
static gulong   count_v27( ofaMysqlDBModel *model );
static gboolean dbmodel_v28( ofaMysqlDBModel *model, gint version );
static gulong   count_v28( ofaMysqlDBModel *model );

static sMigration st_migrates[] = {
		{ 20, dbmodel_v20, count_v20 },
		{ 21, dbmodel_v21, count_v21 },
		{ 22, dbmodel_v22, count_v22 },
		{ 23, dbmodel_v23, count_v23 },
		{ 24, dbmodel_v24, count_v24 },
		{ 25, dbmodel_v25, count_v25 },
		{ 26, dbmodel_v26, count_v26 },
		{ 27, dbmodel_v27, count_v27 },
		{ 28, dbmodel_v28, count_v28 },
		{ 0 }
};

GType
ofa_mysql_dbmodel_get_type( void )
{
	return( st_module_type );
}

void
ofa_mysql_dbmodel_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_register_type";

	static GTypeInfo info = {
		sizeof( ofaMysqlDBModelClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMysqlDBModel ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iident_iface_info = {
		( GInterfaceInitFunc ) iident_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo idbmodel_iface_info = {
		( GInterfaceInitFunc ) idbmodel_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaMysqlDBModel", &info, 0 );

	g_type_module_add_interface( module, st_module_type, MY_TYPE_IIDENT, &iident_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IDBMODEL, &idbmodel_iface_info );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_MYSQL_DBMODEL( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
instance_dispose( GObject *object )
{
	ofaMysqlDBModelPrivate *priv;

	g_return_if_fail( object && OFA_IS_MYSQL_DBMODEL( object ));

	priv = OFA_MYSQL_DBMODEL( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_instance_init";
	ofaMysqlDBModel *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_MYSQL_DBMODEL( instance ));

	self = OFA_MYSQL_DBMODEL( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModelPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
class_init( ofaMysqlDBModelClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaMysqlDBModelPrivate ));
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_iident_iface_init";

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
 */
static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	guint version;

	if( user_data && OFA_IS_IDBCONNECT( user_data )){
		version = idbmodel_get_current_version( OFA_IDBMODEL( instance ), OFA_IDBCONNECT( user_data ));
		return( g_strdup_printf( "%u", version ));
	}

	return( g_strdup( "" ));
}

/*
 * ofaIDBModel interface setup
 */
static void
idbmodel_iface_init( ofaIDBModelInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbmodel_idbmodel_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_current_version = idbmodel_get_current_version;
	iface->get_last_version = idbmodel_get_last_version;
	iface->ddl_update = idbmodel_ddl_update;
}

static guint
idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	gint cur_version;

	ofa_idbconnect_query_int(
			connect,
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0", &cur_version, FALSE );

	return( abs( cur_version ));
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
idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *window )
{
	ofaMysqlDBModelPrivate *priv;
	guint i, cur_version, last_version;
	gboolean ok;
	GtkWidget *label;
	gchar *str;

	priv = OFA_MYSQL_DBMODEL( instance )->priv;

	ok = TRUE;
	priv->hub = hub;
	priv->connect = ofa_hub_get_connect( hub );
	priv->window = window;

	cur_version = idbmodel_get_current_version( instance, priv->connect );
	last_version = idbmodel_get_last_version( instance, priv->connect );

	label = gtk_label_new( _( " Updating DBMS main model " ));
	my_iprogress_start_work( window, instance, label );

	str = g_strdup_printf( _( "Current version is v %u" ), cur_version );
	label = gtk_label_new( str );
	g_free( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	my_iprogress_start_work( window, instance, label );

	if( cur_version < last_version ){
		for( i=0 ; st_migrates[i].ver_target ; ++i ){
			if( cur_version < st_migrates[i].ver_target ){
				if( !upgrade_to( OFA_MYSQL_DBMODEL( instance ), &st_migrates[i] )){
					str = g_strdup_printf(
								_( "Unable to upgrade current DBMS model to v %d" ),
								st_migrates[i].ver_target );
					label = gtk_label_new( str );
					g_free( str );
					my_utils_widget_set_margins( label, 0, 0, 2*MARGIN_LEFT, 0 );
					my_utils_widget_set_style( label, "labelerror" );
					gtk_label_set_xalign( GTK_LABEL( label ), 0 );
					my_iprogress_start_progress( window, instance, label, FALSE );
					ok = FALSE;
					break;
				}
			}
		}
		if( ok ){
			for( i=0 ; st_imports[i].label ; ++i ){
				if( !import_utf8_comma_pipe_file( OFA_MYSQL_DBMODEL( instance ), &st_imports[i] )){
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
		my_iprogress_start_progress( window, instance, label, FALSE );
	}

	return( ok );
}

/*
 * upgrade the DB model to the specified version
 */
static gboolean
upgrade_to( ofaMysqlDBModel *model, sMigration *smig )
{
	ofaMysqlDBModelPrivate *priv;
	gboolean ok;
	GtkWidget *label;
	gchar *str;

	priv = model->priv;

	str = g_strdup_printf( _( "Upgrading to v %d :" ), smig->ver_target );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_valign( label, GTK_ALIGN_END );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	my_iprogress_start_progress( priv->window, model, label, TRUE );

	priv->total = smig->fncount( model )+3;	/* counting version_begin+version_end */
	priv->current = 0;

	ok = version_begin( model, smig->ver_target ) &&
			smig->fnquery( model, smig->ver_target ) &&
			version_end( model, smig->ver_target );

	my_iprogress_set_ok( priv->window, model, NULL, ok ? 0 : 1 );

	return( ok );
}

static gboolean
exec_query( ofaMysqlDBModel *model, const gchar *query )
{
	ofaMysqlDBModelPrivate *priv;
	gboolean ok;

	priv = model->priv;

	my_iprogress_set_text( priv->window, model, query );

	ok = ofa_idbconnect_query( priv->connect, query, TRUE );

	priv->current += 1;
	my_iprogress_pulse( priv->window, model, priv->current, priv->total );

	return( ok );
}

static gboolean
version_begin( ofaMysqlDBModel *model, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'Version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = exec_query( model, query );
	g_free( query );

	return( ok );
}

static gboolean
version_end( ofaMysqlDBModel *model, gint version )
{
	gchar *query;
	gboolean ok;

	/* we do this only at the end of the DB model udpate
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE OFA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( model, query );
	g_free( query );

	return( ok );
}

static gboolean
import_utf8_comma_pipe_file( ofaMysqlDBModel *model, sImport *import )
{
	ofaMysqlDBModelPrivate *priv;
	gint count;
	gboolean ok;
	ofaFileFormat *settings;
	ofoBase *object;
	gchar *uri, *fname, *str;
	GtkWidget *label;

	priv = model->priv;

	ok = TRUE;
	count = count_rows( model, import->table );
	if( !count ){
		str = g_strdup_printf( _( "Importing into %s :" ), import->table );
		label = gtk_label_new( str );
		g_free( str );
		my_iprogress_start_progress( priv->window, model, label, FALSE );

		settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
		ofa_file_format_set( settings,
				NULL, OFA_FFTYPE_CSV, OFA_FFMODE_IMPORT, "UTF-8", MY_DATE_SQL, ',', '|', '\0', import->header_count );
		object = g_object_new( import->typefn(), NULL );
		fname = g_strdup_printf( "%s/%s", INIT1DIR, import->filename );
		uri = g_filename_to_uri( fname, NULL, NULL );
		g_free( fname );
		count = ofa_hub_import_csv( priv->hub, OFA_IIMPORTABLE( object ), uri, settings, NULL, NULL );
		ok = ( count > 0 );
		g_free( uri );
		g_object_unref( object );
		g_object_unref( settings );

		str = g_strdup_printf( _( "%d lines" ), count );
		label = gtk_label_new( str );
		g_free( str );
		my_iprogress_set_row( priv->window, model, label );
	}

	return( ok );
}

static gint
count_rows( ofaMysqlDBModel *model, const gchar *table )
{
	ofaMysqlDBModelPrivate *priv;
	gint count;
	gchar *query;

	priv = model->priv;

	query = g_strdup_printf( "SELECT COUNT(*) FROM %s", table );
	ofa_idbconnect_query_int( priv->connect, query, &count, TRUE );
	g_free( query );

	return( count );
}

/*
 * dbmodel_v20:
 *
 * This is the initial creation of the schema
 */
static gboolean
dbmodel_v20( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v20";
	ofaMysqlDBModelPrivate *priv;
	gchar *query, *dossier_name;
	gboolean ok;
	ofaIDBMeta *meta;

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	priv = model->priv;

	/* n° 1 */
	/* ACC_TYPE is renamed to ACC_ROOT in v27 */
	/* ACC_FORWARD is renamed to ACC_FORWARDABLE in v27 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_ACCOUNTS ("
			"	ACC_NUMBER          VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
			"	ACC_LABEL           VARCHAR(80)   NOT NULL           COMMENT 'Account label',"
			"	ACC_CURRENCY        VARCHAR(3)                       COMMENT 'ISO 3A identifier of the currency of the account',"
			"	ACC_NOTES           VARCHAR(4096)                    COMMENT 'Account notes',"
			"	ACC_TYPE            CHAR(1)                          COMMENT 'Account type, values R/D',"
			"	ACC_SETTLEABLE      CHAR(1)                          COMMENT 'Whether the account is settleable',"
			"	ACC_RECONCILIABLE   CHAR(1)                          COMMENT 'Whether the account is reconciliable',"
			"	ACC_FORWARD         CHAR(1)                          COMMENT 'Whether the account supports carried forwards',"
			"	ACC_UPD_USER        VARCHAR(20)                      COMMENT 'User responsible of properties last update',"
			"	ACC_UPD_STAMP       TIMESTAMP                        COMMENT 'Properties last update timestamp',"
			"	ACC_VAL_DEBIT       DECIMAL(20,5)                    COMMENT 'Debit balance of validated entries',"
			"	ACC_VAL_CREDIT      DECIMAL(20,5)                    COMMENT 'Credit balance of validated entries',"
			"	ACC_ROUGH_DEBIT     DECIMAL(20,5)                    COMMENT 'Debit balance of rough entries',"
			"	ACC_ROUGH_CREDIT    DECIMAL(20,5)                    COMMENT 'Credit balance of rough entries',"
			"	ACC_OPEN_DEBIT      DECIMAL(20,5)                    COMMENT 'Debit balance at the exercice opening',"
			"	ACC_OPEN_CREDIT     DECIMAL(20,5)                    COMMENT 'Credit balance at the exercice opening',"
			"	ACC_FUT_DEBIT       DECIMAL(20,5)                    COMMENT 'Debit balance of future entries',"
			"	ACC_FUT_CREDIT      DECIMAL(20,5)                    COMMENT 'Credit balance of future entries'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

#if 0
	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS ("
			"	ASS_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern asset identifier',"
			"	ASS_LABEL     VARCHAR(80)                 COMMENT 'Asset label',"
			"	ASS_DATE_IN   DATE                        COMMENT 'Entry date',"
			"	ASS_TOTAL     DECIMAL(20,5)               COMMENT 'Total payed (TVA inc.)',"
			"	ASS_IMMO      DECIMAL(20,5)               COMMENT 'Montant immobilisé',"
			"	ASS_IMMO_FISC INTEGER                     COMMENT 'Montant fiscal immobilisé (à amortir)',"
			"	ASS_DUREE     INTEGER                     COMMENT 'Durée d\\'amortissement',"
			"	ASS_TYPE      VARCHAR(1)                  COMMENT 'Type d\\'amortissement',"
			"	ASS_COEF_DEG  DECIMAL(20,5)               COMMENT 'Coefficient degressif',"
			"	ASS_RATE      DECIMAL(20,5)               COMMENT 'Taux d\\'amortissement',"
			"	ASS_DATE_OUT  DATE                        COMMENT 'Outgoing date',"
			"	ASS_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	ASS_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of last update',"
			"	ASS_UPD_STAMP TIMESTAMP                   COMMENT 'Last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS_EXE ("
			"	ASS_ID           INTEGER                  COMMENT 'Intern asset identifier',"
			"	ASS_EXE_NUM      INTEGER                  COMMENT 'Numéro d\\'annuité',"
			"	ASS_EXE_DUREE    INTEGER                  COMMENT 'Duree (en mois)',"
			"	ASS_EXE_PREV     INTEGER                  COMMENT 'Total des amortissements antérieurs',"
			"	ASS_EXE_TAUX_LIN DECIMAL(20,5)            COMMENT 'Taux lineaire',"
			"	ASS_EXE_TAUX_DEG DECIMAL(20,5)            COMMENT 'Taux degressif',"
			"	ASS_EXE_AMORT    INTEGER                  COMMENT 'Montant de l\\'annuite',"
			"	ASS_EXE_REST     INTEGER                  COMMENT 'Valeur residuelle',"
			"	CONSTRAINT PRIMARY KEY (ASS_ID,ASS_EXE_NUM)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}
#endif

	/* n° 2 */
	/* BAT_SOLDE is remediated in v22 */
	/* Labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT ("
			"	BAT_ID        BIGINT      NOT NULL UNIQUE            COMMENT 'Intern import identifier',"
			"	BAT_URI       VARCHAR(256)                           COMMENT 'Imported URI',"
			"	BAT_FORMAT    VARCHAR(80)                            COMMENT 'Identified file format',"
			"	BAT_BEGIN     DATE                                   COMMENT 'Begin date of the transaction list',"
			"	BAT_END       DATE                                   COMMENT 'End date of the transaction list',"
			"	BAT_RIB       VARCHAR(80)                            COMMENT 'Bank provided RIB',"
			"	BAT_CURRENCY  VARCHAR(3)                             COMMENT 'Account currency',"
			"	BAT_SOLDE     DECIMAL(20,5),"
			"	BAT_NOTES     VARCHAR(4096)                          COMMENT 'Import notes',"
			"	BAT_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of import',"
			"	BAT_UPD_STAMP TIMESTAMP                              COMMENT 'Import timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 3 */
	/* BAT_LINE_UPD_STAMP is remediated in v21 */
	/* BAT_LINE_ENTRY and BAT_LINE_UPD_USER are remediated in v24 */
	/* Labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT_LINES ("
			"	BAT_ID             BIGINT   NOT NULL                 COMMENT 'Intern import identifier',"
			"	BAT_LINE_ID        BIGINT   NOT NULL UNIQUE          COMMENT 'Intern imported line identifier',"
			"	BAT_LINE_DEFFECT   DATE                              COMMENT 'Effect date',"
			"	BAT_LINE_DOPE      DATE                              COMMENT 'Operation date',"
			"	BAT_LINE_REF       VARCHAR(80)                       COMMENT 'Bank reference',"
			"	BAT_LINE_LABEL     VARCHAR(80)                       COMMENT 'Line label',"
			"	BAT_LINE_CURRENCY  VARCHAR(3)                        COMMENT 'Line currency',"
			"	BAT_LINE_AMOUNT    DECIMAL(20,5)                     COMMENT 'Signed amount of the line',"
			"	BAT_LINE_ENTRY     BIGINT,"
			"	BAT_LINE_UPD_USER  VARCHAR(20),"
			"	BAT_LINE_UPD_STAMP TIMESTAMP"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 4 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_CLASSES ("
			"	CLA_NUMBER       INTEGER     NOT NULL UNIQUE         COMMENT 'Class number',"
			"	CLA_LABEL        VARCHAR(80) NOT NULL                COMMENT 'Class label',"
			"	CLA_NOTES        VARCHAR(4096)                       COMMENT 'Class notes',"
			"	CLA_UPD_USER     VARCHAR(20)                         COMMENT 'User responsible of properties last update',"
			"	CLA_UPD_STAMP    TIMESTAMP                           COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 5 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_CURRENCIES ("
			"	CUR_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	CUR_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	CUR_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	CUR_DIGITS    INTEGER     DEFAULT 2                  COMMENT 'Decimal digits on display',"
			"	CUR_NOTES     VARCHAR(4096)                          COMMENT 'Currency notes',"
			"	CUR_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	CUR_UPD_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 6 */
	/* DOS_STATUS is renamed to DOS_CURRENT in v27 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID               INTEGER   NOT NULL UNIQUE       COMMENT 'Row identifier',"
			"	DOS_DEF_CURRENCY     VARCHAR(3)                      COMMENT 'Default currency identifier',"
			"	DOS_EXE_BEGIN        DATE                            COMMENT 'Exercice beginning date',"
			"	DOS_EXE_END          DATE                            COMMENT 'Exercice ending date',"
			"	DOS_EXE_LENGTH       INTEGER                         COMMENT 'Exercice length in months',"
			"	DOS_EXE_NOTES        VARCHAR(4096)                   COMMENT 'Exercice notes',"
			"	DOS_FORW_OPE         VARCHAR(6)                      COMMENT 'Operation mnemo for carried forward entries',"
			"	DOS_IMPORT_LEDGER    VARCHAR(6)                      COMMENT 'Default import ledger',"
			"	DOS_LABEL            VARCHAR(80)                     COMMENT 'Raison sociale',"
			"	DOS_NOTES            VARCHAR(4096)                   COMMENT 'Dossier notes',"
			"	DOS_SIREN            VARCHAR(9)                      COMMENT 'Siren identifier',"
			"	DOS_SLD_OPE          VARCHAR(6)                      COMMENT 'Operation mnemo for balancing entries',"
			"	DOS_UPD_USER         VARCHAR(20)                     COMMENT 'User responsible of properties last update',"
			"	DOS_UPD_STAMP        TIMESTAMP                       COMMENT 'Properties last update timestamp',"
			"	DOS_LAST_BAT         BIGINT  DEFAULT 0               COMMENT 'Last BAT file number used',"
			"	DOS_LAST_BATLINE     BIGINT  DEFAULT 0               COMMENT 'Last BAT line number used',"
			"	DOS_LAST_ENTRY       BIGINT  DEFAULT 0               COMMENT 'Last entry number used',"
			"	DOS_LAST_SETTLEMENT  BIGINT  DEFAULT 0               COMMENT 'Last settlement number used',"
			"	DOS_STATUS           CHAR(1)                         COMMENT 'Status of this exercice'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 7
	 * dossier name is set as a default value for the label */
	meta = ofa_idbconnect_get_meta( priv->connect );
	dossier_name = ofa_idbmeta_get_dossier_name( meta );
	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_DOSSIER "
			"	(DOS_ID,DOS_LABEL,DOS_EXE_LENGTH,DOS_DEF_CURRENCY,"
			"	 DOS_STATUS,DOS_FORW_OPE,DOS_SLD_OPE) "
			"	VALUES (1,'%s',%u,'EUR','%s','%s','%s')",
			dossier_name, DOSSIER_EXERCICE_DEFAULT_LENGTH, "O", "CLORAN", "CLOSLD" );
	ok = exec_query( model, query );
	g_free( query );
	g_free( dossier_name );
	g_object_unref( meta );
	if( !ok ){
		return( FALSE );
	}

	/* n° 8 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER_CUR ("
			"	DOS_ID               INTEGER   NOT NULL              COMMENT 'Row identifier',"
			"	DOS_CURRENCY         VARCHAR(3)                      COMMENT 'Currency identifier',"
			"	DOS_SLD_ACCOUNT      VARCHAR(20)                     COMMENT 'Balancing account when closing the exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_CURRENCY)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 9 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_ENTRIES ("
			"	ENT_DEFFECT      DATE NOT NULL                       COMMENT 'Imputation effect date',"
			"	ENT_NUMBER       BIGINT  NOT NULL UNIQUE             COMMENT 'Entry number',"
			"	ENT_DOPE         DATE NOT NULL                       COMMENT 'Operation date',"
			"	ENT_LABEL        VARCHAR(80)                         COMMENT 'Entry label',"
			"	ENT_REF          VARCHAR(20)                         COMMENT 'Piece reference',"
			"	ENT_ACCOUNT      VARCHAR(20)                         COMMENT 'Account number',"
			"	ENT_CURRENCY     VARCHAR(3)                          COMMENT 'ISO 3A identifier of the currency',"
			"	ENT_DEBIT        DECIMAL(20,5) DEFAULT 0             COMMENT 'Debiting amount',"
			"	ENT_CREDIT       DECIMAL(20,5) DEFAULT 0             COMMENT 'Crediting amount',"
			"	ENT_LEDGER       VARCHAR(6)                          COMMENT 'Mnemonic identifier of the ledger',"
			"	ENT_OPE_TEMPLATE VARCHAR(6)                          COMMENT 'Mnemonic identifier of the operation template',"
			"	ENT_STATUS       INTEGER       DEFAULT 1             COMMENT 'Is the entry validated or deleted ?',"
			"	ENT_UPD_USER     VARCHAR(20)                         COMMENT 'User responsible of last update',"
			"	ENT_UPD_STAMP    TIMESTAMP                           COMMENT 'Last update timestamp',"
			"	ENT_CONCIL_DVAL  DATE                                COMMENT 'Reconciliation value date',"
			"	ENT_CONCIL_USER  VARCHAR(20)                         COMMENT 'User responsible of the reconciliation',"
			"	ENT_CONCIL_STAMP TIMESTAMP                           COMMENT 'Reconciliation timestamp',"
			"	ENT_STLMT_NUMBER BIGINT                              COMMENT 'Settlement number',"
			"	ENT_STLMT_USER   VARCHAR(20)                         COMMENT 'User responsible of the settlement',"
			"	ENT_STLMT_STAMP  TIMESTAMP                           COMMENT 'Settlement timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 10 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS ("
			"	LED_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE     COMMENT 'Mnemonic identifier of the ledger',"
			"	LED_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Ledger label',"
			"	LED_NOTES     VARCHAR(4096)                          COMMENT 'Ledger notes',"
			"	LED_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	LED_UPD_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp',"
			"	LED_LAST_CLO  DATE                                   COMMENT 'Last closing date'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 11 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS_CUR ("
			"	LED_MNEMO            VARCHAR(6) NOT NULL             COMMENT 'Internal ledger identifier',"
			"	LED_CUR_CODE         VARCHAR(3) NOT NULL             COMMENT 'Internal currency identifier',"
			"	LED_CUR_VAL_DEBIT    DECIMAL(20,5)                   COMMENT 'Validated debit total for this exercice on this journal',"
			"	LED_CUR_VAL_CREDIT   DECIMAL(20,5)                   COMMENT 'Validated credit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_DEBIT  DECIMAL(20,5)                   COMMENT 'Rough debit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_CREDIT DECIMAL(20,5)                   COMMENT 'Rough credit total for this exercice on this journal',"
			"	LED_CUR_FUT_DEBIT    DECIMAL(20,5)                   COMMENT 'Futur debit total on this journal',"
			"	LED_CUR_FUT_CREDIT   DECIMAL(20,5)                   COMMENT 'Futur credit total on this journal',"
			"	CONSTRAINT PRIMARY KEY (LED_MNEMO,LED_CUR_CODE)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 12 */
	/* locked indicators are remediated in v27 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES ("
			"	OTE_MNEMO      VARCHAR(6) BINARY NOT NULL UNIQUE     COMMENT 'Operation template mnemonic',"
			"	OTE_LABEL      VARCHAR(80)       NOT NULL            COMMENT 'Template label',"
			"	OTE_LED_MNEMO  VARCHAR(6)                            COMMENT 'Generated entries imputation ledger',"
			"	OTE_LED_LOCKED INTEGER                               COMMENT 'Ledger is locked',"
			"	OTE_REF        VARCHAR(20)                           COMMENT 'Operation reference',"
			"	OTE_REF_LOCKED INTEGER                               COMMENT 'Operation reference is locked',"
			"	OTE_NOTES      VARCHAR(4096)                         COMMENT 'Template notes',"
			"	OTE_UPD_USER   VARCHAR(20)                           COMMENT 'User responsible of properties last update',"
			"	OTE_UPD_STAMP  TIMESTAMP                             COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 13 */
	/* locked indicators are remediated in v27 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES_DET ("
			"	OTE_MNEMO              VARCHAR(6) NOT NULL           COMMENT 'Operation template menmonic',"
			"	OTE_DET_ROW            INTEGER    NOT NULL           COMMENT 'Detail line number',"
			"	OTE_DET_COMMENT        VARCHAR(80)                   COMMENT 'Detail line comment',"
			"	OTE_DET_ACCOUNT        VARCHAR(20)                   COMMENT 'Account number',"
			"	OTE_DET_ACCOUNT_LOCKED INTEGER                       COMMENT 'Account number is locked',"
			"	OTE_DET_LABEL          VARCHAR(80)                   COMMENT 'Entry label',"
			"	OTE_DET_LABEL_LOCKED   INTEGER                       COMMENT 'Entry label is locked',"
			"	OTE_DET_DEBIT          VARCHAR(80)                   COMMENT 'Debit amount',"
			"	OTE_DET_DEBIT_LOCKED   INTEGER                       COMMENT 'Debit amount is locked',"
			"	OTE_DET_CREDIT         VARCHAR(80)                   COMMENT 'Credit amount',"
			"	OTE_DET_CREDIT_LOCKED  INTEGER                       COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (OTE_MNEMO, OTE_DET_ROW)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

#if 0
	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RECURRENT ("
			"	REC_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Internal identifier',"
			"	REC_MOD_MNEMO VARCHAR(6)                  COMMENT 'Entry model mnemmonic',"
			"	REC_PERIOD    VARCHAR(1)                  COMMENT 'Periodicity',"
			"	REC_DAY       INTEGER                     COMMENT 'Day of the period',"
			"	REC_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	REC_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	REC_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	REC_LAST      DATE                        COMMENT 'Effect date of the last generation'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}
#endif

	/* n° 14 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL UNIQUE  COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_LABEL         VARCHAR(80)       NOT NULL         COMMENT 'Rate label',"
			"	RAT_NOTES         VARCHAR(4096)                      COMMENT 'Rate notes',"
			"	RAT_UPD_USER      VARCHAR(20)                        COMMENT 'User responsible of properties last update',"
			"	RAT_UPD_STAMP     TIMESTAMP                          COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 15 */
	/* RAT_VAL_BEG is renamed as RAT_VAL_BEGIN in v27 */
	/* Identifiers and labels are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES_VAL ("
			"	RAT_UNUSED        INTEGER AUTO_INCREMENT PRIMARY KEY COMMENT 'An unused counter to have a unique key while keeping NULL values',"
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL         COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_VAL_BEG       DATE    DEFAULT NULL               COMMENT 'Validity begin date',"
			"	RAT_VAL_END       DATE    DEFAULT NULL               COMMENT 'Validity end date',"
			"	RAT_VAL_RATE      DECIMAL(20,5)                      COMMENT 'Rate value',"
			"	UNIQUE (RAT_MNEMO,RAT_VAL_BEG,RAT_VAL_END)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v20( ofaMysqlDBModel *model )
{
	return( 15 );
}

/*
 * ofa_ddl_update_dbmodel_v21:
 * have zero timestamp on unreconciliated batlines
 */
static gboolean
dbmodel_v21( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v21";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	/* n° 1 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT_LINES "
			"	MODIFY COLUMN BAT_LINE_UPD_STAMP TIMESTAMP DEFAULT 0 "
			"	COMMENT 'Reconciliation timestamp'" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( model,
			"UPDATE OFA_T_BAT_LINES "
			"	SET BAT_LINE_UPD_STAMP=0 WHERE BAT_LINE_ENTRY IS NULL" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v21( ofaMysqlDBModel *model )
{
	return( 2 );
}

/*
 * ofa_ddl_update_dbmodel_v22:
 * have begin_solde and end_solde in bat
 */
static gboolean
dbmodel_v22( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v22";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	/* n° 1 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT "
			"	CHANGE COLUMN BAT_SOLDE BAT_SOLDE_END DECIMAL(20,5) "
			"	COMMENT 'Signed end balance of the account'" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT "
			"	ADD COLUMN BAT_SOLDE_BEGIN DECIMAL(20,5) "
			"	COMMENT 'Signed begin balance of the account'" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v22( ofaMysqlDBModel *model )
{
	return( 2 );
}

/*
 * ofa_ddl_update_dbmodel_v23:
 * closed accounts
 * remediated in v27
 */
static gboolean
dbmodel_v23( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v23";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	/* n° 1 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_ACCOUNTS "
			"	ADD COLUMN ACC_CLOSED CHAR(1) "
			"	COMMENT 'Whether the account is closed'" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v23( ofaMysqlDBModel *model )
{
	return( 1 );
}

/*
 * ofa_ddl_update_dbmodel_v24:
 *
 * This is an intermediate DB model wrongly introduced in v0.37 as a
 * reconciliation improvement try, and replaced in v0.38
 * (cf. dbmodel v25 below)
 */
static gboolean
dbmodel_v24( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v24";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	/* n° 1 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT_CONCIL ("
			"       BAT_LINE_ID       BIGINT      NOT NULL           COMMENT 'BAT line identifier',"
			"       BAT_REC_ENTRY     BIGINT      NOT NULL           COMMENT 'Entry the BAT line was reconciliated against',"
			"       BAT_REC_UPD_USER  VARCHAR(20)                    COMMENT 'User responsible of the reconciliation',"
			"       BAT_REC_UPD_STAMP TIMESTAMP                      COMMENT 'Reconciliation timestamp',"
			"       UNIQUE (BAT_LINE_ID,BAT_REC_ENTRY)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( model,
			"INSERT INTO OFA_T_BAT_CONCIL "
			"       (BAT_LINE_ID,BAT_REC_ENTRY,BAT_REC_UPD_USER,BAT_REC_UPD_STAMP) "
			"       SELECT BAT_LINE_ID,BAT_LINE_ENTRY,BAT_LINE_UPD_USER,BAT_LINE_UPD_STAMP "
			"         FROM OFA_T_BAT_LINES "
			"           WHERE BAT_LINE_ENTRY IS NOT NULL "
			"           AND BAT_LINE_UPD_USER IS NOT NULL "
			"           AND BAT_LINE_UPD_STAMP!=0" )){
		return( FALSE );
	}

	/* n° 3 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT_LINES "
			"       DROP COLUMN BAT_LINE_ENTRY,"
			"       DROP COLUMN BAT_LINE_UPD_USER,"
			"       DROP COLUMN BAT_LINE_UPD_STAMP" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v24( ofaMysqlDBModel *model )
{
	return( 3 );
}

/*
 * ofa_ddl_update_dbmodel_v25:
 *
 * Define a new b-e reconciliation model where any 'b' bat lines may be
 * reconciliated against any 'e' entries, where 'b' and 'e' may both be
 * equal to zero.
 * This is a rupture from the previous model where the relation was only
 * 1-1.
 */
static gboolean
dbmodel_v25( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v25";
	ofaMysqlDBModelPrivate *priv;
	GSList *result, *irow, *icol;
	ofxCounter last_concil, number, rec_id, bat_id;
	gchar *query, *sdval, *user, *stamp;
	gboolean ok;

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	priv = model->priv;

	last_concil = 0;

	/* n° 1 */
	/* Labels and identifiers are resized in v28 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_CONCIL ("
			"	REC_ID        BIGINT PRIMARY KEY NOT NULL            COMMENT 'Reconciliation identifier',"
			"	REC_DVAL      DATE               NOT NULL            COMMENT 'Bank value date',"
			"	REC_USER  VARCHAR(20)                                COMMENT 'User responsible of the reconciliation',"
			"	REC_STAMP TIMESTAMP                                  COMMENT 'Reconciliation timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( model,
			"CREATE TABLE IF NOT EXISTS OFA_T_CONCIL_IDS ("
			"	REC_ID         BIGINT             NOT NULL           COMMENT 'Reconciliation identifier',"
			"	REC_IDS_TYPE   CHAR(1)            NOT NULL           COMMENT 'Identifier type Bat/Entry',"
			"	REC_IDS_OTHER  BIGINT             NOT NULL           COMMENT 'Bat line identifier or Entry number'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 3 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_DOSSIER "
			"	ADD COLUMN DOS_LAST_CONCIL BIGINT NOT NULL DEFAULT 0 COMMENT 'Last reconciliation identifier used'" )){
		return( FALSE );
	}

	/* not counted */
	if( !ofa_idbconnect_query_ex( priv->connect,
			"SELECT ENT_NUMBER,ENT_CONCIL_DVAL,ENT_CONCIL_USER,ENT_CONCIL_STAMP "
			"	FROM OFA_T_ENTRIES "
			"	WHERE ENT_CONCIL_DVAL IS NOT NULL", &result, TRUE )){
		return( FALSE );
	} else {
		ok = TRUE;
		priv->total += 2*g_slist_length( result );
		for( irow=result ; irow && ok ; irow=irow->next ){
			/* read reconciliated entries */
			icol = ( GSList * ) irow->data;
			number = atol(( gchar * ) icol->data );
			icol = icol->next;
			sdval = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			user = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			stamp = g_strdup(( const gchar * ) icol->data );
			/* allocate a new reconciliation id and insert into main table */
			rec_id = ++last_concil;
			query = g_strdup_printf(
					"INSERT INTO OFA_T_CONCIL "
					"	(REC_ID,REC_DVAL,REC_USER,REC_STAMP) "
					"	VALUES (%ld,'%s','%s','%s')", rec_id, sdval, user, stamp );
			ok = exec_query( model, query );
			g_free( query );
			g_free( stamp );
			g_free( user );
			g_free( sdval );
			if( !ok ){
				break;
			}
			/* insert into table of ids */
			query = g_strdup_printf(
					"INSERT INTO OFA_T_CONCIL_IDS "
					"	(REC_ID,REC_IDS_TYPE,REC_IDS_OTHER) "
					"	VALUES (%ld,'E',%ld)",
					rec_id, number );
			ok = exec_query( model, query );
			g_free( query );
			if( !ok ){
				break;
			}
		}
		ofa_idbconnect_free_results( result );
		if( !ok ){
			return( FALSE );
		}
	}

	/* n° 4 */
	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER SET DOS_LAST_CONCIL=%ld WHERE DOS_ID=%u", last_concil, DOSSIER_ROW_ID );
	ok = exec_query( model, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* not counted */
	if( !ofa_idbconnect_query_ex( priv->connect,
			"SELECT a.BAT_LINE_ID,b.REC_ID "
			"	FROM OFA_T_BAT_CONCIL a, OFA_T_CONCIL_IDS b "
			"	WHERE a.BAT_REC_ENTRY=b.REC_IDS_OTHER "
			"	AND b.REC_IDS_TYPE='E'", &result, TRUE )){
		return( FALSE );
	} else {
		ok = TRUE;
		priv->total += g_slist_length( result );
		for( irow=result ; irow && ok ; irow=irow->next ){
			/* read reconciliated bat lines */
			icol = ( GSList * ) irow->data;
			bat_id = atol(( gchar * ) icol->data );
			icol = icol->next;
			rec_id = atol(( gchar * ) icol->data );
			/* insert into table of ids */
			query = g_strdup_printf(
					"INSERT INTO OFA_T_CONCIL_IDS "
					"	(REC_ID,REC_IDS_TYPE,REC_IDS_OTHER) "
					"	VALUES (%ld,'B',%ld)", rec_id, bat_id );
			ok = exec_query( model, query );
			g_free( query );
		}
		ofa_idbconnect_free_results( result );
		if( !ok ){
			return( FALSE );
		}
	}

	/* n° 5 */
	if( !exec_query( model,
			"DROP TABLE OFA_T_BAT_CONCIL" )){
		return( FALSE );
	}

	/* n° 6 */
	if( !exec_query( model,
			"ALTER TABLE OFA_T_ENTRIES "
			"	DROP COLUMN ENT_CONCIL_DVAL, "
			"	DROP COLUMN ENT_CONCIL_USER, "
			"	DROP COLUMN ENT_CONCIL_STAMP" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v25( ofaMysqlDBModel *model )
{
	return( 6 );
}

/*
 * ofa_ddl_update_dbmodel_v26:
 *
 * - archive the last entry number when opening an exercice as an audit
 *   trace
 * - add the row number in rate validity details in order to let the
 *   user reorder the lines
 * - associate the BAT file with an Openbook account
 * - have a date in order to be able to close a period.
 */
static gboolean
dbmodel_v26( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v26";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	if( !exec_query( model,
			"ALTER TABLE OFA_T_DOSSIER "
			"	ADD COLUMN DOS_LAST_CLOSING DATE COMMENT 'Last closed period',"
			"	ADD COLUMN DOS_PREVEXE_ENTRY BIGINT COMMENT 'last entry number of the previous exercice'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_RATES_VAL "
			"	ADD COLUMN RAT_VAL_ROW INTEGER COMMENT 'Row number of the validity detail line'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT "
			"	ADD COLUMN BAT_ACCOUNT VARCHAR(20) COMMENT 'Associated Openbook account'" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v26( ofaMysqlDBModel *model )
{
	return( 3 );
}

/*
 * ofa_ddl_update_dbmodel_v27:
 *
 * - DOSSIER_STATUS -> DOSSIER_CURRENT Y/N
 * - ACC_TYPE -> ACC_ROOT
 * - OTE_xxx_LOCKED: CHAR(1)
 */
static gboolean
dbmodel_v27( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v27";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	if( !exec_query( model,
			"ALTER TABLE OFA_T_DOSSIER "
			"	ADD COLUMN DOS_SIRET VARCHAR(13) COMMENT 'SIRET',"
			"	CHANGE COLUMN DOS_STATUS "
			"		       DOS_CURRENT CHAR(1) DEFAULT 'Y' COMMENT 'Dossier is current'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_DOSSIER "
			"	SET DOS_CURRENT='Y' WHERE DOS_CURRENT='O'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_DOSSIER "
			"	SET DOS_CURRENT='N' WHERE DOS_CURRENT!='Y' OR DOS_CURRENT IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_ACCOUNTS "
			"	CHANGE COLUMN ACC_TYPE "
			"              ACC_ROOT        CHAR(1) DEFAULT 'N' COMMENT 'Root account',"
			"	CHANGE COLUMN ACC_FORWARD "
			"              ACC_FORWARDABLE CHAR(1) DEFAULT 'N' COMMENT 'Whether the account supports carried forwards'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_ROOT='Y' WHERE ACC_ROOT='R'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_ROOT='N' WHERE ACC_ROOT!='Y' OR ACC_ROOT IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_SETTLEABLE='Y' WHERE ACC_SETTLEABLE='S'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_SETTLEABLE='N' WHERE ACC_SETTLEABLE!='Y' OR ACC_SETTLEABLE IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_RECONCILIABLE='Y' WHERE ACC_RECONCILIABLE='R'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_RECONCILIABLE='N' WHERE ACC_RECONCILIABLE!='Y' OR ACC_RECONCILIABLE IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_FORWARDABLE='Y' WHERE ACC_FORWARDABLE='F'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_FORWARDABLE='N' WHERE ACC_FORWARDABLE!='Y' OR ACC_FORWARDABLE IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_CLOSED='Y' WHERE ACC_CLOSED='C'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_ACCOUNTS "
			"	SET ACC_CLOSED='N' WHERE ACC_CLOSED!='Y' OR ACC_CLOSED IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES "
			"	CHANGE COLUMN OTE_LED_LOCKED OTE_LED_LOCKED2 INTEGER,"
			"	CHANGE COLUMN OTE_REF_LOCKED OTE_REF_LOCKED2 INTEGER" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES "
			"	ADD COLUMN OTE_LED_LOCKED CHAR(1) DEFAULT 'N' COMMENT 'Ledger is locked',"
			"	ADD COLUMN OTE_REF_LOCKED CHAR(1) DEFAULT 'N' COMMENT 'Operation reference is locked'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES "
			"	SET OTE_LED_LOCKED='Y' WHERE OTE_LED_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES "
			"	SET OTE_LED_LOCKED='N' WHERE OTE_LED_LOCKED2=0 OR OTE_LED_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES "
			"	SET OTE_REF_LOCKED='Y' WHERE OTE_REF_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES "
			"	SET OTE_REF_LOCKED='N' WHERE OTE_REF_LOCKED2=0 OR OTE_REF_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES_DET "
			"	CHANGE COLUMN OTE_DET_ACCOUNT_LOCKED OTE_DET_ACCOUNT_LOCKED2 INTEGER,"
			"	CHANGE COLUMN OTE_DET_LABEL_LOCKED OTE_DET_LABEL_LOCKED2 INTEGER,"
			"	CHANGE COLUMN OTE_DET_DEBIT_LOCKED OTE_DET_DEBIT_LOCKED2 INTEGER,"
			"	CHANGE COLUMN OTE_DET_CREDIT_LOCKED OTE_DET_CREDIT_LOCKED2 INTEGER" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES_DET "
			"	ADD COLUMN OTE_DET_ACCOUNT_LOCKED CHAR(1) DEFAULT 'N' COMMENT 'Account number is locked',"
			"	ADD COLUMN OTE_DET_LABEL_LOCKED   CHAR(1) DEFAULT 'N' COMMENT 'Entry label is locked',"
			"	ADD COLUMN OTE_DET_DEBIT_LOCKED   CHAR(1) DEFAULT 'N' COMMENT 'Debit amount is locked',"
			"	ADD COLUMN OTE_DET_CREDIT_LOCKED  CHAR(1) DEFAULT 'N' COMMENT 'Credit amount is locked'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_ACCOUNT_LOCKED='Y' WHERE OTE_DET_ACCOUNT_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_ACCOUNT_LOCKED='N' WHERE OTE_DET_ACCOUNT_LOCKED2=0 OR OTE_DET_ACCOUNT_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_LABEL_LOCKED='Y' WHERE OTE_DET_LABEL_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_LABEL_LOCKED='N' WHERE OTE_DET_LABEL_LOCKED2=0 OR OTE_DET_LABEL_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_DEBIT_LOCKED='Y' WHERE OTE_DET_DEBIT_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_DEBIT_LOCKED='N' WHERE OTE_DET_DEBIT_LOCKED2=0 OR OTE_DET_DEBIT_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_CREDIT_LOCKED='Y' WHERE OTE_DET_CREDIT_LOCKED2!=0" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"UPDATE OFA_T_OPE_TEMPLATES_DET "
			"	SET OTE_DET_CREDIT_LOCKED='N' WHERE OTE_DET_CREDIT_LOCKED2=0 OR OTE_DET_CREDIT_LOCKED2 IS NULL" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_RATES_VAL "
			"	CHANGE COLUMN RAT_VAL_BEG "
			"              RAT_VAL_BEGIN DATE DEFAULT NULL COMMENT 'Validity begin date'" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v27( ofaMysqlDBModel *model )
{
	return( 31 );
}

/*
 * ofa_ddl_update_dbmodel_v28:
 *
 * - Review all identifiers and labels size
 */
static gboolean
dbmodel_v28( ofaMysqlDBModel *model, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v28";

	g_debug( "%s: model=%p, version=%d", thisfn, ( void * ) model, version );

	if( !exec_query( model,
			"ALTER TABLE OFA_T_ACCOUNTS"
			"	MODIFY COLUMN ACC_NUMBER        VARCHAR(64)    BINARY NOT NULL UNIQUE COMMENT 'Account identifier',"
			"   MODIFY COLUMN ACC_LABEL         VARCHAR(256)   NOT NULL               COMMENT 'Account label',"
			"	MODIFY COLUMN ACC_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_AUDIT "
			"	MODIFY COLUMN AUD_QUERY         VARCHAR(65520) NOT NULL               COMMENT 'Query content'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT "
			"	MODIFY COLUMN BAT_FORMAT        VARCHAR(128)                          COMMENT 'Identified file format',"
			"	MODIFY COLUMN BAT_RIB           VARCHAR(128)                          COMMENT 'Bank provided RIB',"
			"	MODIFY COLUMN BAT_ACCOUNT       VARCHAR(64)                           COMMENT 'Associated Openbook account',"
			"	MODIFY COLUMN BAT_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of BAT file import'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_BAT_LINES "
			"	MODIFY COLUMN BAT_LINE_REF      VARCHAR(256)                          COMMENT 'Line reference as recorded by the Bank',"
			"	MODIFY COLUMN BAT_LINE_LABEL    VARCHAR(256)                          COMMENT 'Line label'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_CLASSES "
			"	MODIFY COLUMN CLA_LABEL         VARCHAR(256) NOT NULL                 COMMENT 'Class label',"
			"	MODIFY COLUMN CLA_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_CONCIL "
			"	MODIFY COLUMN REC_USER          VARCHAR(64)                           COMMENT 'User responsible of the reconciliation'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_CURRENCIES "
			"	MODIFY COLUMN CUR_LABEL         VARCHAR(256) NOT NULL                 COMMENT 'Currency label',"
			"	MODIFY COLUMN CUR_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_DOSSIER "
			"	MODIFY COLUMN DOS_FORW_OPE      VARCHAR(64)                           COMMENT 'Operation mnemo for carried forward entries',"
			"	MODIFY COLUMN DOS_IMPORT_LEDGER VARCHAR(64)                           COMMENT 'Default import ledger',"
			"	MODIFY COLUMN DOS_LABEL         VARCHAR(256)                          COMMENT 'Raison sociale',"
			"	MODIFY COLUMN DOS_SIREN         VARCHAR(64)                           COMMENT 'Siren identifier',"
			"	MODIFY COLUMN DOS_SIRET         VARCHAR(64)                           COMMENT 'Siret identifier',"
			"	MODIFY COLUMN DOS_SLD_OPE       VARCHAR(64)                           COMMENT 'Operation mnemo for balancing entries',"
			"	MODIFY COLUMN DOS_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_DOSSIER_CUR "
			"	MODIFY COLUMN DOS_SLD_ACCOUNT   VARCHAR(64)                           COMMENT 'Balancing account when closing the exercice'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_ENTRIES "
			"	MODIFY COLUMN ENT_LABEL         VARCHAR(256)                          COMMENT 'Entry label',"
			"	MODIFY COLUMN ENT_REF           VARCHAR(256)                          COMMENT 'Piece reference',"
			"	MODIFY COLUMN ENT_ACCOUNT       VARCHAR(64)                           COMMENT 'Account identifier',"
			"	MODIFY COLUMN ENT_LEDGER        VARCHAR(64)                           COMMENT 'Ledger identifier',"
			"	MODIFY COLUMN ENT_OPE_TEMPLATE  VARCHAR(64)                           COMMENT 'Operation template identifier',"
			"	MODIFY COLUMN ENT_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of last update',"
			"	MODIFY COLUMN ENT_STLMT_USER    VARCHAR(64)                           COMMENT 'User responsible of the settlement'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_LEDGERS "
			"	MODIFY COLUMN LED_MNEMO         VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Ledger identifier',"
			"	MODIFY COLUMN LED_LABEL         VARCHAR(256) NOT NULL                 COMMENT 'Ledger label',"
			"	MODIFY COLUMN LED_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_LEDGERS_CUR "
			"	MODIFY COLUMN LED_MNEMO         VARCHAR(64)  BINARY NOT NULL          COMMENT 'Ledger identifier'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES "
			"	MODIFY COLUMN OTE_MNEMO         VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Operation template identifier',"
			"	MODIFY COLUMN OTE_LABEL         VARCHAR(256) NOT NULL                 COMMENT 'Operation template label',"
			"	MODIFY COLUMN OTE_LED_MNEMO     VARCHAR(64)                           COMMENT 'Generated entries imputation ledger',"
			"	MODIFY COLUMN OTE_REF           VARCHAR(256)                          COMMENT 'Operation reference',"
			"	MODIFY COLUMN OTE_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_OPE_TEMPLATES_DET "
			"	MODIFY COLUMN OTE_MNEMO         VARCHAR(64)  BINARY NOT NULL          COMMENT 'Operation template identifier',"
			"	MODIFY COLUMN OTE_DET_COMMENT   VARCHAR(128)                          COMMENT 'Detail line comment',"
			"	MODIFY COLUMN OTE_DET_ACCOUNT   VARCHAR(128)                          COMMENT 'Account identifier computing rule',"
			"	MODIFY COLUMN OTE_DET_LABEL     VARCHAR(256)                          COMMENT 'Entry label computing rule',"
			"	MODIFY COLUMN OTE_DET_DEBIT     VARCHAR(128)                          COMMENT 'Debit amount computing rule',"
			"	MODIFY COLUMN OTE_DET_CREDIT    VARCHAR(128)                          COMMENT 'Credit amount computing rule'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_RATES "
			"	MODIFY COLUMN RAT_MNEMO         VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'Rate identifier',"
			"	MODIFY COLUMN RAT_LABEL         VARCHAR(256) NOT NULL                 COMMENT 'Rate label',"
			"	MODIFY COLUMN RAT_UPD_USER      VARCHAR(64)                           COMMENT 'User responsible of properties last update'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_RATES_VAL "
			"	MODIFY COLUMN RAT_MNEMO         VARCHAR(64)  BINARY NOT NULL          COMMENT 'Rate identifier'" )){
		return( FALSE );
	}

	if( !exec_query( model,
			"ALTER TABLE OFA_T_ROLES "
			"	MODIFY COLUMN ROL_USER          VARCHAR(64)  BINARY NOT NULL UNIQUE   COMMENT 'User account'" )){
		return( FALSE );
	}

	return( TRUE );
}

/*
 * returns the count of queries in the dbmodel_vxx
 * to be used as the progression indicator
 */
static gulong
count_v28( ofaMysqlDBModel *model )
{
	return( 17 );
}