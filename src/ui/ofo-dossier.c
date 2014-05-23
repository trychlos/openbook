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

static gint     dbmodel_get_version( ofoSgbd *sgbd );
static gboolean dbmodel_to_v1( ofoSgbd *sgbd, const gchar *account );
static gint     entry_get_next_number( ofoDossier *dossier );

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
			"	DOS_EXE_ID       INTEGER      NOT NULL        COMMENT 'Exercice identifier',"
			"	DOS_EXE_DEB      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de début d\\'exercice',"
			"	DOS_EXE_FIN      DATE         NOT NULL DEFAULT 0 COMMENT 'Date de fin d\\'exercice',"
			"	DOS_EXE_ECR      INTEGER      NOT NULL DEFAULT 0 COMMENT 'Last entry number used',"
			"	DOS_EXE_STATUS   INTEGER                         COMMENT 'Status of this exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_EXE_ID)"
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
 * ofo_dossier_update_taux:
 */
gboolean
ofo_dossier_update_taux( ofoDossier *dossier, ofoTaux *taux )
{
	gboolean ok;

	ok = FALSE;

	return( ok );
}

/**
 * ofo_dossier_delete_taux:
 */
gboolean
ofo_dossier_delete_taux( ofoDossier *dossier, ofoTaux *taux )
{
	gboolean ok;

	ok = FALSE;

	return( ok );
}
