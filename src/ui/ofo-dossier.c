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

#include "ui/my-utils.h"
#include "ui/ofo-base.h"
#include "ui/ofo-base-prot.h"
#include "ui/ofo-account.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoDossierPrivate {

	/* internals
	 */
	gchar    *name;
	ofoSgbd  *sgbd;
	gchar    *userid;

	/* row id 1
	 */
	gchar    *label;					/* raison sociale */
	gint      duree_exe;				/* exercice length (in month) */
	gint      devise;
	gchar    *notes;					/* notes */
	gchar    *maj_user;
	GTimeVal  maj_stamp;

	/* details of current exercice
	 */
	gint      exe_id;					/* current exercice identifier */
	GDate     exe_deb;					/* début d'exercice */
	GDate     exe_fin;					/* fin d'exercice */
	gint      last_ecr;

	/* other datas
	 */
	GDate     last_closed_exe;
};

/* signals defined here
 */
enum {
	NEW_ENTRY,
	ACCOUNT_UPDATED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofoDossier, ofo_dossier, OFO_TYPE_BASE )

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1
#define THIS_DOS_ID                     1

static gint     dbmodel_get_version( ofoSgbd *sgbd );
static gboolean dbmodel_to_v1( ofoSgbd *sgbd, const gchar *account );
static void     set_last_closed_exercice( const ofoDossier *dossier );
static void     on_new_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry, gpointer user_data );
static void     on_account_updated_cleanup_handler( ofoDossier *dossier, ofoAccount *account, gpointer user_data );
static gboolean dossier_do_read( ofoDossier *dossier );
static gboolean dossier_read_properties( ofoDossier *dossier );
static gboolean dossier_read_current_exercice( ofoDossier *dossier );
static gboolean dossier_do_update( ofoDossier *dossier, ofoSgbd *sgbd, const gchar *user );

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

	self->private->exe_id = OFO_BASE_UNSET_ID;
	g_date_clear( &self->private->last_closed_exe, 1 );
}

