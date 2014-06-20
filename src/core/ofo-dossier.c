/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "core/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-devise.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"
#include "api/ofo-model.h"
#include "api/ofo-sgbd.h"

#include "core/ofo-marshal.h"

/* priv instance data
 */
typedef struct _sDetailExe              sDetailExe;

struct _ofoDossierPrivate {

	/* internals
	 */
	gchar      *name;
	ofoSgbd    *sgbd;
	gchar      *userid;

	/* row id 1
	 */
	gchar      *label;					/* raison sociale */
	gint        duree_exe;				/* exercice length (in month) */
	gchar      *devise;
	gchar      *notes;					/* notes */
	gchar      *maj_user;
	GTimeVal    maj_stamp;

	/* all found exercices are loaded on opening
	 * as a GList * of sDetailExe structures
	 */
	GList      *exes;

	/* a pointer to the current exercice
	 * it is specially kept as we often need these infos
	 */
	sDetailExe *current;
};

struct _sDetailExe {
	gint             exe_id;
	GDate            exe_deb;
	GDate            exe_fin;
	gint             last_ecr;
	ofaDossierStatus status;
};

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	VALIDATED_ENTRY,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofoDossier, ofo_dossier, OFO_TYPE_BASE )

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1
#define THIS_DOS_ID                     1

static void        connect_objects_handlers( const ofoDossier *dossier );
static void        on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static gint        dbmodel_get_version( ofoSgbd *sgbd );
static gboolean    dbmodel_to_v1( ofoSgbd *sgbd, const gchar *account );
static sDetailExe *get_current_exe( const ofoDossier *dossier );
static sDetailExe *get_exe_by_id( const ofoDossier *dossier, gint exe_id );
static sDetailExe *get_exe_by_date( const ofoDossier *dossier, const GDate *date );
static void        on_new_object_cleanup_handler( ofoDossier *dossier, ofoBase *object );
static void        on_updated_object_cleanup_handler( ofoDossier *dossier, ofoBase *object, const gchar *prev_id );
static void        on_deleted_object_cleanup_handler( ofoDossier *dossier, ofoBase *object );
static void        on_reloaded_dataset_cleanup_handler( ofoDossier *dossier, GType type );
static void        on_validated_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry );
static gboolean    dossier_do_read( ofoDossier *dossier );
static gboolean    dossier_read_properties( ofoDossier *dossier );
static gboolean    dossier_read_exercices( ofoDossier *dossier );
static gboolean    dossier_do_update( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user );
static gboolean    do_update_properties( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user );
static gboolean    do_update_current_exe( ofoDossier *dossier, const ofoSgbd *sgbd );

static void
ofo_dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_finalize";
	ofoDossierPrivate *priv;

	priv = OFO_DOSSIER( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->name );
	g_free( priv->userid );

	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->maj_user );

	g_list_free_full( priv->exes, ( GDestroyNotify ) g_free );

	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->finalize( instance );
}

static void
ofo_dossier_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_dispose";
	ofoDossierPrivate *priv;

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv = OFO_DOSSIER( instance )->private;

		/* unref object members here */
		if( priv->sgbd ){
			g_clear_object( &priv->sgbd );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->dispose( instance );
}

static void
ofo_dossier_init( ofoDossier *self )
{
	static const gchar *thisfn = "ofo_dossier_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoDossierPrivate, 1 );
}

static void
ofo_dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_dossier_finalize;

	/**
	 * ofoDossier::ofa-signal-new-object:
	 *
	 * The signal is emitted after a new object has been successfully
	 * inserted in the SGBD. A connected handler may take advantage of
	 * this signal e.g. to update its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoBase      *object,
	 * 								gpointer      user_data );
	 */
	st_signals[ NEW_OBJECT ] = g_signal_new_class_handler(
				OFA_SIGNAL_NEW_OBJECT,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_new_object_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofoDossier::ofa-signal-updated-object:
	 *
	 * The signal is emitted just after an object has been successfully
	 * updated in the SGBD. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects,
	 * or for updating its internal links, or so.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoBase      *object,
	 * 								const gchar  *prev_id,
	 * 								gpointer      user_data );
	 */
	st_signals[ UPDATED_OBJECT ] = g_signal_new_class_handler(
				OFA_SIGNAL_UPDATED_OBJECT,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_updated_object_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				ofo_cclosure_marshal_VOID__OBJECT_STRING,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_STRING );

	/**
	 * ofoDossier::ofa-signal-deleted-object:
	 *
	 * The signal is emitted just after an object has been successfully
	 * deleted from the SGBD. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoBase      *object,
	 * 								gpointer      user_data );
	 */
	st_signals[ DELETED_OBJECT ] = g_signal_new_class_handler(
				OFA_SIGNAL_DELETED_OBJECT,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_deleted_object_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofoDossier::ofa-signal-reload-dataset:
	 *
	 * The signal is emitted when such an update has been made in the
	 * SGBD that it is considered easier for a connected handler just
	 * to reload the whole dataset if this later whishes keep
	 * synchronized with the database.
	 *
	 * This signal is so less an information signal that an action hint.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								GType     type_class,
	 * 								gpointer  user_data );
	 */
	st_signals[ RELOAD_DATASET ] = g_signal_new_class_handler(
				OFA_SIGNAL_RELOAD_DATASET,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_reloaded_dataset_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_GTYPE );

	/**
	 * ofoDossier::ofa-signal-validated-entry:
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoEntry *entry,
	 * 								gpointer  user_data );
	 */
	st_signals[ VALIDATED_ENTRY ] = g_signal_new_class_handler(
				OFA_SIGNAL_VALIDATED_ENTRY,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_validated_entry_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofo_dossier_new:
 */
ofoDossier *
ofo_dossier_new( const gchar *name )
{
	ofoDossier *dossier;

	dossier = g_object_new( OFO_TYPE_DOSSIER, NULL );
	dossier->private->name = g_strdup( name );

	return( dossier );
}

static gboolean
check_user_exists( ofoSgbd *sgbd, const gchar *account )
{
	gchar *query;
	GSList *res;
	gboolean exists;

	exists = FALSE;
	query = g_strdup_printf( "SELECT ROL_USER FROM OFA_T_ROLES WHERE ROL_USER='%s'", account );
	res = ofo_sgbd_query_ex( sgbd, query );
	if( res ){
		gchar *s = ( gchar * )(( GSList * ) res->data )->data;
		if( !g_utf8_collate( account, s )){
			exists = TRUE;
		}
		ofo_sgbd_free_result( res );
	}
	g_free( query );

	return( exists );
}

static void
error_user_not_exists( ofoDossier *dossier, const gchar *account )
{
	GtkMessageDialog *dlg;
	gchar *str;

	str = g_strdup_printf(
			_( "'%s' account is not allowed to connect to '%s' dossier" ),
			account, dossier->private->name );

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str ));

	g_free( str );
	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/**
 * ofo_dossier_open:
 */
