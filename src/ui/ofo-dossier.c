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

#include <stdlib.h>

#include "ui/my-utils.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-account.h"
#include "ui/ofo-journal.h"

/* private class data
 */
struct _ofoDossierClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofoDossierPrivate {
	gboolean  dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar    *name;
	ofaSgbd  *sgbd;
	gchar    *userid;
	GList    *accounts;					/* chart of accounts */
	GList    *devises;					/* devises */
	GList    *journals;					/* journals set */
	GList    *models;					/* entry models set */

	/* row id 0
	 */
	gchar    *label;					/* raison sociale */
	gchar    *notes;					/* notes */
	GDate    *exe_deb;					/* début d'exercice */
	GDate    *exe_fin;					/* fin d'exercice */
};

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1

static ofoBaseClass *st_parent_class  = NULL;

static GType    register_type( void );
static void     class_init( ofoDossierClass *klass );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     instance_dispose( GObject *instance );
static void     instance_finalize( GObject *instance );

static gboolean check_user_exists( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account );
static gint     dbmodel_get_version( ofaSgbd *sgbd );
static gboolean dbmodel_to_v1( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account );
static void     accounts_chart_free( GList *chart );
static gint     accounts_cmp( const ofoAccount *a, const ofoAccount *b );
static gint     accounts_find( const ofoAccount *a, const gchar *searched_number );
static void     devises_set_free( GList *set );
static gint     devises_cmp( const ofoDevise *a, const ofoDevise *b );
static gint     devises_find( const ofoDevise *a, const gchar *searched );
static void     journals_set_free( GList *set );
static gint     journals_cmp( const ofoJournal *a, const ofoJournal *b );
static gint     journals_find( const ofoJournal *a, const gchar *searched_mnemo );
static void     models_set_free( GList *set );
static gint     models_cmp( const ofoModel *a, const ofoModel *b );
static gint     models_find( const ofoModel *a, const gchar *searched_mnemo );

GType
ofo_dossier_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_dossier_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoDossierClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoDossier ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoDossier", &info, 0 );

	return( type );
}

