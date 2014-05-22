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
#include "ui/ofo-dossier.h"
#include "ui/ofo-account.h"
#include "ui/ofo-journal.h"

/* priv instance data
 */
struct _ofoDossierPrivate {
	gboolean  dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar    *name;
	ofoSgbd  *sgbd;
	gchar    *userid;

	GList    *models;					/* entry models */
	GList    *taux;						/* taux */

	/* row id 0
	 */
	gchar    *label;					/* raison sociale */
	gchar    *notes;					/* notes */
	GDate     exe_deb;					/* début d'exercice */
	GDate     exe_fin;					/* fin d'exercice */
};

G_DEFINE_TYPE( ofoDossier, ofo_dossier, OFO_TYPE_BASE )

#define OFO_DOSSIER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_DOSSIER, ofoDossierPrivate))

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1
#define THIS_DOS_ID                     1

typedef struct {
	gint         id;
	const gchar *mnemo;
	const GDate *begin;
	const GDate *end;
}
	sCheckTaux;

typedef struct {
	const gchar *mnemo;
	const GDate *date;
}
	sFindTaux;

static gint     dbmodel_get_version( ofoSgbd *sgbd );
static gboolean dbmodel_to_v1( ofoSgbd *sgbd, const gchar *account );
static gint     entry_get_next_number( ofoDossier *dossier );
static void     models_set_free( GList *set );
static gint     models_cmp( const ofoModel *a, const ofoModel *b );
static gint     models_find( const ofoModel *a, const gchar *searched_mnemo );
static void     taux_set_free( GList *set );
static gint     taux_cmp( const ofoTaux *a, const ofoTaux *b );
static gint     taux_check( const ofoTaux *a, sCheckTaux *check );
static gint     taux_find( const ofoTaux *a, const sFindTaux *parms );

static void
ofo_dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_finalize";
	ofoDossier *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_DOSSIER( instance );

	g_free( self->priv->name );
	g_free( self->priv->userid );

	if( self->priv->models ){
		models_set_free( self->priv->models );
		self->priv->models = NULL;
	}
	if( self->priv->taux ){
		taux_set_free( self->priv->taux );
		self->priv->taux = NULL;
	}

	g_free( self->priv->label );
	g_free( self->priv->notes );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->finalize( instance );
}

static void
ofo_dossier_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_dispose";
	ofoDossier *self;

	self = OFO_DOSSIER( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		self->priv->dispose_has_run = TRUE;

		ofo_account_clear_static();
		ofo_devise_clear_static();
		ofo_journal_clear_static();

		if( self->priv->sgbd ){
			g_clear_object( &self->priv->sgbd );
		}
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->dispose( instance );
}

static void
ofo_dossier_init( ofoDossier *self )
{
	static const gchar *thisfn = "ofo_dossier_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_DOSSIER_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;
}

static void
ofo_dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoDossierPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_dossier_finalize;
}

/**
 * ofo_dossier_new:
 */
