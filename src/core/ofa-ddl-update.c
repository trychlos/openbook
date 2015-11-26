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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/my-progress-bar.h"
#include "core/ofa-ddl-update.h"

/* private instance data
 */
struct _ofaDDLUpdatePrivate {

	/* input data
	 */
	ofoDossier          *dossier;

	/* runtime data
	 */
	gint                 cur_version;
	gint                 last_version;
	const ofaIDBConnect *cnx;
	gint                 row;
	gulong               total;				/* for the lastly added row */
	gulong               current;			/* for the lastly added row */

	/* UI
	 */
	GtkWidget           *close_btn;
	GtkWidget           *paned;
	GtkWidget           *grid;
	GtkWidget           *textview;
	GtkTextBuffer       *buffer;
	GtkSizeGroup        *hgroup;
	GtkWidget           *bar;				/* the bar for the lastly added row */

	/* settings
	 */
	gint                 paned_pos;

	/* returned value
	 */
	gboolean             up_to_date;
};

static const gchar      *st_ui_xml      = PKGUIDIR "/ofa-ddl-update.ui";
static const gchar      *st_ui_id       = "DDLUpdateDlg";
static const gchar      *st_settings    = "DDLUpdateDlg-settings";

typedef GType ( *fnType )( void );

static const gchar *st_classes          = INIT1DIR "/classes-h1.csv";
static const gchar *st_currencies       = INIT1DIR "/currencies-h1.csv";
static const gchar *st_ledgers          = INIT1DIR "/ledgers-h1.csv";
static const gchar *st_ope_templates    = INIT1DIR "/ope-templates-h2.csv";
static const gchar *st_rates            = INIT1DIR "/rates-h2.csv";

static gboolean dbmodel_v20( ofaDDLUpdate *dialog, gint version );
static gulong   count_v20( ofaDDLUpdate *dialog );
static gboolean dbmodel_v21( ofaDDLUpdate *dialog, gint version );
static gulong   count_v21( ofaDDLUpdate *dialog );
static gboolean dbmodel_v22( ofaDDLUpdate *dialog, gint version );
static gulong   count_v22( ofaDDLUpdate *dialog );
static gboolean dbmodel_v23( ofaDDLUpdate *dialog, gint version );
static gulong   count_v23( ofaDDLUpdate *dialog );
static gboolean dbmodel_v24( ofaDDLUpdate *dialog, gint version );
static gulong   count_v24( ofaDDLUpdate *dialog );
static gboolean dbmodel_v25( ofaDDLUpdate *dialog, gint version );
static gulong   count_v25( ofaDDLUpdate *dialog );
static gboolean dbmodel_v26( ofaDDLUpdate *dialog, gint version );
static gulong   count_v26( ofaDDLUpdate *dialog );

/* the functions which update the DB model
 */
typedef struct {
	gint        ver_target;
	gboolean ( *fnquery )( ofaDDLUpdate *dialog, gint version );
	gulong   ( *fncount )( ofaDDLUpdate *dialog );
}
	sMigration;

static sMigration st_migrates[] = {
		{ 20, dbmodel_v20, count_v20 },
		{ 21, dbmodel_v21, count_v21 },
		{ 22, dbmodel_v22, count_v22 },
		{ 23, dbmodel_v23, count_v23 },
		{ 24, dbmodel_v24, count_v24 },
		{ 25, dbmodel_v25, count_v25 },
		{ 26, dbmodel_v26, count_v26 },
		{ 0 }
};

G_DEFINE_TYPE( ofaDDLUpdate, ofa_ddl_update, MY_TYPE_DIALOG )