gboolean
ofo_dossier_open( ofoDossier *dossier,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofo_dossier_open";
	ofoSgbd *sgbd;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	g_debug( "%s: dossier=%p, host=%s, port=%d, socket=%s, dbname=%s, account=%s, password=%s",
			thisfn,
			( void * ) dossier, host, port, socket, dbname, account, password );

	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect( sgbd,
			host,
			port,
			socket,
			dbname,
			account,
			password )){

		g_object_unref( sgbd );
		return( FALSE );
	}

	if( !check_user_exists( sgbd, account )){
		error_user_not_exists( dossier, account );
		g_object_unref( sgbd );
		return( FALSE );
	}

	dossier->private->sgbd = sgbd;
	dossier->private->userid = g_strdup( account );

	ofo_dossier_dbmodel_update( sgbd, account );
	connect_objects_handlers( dossier );

	return( dossier_do_read( dossier ));
}

/*
 * be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself
 *
 * use case: the intermediate closing by journal may be run without
 * having first loaded the accounts, but the accounts should be
 * connected in order to update themselves.
 */
static void
connect_objects_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_connect_objects_handlers";

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	ofo_account_connect_handlers( dossier );
	ofo_entry_connect_handlers( dossier );
	ofo_journal_connect_handlers( dossier );
	ofo_model_connect_handlers( dossier );

	g_signal_connect( G_OBJECT( dossier ),
				OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );
}

static void
on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_dossier_on_updated_object";
	const gchar *code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_DEVISE( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_devise_get_code( OFO_DEVISE( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( dossier, prev_id, code );
			}
		}
	}
}

static void
on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_DEV_CODE='%s' WHERE DOS_DEV_CODE='%s'", code, prev_id );

	ofo_sgbd_query( ofo_dossier_get_sgbd( dossier ), query );

	g_free( query );
}

/**
 * ofo_dossier_dbmodel_update:
 * @sgbd: an already opened connection
 * @parent: the GtkWindow which will be the parent of the error dialog, if any
 * @account: the account which has opened this connection; it will be checked
 *  against its permissions when trying to update the data model
 *
 * Update the DB model in the SGBD
 */
gboolean
ofo_dossier_dbmodel_update( ofoSgbd *sgbd, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_update";
	gint cur_version;

	g_debug( "%s: sgbd=%p, account=%s", thisfn, ( void * ) sgbd, account );

	cur_version = dbmodel_get_version( sgbd );
	g_debug( "%s: cur_version=%d, THIS_DBMODEL_VERSION=%d", thisfn, cur_version, THIS_DBMODEL_VERSION );

	if( cur_version < THIS_DBMODEL_VERSION ){
		if( cur_version < 1 ){
			dbmodel_to_v1( sgbd, account );
		}
	}

	return( TRUE );
}

/*
 * returns the last complete version
 * i.e. une version where the version date is set
 */
