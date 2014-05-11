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

#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofoDossierClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofoDossierPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar   *name;
	ofaSgbd *sgbd;

	/* row id 0
	 */
	gchar   *label;						/* raison sociale */
	gchar   *notes;						/* notes */
	GDate   *exe_deb;					/* début d'exercice */
	GDate   *exe_fin;					/* fin d'exercice */
};

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1

static GObjectClass *st_parent_class  = NULL;

static GType    register_type( void );
static void     class_init( ofoDossierClass *klass );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     instance_dispose( GObject *instance );
static void     instance_finalize( GObject *instance );

static gboolean check_user_exists( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account );
static gint     dbmodel_get_version( ofaSgbd *sgbd );
static gboolean dbmodel_to_v1( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account );

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

	type = g_type_register_static( G_TYPE_OBJECT, "ofoDossier", &info, 0 );

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

		if( priv->sgbd ){
			g_object_unref( priv->sgbd );
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
				"ROL_USER     VARCHAR(20) NOT NULL UNIQUE COMMENT 'User account',"
				"ROL_IS_ADMIN INTEGER                     COMMENT 'Whether the user has administration role')")){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT INTO OFA_T_ROLES (ROL_USER, ROL_IS_ADMIN) VALUES ('%s',1)"
				"ON DUPLICATE KEY UPDATE ROL_USER='%s'", account, account );
	if( !ofa_sgbd_query( sgbd, parent, query )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
				"DOS_ID           INTEGER UNIQUE NOT NULL     COMMENT 'Row identifier',"
				"DOS_LABEL        VARCHAR(80)                 COMMENT 'Raison sociale',"
				"DOS_NOTES        VARCHAR(512)                COMMENT 'Notes',"
				"DOS_MAJ_USER     VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
				"DOS_MAJ_STAMP    TIMESTAMP                   COMMENT 'Properties last update timestamp',"
				"DOS_EXE_DEB      DATE           NOT NULL     COMMENT 'Date de début d\\'exercice',"
				"DOS_EXE_FIN      DATE           NOT NULL     COMMENT 'Date de fin d\\'exercice'"
			")" )){
		return( FALSE );
	}

	if( !ofa_sgbd_query( sgbd, parent,
			"CREATE TABLE IF NOT EXISTS OFA_T_COMPTES ("
				"CPT_NUMBER       VARCHAR(20) NOT NULL UNIQUE COMMENT 'Account number',"
				"CPT_LABEL        VARCHAR(80)                 COMMENT 'Account label',"
				"CPT_DEV          CHAR(3)     NOT NULL        COMMENT 'ISO 3A devise of the account',"
				"CPT_NOTES        VARCHAR(512)                COMMENT 'Account notes',"
				"CPT_TYPE         CHAR(1)     NOT NULL        COMMENT 'Account type, values R/D',"
				"CPT_MAJ_USER     VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
				"CPT_MAJ_STAMP    TIMESTAMP                   COMMENT 'Properties last update timestamp',"
				"CPT_DEB_MNT      DECIMAL(15,5)               COMMENT 'Montant débiteur écritures validées',"
				"CPT_DEB_ECR      INTEGER                     COMMENT 'Numéro de la dernière écriture validée imputée au débit',"
				"CPT_DEB_DATE     DATE                        COMMENT 'Date d\\'effet',"
				"CPT_CRE_MNT      DECIMAL(15,5)               COMMENT 'Montant créditeur écritures validées',"
				"CPT_CRE_ECR      INTEGER                     COMMENT 'Numéro de la dernière écriture validée imputée au crédit',"
				"CPT_CRE_DATE     DATE                        COMMENT 'Date d\\'effet',"
				"CPT_BRO_DEB_MNT  DECIMAL(15,5)               COMMENT 'Montant débiteur écritures en brouillard',"
				"CPT_BRO_DEB_ECR  INTEGER                     COMMENT 'Numéro de la dernière écriture en brouillard imputée au débit',"
				"CPT_BRO_DEB_DATE DATE                        COMMENT 'Date d\\'effet',"
				"CPT_BRO_CRE_MNT  DECIMAL(15,5)               COMMENT 'Montant créditeur écritures en brouillard',"
				"CPT_BRO_CRE_ECR  INTEGER                     COMMENT 'Numéro de la dernière écriture de brouillard imputée au crédit',"
				"CPT_BRO_CRE_DATE DATE                        COMMENT 'Date d\\'effet'"
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