static gint       get_last_version( void );
static gboolean   run_dialog( ofoDossier *dossier, gint cur_version, gint last_version );
static void       v_init_dialog( myDialog *dialog );
static gboolean   do_run( ofaDDLUpdate *self );
static gboolean   upgrade_to( ofaDDLUpdate *self, sMigration *smig );
static GtkWidget *add_row( ofaDDLUpdate *self, const gchar *title, gboolean with_bar );
static gboolean   exec_query( ofaDDLUpdate *self, const gchar *query );
static gboolean   version_begin( ofaDDLUpdate *self, gint version );
static gboolean   version_end( ofaDDLUpdate *self, gint version );
static gboolean   insert_classes( ofaDDLUpdate *self );
static gboolean   insert_currencies( ofaDDLUpdate *self );
static gboolean   insert_ledgers( ofaDDLUpdate *self );
static gboolean   insert_ope_templates( ofaDDLUpdate *self );
static gboolean   insert_rates( ofaDDLUpdate *self );
static gboolean   import_utf8_comma_pipe_file( ofaDDLUpdate *self, const gchar *table, const gchar *fname, gint headers, fnType fn );
static gint       count_rows( const ofoDossier *dossier, const gchar *table );
static void       set_bar_progression( ofaDDLUpdate *self );
static void       load_settings( ofaDDLUpdate *self );
static void       write_settings( ofaDDLUpdate *self );

static void
ddl_update_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ddl_update_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DDL_UPDATE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ddl_update_parent_class )->finalize( instance );
}

static void
ddl_update_dispose( GObject *instance )
{
	ofaDDLUpdatePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DDL_UPDATE( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_DDL_UPDATE( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->hgroup );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ddl_update_parent_class )->dispose( instance );
}

static void
ofa_ddl_update_init( ofaDDLUpdate *self )
{
	static const gchar *thisfn = "ofa_ddl_update_init";

	g_return_if_fail( self && OFA_IS_DDL_UPDATE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DDL_UPDATE, ofaDDLUpdatePrivate );

	self->priv->up_to_date = FALSE;
	self->priv->row = 0;
}

static void
ofa_ddl_update_class_init( ofaDDLUpdateClass *klass )
{
	static const gchar *thisfn = "ofa_ddl_update_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ddl_update_dispose;
	G_OBJECT_CLASS( klass )->finalize = ddl_update_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;

	g_type_class_add_private( klass, sizeof( ofaDDLUpdatePrivate ));
}

/**
 * ofa_ddl_update_run:
 * @dossier: the dossier being opened.
 *
 * Make sure the database is up to date.
 *
 * Returns: %TRUE if the database is up to date.
 */
gboolean
ofa_ddl_update_run( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofa_ddl_update_run";
	gboolean up_to_date;
	gint cur_version, last_version;

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	up_to_date = TRUE;
	last_version = get_last_version();
	cur_version = ofo_dossier_get_database_version( dossier );
	g_debug( "%s: cur_version=%d, last_version=%d", thisfn, cur_version, last_version );

	if( cur_version < last_version ){
		up_to_date = run_dialog( dossier, cur_version, last_version );
	}

	return( up_to_date );
}

static gint
get_last_version( void )
{
	gint last_version, i;

	last_version = 0;

	for( i=0 ; st_migrates[i].ver_target ; ++i ){
		if( last_version < st_migrates[i].ver_target ){
			last_version = st_migrates[i].ver_target;
		}
	}

	return( last_version );
}

/*
 * this is only run if database needs an update
 */