static gint
dbmodel_get_version( ofoSgbd *sgbd )
{
	GSList *res;
	gint vmax = 0;

	res = ofo_sgbd_query_ex( sgbd,
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0" );
	if( res ){
		gchar *s = ( gchar * )(( GSList * ) res->data )->data;
		if( s ){
			vmax = atoi( s );
		}
		ofo_sgbd_free_result( res );
	}

	return( vmax );
}

/**
 * ofo_dossier_dbmodel_to_v1:
 */
static gboolean
dbmodel_to_v1( ofoSgbd *sgbd, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_to_v1";
	gchar *query;

	g_debug( "%s: sgbd=%p, account=%s", thisfn, ( void * ) sgbd, account );

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query actual timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query')")){
		return( FALSE );
	}

	/* default value for timestamp cannot be null */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER NOT NULL UNIQUE DEFAULT 0     COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP DEFAULT 0                   COMMENT 'Version application timestamp')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (1, 0)" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ROLES ("
				"ROL_USER     VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'User account',"
				"ROL_IS_ADMIN INTEGER                            COMMENT 'Whether the user has administration role')")){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_ROLES "
			"	(ROL_USER, ROL_IS_ADMIN) VALUES ('%s',1)", account );
	if( !ofo_sgbd_query( sgbd, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	/* defined post v1 */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS ("
			"	ASS_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern asset identifier',"
			"	ASS_LABEL     VARCHAR(80)                 COMMENT 'Asset label',"
			"	ASS_DATE_IN   DATE                        COMMENT 'Entry date',"
			"	ASS_TOTAL     DECIMAL(15,5)               COMMENT 'Total payed (TVA inc.)',"
			"	ASS_IMMO      DECIMAL(15,5)               COMMENT 'Montant immobilisé',"
			"	ASS_IMMO_FISC INTEGER                     COMMENT 'Montant fiscal immobilisé (à amortir)',"
			"	ASS_DUREE     INTEGER                     COMMENT 'Durée d\\'amortissement',"
			"	ASS_TYPE      VARCHAR(1)                  COMMENT 'Type d\\'ammortissement,"
			"	ASS_COEF_DEG  DECIMAL(15,5)               COMMENT 'Coefficient dégressif',"
			"	ASS_TAUX      DECIMAL(15,5)               COMMENT 'Taux d\\'amortissement',"
			"	ASS_DATE_OUT  DATE                        COMMENT 'Out date',"
			"	ASS_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	ASS_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of last update',"
			"	ASS_MAJ_STAMP TIMESTAMP                   COMMENT 'Last update timestamp'"
			")" )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS_EXE ("
			"	ASS_ID           INTEGER                  COMMENT 'Intern asset identifier',"
			"	ASS_EXE_NUM      INTEGER                  COMMENT 'Numéro d\\'annuité',"
			"	ASS_EXE_DUREE    INTEGER                  COMMENT 'Duree (en mois)',"
			"	ASS_EXE_PREV     INTEGER                  COMMENT 'Total des amortissements antérieurs',"
			"	ASS_EXE_TAUX_LIN DECIMAL(15,5)            COMMENT 'Taux lineaire',"
			"	ASS_EXE_TAUX_DEG DECIMAL(15,5)            COMMENT 'Taux degressif',"
			"	ASS_EXE_AMORT    INTEGER                  COMMENT 'Montant de l\\'annuite',"
			"	ASS_EXE_REST     INTEGER                  COMMENT 'Valeur residuelle'"
			"	CONSTRAINT PRIMARY KEY (ASS_ID,ASS_EXE_NUM)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT ("
			"	BAT_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern import identifier',"
			"	BAT_URI       VARCHAR(128)                COMMENT 'Imported URI',"
			"	BAT_FORMAT    VARCHAR(80)                 COMMENT 'Identified file format',"
			"	BAT_COUNT     INTEGER                     COMMENT 'Imported lines count',"
			"	BAT_BEGIN     DATE                        COMMENT 'Begin date of the transaction list',"
			"	BAT_END       DATE                        COMMENT 'End date of the transaction list',"
			"	BAT_RIB       VARCHAR(80)                 COMMENT 'Bank provided RIB',"
			"	BAT_DEVISE    VARCHAR(3)                  COMMENT 'Account currency',"
			"	BAT_SOLDE     DECIMAL(15,5)               COMMENT 'Signed balance of the account',"
			"	BAT_NOTES     VARCHAR(4096)               COMMENT 'Import notes',"
			"	BAT_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of import',"
			"	BAT_MAJ_STAMP TIMESTAMP                   COMMENT 'Import timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT_LINES ("
			"	BAT_ID             INTEGER  NOT NULL      COMMENT 'Intern import identifier',"
			"	BAT_LINE_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern imported line identifier',"
			"	BAT_LINE_VALEUR    DATE                   COMMENT 'Effect date',"
			"	BAT_LINE_OPE       DATE                   COMMENT 'Operation date',"
			"	BAT_LINE_REF       VARCHAR(80)            COMMENT 'Bank reference',"
			"	BAT_LINE_LABEL     VARCHAR(80)            COMMENT 'Line label',"
			"	BAT_LINE_DEVISE    VARCHAR(3)             COMMENT 'Line currency',"
			"	BAT_LINE_MONTANT   DECIMAL(15,5)          COMMENT 'Signed amount of the line',"
			"	BAT_LINE_ECR       INTEGER                COMMENT 'Reciliated entry',"
			"	BAT_LINE_MAJ_USER  VARCHAR(20)            COMMENT 'User responsible of the reconciliation',"
			"	BAT_LINE_MAJ_STAMP TIMESTAMP              COMMENT 'Reconciliation timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_CLASSES ("
			"	CLA_NUMBER       INTEGER     NOT NULL UNIQUE   COMMENT 'Class number',"
			"	CLA_LABEL        VARCHAR(80) NOT NULL          COMMENT 'Class label',"
			"	CLA_NOTES        VARCHAR(4096)                 COMMENT 'Class notes',"
			"	CLA_MAJ_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
			"	CLA_MAJ_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (1,'Comptes de capitaux')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (2,'Comptes d\\'immobilisations')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (3,'Comptes de stocks et en-cours')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (4,'Comptes de tiers')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (5,'Comptes financiers')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (6,'Comptes de charges')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (7,'Comptes de produits')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (8,'Comptes spéciaux')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (9,'Comptes analytiques')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_COMPTES ("
			"	CPT_NUMBER       VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
			"	CPT_LABEL        VARCHAR(80)   NOT NULL        COMMENT 'Account label',"
			"	CPT_DEV_CODE     VARCHAR(3)                    COMMENT 'ISO 3A identifier of the currency of the account',"
			"	CPT_NOTES        VARCHAR(4096)                 COMMENT 'Account notes',"
			"	CPT_TYPE         CHAR(1)                       COMMENT 'Account type, values R/D',"
			"	CPT_MAJ_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
			"	CPT_MAJ_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp',"
			"	CPT_DEB_ECR      INTEGER                       COMMENT 'Numéro de la dernière écriture validée imputée au débit',"
			"	CPT_DEB_DATE     DATE                          COMMENT 'Date d\\'effet',"
			"	CPT_DEB_MNT      DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant débiteur écritures validées',"
			"	CPT_CRE_ECR      INTEGER                       COMMENT 'Numéro de la dernière écriture validée imputée au crédit',"
			"	CPT_CRE_DATE     DATE                          COMMENT 'Date d\\'effet',"
			"	CPT_CRE_MNT      DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant créditeur écritures validées',"
			"	CPT_BRO_DEB_ECR  INTEGER                       COMMENT 'Numéro de la dernière écriture en brouillard imputée au débit',"
			"	CPT_BRO_DEB_DATE DATE                          COMMENT 'Date d\\'effet',"
			"	CPT_BRO_DEB_MNT  DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant débiteur écritures en brouillard',"
			"	CPT_BRO_CRE_ECR  INTEGER                       COMMENT 'Numéro de la dernière écriture de brouillard imputée au crédit',"
			"	CPT_BRO_CRE_DATE DATE                          COMMENT 'Date d\\'effet',"
			"	CPT_BRO_CRE_MNT  DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant créditeur écritures en brouillard'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DEVISES ("
			"	DEV_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	DEV_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	DEV_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	DEV_DIGITS    INTEGER     DEFAULT 2                  COMMENT 'Decimal digits on display',"
			"	DEV_NOTES     VARCHAR(4096)                          COMMENT 'Currency notes',"
			"	DEV_MAJ_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	DEV_MAJ_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_DEVISES "
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL) VALUES ('EUR','Euro','€')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID           INTEGER   NOT NULL UNIQUE    COMMENT 'Row identifier',"
			"	DOS_LABEL        VARCHAR(80)                  COMMENT 'Raison sociale',"
			"	DOS_NOTES        VARCHAR(4096)                COMMENT 'Notes',"
			"	DOS_DUREE_EXE    INTEGER                      COMMENT 'Exercice length in month',"
			"	DOS_DEV_CODE     VARCHAR(3)                   COMMENT 'Default currency identifier',"
			"	DOS_MAJ_USER     VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
			"	DOS_MAJ_STAMP    TIMESTAMP NOT NULL           COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_DOSSIER (DOS_ID) VALUE (1)" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER_EXE ("
			"	DOS_ID           INTEGER      NOT NULL        COMMENT 'Row identifier',"
			"	DOS_EXE_ID       INTEGER      NOT NULL        COMMENT 'Exercice identifier',"
			"	DOS_EXE_DEB      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de début d\\'exercice',"
			"	DOS_EXE_FIN      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de fin d\\'exercice',"
			"	DOS_EXE_LAST_ECR INTEGER      NOT NULL DEFAULT 0 COMMENT 'Last entry number used',"
			"	DOS_EXE_STATUS   INTEGER      NOT NULL DEFAULT 0 COMMENT 'Status of this exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_EXE_ID)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_DOSSIER_EXE "
			"	(DOS_ID,DOS_EXE_ID,DOS_EXE_STATUS) VALUE (1,1,1)" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ECRITURES ("
			"	ECR_DEFFET    DATE NOT NULL               COMMENT 'Imputation effect date',"
			"	ECR_NUMBER    INTEGER NOT NULL            COMMENT 'Entry number',"
			"	ECR_DOPE      DATE NOT NULL               COMMENT 'Operation date',"
			"	ECR_LABEL     VARCHAR(80)                 COMMENT 'Entry label',"
			"	ECR_REF       VARCHAR(20)                 COMMENT 'Piece reference',"
			"	ECR_COMPTE    VARCHAR(20)                 COMMENT 'Account number',"
			"	ECR_DEV_CODE  VARCHAR(3)                  COMMENT 'ISO 3A identifier of the currency',"
			"	ECR_DEBIT     DECIMAL(15,5) DEFAULT 0     COMMENT 'Debiting amount',"
			"	ECR_CREDIT    DECIMAL(15,5) DEFAULT 0     COMMENT 'Crediting amount',"
			"	ECR_JOU_MNEMO VARCHAR(6)                  COMMENT 'Mnemonic identifier of the journal',"
			"	ECR_STATUS    INTEGER       DEFAULT 1     COMMENT 'Is the entry validated or deleted ?',"
			"	ECR_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of last update',"
			"	ECR_MAJ_STAMP TIMESTAMP                   COMMENT 'Last update timestamp',"
			"	ECR_RAPPRO    DATE NOT NULL DEFAULT 0     COMMENT 'Reconciliation date',"
			"	CONSTRAINT PRIMARY KEY (ECR_DEFFET,ECR_NUMBER),"
			"	INDEX (ECR_NUMBER)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX ("
			"	JOU_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the journal',"
			"	JOU_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Journal label',"
			"	JOU_NOTES     VARCHAR(4096)               COMMENT 'Journal notes',"
			"	JOU_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	JOU_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX_DEV ("
			"	JOU_MNEMO        VARCHAR(6) NOT NULL      COMMENT 'Internal journal identifier',"
			"	JOU_EXE_ID       INTEGER    NOT NULL      COMMENT 'Internal exercice identifier',"
			"	JOU_DEV_CODE     VARCHAR(3) NOT NULL      COMMENT 'Internal currency identifier',"
			"	JOU_DEV_CLO_DEB  DECIMAL(15,5)            COMMENT 'Debit balance at last closing',"
			"	JOU_DEV_CLO_CRE  DECIMAL(15,5)            COMMENT 'Credit balance at last closing',"
			"	JOU_DEV_DEB      DECIMAL(15,5)            COMMENT 'Current debit balance',"
			"	JOU_DEV_DEB_DATE DATE                     COMMENT 'Most recent debit entry effect date',"
			"	JOU_DEV_CRE      DECIMAL(15,5)            COMMENT 'Current credit balance',"
			"	JOU_DEV_CRE_DATE DATE                     COMMENT 'Most recent credit entry effect date',"
			"	CONSTRAINT PRIMARY KEY (JOU_MNEMO,JOU_EXE_ID,JOU_DEV_CODE)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX_EXE ("
			"	JOU_MNEMO        VARCHAR(6) NOT NULL      COMMENT 'Internal journal identifier',"
			"	JOU_EXE_ID       INTEGER    NOT NULL      COMMENT 'Internal exercice identifier',"
			"	JOU_EXE_LAST_CLO DATE                     COMMENT 'Last closing date of the exercice',"
			"	CONSTRAINT PRIMARY KEY (JOU_MNEMO,JOU_EXE_ID)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('ACH','Journal des achats','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('VEN','Journal des ventes','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('EXP','Journal de l\\'exploitant','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('OD','Journal des opérations diverses','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('BQ','Journal de banque','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES ("
			"	MOD_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Model mnemonic',"
			"	MOD_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Model label',"
			"	MOD_JOU_MNEMO VARCHAR(6)                  COMMENT 'Model journal',"
			"	MOD_JOU_VER   INTEGER                     COMMENT 'Journal is locked',"
			"	MOD_NOTES     VARCHAR(4096)               COMMENT 'Model notes',"
			"	MOD_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	MOD_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES_DET ("
			"	MOD_MNEMO           VARCHAR(6) NOT NULL     COMMENT 'Internal model identifier',"
			"	MOD_DET_RANG        INTEGER    NOT NULL     COMMENT 'Entry number',"
			"	MOD_DET_COMMENT     VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_ACCOUNT     VARCHAR(20)             COMMENT 'Account number',"
			"	MOD_DET_ACCOUNT_VER INTEGER                 COMMENT 'Account number is locked',"
			"	MOD_DET_LABEL       VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_LABEL_VER   INTEGER                 COMMENT 'Entry label is locked',"
			"	MOD_DET_DEBIT       VARCHAR(80)             COMMENT 'Debit amount',"
			"	MOD_DET_DEBIT_VER   INTEGER                 COMMENT 'Debit amount is locked',"
			"	MOD_DET_CREDIT      VARCHAR(80)             COMMENT 'Credit amount',"
			"	MOD_DET_CREDIT_VER  INTEGER                 COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (MOD_MNEMO, MOD_DET_RANG)"
			")" )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_RECURRENT ("
			"	REC_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Internal identifier',"
			"	REC_MOD_MNEMO VARCHAR(6)                  COMMENT 'Entry model mnemmonic',"
			"	REC_PERIOD    VARCHAR(1)                  COMMENT 'Periodicity',"
			"	REC_DAY       INTEGER                     COMMENT 'Day of the period',"
			"	REC_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	REC_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	REC_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	REC_LAST      DATE                        COMMENT 'Effect date of the last generation"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_TAUX ("
			"	TAX_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the rate',"
			"	TAX_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Rate label',"
			"	TAX_NOTES     VARCHAR(4096)               COMMENT 'Rate notes',"
			"	TAX_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	TAX_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_TAUX_VAL ("
			"	TAX_MNEMO         VARCHAR(6)  NOT NULL    COMMENT 'Mnemonic identifier of the rate',"
			"	TAX_VAL_DEB       DATE                    COMMENT 'Validity begin date',"
			"	TAX_VAL_FIN       DATE                    COMMENT 'Validity end date',"
			"	TAX_VAL_TAUX      DECIMAL(15,5)           COMMENT 'Taux value',"
			"	CONSTRAINT PRIMARY KEY (TAX_MNEMO,TAX_VAL_DEB,TAX_VAL_FIN)"
			")" )){
		return( FALSE );
	}

	/* we do this only at the end of the model creation
	 * as a mark that all has been successfully done
	 */
	if( !ofo_sgbd_query( sgbd,
			"UPDATE OFA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=1" )){
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_dossier_get_name:
 *
 * Returns: the searched account.
 */
const gchar *
ofo_dossier_get_name( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->name );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_user:
 *
 * Returns: the currently connected user identifier.
 */
const gchar *
ofo_dossier_get_user( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->userid );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_sgbd:
 *
 * Returns: the current sgbd handler.
 */
const ofoSgbd *
ofo_dossier_get_sgbd( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const ofoSgbd * ) dossier->private->sgbd );
	}

	return( NULL );
}

/**
 * ofo_dossier_use_devise:
 *
 * Returns: %TRUE if the dossier makes use of this currency, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_devise( const ofoDossier *dossier, const gchar *devise )
{
	const gchar *default_dev;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		default_dev = ofo_dossier_get_default_devise( dossier );

		if( default_dev && g_utf8_strlen( default_dev, -1 )){
			return( g_utf8_collate( default_dev, devise ) == 0 );
		}
	}

	return( FALSE );
}

/**
 * ofo_dossier_get_label:
 *
 * Returns: the label of the dossier.
 */
const gchar *
ofo_dossier_get_label( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->label );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_exercice_length:
 *
 * Returns: the length of the exercice, in months.
 */
gint
ofo_dossier_get_exercice_length( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), -1 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->private->duree_exe );
	}

	return( -1 );
}