static void
ofo_dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_dossier_finalize;

	/**
	 * ofoDossier::ofa-signal-new-entry:
	 *
	 * This signal is sent on the dossier by the entry being just
	 * inserted. Other objects are suggested to connect to this signal
	 * in order to update themselves.
	 *
	 * The #ofoEntry entry object is passed as an argument. The emitter
	 * of the signal - usually the #ofoEntry class itself - should take
	 * care of getting a reference on the object, so that consumers are
	 * sure that the object stays alive during signal processing.
	 *
	 * The default signal handler will decrement the reference count,
	 * thus releasing the object for application purposes.
	 *
	 * Handler is of type:
	 * 		void ( *handler )( ofoDossier *dossier,
	 * 							ofoEntry *entry,
	 * 							gpointer user_data );
	 */
	st_signals[ NEW_ENTRY ] = g_signal_new_class_handler(
				OFA_SIGNAL_NEW_ENTRY,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_new_entry_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofoDossier::ofa-signal-account-updated:
	 *
	 * This signal is sent on the dossier by the account whose amounts
	 * have just been updated.
	 *
	 * The #ofoAccount account object is passed as an argument. The
	 * emitter of the signal - usually the #ofoAccount class itself -
	 * should take care of getting a reference on the object, so that
	 * consumers are sure that the object stays alive during signal
	 * processing.
	 *
	 * The default signal handler will decrement the reference count,
	 * thus releasing the object for application purposes.
	 *
	 * Handler is of type:
	 * 		void ( *handler )( ofoDossier *dossier,
	 * 							ofoAccount *account,
	 * 							gpointer user_data );
	 */
	st_signals[ ACCOUNT_UPDATED ] = g_signal_new_class_handler(
				OFA_SIGNAL_ACCOUNT_UPDATED,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_account_updated_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
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

	ofo_dossier_dbmodel_update( sgbd, account );

	dossier->private->sgbd = sgbd;
	dossier->private->userid = g_strdup( account );

	return( dossier_do_read( dossier ));
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

	/* default value for timestamp cannot be null */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER NOT NULL UNIQUE DEFAULT 0 COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP DEFAULT 0               COMMENT 'Version application timestamp')" )){
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

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_COMPTES ("
			"	CPT_NUMBER       VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
			"	CPT_LABEL        VARCHAR(80)   NOT NULL        COMMENT 'Account label',"
			"	CPT_DEV_ID       INTEGER                       COMMENT 'Identifier of the currency of the account',"
			"	CPT_NOTES        VARCHAR(512)                  COMMENT 'Account notes',"
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
			"	DEV_ID        INTEGER NOT NULL AUTO_INCREMENT UNIQUE COMMENT 'Internal identifier of the currency',"
			"	DEV_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	DEV_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	DEV_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	DEV_NOTES     VARCHAR(512)                           COMMENT 'Currency notes',"
			"	DEV_MAJ_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	DEV_MAJ_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	query = g_strdup(
			"INSERT IGNORE INTO OFA_T_DEVISES "
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL) VALUES "
			"	('EUR','Euro','€')" );
	if( !ofo_sgbd_query( sgbd, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID           INTEGER   NOT NULL UNIQUE    COMMENT 'Row identifier',"
			"	DOS_LABEL        VARCHAR(80)                  COMMENT 'Raison sociale',"
			"	DOS_NOTES        VARCHAR(512)                 COMMENT 'Notes',"
			"	DOS_DUREE_EXE    INTEGER                      COMMENT 'Exercice length in month',"
			"	DOS_DEV_ID       INTEGER                      COMMENT 'Default currency identifier',"
			"	DOS_MAJ_USER     VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
			"	DOS_MAJ_STAMP    TIMESTAMP NOT NULL           COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	query = g_strdup(
			"INSERT IGNORE INTO OFA_T_DOSSIER (DOS_ID) VALUE (1)" );
	if( !ofo_sgbd_query( sgbd, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

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

	query = g_strdup(
			"INSERT IGNORE INTO OFA_T_DOSSIER_EXE (DOS_ID,DOS_EXE_ID,DOS_EXE_STATUS) VALUE (1,1,1)" );
	if( !ofo_sgbd_query( sgbd, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ECRITURES ("
			"	ECR_DEFFET    DATE NOT NULL               COMMENT 'Imputation effect date',"
			"	ECR_NUMBER    INTEGER NOT NULL            COMMENT 'Entry number',"
			"	ECR_DOPE      DATE NOT NULL               COMMENT 'Operation date',"
			"	ECR_LABEL     VARCHAR(80)                 COMMENT 'Entry label',"
			"	ECR_REF       VARCHAR(20)                 COMMENT 'Piece reference',"
			"	ECR_COMPTE    VARCHAR(20)                 COMMENT 'Account number',"
			"	ECR_DEV_ID    INTEGER                     COMMENT 'Internal identifier of the currency',"
			"	ECR_MONTANT   DECIMAL(15,5)               COMMENT 'Entry amount',"
			"	ECR_SENS      INTEGER                     COMMENT 'Sens of the entry \\'DB\\' or \\'CR\\'',"
			"	ECR_JOU_ID    INTEGER                     COMMENT 'Internal identifier of the journal',"
			"	ECR_STATUS    INTEGER                     COMMENT 'Is the entry validated or deleted ?',"
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
			"	JOU_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern journal identifier',"
			"	JOU_MNEMO     VARCHAR(3) BINARY  NOT NULL UNIQUE COMMENT 'Journal mnemonic',"
			"	JOU_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Journal label',"
			"	JOU_NOTES     VARCHAR(512)                COMMENT 'Journal notes',"
			"	JOU_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	JOU_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
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
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX_DEV ("
			"	JOU_ID          INTEGER  NOT NULL         COMMENT 'Internal journal identifier',"
			"	JOU_EXE_ID      INTEGER  NOT NULL         COMMENT 'Internal exercice identifier',"
			"	JOU_DEV_ID      INTEGER  NOT NULL         COMMENT 'Internal currency identifier',"
			"	JOU_DEV_CLO_DEB DECIMAL(15,5)             COMMENT 'Debit balance at last closing',"
			"	JOU_DEV_CLO_CRE DECIMAL(15,5)             COMMENT 'Credit balance at last closing',"
			"	JOU_DEV_DEB     DECIMAL(15,5)             COMMENT 'Current debit balance',"
			"	JOU_DEV_CRE     DECIMAL(15,5)             COMMENT 'Current credit balance',"
			"	CONSTRAINT PRIMARY KEY (JOU_ID,JOU_EXE_ID,JOU_DEV_ID)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX_EXE ("
			"	JOU_ID           INTEGER  NOT NULL        COMMENT 'Internal journal identifier',"
			"	JOU_EXE_ID       INTEGER  NOT NULL        COMMENT 'Internal exercice identifier',"
			"	JOU_EXE_LAST_CLO DATE                     COMMENT 'Last closing date of the exercice',"
			"	CONSTRAINT PRIMARY KEY (JOU_ID,JOU_EXE_ID)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES ("
			"	MOD_ID        INTEGER NOT NULL UNIQUE AUTO_INCREMENT COMMENT 'Internal model identifier',"
			"	MOD_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Model mnemonic',"
			"	MOD_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Model label',"
			"	MOD_JOU_ID    INTEGER                     COMMENT 'Model journal',"
			"	MOD_JOU_VER   INTEGER                     COMMENT 'Journal is locked',"
			"	MOD_NOTES     VARCHAR(512)                COMMENT 'Model notes',"
			"	MOD_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	MOD_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES_DET ("
			"	MOD_ID              INTEGER NOT NULL        COMMENT 'Internal model identifier',"
			"	MOD_DET_RANG        INTEGER NOT NULL        COMMENT 'Entry number',"
			"	MOD_DET_COMMENT     VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_ACCOUNT     VARCHAR(20)             COMMENT 'Account number',"
			"	MOD_DET_ACCOUNT_VER INTEGER                 COMMENT 'Account number is locked',"
			"	MOD_DET_LABEL       VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_LABEL_VER   INTEGER                 COMMENT 'Entry label is locked',"
			"	MOD_DET_DEBIT       VARCHAR(80)             COMMENT 'Debit amount',"
			"	MOD_DET_DEBIT_VER   INTEGER                 COMMENT 'Debit amount is locked',"
			"	MOD_DET_CREDIT      VARCHAR(80)             COMMENT 'Credit amount',"
			"	MOD_DET_CREDIT_VER  INTEGER                 COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (MOD_ID, MOD_DET_RANG)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_TAUX ("
			"	TAX_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern taux identifier',"
			"	TAX_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Taux mnemonic',"
			"	TAX_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Taux label',"
			"	TAX_NOTES     VARCHAR(512)                COMMENT 'Taux notes',"
			"	TAX_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	TAX_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_TAUX_VAL ("
			"	TAX_ID            INTEGER     NOT NULL    COMMENT 'Intern taux identifier',"
			"	TAX_VAL_DEB       DATE                    COMMENT 'Validity begin date',"
			"	TAX_VAL_FIN       DATE                    COMMENT 'Validity end date',"
			"	TAX_VAL_TAUX      DECIMAL(15,5)           COMMENT 'Taux value',"
			"	CONSTRAINT PRIMARY KEY (TAX_ID,TAX_VAL_DEB,TAX_VAL_FIN)"
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
ofo_dossier_use_devise( const ofoDossier *dossier, gint devise )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( ofo_dossier_get_default_devise( dossier ) == devise );
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
gint
ofo_dossier_get_default_devise( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->private->devise );
	}

	return( OFO_BASE_UNSET_ID );
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

/**
 * ofo_dossier_get_current_exe_id:
 *
 * Returns: the internal identifier of the current exercice.
 */
gint
ofo_dossier_get_current_exe_id( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->private->exe_id );
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
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->private->exe_deb );
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
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->private->exe_fin );
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
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->private->last_ecr );
	}

	return( 0 );
}

/**
 * ofo_dossier_get_last_closed_exercice:
 *
 * Returns: the last closing date of an exercice, or a zero-ed date
 * if there is no previous exercice.
 */
const GDate *
ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		if( !g_date_valid( &dossier->private->last_closed_exe )){

			set_last_closed_exercice( dossier );
		}
		return(( const GDate * ) &dossier->private->last_closed_exe );
	}

	return( NULL );
}