ofoDossier *
ofo_dossier_new( const gchar *name )
{
	ofoDossier *dossier;

	dossier = g_object_new( OFO_TYPE_DOSSIER, NULL );
	dossier->priv->name = g_strdup( name );

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
			account, dossier->priv->name );

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

	dossier->priv->sgbd = sgbd;
	dossier->priv->userid = g_strdup( account );

	return( TRUE );
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
				"VER_NUMBER INTEGER   NOT NULL UNIQUE COMMENT 'DB model version number',"
				"VER_DATE   TIMESTAMP DEFAULT 0       COMMENT 'Version application timestamp')" )){
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
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID           INTEGER      NOT NULL UNIQUE COMMENT 'Row identifier',"
			"	DOS_LABEL        VARCHAR(80)                  COMMENT 'Raison sociale',"
			"	DOS_NOTES        VARCHAR(512)                 COMMENT 'Notes',"
			"	DOS_DUREE_EXE    INTEGER                      COMMENT 'Exercice length in month',"
			"	DOS_JOU_VALID    INTEGER                      COMMENT 'Whether journal closing automatically validate pending entries',"
			"	DOS_MAJ_USER     VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
			"	DOS_MAJ_STAMP    TIMESTAMP    NOT NULL        COMMENT 'Properties last update timestamp'"
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
			"	DOS_EXE_DEB      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de début d\\'exercice',"
			"	DOS_EXE_FIN      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de fin d\\'exercice',"
			"	DOS_EXE_ECR      INTEGER      NOT NULL DEFAULT 0 COMMENT 'Last entry number used',"
			"	DOS_EXE_STATUS   INTEGER                         COMMENT 'Status of this exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_EXE_DEB,DOS_EXE_FIN)"
			")" )){
		return( FALSE );
	}

	query = g_strdup(
			"INSERT IGNORE INTO OFA_T_DOSSIER_EXE (DOS_ID,DOS_EXE_STATUS) VALUE (1,1)" );
	if( !ofo_sgbd_query( sgbd, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DEVISES ("
			"	DEV_ID INTEGER NOT NULL AUTO_INCREMENT UNIQUE COMMENT 'Internal identifier of the currency',"
			"	DEV_CODE    VARCHAR(3) BINARY NOT NULL UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	DEV_LABEL        VARCHAR(80) NOT NULL        COMMENT 'Currency label',"
			"	DEV_SYMBOL       VARCHAR(3)  NOT NULL        COMMENT 'Label of the currency'"
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
			"CREATE TABLE IF NOT EXISTS OFA_T_COMPTES ("
				"CPT_NUMBER       VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
				"CPT_LABEL        VARCHAR(80)   NOT NULL        COMMENT 'Account label',"
				"CPT_DEV_ID       INTEGER                       COMMENT 'Identifier of the currency of the account',"
				"CPT_NOTES        VARCHAR(512)                  COMMENT 'Account notes',"
				"CPT_TYPE         CHAR(1)                       COMMENT 'Account type, values R/D',"
				"CPT_MAJ_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
				"CPT_MAJ_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp',"
				"CPT_DEB_MNT      DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant débiteur écritures validées',"
				"CPT_DEB_ECR      INTEGER                       COMMENT 'Numéro de la dernière écriture validée imputée au débit',"
				"CPT_DEB_DATE     DATE                          COMMENT 'Date d\\'effet',"
				"CPT_CRE_MNT      DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant créditeur écritures validées',"
				"CPT_CRE_ECR      INTEGER                       COMMENT 'Numéro de la dernière écriture validée imputée au crédit',"
				"CPT_CRE_DATE     DATE                          COMMENT 'Date d\\'effet',"
				"CPT_BRO_DEB_MNT  DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant débiteur écritures en brouillard',"
				"CPT_BRO_DEB_ECR  INTEGER                       COMMENT 'Numéro de la dernière écriture en brouillard imputée au débit',"
				"CPT_BRO_DEB_DATE DATE                          COMMENT 'Date d\\'effet',"
				"CPT_BRO_CRE_MNT  DECIMAL(15,5) NOT NULL DEFAULT 0 COMMENT 'Montant créditeur écritures en brouillard',"
				"CPT_BRO_CRE_ECR  INTEGER                       COMMENT 'Numéro de la dernière écriture de brouillard imputée au crédit',"
				"CPT_BRO_CRE_DATE DATE                          COMMENT 'Date d\\'effet'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ECRITURES ("
			"	ECR_EFFET     DATE NOT NULL               COMMENT 'Imputation effect date',"
			"	ECR_NUMBER    INTEGER NOT NULL            COMMENT 'Entry number',"
			"	ECR_OPE       DATE NOT NULL               COMMENT 'Operation date',"
			"	ECR_LABEL     VARCHAR(80)                 COMMENT 'Entry label',"
			"	ECR_REF       VARCHAR(20)                 COMMENT 'Piece reference',"
			"	ECR_COMPTE    VARCHAR(20)                 COMMENT 'Account number',"
			"	ECR_DEV_ID    INTEGER                     COMMENT 'Internal identifier of the currency',"
			"	ECR_JOU_ID    INTEGER                     COMMENT 'Internal identifier of the journal',"
			"	ECR_MONTANT   DECIMAL(15,5)               COMMENT 'Entry amount',"
			"	ECR_SENS      INTEGER                     COMMENT 'Sens of the entry \\'DB\\' or \\'CR\\'',"
			"	ECR_STATUS    INTEGER                     COMMENT 'Is the entry validated or deleted ?',"
			"	ECR_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of last update',"
			"	ECR_MAJ_STAMP TIMESTAMP                   COMMENT 'Last update timestamp',"
			"	CONSTRAINT PRIMARY KEY (ECR_EFFET,ECR_NUMBER),"
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
			"	JOU_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	JOU_CLO       DATE                        COMMENT 'Last closing date'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX_DET ("
			"	JOU_ID        INTEGER  NOT NULL           COMMENT 'Internal journal identifier',"
			"	JOU_DEV_ID    INTEGER  NOT NULL UNIQUE    COMMENT 'Internal currency identifier',"
			"	JOU_CLO_DEB   DECIMAL(15,5)               COMMENT 'Debit balance at last closing',"
			"	JOU_CLO_CRE   DECIMAL(15,5)               COMMENT 'Credit balance at last closing',"
			"	JOU_DEB       DECIMAL(15,5)               COMMENT 'Current debit balance',"
			"	JOU_CRE       DECIMAL(15,5)               COMMENT 'Current credit balance',"
			"	CONSTRAINT PRIMARY KEY (JOU_ID,JOU_DEV_ID)"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('ACH','Taux des achats','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('VEN','Taux des ventes','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('EXP','Taux de l\\'exploitant','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('OD','Taux des opérations diverses','Default')" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('BQ','Taux de banque','Default')" )){
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
			"	TAX_VAL_DEB   DATE                        COMMENT 'Validity begin date',"
			"	TAX_VAL_FIN   DATE                        COMMENT 'Validity end date',"
			"	TAX_TAUX      DECIMAL(15,5)               COMMENT 'Taux value',"
			"	TAX_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	TAX_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_TAUX_DET ("
			"	TAX_ID        INTEGER     NOT NULL        COMMENT 'Intern taux identifier',"
			"	TAX_VAL_DEB   DATE                        COMMENT 'Validity begin date',"
			"	TAX_VAL_FIN   DATE                        COMMENT 'Validity end date',"
			"	TAX_TAUX      DECIMAL(15,5)               COMMENT 'Taux value',"
			"	TAX_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	TAX_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
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

	if( !dossier->priv->dispose_has_run ){

		return(( const gchar * ) dossier->priv->name );
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

	if( !dossier->priv->dispose_has_run ){

		return(( const gchar * ) dossier->priv->userid );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_sgbd:
 *
 * Returns: the current sgbd handler.
 */
ofoSgbd *
ofo_dossier_get_sgbd( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !dossier->priv->dispose_has_run ){

		return( dossier->priv->sgbd );
	}

	return( NULL );
}

/**
 * ofo_dossier_entry_insert:
 */
gboolean
ofo_dossier_entry_insert( ofoDossier *dossier,
								const GDate *effet, const GDate *ope,
								const gchar *label, const gchar *ref, const gchar *account,
								gint dev_id, gint jou_id, gdouble amount, ofaEntrySens sens )
{
	gint number;
	ofoEntry *entry;
	ofoJournal *journal;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( effet && g_date_valid( effet ), FALSE );
	g_return_val_if_fail( ope && g_date_valid( ope ), FALSE );
	g_return_val_if_fail( label && g_utf8_strlen( label, -1 ), FALSE );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), FALSE );
	g_return_val_if_fail( amount != 0.0, FALSE );

	number = entry_get_next_number( dossier );

	entry = ofo_entry_insert_new(
				dossier->priv->sgbd, dossier->priv->userid,
				effet, ope, label, ref, account,
				dev_id, jou_id, amount, sens, number );

	journal = ofo_journal_get_by_id( dossier, jou_id );
	g_return_val_if_fail( journal && OFO_IS_JOURNAL( journal ), FALSE );

	ofo_journal_record_entry( journal, dossier->priv->sgbd, entry );

	return( entry != NULL );
}

/*
 * entry_get_next_number:
 */
static gint
entry_get_next_number( ofoDossier *dossier )
{
	gint next_number;
	gchar *query;
	GSList *result, *icol;

	next_number = 1;

	query = g_strdup_printf(
			"SELECT DOS_EXE_ECR FROM OFA_T_DOSSIER_EXE "
			"	WHERE DOS_ID=%d AND DOS_EXE_STATUS=%d",
					THIS_DOS_ID, DOS_STATUS_OPENED );

	result = ofo_sgbd_query_ex( dossier->priv->sgbd, query );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		next_number = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );

		next_number += 1;
		query = g_strdup_printf(
				"UPDATE OFA_T_DOSSIER_EXE "
				"	SET DOS_EXE_ECR=%d "
				"	WHERE DOS_ID=%d AND DOS_EXE_STATUS=%d",
						next_number, THIS_DOS_ID, DOS_STATUS_OPENED );

		ofo_sgbd_query( dossier->priv->sgbd, query );
		g_free( query );
	}

	return( next_number );
}

/**
 * ofo_dossier_get_model:
 *
 * Returns: the searched model.
 */
ofoModel *
ofo_dossier_get_model( ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_dossier_get_model";
	ofoModel *model;
	GList *set, *found;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	model = NULL;

	if( !dossier->priv->dispose_has_run ){

		set = ofo_dossier_get_models_set( dossier );
		found = g_list_find_custom( set, mnemo, ( GCompareFunc ) models_find );
		if( found ){
			model = OFO_MODEL( found->data );
		}
	}

	return( model );
}

/**
 * ofo_dossier_get_models_set:
 *
 * Returns: a copy of the list (not the data) of models.
 * This returned list should be g_list_free() by the caller.
 */
GList *
ofo_dossier_get_models_set( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_models_set";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	if( !dossier->priv->dispose_has_run ){

		if( !dossier->priv->models ){

			dossier->priv->models = ofo_model_load_set( dossier->priv->sgbd );
		}
	}

	return( dossier->priv->models );
}