/**
 * ofo_dossier_get_default_devise:
 *
 * Returns: the default currency of the dossier.
 */
const gchar *
ofo_dossier_get_default_devise( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->devise );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_notes:
 *
 * Returns: the notes attached to the dossier.
 */
const gchar *
ofo_dossier_get_notes( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->notes );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_maj_user:
 *
 * Returns: the identifier of the user who has last updated the
 * properties of the dossier.
 */
const gchar *
ofo_dossier_get_maj_user( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->private->maj_user );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_stamp:
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const GTimeVal *
ofo_dossier_get_maj_stamp( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &dossier->private->maj_stamp );
	}

	return( NULL );
}

static sDetailExe *
get_current_exe( const ofoDossier *dossier )
{
	GList *exe;
	sDetailExe *sexe;

	if( !dossier->private->current ){
		for( exe=dossier->private->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;
			if( sexe->status == DOS_STATUS_OPENED ){
				dossier->private->current = sexe;
				break;
			}
		}
	}

	return( dossier->private->current );
}

static sDetailExe *
get_exe_by_id( const ofoDossier *dossier, gint exe_id )
{
	GList *exe;
	sDetailExe *sexe;

	for( exe=dossier->private->exes ; exe ; exe=exe->next ){
		sexe = ( sDetailExe * ) exe->data;
		if( sexe->exe_id == exe_id ){
			return( sexe );
		}
	}

	/* non existant */
	return( NULL );
}