static gboolean
run_dialog( ofoDossier *dossier, gint cur_version, gint last_version )
{
	ofaDDLUpdate *self;
	ofaDDLUpdatePrivate *priv;
	gboolean up_to_date;

	self = g_object_new(
				OFA_TYPE_DDL_UPDATE,
				MY_PROP_WINDOW_XML,    st_ui_xml,
				MY_PROP_WINDOW_NAME,   st_ui_id,
				NULL );

	priv = self->priv;
	priv->dossier = dossier;
	priv->cur_version = cur_version;
	priv->last_version = last_version;
	priv->up_to_date = FALSE;

	load_settings( self );
	my_dialog_run_dialog( MY_DIALOG( self ));
	write_settings( self );

	up_to_date = priv->up_to_date;
	g_object_unref( self );

	return( up_to_date );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDDLUpdatePrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *label;
	gchar *str;

	priv = OFA_DDL_UPDATE( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-close" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	priv->hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	priv->paned = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-paned" );
	g_return_if_fail( priv->paned && GTK_IS_PANED( priv->paned ));
	gtk_paned_set_position( GTK_PANED( priv->paned ), priv->paned_pos );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-open-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-current-version" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	str = g_strdup_printf( "%d", priv->cur_version );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-last-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-last-version" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	str = g_strdup_printf( "%d", priv->last_version );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	priv->grid = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-grid" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));

	priv->textview = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dud-textview" );
	g_return_if_fail( priv->textview && GTK_IS_TEXT_VIEW( priv->textview ));
	priv->buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( priv->buffer, "", -1 );
	gtk_text_view_set_buffer( GTK_TEXT_VIEW( priv->textview ), priv->buffer );

	g_idle_add(( GSourceFunc ) do_run, dialog );
}

static gboolean
do_run( ofaDDLUpdate *self )
{
	static const gchar *thisfn = "ofa_ddl_update_do_run";
	ofaDDLUpdatePrivate *priv;
	gint i;
	gboolean ok;
	GtkWidget *dlg;
	gchar *str;

	priv = self->priv;
	ok = TRUE;
	priv->cnx = ofo_dossier_get_connect( priv->dossier );

	for( i=0 ; st_migrates[i].ver_target ; ++i ){
		if( priv->cur_version < st_migrates[i].ver_target ){
			if( !upgrade_to( self, &st_migrates[i] )){
				g_warning( "%s: current DBMS model is version %d, unable to update it to v %d",
						thisfn, priv->cur_version, st_migrates[i].ver_target );
				ok = FALSE;
				break;
			}
		}
	}
	if( ok ){
		insert_classes( self );
		insert_currencies( self );
		insert_ledgers( self );
		insert_ope_templates( self );
		insert_rates( self );
	}

	priv->up_to_date = ok;

	if( ok ){
		str = g_strdup_printf(
				_( "The database has been successfully upgraded to v %d" ),
				priv->last_version );
	} else {
		str = g_strdup( "An error has occured while upgrading the database model" );
	}
	dlg = gtk_message_dialog_new(
				my_window_get_toplevel( MY_WINDOW( self )),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
	g_free( str );

	gtk_widget_set_sensitive( priv->close_btn, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static gboolean
upgrade_to( ofaDDLUpdate *self, sMigration *smig )
{
	ofaDDLUpdatePrivate *priv;
	gboolean ok;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( _( "Upgrading to v %d :" ), smig->ver_target );
	priv->bar = add_row( self, str, TRUE );
	g_free( str );

	priv->total = smig->fncount( self )+3;	/* counting version_begin+version_end */
	priv->current = 0;

	ok = version_begin( self, smig->ver_target ) &&
			smig->fnquery( self, smig->ver_target ) &&
			version_end( self, smig->ver_target );

	return( ok );
}

/*
 * if with_bar, then add a progress bar in column 1
 * else add a label
 */
static GtkWidget *
add_row( ofaDDLUpdate *self, const gchar *title, gboolean with_bar )
{
	ofaDDLUpdatePrivate *priv;
	GtkWidget *label;
	myProgressBar *bar;
	gint row;

	priv = self->priv;
	row = priv->row++;
	bar = NULL;

	label = gtk_label_new( title );
	gtk_label_set_xalign( GTK_LABEL( label ), 1 );
	gtk_grid_attach( GTK_GRID( priv->grid ), label, 0, row, 1, 1 );
	gtk_size_group_add_widget( priv->hgroup, label );

	if( with_bar ){
		bar = my_progress_bar_new();
		gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( bar ), 1, row, 1, 1 );
	} else {
		label = gtk_label_new( title );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		gtk_grid_attach( GTK_GRID( priv->grid ), label, 1, row, 1, 1 );
	}

	gtk_widget_show_all( priv->grid );

	return( with_bar ? GTK_WIDGET( bar ) : label );
}

static gboolean
exec_query( ofaDDLUpdate *self, const gchar *query )
{
	ofaDDLUpdatePrivate *priv;
	gboolean ok;
	GtkTextIter iter;
	gchar *str;

	priv = self->priv;

	gtk_text_buffer_get_end_iter( priv->buffer, &iter );
	str = g_strdup_printf( "%s\n", query );
	gtk_text_buffer_insert( priv->buffer, &iter, str, -1 );
	g_free( str );
	gtk_text_buffer_get_end_iter( priv->buffer, &iter );
	gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( priv->textview ), &iter, 0, FALSE, 0, 0 );

	ok = ofa_idbconnect_query( priv->cnx, query, TRUE );

	priv->current += 1;
	set_bar_progression( self );

	return( ok );
}

static gboolean
version_begin( ofaDDLUpdate *self, gint version )
{
	gboolean ok;
	gchar *query;

	/* default value for timestamp cannot be null */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER   NOT NULL UNIQUE DEFAULT 0 COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP                 DEFAULT 0 COMMENT 'Version application timestamp') "
			"CHARACTER SET utf8" )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