/**
 * ofo_dossier_insert_model:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_dossier_insert_model( ofoDossier *dossier, ofoModel *model )
{
	GList *set;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	ok = FALSE;

	if( ofo_model_insert( model, dossier->priv->sgbd, dossier->priv->userid )){

		set = g_list_insert_sorted( dossier->priv->models, model, ( GCompareFunc ) models_cmp );
		dossier->priv->models = set;
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update_model:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_dossier_update_model( ofoDossier *dossier, ofoModel *model, const gchar *prev_mnemo )
{
	gboolean ok;
	const gchar *new_mnemo;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	ok = FALSE;

	if( ofo_model_update( model, dossier->priv->sgbd, dossier->priv->userid, prev_mnemo )){

		new_mnemo = ofo_model_get_mnemo( model );

		if( g_utf8_collate( new_mnemo, prev_mnemo )){
			set = g_list_remove( dossier->priv->models, model );
			set = g_list_insert_sorted( set, model, ( GCompareFunc ) models_cmp );
			dossier->priv->models = set;
		}

		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_delete_model:
 */
gboolean
ofo_dossier_delete_model( ofoDossier *dossier, ofoModel *model )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	ok = FALSE;

	if( ofo_model_delete( model, dossier->priv->sgbd, dossier->priv->userid )){

		set = g_list_remove( dossier->priv->models, model );
		dossier->priv->models = set;
		g_object_unref( model );
		ok = TRUE;
	}

	return( ok );
}