static sDetailExe *
get_exe_by_date( const ofoDossier *dossier, const GDate *date )
{
	GList *exe;
	sDetailExe *sexe;

	for( exe=dossier->private->exes ; exe ; exe=exe->next ){
		sexe = ( sDetailExe * ) exe->data;
		if( !g_date_valid( &sexe->exe_deb ) || g_date_compare( &sexe->exe_deb, date ) > 0 ){
			continue;
		}
		if( !g_date_valid( &sexe->exe_fin ) || g_date_compare( &sexe->exe_fin, date ) < 0 ){
			continue;
		}
		return( sexe );
	}

	/* non existant */
	return( NULL );
}

/**
 * ofo_dossier_get_current_exe_id:
 *
 * Returns: the internal identifier of the current exercice.
 */
gint
ofo_dossier_get_current_exe_id( const ofoDossier *dossier )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			return( sexe->exe_id );
		}
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_dossier_get_current_exe_deb:
 *
 * Returns: the beginning date of the current exercice.
 */
const GDate *
ofo_dossier_get_current_exe_deb( const ofoDossier *dossier )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			return(( const GDate * ) &sexe->exe_deb );
		}
	}

	return( NULL );
}

/**
 * ofo_dossier_get_current_exe_fin:
 *
 * Returns: the ending date of the current exercice.
 */