static void
set_last_closed_exercice( const ofoDossier *dossier )
{
	GSList *result, *icol;

	result = ofo_sgbd_query_ex( dossier->private->sgbd,
					"SELECT MAX(DOS_EXE_FIN) FROM OFA_T_DOSSIER_EXE "
					"	WHERE DOS_EXE_STATUS=2" );

	if( result ){
		icol = ( GSList * ) result->data;
		if( icol->data ){
			memcpy( &dossier->private->last_closed_exe,
						my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));
		}
		ofo_sgbd_free_result( result );
	}
}

/**
 * ofo_dossier_get_next_entry_number:
 */
gint
ofo_dossier_get_next_entry_number( const ofoDossier *dossier )
{
	gint next_number;
	gchar *query;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	next_number = 0;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->private->last_ecr += 1;
		next_number = dossier->private->last_ecr;

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
ofo_dossier_is_valid( const gchar *label, gint duree, gint devise )
{
	return( label && g_utf8_strlen( label, -1 ) &&
				duree > 0 &&
				devise > 0 );
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
ofo_dossier_set_default_devise( ofoDossier *dossier, gint dev )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->private->devise = dev;
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
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( exe_id > 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->private->exe_id = exe_id;
	}
}

/**
 * ofo_dossier_set_current_exe_deb:
 */
void
ofo_dossier_set_current_exe_deb( const ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		memcpy( &dossier->private->exe_deb, date, sizeof( GDate ));
	}
}