static gboolean
version_end( ofaDDLUpdate *self, gint version )
{
	gchar *query;
	gboolean ok;

	/* we do this only at the end of the DB model udpate
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE OFA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", version );
	ok = exec_query( self, query );
	g_free( query );

	return( ok );
}

/*
 * dbmodel_v20:
 *
 * This is the initial creation of the schema
 */
static gboolean
dbmodel_v20( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v20";
	ofaDDLUpdatePrivate *priv;
	gchar *query;
	gboolean ok;

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	priv = self->priv;

	/* n° 1 */
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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

	/* n° 7 */
	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_DOSSIER "
			"	(DOS_ID,DOS_LABEL,DOS_EXE_LENGTH,DOS_DEF_CURRENCY,"
			"	 DOS_STATUS,DOS_FORW_OPE,DOS_SLD_OPE) "
			"	VALUES (1,'%s',%u,'EUR','%s','%s','%s')",
			ofo_dossier_get_name( priv->dossier ),
			DOS_DEFAULT_LENGTH, DOS_STATUS_OPENED, "CLORAN", "CLOSLD" );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* n° 8 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER_CUR ("
			"	DOS_ID               INTEGER   NOT NULL              COMMENT 'Row identifier',"
			"	DOS_CURRENCY         VARCHAR(3)                      COMMENT 'Currency identifier',"
			"	DOS_SLD_ACCOUNT      VARCHAR(20)                     COMMENT 'Balancing account when closing the exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_CURRENCY)"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 9 */
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
count_v20( ofaDDLUpdate *self )
{
	return( 15 );
}

/*
 * ofa_ddl_update_dbmodel_v21:
 * have zero timestamp on unreconciliated batlines
 */
static gboolean
dbmodel_v21( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v21";

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	/* n° 1 */
	if( !exec_query( self,
			"ALTER TABLE OFA_T_BAT_LINES "
			"	MODIFY COLUMN BAT_LINE_UPD_STAMP TIMESTAMP DEFAULT 0 "
			"	COMMENT 'Reconciliation timestamp'" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( self,
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
count_v21( ofaDDLUpdate *self )
{
	return( 2 );
}

/*
 * ofa_ddl_update_dbmodel_v22:
 * have begin_solde and end_solde in bat
 */
static gboolean
dbmodel_v22( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v22";

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	/* n° 1 */
	if( !exec_query( self,
			"ALTER TABLE OFA_T_BAT "
			"	CHANGE COLUMN BAT_SOLDE BAT_SOLDE_END DECIMAL(20,5) "
			"	COMMENT 'Signed end balance of the account'" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( self,
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
count_v22( ofaDDLUpdate *self )
{
	return( 2 );
}

/*
 * ofa_ddl_update_dbmodel_v23:
 * closed accounts
 */
static gboolean
dbmodel_v23( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v23";

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	/* n° 1 */
	if( !exec_query( self,
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
count_v23( ofaDDLUpdate *self )
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
dbmodel_v24( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v24";

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	/* n° 1 */
	if( !exec_query( self,
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
	if( !exec_query( self,
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
	if( !exec_query( self,
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
count_v24( ofaDDLUpdate *self )
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
dbmodel_v25( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v25";
	ofaDDLUpdatePrivate *priv;
	GSList *result, *irow, *icol;
	ofxCounter last_concil, number, rec_id, bat_id;
	gchar *query, *sdval, *user, *stamp;
	gboolean ok;

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	priv = self->priv;
	last_concil = 0;

	/* n° 1 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS OFA_T_CONCIL ("
			"	REC_ID        BIGINT PRIMARY KEY NOT NULL            COMMENT 'Reconciliation identifier',"
			"	REC_DVAL      DATE               NOT NULL            COMMENT 'Bank value date',"
			"	REC_USER  VARCHAR(20)                                COMMENT 'User responsible of the reconciliation',"
			"	REC_STAMP TIMESTAMP                                  COMMENT 'Reconciliation timestamp'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 2 */
	if( !exec_query( self,
			"CREATE TABLE IF NOT EXISTS OFA_T_CONCIL_IDS ("
			"	REC_ID         BIGINT             NOT NULL           COMMENT 'Reconciliation identifier',"
			"	REC_IDS_TYPE   CHAR(1)            NOT NULL           COMMENT 'Identifier type Bat/Entry',"
			"	REC_IDS_OTHER  BIGINT             NOT NULL           COMMENT 'Bat line identifier or Entry number'"
			") CHARACTER SET utf8" )){
		return( FALSE );
	}

	/* n° 3 */
	if( !exec_query( self,
			"ALTER TABLE OFA_T_DOSSIER "
			"	ADD COLUMN DOS_LAST_CONCIL BIGINT NOT NULL DEFAULT 0 COMMENT 'Last reconciliation identifier used'" )){
		return( FALSE );
	}

	/* not counted */
	if( !ofa_idbconnect_query_ex( priv->cnx,
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
			ok = exec_query( self, query );
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
			ok = exec_query( self, query );
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
			"UPDATE OFA_T_DOSSIER SET DOS_LAST_CONCIL=%ld WHERE DOS_ID=%u", last_concil, THIS_DOS_ID );
	ok = exec_query( self, query );
	g_free( query );
	if( !ok ){
		return( FALSE );
	}

	/* not counted */
	if( !ofa_idbconnect_query_ex( priv->cnx,
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
			ok = exec_query( self, query );
			g_free( query );
		}
		ofa_idbconnect_free_results( result );
		if( !ok ){
			return( FALSE );
		}
	}

	/* n° 5 */
	if( !exec_query( self,
			"DROP TABLE OFA_T_BAT_CONCIL" )){
		return( FALSE );
	}

	/* n° 6 */
	if( !exec_query( self,
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
count_v25( ofaDDLUpdate *self )
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
dbmodel_v26( ofaDDLUpdate *self, gint version )
{
	static const gchar *thisfn = "ofa_ddl_update_dbmodel_v26";

	g_debug( "%s: self=%p, version=%d", thisfn, ( void * ) self, version );

	if( !exec_query( self,
			"ALTER TABLE OFA_T_DOSSIER "
			"	ADD COLUMN DOS_LAST_CLOSING DATE COMMENT 'Last closed period',"
			"	ADD COLUMN DOS_PREVEXE_ENTRY BIGINT COMMENT 'last entry number of the previous exercice'" )){
		return( FALSE );
	}

	if( !exec_query( self,
			"ALTER TABLE OFA_T_RATES_VAL "
			"	ADD COLUMN RAT_VAL_ROW INTEGER COMMENT 'Row number of the validity detail line'" )){
		return( FALSE );
	}

	if( !exec_query( self,
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
count_v26( ofaDDLUpdate *self )
{
	return( 3 );
}

static gboolean
insert_classes( ofaDDLUpdate *self )
{
	return( import_utf8_comma_pipe_file(
					self, "OFA_T_CLASSES", st_classes, 1, ofo_class_get_type ));
}

static gboolean
insert_currencies( ofaDDLUpdate *self )
{
	return( import_utf8_comma_pipe_file(
					self, "OFA_T_CURRENCIES", st_currencies, 1, ofo_currency_get_type ));
}

static gboolean
insert_ledgers( ofaDDLUpdate *self )
{
	return( import_utf8_comma_pipe_file(
					self, "OFA_T_LEDGERS", st_ledgers, 1, ofo_ledger_get_type ));
}

static gboolean
insert_ope_templates( ofaDDLUpdate *self )
{
	return( import_utf8_comma_pipe_file(
					self, "OFA_T_OPE_TEMPLATES", st_ope_templates, 2, ofo_ope_template_get_type ));
}

static gboolean
insert_rates( ofaDDLUpdate *self )
{
	return( import_utf8_comma_pipe_file(
					self, "OFA_T_RATES", st_rates, 2, ofo_rate_get_type ));
}

static gboolean
import_utf8_comma_pipe_file( ofaDDLUpdate *self, const gchar *table, const gchar *fname, gint headers, fnType fn )
{
	ofaDDLUpdatePrivate *priv;
	gint count;
	gboolean ok;
	ofaFileFormat *settings;
	ofoBase *object;
	gchar *uri;
	gchar *str;
	GtkWidget *label;

	priv = self->priv;
	ok = TRUE;
	count = count_rows( priv->dossier, table );
	if( !count ){
		str = g_strdup_printf( "Importing into %s :", table );
		label = add_row( self, str, FALSE );
		g_free( str );

		settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
		ofa_file_format_set( settings,
				NULL, OFA_FFTYPE_CSV, OFA_FFMODE_IMPORT, "UTF-8", MY_DATE_SQL, ',', '|', headers );
		object = g_object_new( fn(), NULL );
		uri = g_filename_to_uri( fname, NULL, NULL );
		count = ofa_dossier_misc_import_csv(
						priv->dossier, OFA_IIMPORTABLE( object ), uri, settings, NULL, NULL );
		ok = ( count > 0 );
		g_free( uri );
		g_object_unref( object );
		g_object_unref( settings );

		str = g_strdup_printf( _( "%d lines" ), count );
		gtk_label_set_text( GTK_LABEL( label ), str );
		gtk_widget_show( label );
		g_free( str );
	}

	return( ok );
}

static gint
count_rows( const ofoDossier *dossier, const gchar *table )
{
	gint count;
	gchar *query;

	query = g_strdup_printf( "SELECT COUNT(*) FROM %s", table );
	ofa_idbconnect_query_int( ofo_dossier_get_connect( dossier ), query, &count, TRUE );
	g_free( query );

	return( count );
}

static void
set_bar_progression( ofaDDLUpdate *self )
{
	ofaDDLUpdatePrivate *priv;
	gdouble progress;
	gchar *text;

	priv = self->priv;

	if( priv->total > 0 ){
		progress = ( gdouble ) priv->current / ( gdouble ) priv->total;
		g_signal_emit_by_name( priv->bar, "ofa-double", progress );
	}
	text = g_strdup_printf( "%lu/%lu", priv->current, priv->total );
	g_signal_emit_by_name( priv->bar, "ofa-text", text );
	g_free( text );
}

/*
 * settings are a string list, with:
 * - paned pos
 */
static void
load_settings( ofaDDLUpdate *self )
{
	ofaDDLUpdatePrivate *priv;
	GList *slist, *it;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_settings );
	it = slist ? slist : NULL;
	priv->paned_pos = it ? atoi( it->data ) : 50;

	ofa_settings_free_string_list( slist );
}

static void
write_settings( ofaDDLUpdate *self )
{
	ofaDDLUpdatePrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( priv->paned )));

	ofa_settings_set_string( st_settings, str );

	g_free( str );
}