const GDate *
ofo_dossier_get_current_exe_fin( const ofoDossier *dossier )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			return(( const GDate * ) &sexe->exe_fin );
		}
	}

	return( NULL );
}

/**
 * ofo_dossier_get_current_exe_last_ecr:
 *
 * Returns: the last entry number allocated in the current exercice.
 */
gint
ofo_dossier_get_current_exe_last_ecr( const ofoDossier *dossier )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			return( sexe->last_ecr );
		}
	}

	return( 0 );
}

/**
 * ofo_dossier_get_exe_by_date:
 *
 * Returns the exercice id which contains the specified date.
 *
 * Default to the current exercice identifier.
 */
gint
ofo_dossier_get_exe_by_date( const ofoDossier *dossier, const GDate *date )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( date && g_date_valid( date ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_exe_by_date( dossier, date );
		if( !sexe ){
			sexe = get_current_exe( dossier );
		}
		return( sexe->exe_id );
	}

	return( 0 );
}

/**
 * ofo_dossier_get_exe_fin:
 *
 * Returns: the date of the beginning of the specified exercice.
 */
const GDate *
ofo_dossier_get_exe_deb( const ofoDossier *dossier, gint exe_id )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_exe_by_id( dossier, exe_id );
		if( sexe ){
			return(( const GDate * ) &sexe->exe_deb );
		}
	}

	return( NULL );
}

/**
 * ofo_dossier_get_exe_fin:
 *
 * Returns: the date of the end of the specified exercice.
 */
const GDate *
ofo_dossier_get_exe_fin( const ofoDossier *dossier, gint exe_id )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_exe_by_id( dossier, exe_id );
		if( sexe ){
			return(( const GDate * ) &sexe->exe_fin );
		}
	}

	return( NULL );
}

/**
 * ofo_dossier_get_exe_status_label:
 */
const gchar *
ofo_dossier_get_exe_status_label( ofaDossierStatus status )
{
	gchar *str;

	switch( status ){
		case DOS_STATUS_OPENED:
			return( _( "Opened" ));
			break;
		case DOS_STATUS_CLOSED:
			return( _( "Closed" ));
			break;
	}

	/* memory leak, but don't know how to prevent it if I want
	 * return the erroneous status */
	str = g_strdup_printf( "%s: %d", _( "Unknown status" ), status );
	return( str );
}
/**
 * ofo_dossier_get_last_closed_exercice:
 *
 * Returns: the last exercice closing date.
 */
const GDate *
ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier )
{
	GList *exe;
	sDetailExe *sexe;
	const GDate *dmax;
	gboolean first;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	dmax = NULL;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		first = TRUE;
		for( exe=dossier->private->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;

			if( first ){
				dmax = &sexe->exe_fin;
				first = FALSE;

			} else if( g_date_valid( &sexe->exe_fin ) &&
						g_date_compare( &sexe->exe_fin, dmax ) > 0 ){

				dmax = &sexe->exe_fin;
			}
		}
	}

	return( dmax );
}

/**
 * ofo_dossier_get_next_entry_number:
 */
gint
ofo_dossier_get_next_entry_number( const ofoDossier *dossier )
{
	sDetailExe *current;
	gint next_number;
	gchar *query;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	next_number = 0;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		current = get_current_exe( dossier );
		current->last_ecr += 1;
		next_number = current->last_ecr;

		query = g_strdup_printf(
				"UPDATE OFA_T_DOSSIER_EXE "
				"	SET DOS_EXE_LAST_ECR=%d "
				"	WHERE DOS_ID=%d AND DOS_EXE_STATUS=%d",
						next_number, THIS_DOS_ID, DOS_STATUS_OPENED );

		ofo_sgbd_query( dossier->private->sgbd, query );
		g_free( query );
	}

	return( next_number );
}