/**
 * ofo_dossier_set_current_exe_fin:
 */
void
ofo_dossier_set_current_exe_fin( const ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		memcpy( &dossier->private->exe_fin, date, sizeof( GDate ));
	}
}

/**
 * ofo_dossier_set_current_exe_last_ecr:
 */
void
ofo_dossier_set_current_exe_last_ecr( const ofoDossier *dossier, gint number )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->private->last_ecr = number;
	}
}

static void
on_new_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry, gpointer user_data )
{
	static const gchar *thisfn = "ofo_dossier_on_new_entry_cleanup_handler";

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	g_object_unref( entry );
}

static void
on_account_updated_cleanup_handler( ofoDossier *dossier, ofoAccount *account, gpointer user_data )
{
	static const gchar *thisfn = "ofo_dossier_on_account_updated_cleanup_handler";

	g_debug( "%s: dossier=%p, account=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) account, ( void * ) user_data );

	g_object_unref( account );
}

static gboolean
dossier_do_read( ofoDossier *dossier )
{
	return( dossier_read_properties( dossier ) &&
			dossier_read_current_exercice( dossier ));
}

static gboolean
dossier_read_properties( ofoDossier *dossier )
{
	gchar *query;
	GSList *result, *icol;
	gboolean ok;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_LABEL,DOS_DUREE_EXE,DOS_NOTES,DOS_DEV_ID,"
			"	DOS_MAJ_USER,DOS_MAJ_STAMP "
			"	FROM OFA_T_DOSSIER "
			"	WHERE DOS_ID=%d", THIS_DOS_ID );

	result = ofo_sgbd_query_ex( dossier->private->sgbd, query );

	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_dossier_set_label( dossier, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_dossier_set_exercice_length( dossier, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_dossier_set_default_devise( dossier, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_dossier_set_notes( dossier, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_dossier_set_maj_user( dossier, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_dossier_set_maj_stamp( dossier, my_utils_stamp_from_str(( gchar * ) icol->data ));

		ok = TRUE;
		ofo_sgbd_free_result( result );
	}

	return( ok );
}

static gboolean
dossier_read_current_exercice( ofoDossier *dossier )
{
	gchar *query;
	GSList *result, *icol;
	gboolean ok;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_EXE_ID,DOS_EXE_DEB,DOS_EXE_FIN,DOS_EXE_LAST_ECR "
			"	FROM OFA_T_DOSSIER_EXE "
			"	WHERE DOS_ID=%d AND DOS_EXE_STATUS=1", THIS_DOS_ID );

	result = ofo_sgbd_query_ex( dossier->private->sgbd, query );

	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_dossier_set_current_exe_id( dossier, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_dossier_set_current_exe_deb( dossier, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_dossier_set_current_exe_fin( dossier, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_dossier_set_current_exe_last_ecr( dossier, atoi(( gchar * ) icol->data ));

		ok = TRUE;
		ofo_sgbd_free_result( result );
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
dossier_do_update( ofoDossier *dossier, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	notes = my_utils_quote( ofo_dossier_get_notes( dossier ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_DOSSIER SET " );

	g_string_append_printf( query,
			"	DOS_LABEL='%s',DOS_DUREE_EXE=%d,DOS_DEV_ID=%d,",
					label,
					ofo_dossier_get_exercice_length( dossier ),
					ofo_dossier_get_default_devise( dossier ));

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