static void
class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoDossierClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_dossier_instance_init";
	ofoDossier *self;

	g_return_if_fail( OFO_IS_DOSSIER( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_DOSSIER( instance );

	self->private = g_new0( ofoDossierPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_instance_dispose";
	ofoDossierPrivate *priv;

	g_return_if_fail( OFO_IS_DOSSIER( instance ));

	priv = ( OFO_DOSSIER( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		g_free( priv->name );
		g_free( priv->userid );

		if( priv->sgbd ){
			g_object_unref( priv->sgbd );
		}

		if( priv->accounts ){
			accounts_chart_free( priv->accounts );
			priv->accounts = NULL;
		}
		if( priv->devises ){
			devises_set_free( priv->devises );
			priv->devises = NULL;
		}
		if( priv->journals ){
			journals_set_free( priv->journals );
			priv->journals = NULL;
		}
		if( priv->models ){
			models_set_free( priv->models );
			priv->models = NULL;
		}

		g_free( priv->label );
		g_free( priv->notes );
		if( priv->exe_deb ){
			g_date_free( priv->exe_deb );
		}
		if( priv->exe_fin ){
			g_date_free( priv->exe_fin );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_instance_finalize";
	ofoDossierPrivate *priv;

	g_return_if_fail( OFO_IS_DOSSIER( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_DOSSIER( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
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

/**
 * ofo_dossier_open:
 */
gboolean
ofo_dossier_open( ofoDossier *dossier, GtkWindow *parent,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofo_dossier_open";
	ofaSgbd *sgbd;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	g_debug( "%s: dossier=%p, parent=%p, host=%s, port=%d, socket=%s, dbname=%s, account=%s, password=%s",
			thisfn,
			( void * ) dossier, ( void * ) parent, host, port, socket, dbname, account, password );

	sgbd = ofa_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofa_sgbd_connect( sgbd,
			parent,
			host,
			port,
			socket,
			dbname,
			account,
			password )){

		g_object_unref( sgbd );
		return( FALSE );
	}

	if( !check_user_exists( sgbd, parent, account )){
		g_object_unref( sgbd );
		return( FALSE );
	}

	ofo_dossier_dbmodel_update( sgbd, parent, account );

	dossier->private->sgbd = sgbd;
	dossier->private->userid = g_strdup( account );

	return( TRUE );
}

static gboolean
check_user_exists( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account )
{
	gchar *query;
	GSList *res;
	gboolean exists;

	exists = FALSE;
	query = g_strdup_printf( "SELECT ROL_USER FROM OFA_T_ROLES WHERE ROL_USER='%s'", account );
	res = ofa_sgbd_query_ex( sgbd, parent, query );
	if( res ){
		gchar *s = ( gchar * )(( GSList * ) res->data )->data;
		if( !g_utf8_collate( account, s )){
			exists = TRUE;
		}
		ofa_sgbd_free_result( res );
	}
	g_free( query );

	return( exists );
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
ofo_dossier_dbmodel_update( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_update";
	gint cur_version;

	g_debug( "%s: sgbd=%p, parent=%p, account=%s",
			thisfn, ( void * ) sgbd, ( void * ) parent, account );

	cur_version = dbmodel_get_version( sgbd );
	g_debug( "%s: cur_version=%d, THIS_DBMODEL_VERSION=%d", thisfn, cur_version, THIS_DBMODEL_VERSION );

	if( cur_version < THIS_DBMODEL_VERSION ){
		if( cur_version < 1 ){
			dbmodel_to_v1( sgbd, parent, account );
		}
	}

	return( TRUE );
}

/*
 * returns the last complete version
 * i.e. une version where the version date is set
 */
static gint
dbmodel_get_version( ofaSgbd *sgbd )
{
	GSList *res;
	gint vmax = 0;

	res = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0" );
	if( res ){
		gchar *s = ( gchar * )(( GSList * ) res->data )->data;
		if( s ){
			vmax = atoi( s );
		}
		ofa_sgbd_free_result( res );
	}

	return( vmax );
}

/**
 * ofo_dossier_dbmodel_to_v1:
 */
static gboolean
dbmodel_to_v1( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_to_v1";
	gchar *query;

	g_debug( "%s: sgbd=%p, parent=%p, account=%s",
			thisfn, ( void * ) sgbd, ( void * ) parent, account );

	/* default value for timestamp cannot be null */
	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
				"VER_NUMBER INTEGER   NOT NULL UNIQUE COMMENT 'DB model version number',"
				"VER_DATE   TIMESTAMP DEFAULT 0       COMMENT 'Version application timestamp')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT INTO OFA_T_VERSION (VER_NUMBER, VER_DATE) VALUES (1, 0)"
				"ON DUPLICATE KEY UPDATE VER_NUMBER=1, VER_DATE=0" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_ROLES ("
				"ROL_USER     VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'User account',"
				"ROL_IS_ADMIN INTEGER                            COMMENT 'Whether the user has administration role')")){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_ROLES (ROL_USER, ROL_IS_ADMIN) VALUES ('%s',1)", account );
	if( !ofa_sgbd_query( sgbd, parent, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
				"DOS_ID           INTEGER      NOT NULL UNIQUE COMMENT 'Row identifier',"
				"DOS_LABEL        VARCHAR(80)                  COMMENT 'Raison sociale',"
				"DOS_NOTES        VARCHAR(512)                 COMMENT 'Notes',"
				"DOS_MAJ_USER     VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
				"DOS_MAJ_STAMP    TIMESTAMP    NOT NULL        COMMENT 'Properties last update timestamp',"
				"DOS_EXE_DEB      DATE         NOT NULL        COMMENT 'Date de début d\\'exercice',"
				"DOS_EXE_FIN      DATE         NOT NULL        COMMENT 'Date de fin d\\'exercice',"
				"DOS_LAST_ECR     INTEGER            DEFAULT 0 COMMENT 'Dernier numéro d\\'écriture attribué'"
			")" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
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
	if( !ofa_sgbd_query( sgbd, parent, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_COMPTES ("
				"CPT_NUMBER       VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
				"CPT_LABEL        VARCHAR(80)   NOT NULL        COMMENT 'Account label',"
				"CPT_DEV          CHAR(3)       NOT NULL        COMMENT 'ISO 3A currency of the account',"
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

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_JOURNAUX ("
			"	JOU_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern journal identifier',"
			"	JOU_MNEMO     VARCHAR(3) BINARY  NOT NULL UNIQUE COMMENT 'Journal mnemonic',"
			"	JOU_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Journal label',"
			"	JOU_NOTES     VARCHAR(512)                COMMENT 'Journal notes',"
			"	JOU_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	JOU_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	JOU_MAXDATE   DATE                        COMMENT 'Most recent effect date of the entries',"
			"	JOU_CLO       DATE                        COMMENT 'Last closing date'"
			")" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('ACH','Journal des achats','Default')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('VEN','Journal des ventes','Default')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('EXP','Journal de l\\'exploitant','Default')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('OD','Journal des opérations diverses','Default')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"INSERT IGNORE INTO OFA_T_JOURNAUX (JOU_MNEMO, JOU_LABEL, JOU_MAJ_USER) "
			"	VALUES ('BQ','Journal de banque','Default')" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES ("
			"	MOD_ID        INTEGER NOT NULL UNIQUE AUTO_INCREMENT COMMENT 'Internal model identifier',"
			"	MOD_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Model mnemonic',"
			"	MOD_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Model label',"
			"	MOD_JOU_ID    INTEGER                     COMMENT 'Model journal',"
			"	MOD_NOTES     VARCHAR(512)                COMMENT 'Model notes',"
			"	MOD_MAJ_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	MOD_MAJ_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp'"
			")" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_MODELES_DET ("
			"	MOD_ID              INTEGER NOT NULL UNIQUE COMMENT 'Internal model identifier',"
			"	MOD_DET_RANG        INTEGER                 COMMENT 'Entry number',"
			"	MOD_DET_COMMENT     VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_CPT         VARCHAR(20)             COMMENT 'Account number',"
			"	MOD_DET_CPT_VER     INTEGER                 COMMENT 'Account number is locked',"
			"	MOD_DET_LABEL       VARCHAR(80)             COMMENT 'Entry label',"
			"	MOD_DET_LABEL_VER   INTEGER                 COMMENT 'Entry label is locked',"
			"	MOD_DET_DEBIT       VARCHAR(80)             COMMENT 'Debit amount',"
			"	MOD_DET_DEBIT_VER   INTEGER                 COMMENT 'Debit amount is locked',"
			"	MOD_DET_CREDIT      VARCHAR(80)             COMMENT 'Credit amount',"
			"	MOD_DET_CREDIT_VER  INTEGER                 COMMENT 'Credit amount is locked'"
			")" )){
		return( FALSE );
	}

	/* we do this only at the end of the model creation
	 * as a mark that all has been successfully done
	 */
	if( !ofa_sgbd_query( sgbd, parent,
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

	if( !dossier->private->dispose_has_run ){

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

	if( !dossier->private->dispose_has_run ){

		return(( const gchar * ) dossier->private->userid );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_account:
 *
 * Returns: the searched account.
 */
ofoAccount *
ofo_dossier_get_account( const ofoDossier *dossier, const gchar *number )
{
	static const gchar *thisfn = "ofo_dossier_get_account";
	ofoAccount *account;
	GList *found;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( number && g_utf8_strlen( number, -1 ), NULL );

	g_debug( "%s: dossier=%p, number=%s", thisfn, ( void * ) dossier, number );

	account = NULL;

	if( !dossier->private->dispose_has_run ){

		found = g_list_find_custom(
				dossier->private->accounts, number, ( GCompareFunc ) accounts_find );
		if( found ){
			account = OFO_ACCOUNT( found->data );
		}
	}

	return( account );
}

/**
 * ofo_dossier_get_accounts_chart:
 */
GList *
ofo_dossier_get_accounts_chart( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_accounts_chart";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	if( !dossier->private->dispose_has_run ){

		if( !dossier->private->accounts ){

			dossier->private->accounts = ofo_account_load_chart( dossier->private->sgbd );
		}
	}

	return( dossier->private->accounts );
}

/**
 * ofo_dossier_insert_account:
 *
 * we deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_dossier_insert_account( ofoDossier *dossier, ofoAccount *account )
{
	GList *chart;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	ok = FALSE;

	if( ofo_account_insert( account, dossier->private->sgbd, dossier->private->userid )){

		chart = g_list_insert_sorted( dossier->private->accounts, account, ( GCompareFunc ) accounts_cmp );
		dossier->private->accounts = chart;
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update_account:
 *
 * we deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_dossier_update_account( ofoDossier *dossier, ofoAccount *account, const gchar *prev_number )
{
	gboolean ok;
	const gchar *new_number;
	GList *chart;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( prev_number && g_utf8_strlen( prev_number, -1 ), FALSE );

	ok = FALSE;

	if( ofo_account_update(
			account, dossier->private->sgbd, dossier->private->userid, prev_number )){

		new_number = ofo_account_get_number( account );

		if( g_utf8_collate( new_number, prev_number )){
			chart = g_list_remove( dossier->private->accounts, account );
			chart = g_list_insert_sorted( chart, account, ( GCompareFunc ) accounts_cmp );
			dossier->private->accounts = chart;
		}

		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_delete_account:
 */
gboolean
ofo_dossier_delete_account( ofoDossier *dossier, ofoAccount *account )
{
	gboolean ok;
	GList *chart;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	ok = FALSE;

	if( ofo_account_delete( account, dossier->private->sgbd, dossier->private->userid )){

		chart = g_list_remove( dossier->private->accounts, account );
		dossier->private->accounts = chart;
		g_object_unref( account );
		ok = TRUE;
	}

	return( ok );
}

static void
accounts_chart_free( GList *chart )
{
	g_list_foreach( chart, ( GFunc ) g_object_unref, NULL );
	g_list_free( chart );
}

static gint
accounts_cmp( const ofoAccount *a, const ofoAccount *b )
{
	return( g_utf8_collate( ofo_account_get_number( a ), ofo_account_get_number( b )));
}

static gint
accounts_find( const ofoAccount *a, const gchar *searched_number )
{
	return( g_utf8_collate( ofo_account_get_number( a ), searched_number ));
}

/**
 * ofo_dossier_get_devise:
 *
 * Returns: the searched devise.
 */
ofoDevise *
ofo_dossier_get_devise( const ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_dossier_get_devise";
	GList *found;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	if( !dossier->private->dispose_has_run ){

		found = g_list_find_custom(
				dossier->private->devises, mnemo, ( GCompareFunc ) devises_find );
		if( found ){
			return( OFO_DEVISE( found->data ));
		}
	}

	return( NULL );
}

/**
 * ofo_dossier_get_devises_set:
 */
GList *
ofo_dossier_get_devises_set( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_devises_set";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	if( !dossier->private->dispose_has_run ){

		if( !dossier->private->devises ){

			dossier->private->devises = ofo_devise_load_set( dossier->private->sgbd );
		}
	}

	return( dossier->private->devises );
}

/**
 * ofo_dossier_insert_devise:
 *
 * we deal here with an update of publicly modifiable devise properties
 */
gboolean
ofo_dossier_insert_devise( ofoDossier *dossier, ofoDevise *devise )
{
	GList *set;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );

	ok = FALSE;

	if( ofo_devise_insert( devise, dossier->private->sgbd )){

		set = g_list_insert_sorted( dossier->private->devises, devise, ( GCompareFunc ) devises_cmp );
		dossier->private->devises = set;
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update_devise:
 *
 * we deal here with an update of publicly modifiable devise properties
 */
gboolean
ofo_dossier_update_devise( ofoDossier *dossier, ofoDevise *devise )
{
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );

	ok = ofo_devise_update( devise, dossier->private->sgbd );

	return( ok );
}

/**
 * ofo_dossier_delete_devise:
 */
gboolean
ofo_dossier_delete_devise( ofoDossier *dossier, ofoDevise *devise )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );

	ok = FALSE;

	if( ofo_devise_delete( devise, dossier->private->sgbd )){

		set = g_list_remove( dossier->private->devises, devise );
		dossier->private->devises = set;
		g_object_unref( devise );
		ok = TRUE;
	}

	return( ok );
}

static void
devises_set_free( GList *set )
{
	g_list_foreach( set, ( GFunc ) g_object_unref, NULL );
	g_list_free( set );
}

static gint
devises_cmp( const ofoDevise *a, const ofoDevise *b )
{
	return( g_utf8_collate( ofo_devise_get_mnemo( a ), ofo_devise_get_mnemo( b )));
}

static gint
devises_find( const ofoDevise *a, const gchar *searched )
{
	return( g_utf8_collate( ofo_devise_get_mnemo( a ), searched ));
}

/**
 * ofo_dossier_get_journal:
 *
 * Returns: the searched journal.
 */
ofoJournal *
ofo_dossier_get_journal( const ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_dossier_get_journal";
	ofoJournal *journal;
	GList *found;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	journal = NULL;

	if( !dossier->private->dispose_has_run ){

		found = g_list_find_custom(
				dossier->private->journals, mnemo, ( GCompareFunc ) journals_find );
		if( found ){
			journal = OFO_JOURNAL( found->data );
		}
	}

	return( journal );
}

/**
 * ofo_dossier_get_journals_set:
 */
GList *
ofo_dossier_get_journals_set( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_journals_set";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	if( !dossier->private->dispose_has_run ){

		if( !dossier->private->journals ){

			dossier->private->journals = ofo_journal_load_set( dossier->private->sgbd );
		}
	}

	return( dossier->private->journals );
}

/**
 * ofo_dossier_insert_journal:
 *
 * we deal here with an update of publicly modifiable journal properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_dossier_insert_journal( ofoDossier *dossier, ofoJournal *journal )
{
	GList *set;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	ok = FALSE;

	if( ofo_journal_insert( journal, dossier->private->sgbd, dossier->private->userid )){

		set = g_list_insert_sorted( dossier->private->journals, journal, ( GCompareFunc ) journals_cmp );
		dossier->private->journals = set;
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_update_journal:
 */
gboolean
ofo_dossier_update_journal( ofoDossier *dossier, ofoJournal *journal )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	ok = FALSE;

	if( ofo_journal_update( journal, dossier->private->sgbd, dossier->private->userid )){

		set = g_list_remove( dossier->private->journals, journal );
		set = g_list_insert_sorted( set, journal, ( GCompareFunc ) journals_cmp );
		dossier->private->journals = set;

		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_dossier_delete_journal:
 */
gboolean
ofo_dossier_delete_journal( ofoDossier *dossier, ofoJournal *journal )
{
	gboolean ok;
	GList *set;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	ok = FALSE;

	if( ofo_journal_delete( journal, dossier->private->sgbd, dossier->private->userid )){

		set = g_list_remove( dossier->private->journals, journal );
		dossier->private->journals = set;
		g_object_unref( journal );
		ok = TRUE;
	}

	return( ok );
}

static void
journals_set_free( GList *set )
{
	g_list_foreach( set, ( GFunc ) g_object_unref, NULL );
	g_list_free( set );
}

static gint
journals_cmp( const ofoJournal *a, const ofoJournal *b )
{
	return( g_utf8_collate( ofo_journal_get_mnemo( a ), ofo_journal_get_mnemo( b )));
}

static gint
journals_find( const ofoJournal *a, const gchar *searched_mnemo )
{
	return( g_utf8_collate( ofo_journal_get_mnemo( a ), searched_mnemo ));
}

/**
 * ofo_dossier_get_model:
 *
 * Returns: the searched model.
 */
ofoModel *
ofo_dossier_get_model( const ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_dossier_get_model";
	ofoModel *model;
	GList *found;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	model = NULL;

	if( !dossier->private->dispose_has_run ){

		found = g_list_find_custom(
				dossier->private->models, mnemo, ( GCompareFunc ) models_find );
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

	if( !dossier->private->dispose_has_run ){

		if( !dossier->private->models ){

			dossier->private->models = ofo_model_load_set( dossier->private->sgbd );
		}
	}

	return( dossier->private->models );
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

	if( ofo_model_insert( model, dossier->private->sgbd, dossier->private->userid )){

		set = g_list_insert_sorted( dossier->private->models, model, ( GCompareFunc ) models_cmp );
		dossier->private->models = set;
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

	if( ofo_model_update( model, dossier->private->sgbd, dossier->private->userid, prev_mnemo )){

		new_mnemo = ofo_model_get_mnemo( model );

		if( g_utf8_collate( new_mnemo, prev_mnemo )){
			set = g_list_remove( dossier->private->models, model );
			set = g_list_insert_sorted( set, model, ( GCompareFunc ) models_cmp );
			dossier->private->models = set;
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

	if( ofo_model_delete( model, dossier->private->sgbd, dossier->private->userid )){

		set = g_list_remove( dossier->private->models, model );
		dossier->private->models = set;
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