/**
 * ofo_dossier_is_valid:
 */
gboolean
ofo_dossier_is_valid( const gchar *label, gint duree, const gchar *devise )
{
	return( label && g_utf8_strlen( label, -1 ) &&
				duree > 0 &&
				devise && g_utf8_strlen( devise, -1 ));
}

/**
 * ofo_dossier_set_label:
 */
void
ofo_dossier_set_label( ofoDossier *dossier, const gchar *label )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( label && g_utf8_strlen( label, -1 ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->private->label );
		dossier->private->label = g_strdup( label );
	}
}

/**
 * ofo_dossier_set_exercice_length:
 */
void
ofo_dossier_set_exercice_length( ofoDossier *dossier, gint duree )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( duree > 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->private->duree_exe = duree;
	}
}

/**
 * ofo_dossier_set_default_devise:
 */
void
ofo_dossier_set_default_devise( ofoDossier *dossier, const gchar *devise )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->private->devise );
		dossier->private->devise = g_strdup( devise );
	}
}

/**
 * ofo_dossier_set_notes:
 */
void
ofo_dossier_set_notes( ofoDossier *dossier, const gchar *notes )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->private->notes );
		dossier->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_dossier_set_maj_user:
 */
void
ofo_dossier_set_maj_user( ofoDossier *dossier, const gchar *user )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( user && g_utf8_strlen( user, -1 ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->private->maj_user );
		dossier->private->maj_user = g_strdup( user );
	}
}

/**
 * ofo_dossier_set_maj_stamp:
 */
void
ofo_dossier_set_maj_stamp( ofoDossier *dossier, const GTimeVal *stamp )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( stamp );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		memcpy( &dossier->private->maj_stamp, stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_dossier_set_current_exe_id:
 */
void
ofo_dossier_set_current_exe_id( const ofoDossier *dossier, gint exe_id )
{
	sDetailExe *sexe;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( exe_id > 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			sexe->exe_id = exe_id;
		}
	}
}

/**
 * ofo_dossier_set_current_exe_deb:
 */
void
ofo_dossier_set_current_exe_deb( const ofoDossier *dossier, const GDate *date )
{
	sDetailExe *sexe;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			memcpy( &sexe->exe_deb, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_dossier_set_current_exe_fin:
 */
void
ofo_dossier_set_current_exe_fin( const ofoDossier *dossier, const GDate *date )
{
	sDetailExe *sexe;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			memcpy( &sexe->exe_fin, date, sizeof( GDate ));
		}
	}
}

/**
 * ofo_dossier_set_current_exe_last_ecr:
 */
void
ofo_dossier_set_current_exe_last_ecr( const ofoDossier *dossier, gint number )
{
	sDetailExe *sexe;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sexe = get_current_exe( dossier );
		if( sexe ){
			sexe->last_ecr = number;
		}
	}
}

static void
on_new_object_cleanup_handler( ofoDossier *dossier, ofoBase *object )
{
	static const gchar *thisfn = "ofo_dossier_on_new_object_cleanup_handler";

	g_debug( "%s: dossier=%p, object=%p (%s)",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_object_unref( object );
}

static void
on_updated_object_cleanup_handler( ofoDossier *dossier, ofoBase *object, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_dossier_on_updated_object_cleanup_handler";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id );

	g_object_unref( object );
}

static void
on_deleted_object_cleanup_handler( ofoDossier *dossier, ofoBase *object )
{
	static const gchar *thisfn = "ofo_dossier_on_deleted_object_cleanup_handler";

	g_debug( "%s: dossier=%p, object=%p (%s)",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_object_unref( object );
}

static void
on_reloaded_dataset_cleanup_handler( ofoDossier *dossier, GType type )
{
	static const gchar *thisfn = "ofo_dossier_on_reloaded_dataset_cleanup_handler";

	g_debug( "%s: dossier=%p, type=%lu", thisfn, ( void * ) dossier, type );
}

static void
on_validated_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_dossier_on_validated_entry_cleanup_handler";

	g_debug( "%s: dossier=%p, entry=%p", thisfn, ( void * ) dossier, ( void * ) entry );
}

static gboolean
dossier_do_read( ofoDossier *dossier )
{
	return( dossier_read_properties( dossier ) &&
			dossier_read_exercices( dossier ));
}

static gboolean
dossier_read_properties( ofoDossier *dossier )
{
	gchar *query;
	GSList *result, *icol;
	gboolean ok;
	const gchar *str;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_LABEL,DOS_DUREE_EXE,DOS_NOTES,DOS_DEV_CODE,"
			"	DOS_MAJ_USER,DOS_MAJ_STAMP "
			"	FROM OFA_T_DOSSIER "
			"	WHERE DOS_ID=%d", THIS_DOS_ID );

	result = ofo_sgbd_query_ex( dossier->private->sgbd, query );

	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_label( dossier, str );
		}
		icol = icol->next;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_exercice_length( dossier, atoi( str ));
		}
		icol = icol->next;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_notes( dossier, str );
		}
		icol = icol->next;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_default_devise( dossier, str );
		}
		icol = icol->next;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_maj_user( dossier, str );
		}
		icol = icol->next;
		str = icol->data;
		if( str && g_utf8_strlen( str, -1 )){
			ofo_dossier_set_maj_stamp( dossier, my_utils_stamp_from_str( str ));
		}

		ok = TRUE;
		ofo_sgbd_free_result( result );
	}

	return( ok );
}