static void
models_set_free( GList *set )
{
	g_list_foreach( set, ( GFunc ) g_object_unref, NULL );
	g_list_free( set );
}

static gint
models_cmp( const ofoModel *a, const ofoModel *b )
{
	return( g_utf8_collate( ofo_model_get_mnemo( a ), ofo_model_get_mnemo( b )));
}

static gint
models_find( const ofoModel *a, const gchar *searched_mnemo )
{
	return( g_utf8_collate( ofo_model_get_mnemo( a ), searched_mnemo ));
}

/**
 * ofo_dossier_check_for_taux:
 * @mnemo: desired mnemo
 * @begin: desired beginning of validity.
 *  If not valid, then it is considered without limit.
 * @end: desired end of validity
 *  If not valid, then it is considered without limit.
 *
 * Checks if it is possible to define a new taux with the specified
 * arguments, regarding the other taux already defined. In particular,
 * the desired validity period must not overlap an already existing
 * one.
 *
 * Returns: NULL if the definition would be possible, or a pointer
 * to the object which prevents the definition.
 */
ofoTaux *
ofo_dossier_check_for_taux( ofoDossier *dossier, gint id, const gchar *mnemo, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofo_dossier_check_for_taux";
	ofoTaux *taux;
	GList *found;
	gchar *sbegin, *send;
	sCheckTaux check;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );
	g_return_val_if_fail( begin, NULL );
	g_return_val_if_fail( end, NULL );

	sbegin = my_utils_display_from_date( begin, MY_UTILS_DATE_DMMM );
	send = my_utils_display_from_date( end, MY_UTILS_DATE_DMMM );

	g_debug( "%s: dossier=%p, id=%d, mnemo=%s, begin=%s, end=%s",
			thisfn, ( void * ) dossier, id, mnemo, sbegin, send );

	g_free( send );
	g_free( sbegin );

	taux = NULL;

	if( !dossier->priv->dispose_has_run ){

		check.id = id;
		check.mnemo = mnemo;
		check.begin = begin;
		check.end = end;

		found = g_list_find_custom(
				dossier->priv->taux, &check, ( GCompareFunc ) taux_check );
		if( found ){
			taux = OFO_TAUX( found->data );
		}
	}

	return( taux );
}