static gboolean
dossier_read_exercices( ofoDossier *dossier )
{
	gchar *query;
	GSList *result, *irow, *icol;
	gboolean ok;
	sDetailExe *sexe;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_EXE_ID,DOS_EXE_DEB,DOS_EXE_FIN,DOS_EXE_LAST_ECR,DOS_EXE_STATUS "
			"	FROM OFA_T_DOSSIER_EXE "
			"	WHERE DOS_ID=%d", THIS_DOS_ID );

	result = ofo_sgbd_query_ex( dossier->private->sgbd, query );

	g_free( query );

	if( result ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			sexe = g_new0( sDetailExe, 1 );

			sexe->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			memcpy( &sexe->exe_deb, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));
			icol = icol->next;
			memcpy( &sexe->exe_fin, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));
			icol = icol->next;
			sexe->last_ecr = atoi(( gchar * ) icol->data );
			icol = icol->next;
			sexe->status = atoi(( gchar * ) icol->data );

			dossier->private->exes = g_list_append( dossier->private->exes, sexe );
		}

		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update
 */
gboolean
ofo_dossier_update( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

		return( dossier_do_update(
					dossier, dossier->private->sgbd, dossier->private->userid ));
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
dossier_do_update( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user )
{
	return( do_update_properties( dossier, sgbd, user ) &&
			do_update_current_exe( dossier, sgbd ));
}

static gboolean
do_update_properties( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp;
	gboolean ok;
	const gchar *devise;

	ok = FALSE;
	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	notes = my_utils_quote( ofo_dossier_get_notes( dossier ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_DOSSIER SET " );

	g_string_append_printf( query,
			"	DOS_LABEL='%s',DOS_DUREE_EXE=%d,",
					label,
					ofo_dossier_get_exercice_length( dossier ));

	devise = ofo_dossier_get_default_devise( dossier );
	if( devise && g_utf8_strlen( devise, -1 )){
		g_string_append_printf( query, "DOS_DEV_CODE='%s',", devise );
	} else {
		query = g_string_append( query, "DOS_DEV_CODE=NULL," );
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "DOS_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	DOS_MAJ_USER='%s',DOS_MAJ_STAMP='%s'"
			"	WHERE DOS_ID=%d", user, stamp, THIS_DOS_ID );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_dossier_set_maj_user( dossier, user );
		ofo_dossier_set_maj_stamp( dossier, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp );

	return( ok );
}

static gboolean
do_update_current_exe( ofoDossier *dossier, const ofoSgbd *sgbd )
{
	gchar *query;
	gchar *sdeb, *sfin;
	gboolean ok;

	ok = FALSE;

	if( g_date_valid( &dossier->private->current->exe_deb )){
		sdeb = my_utils_sql_from_date( &dossier->private->current->exe_deb );
	} else {
		sdeb = g_strdup( "0000-00-00" );
	}

	if( g_date_valid( &dossier->private->current->exe_fin )){
		sfin = my_utils_sql_from_date( &dossier->private->current->exe_fin );
	} else {
		sfin = g_strdup( "0000-00-00" );
	}

	query = g_strdup_printf(
				"UPDATE OFA_T_DOSSIER_EXE SET "
				"	DOS_EXE_DEB='%s',"
				"	DOS_EXE_FIN='%s' WHERE "
				"	DOS_ID=%d AND DOS_EXE_ID=%d",
						sdeb,
						sfin,
						THIS_DOS_ID,
						dossier->private->current->exe_id );

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );
	g_free( sdeb );
	g_free( sfin );

	return( ok );
}

/**
 * ofo_dossier_get_csv:
 */
GSList *
ofo_dossier_get_csv( const ofoDossier *dossier )
{
	GSList *lines;
	gchar *str, *stamp;
	const gchar *devise, *muser;
	GList *exe;
	sDetailExe *sexe;
	gchar *sbegin, *send, *notes;

	lines = NULL;

	str = g_strdup_printf( "1;Label;Notes;MajUser;MajStamp;ExeLength;DefaultCurrency" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;ExeBegin;ExeEnd;LastEntry;Status" );
	lines = g_slist_prepend( lines, str );

	notes = my_utils_export_multi_lines( ofo_dossier_get_notes( dossier ));
	g_debug( "notes=%s", notes );
	muser = ofo_dossier_get_maj_user( dossier );
	stamp = my_utils_str_from_stamp( ofo_dossier_get_maj_stamp( dossier ));
	devise = ofo_dossier_get_default_devise( dossier );

	str = g_strdup_printf( "1;%s;%s;%s;%s;%d;%s",
			ofo_dossier_get_label( dossier ),
			notes ? notes : "",
			muser ? muser : "",
			muser ? stamp : "",
			ofo_dossier_get_exercice_length( dossier ),
			devise ? devise : "" );

	g_free( notes );
	g_free( stamp );

	lines = g_slist_prepend( lines, str );

	for( exe=dossier->private->exes ; exe ; exe=exe->next ){
		sexe = ( sDetailExe * ) exe->data;

		if( g_date_valid( &sexe->exe_deb )){
			sbegin = my_utils_sql_from_date( &sexe->exe_deb );
		} else {
			sbegin = g_strdup( "" );
		}

		if( g_date_valid( &sexe->exe_fin )){
			send = my_utils_sql_from_date( &sexe->exe_fin );
		} else {
			send = g_strdup( "" );
		}

		str = g_strdup_printf( "2:%s;%s;%d;%d",
				sbegin,
				send,
				sexe->last_ecr,
				sexe->status );

		g_free( sbegin );
		g_free( send );

		lines = g_slist_prepend( lines, str );
	}

	return( g_slist_reverse( lines ));
}