/**
 * ofo_dossier_get_taux:
 * @date: [allow-none]: the effect date at which the rate must be valid
 *
 * Returns: the searched taux.
 */
ofoTaux *
ofo_dossier_get_taux( ofoDossier *dossier, const gchar *mnemo, const GDate *date )
{
	static const gchar *thisfn = "ofo_dossier_get_taux";
	ofoTaux *taux;
	GList *set, *found;
	sFindTaux parms;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	taux = NULL;

	if( !dossier->priv->dispose_has_run ){

		set = ofo_dossier_get_taux_set( dossier );
		parms.mnemo = mnemo;
		parms.date = date;
		found = g_list_find_custom( set, &parms, ( GCompareFunc ) taux_find );
		if( found ){
			taux = OFO_TAUX( found->data );
		}
	}

	return( taux );
}

/**
 * ofo_dossier_get_taux_set:
 */
GList *
ofo_dossier_get_taux_set( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_taux_set";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	if( !dossier->priv->dispose_has_run ){

		if( !dossier->priv->taux ){

			dossier->priv->taux = ofo_taux_load_set( dossier->priv->sgbd );
		}
	}

	return( dossier->priv->taux );
}

/**
 * ofo_dossier_insert_taux:
 *
 * we deal here with an update of publicly modifiable taux properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_dossier_insert_taux( ofoDossier *dossier, ofoTaux *taux )
{
	GList *set;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );

	ok = FALSE;

	if( ofo_taux_insert( taux, dossier->priv->sgbd, dossier->priv->userid )){

		set = g_list_insert_sorted( dossier->priv->taux, taux, ( GCompareFunc ) taux_cmp );
		dossier->priv->taux = set;
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update_taux:
 */
gboolean
ofo_dossier_update_taux( ofoDossier *dossier, ofoTaux *taux )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );

	ok = FALSE;

	if( ofo_taux_update( taux, dossier->priv->sgbd, dossier->priv->userid )){

		set = g_list_remove( dossier->priv->taux, taux );
		set = g_list_insert_sorted( set, taux, ( GCompareFunc ) taux_cmp );
		dossier->priv->taux = set;

		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_delete_taux:
 */
gboolean
ofo_dossier_delete_taux( ofoDossier *dossier, ofoTaux *taux )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );

	ok = FALSE;

	if( ofo_taux_delete( taux, dossier->priv->sgbd, dossier->priv->userid )){

		set = g_list_remove( dossier->priv->taux, taux );
		dossier->priv->taux = set;
		g_object_unref( taux );
		ok = TRUE;
	}

	return( ok );
}

static void
taux_set_free( GList *set )
{
	g_list_foreach( set, ( GFunc ) g_object_unref, NULL );
	g_list_free( set );
}

static gint
taux_cmp( const ofoTaux *a, const ofoTaux *b )
{
	gint im;
	im = g_utf8_collate( ofo_taux_get_mnemo( a ), ofo_taux_get_mnemo( b ));
	if( im ){
		return( im );
	}
	return( g_date_compare( ofo_taux_get_val_begin( a ), ofo_taux_get_val_begin( b )));
}

/*
 * we are searching here a taux which would prevent the definition of a
 * new record with the specifications given in @check - no comparaison
 * needed, just return zero if such a record is found
 */
static gint
taux_check( const ofoTaux *ref, sCheckTaux *candidate )
{
	const GDate *ref_begin, *ref_end;
	gboolean begin_ok, end_ok; /* TRUE if periods are compatible */

	/* do not check against the same record */
	if( ofo_taux_get_id( ref ) == candidate->id ){
		return( 1 );
	}

	if( g_utf8_collate( ofo_taux_get_mnemo( ref ), candidate->mnemo )){
		return( 1 ); /* anything but zero */
	}

	/* found another taux with the same mnemo
	 * does its validity period overlap ours ?
	 */
	ref_begin = ofo_taux_get_val_begin( ref );
	ref_end = ofo_taux_get_val_begin( ref );

	if( !g_date_valid( candidate->begin )){
		/* candidate begin is invalid => validity since the very
		 * beginning of the world : the reference must have a valid begin
		 * date greater that the candidate end date */
		begin_ok = ( g_date_valid( ref_begin ) &&
						g_date_valid( candidate->end ) &&
						g_date_compare( ref_begin, candidate->end ) > 0 );
	} else {
		/* valid candidate beginning date
		 * => the reference is either before or after the candidate
		 *  so either the reference ends before the candidate begins
		 *   or the reference begins after the candidate has ended */
		begin_ok = ( g_date_valid( ref_end ) &&
						g_date_compare( ref_end, candidate->begin ) < 0 ) ||
					( g_date_valid( ref_begin ) &&
						g_date_valid( candidate->end ) &&
						g_date_compare( ref_begin, candidate->end ) > 0 );
	}

	if( !g_date_valid( candidate->end )){
		/* candidate ending date is invalid => infinite validity is
		 * required - this is possible if reference as an ending
		 * validity before the beginning of the candidate */
		end_ok = ( g_date_valid( ref_end ) &&
					g_date_valid( candidate->begin ) &&
					g_date_compare( ref_end, candidate->begin ) < 0 );
	} else {
		/* candidate ending date valid
		 * => the reference is either before or after the candidate
		 * so the reference ends before the candidate begins
		 *  or the reference begins afer the candidate has ended */
		end_ok = ( g_date_valid( ref_end ) &&
						g_date_valid( candidate->begin ) &&
						g_date_compare( ref_end, candidate->begin ) < 0 ) ||
					( g_date_valid( ref_begin ) &&
							g_date_compare( ref_begin, candidate->end ) > 0 );
	}
	if( !begin_ok || !end_ok ){
		return( 0 );
	}
	return( 1 );
}

static gint
taux_find( const ofoTaux *a, const sFindTaux *parms )
{
	gint cmp_mnemo;
	gint cmp_date;
	const GDate *val_begin, *val_end;

	cmp_mnemo = g_utf8_collate( ofo_taux_get_mnemo( a ), parms->mnemo );

	if( cmp_mnemo == 0 && parms->date && g_date_valid( parms->date )){
		val_begin = ofo_taux_get_val_begin( a );
		val_end = ofo_taux_get_val_end( a );
		if( val_begin && g_date_valid( val_begin )){
			cmp_date = g_date_compare( val_begin, parms->date );
			if( cmp_date >= 0 ){
				return( cmp_date );
			}
		}
		if( val_end && g_date_valid( val_end )){
			cmp_date = g_date_compare( val_end, parms->date );
			if( cmp_date <= 0 ){
				return( cmp_date );
			}
		}
		return( 0 );
	}

	return( cmp_mnemo );
}
