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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-sgbd.h"

#include "core/ofo-marshal.h"

/* priv instance data
 */
typedef struct _sDetailExe              sDetailExe;

struct _ofoDossierPrivate {

	/* internals
	 */
	gchar      *name;					/* the name of the dossier in settings */
	ofoSgbd    *sgbd;
	gchar      *userid;

	/* row id 1
	 */
	gchar      *currency;
	GDate       exe_begin;
	GDate       exe_end;
	gint        exe_length;				/* exercice length (in month) */
	gchar      *exe_notes;
	gchar      *label;					/* raison sociale */
	gchar      *notes;					/* notes */
	gchar      *siren;
	gchar      *upd_user;
	GTimeVal    upd_stamp;
	gint        exe_id;
	gint        last_exe_id;
	ofxCounter  last_bat;
	ofxCounter  last_batline;
	ofxCounter  last_entry;
	ofxCounter  last_settlement;
	gchar      *status;
};

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	VALIDATED_ENTRY,
	DOSSIER_BEGIN,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

/* the last DB model version
 */
#define THIS_DBMODEL_VERSION            1
#define THIS_DOS_ID                     1

/* dossier status
 */
#define DOSSIER_CURRENT                 "C"
#define DOSSIER_ARCHIVED                "A"

static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( const ofaIExportable *instance );
static void        connect_objects_handlers( const ofoDossier *dossier );
static void        on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static gint        dbmodel_get_version( ofoSgbd *sgbd );
static gboolean    dbmodel_to_v1( ofoSgbd *sgbd, const gchar *name, const gchar *account );
static void        dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number );
static void        dossier_set_upd_user( ofoDossier *dossier, const gchar *user );
static void        dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp );
static void        dossier_set_this_exe_id( ofoDossier *dossier, gint exe_id );
static void        dossier_set_last_exe_id( ofoDossier *dossier, gint exe_id );
static void        dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_status( ofoDossier *dossier, const gchar *status );
static void        on_new_object_cleanup_handler( ofoDossier *dossier, ofoBase *object );
static void        on_updated_object_cleanup_handler( ofoDossier *dossier, ofoBase *object, const gchar *prev_id );
static void        on_deleted_object_cleanup_handler( ofoDossier *dossier, ofoBase *object );
static void        on_reloaded_dataset_cleanup_handler( ofoDossier *dossier, GType type );
static void        on_validated_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry );
static gboolean    dossier_do_read( ofoDossier *dossier );
static gboolean    dossier_read_properties( ofoDossier *dossier );
static gboolean    dossier_do_update( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user );
static gboolean    do_update_properties( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, const ofoDossier *dossier );

G_DEFINE_TYPE_EXTENDED( ofoDossier, ofo_dossier, OFO_TYPE_BASE, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IEXPORTABLE, iexportable_iface_init ));

static void
dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_finalize";
	ofoDossierPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_DOSSIER( instance ));

	/* free data members here */
	priv = OFO_DOSSIER( instance )->priv;

	g_free( priv->name );
	g_free( priv->userid );

	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->siren );
	g_free( priv->upd_user );
	g_free( priv->exe_notes );
	g_free( priv->status );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->finalize( instance );
}

static void
dossier_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_dispose";
	ofoDossierPrivate *priv;

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		/* unref object members here */
		priv = OFO_DOSSIER( instance )->priv;

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_DOSSIER, ofoDossierPrivate );
}

static void
ofo_dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_finalize;

	g_type_class_add_private( klass, sizeof( ofoDossierPrivate ));

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

	/**
	 * ofoDossier::ofa-signal-dossier-begin:
	 *
	 * This signal is sent on the dossier when its exercice
	 * beginning date changes.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofoDossier *dossier,
	 *                      GDate    *begin_date,
	 * 						gpointer  user_data );
	 */
	st_signals[ DOSSIER_BEGIN ] = g_signal_new_class_handler(
				OFA_SIGNAL_DOSSIER_BEGIN,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
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

#if 0
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
#endif

/**
 * ofo_dossier_open:
 * @dossier: is expected to have been initialized with the label
 * @account:
 * @password:
 */
gboolean
ofo_dossier_open( ofoDossier *dossier, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofo_dossier_open";
	ofoDossierPrivate *priv;
	ofoSgbd *sgbd;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	g_debug( "%s: dossier=%p, account=%s, password=%s",
			thisfn,
			( void * ) dossier, account, password );

	priv = dossier->priv;
	sgbd = ofo_sgbd_new( priv->name );

	if( !ofo_sgbd_connect( sgbd, account, password, TRUE )){
		g_object_unref( sgbd );
		return( FALSE );
	}

	priv->sgbd = sgbd;
	priv->userid = g_strdup( account );

	ofo_dossier_dbmodel_update( sgbd, dossier->priv->name, account );
	connect_objects_handlers( dossier );

	return( dossier_do_read( dossier ));
}

/*
 * be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself
 *
 * use case: the intermediate closing by ledger may be run without
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
	ofo_ledger_connect_handlers( dossier );
	ofo_ope_template_connect_handlers( dossier );

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

	if( OFO_IS_CURRENCY( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
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
					"	SET DOS_DEF_CURRENCY='%s' WHERE DOS_DEF_CURRENCY='%s'", code, prev_id );

	ofo_sgbd_query( ofo_dossier_get_sgbd( dossier ), query, TRUE );

	g_free( query );
}

/**
 * ofo_dossier_dbmodel_update:
 * @sgbd: an already opened connection
 * @name: the name of the dossier
 * @account: the account which has opened this connection; it will be checked
 *  against its permissions when trying to update the data model
 *
 * Update the DB model in the SGBD
 */
gboolean
ofo_dossier_dbmodel_update( ofoSgbd *sgbd, const gchar *name, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_update";
	gint cur_version;

	g_debug( "%s: sgbd=%p, name=%s, account=%s", thisfn, ( void * ) sgbd, name, account );

	cur_version = dbmodel_get_version( sgbd );
	g_debug( "%s: cur_version=%d, THIS_DBMODEL_VERSION=%d", thisfn, cur_version, THIS_DBMODEL_VERSION );

	if( cur_version < THIS_DBMODEL_VERSION ){
		if( cur_version < 1 ){
			dbmodel_to_v1( sgbd, name, account );
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
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0", FALSE );
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
dbmodel_to_v1( ofoSgbd *sgbd, const gchar *name, const gchar *account )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_to_v1";
	gchar *query;

	g_debug( "%s: sgbd=%p, account=%s", thisfn, ( void * ) sgbd, account );

	/* default value for timestamp cannot be null */
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER NOT NULL UNIQUE DEFAULT 0     COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP DEFAULT 0                   COMMENT 'Version application timestamp')",
			TRUE)){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (1, 0)",
			TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ACCOUNTS ("
			"	ACC_NUMBER          VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
			"	ACC_LABEL           VARCHAR(80)   NOT NULL       COMMENT 'Account label',"
			"	ACC_CURRENCY        VARCHAR(3)                   COMMENT 'ISO 3A identifier of the currency of the account',"
			"	ACC_NOTES           VARCHAR(4096)                COMMENT 'Account notes',"
			"	ACC_TYPE            CHAR(1)                      COMMENT 'Account type, values R/D',"
			"	ACC_SETTLEABLE      CHAR(1)                      COMMENT 'Whether the account is settleable',"
			"	ACC_RECONCILIABLE   CHAR(1)                      COMMENT 'Whether the account is reconciliable',"
			"	ACC_FORWARD         CHAR(1)                      COMMENT 'Whether the account supports carried forwards',"
			"	ACC_UPD_USER        VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
			"	ACC_UPD_STAMP       TIMESTAMP                    COMMENT 'Properties last update timestamp',"
			"	ACC_DEB_ENTRY       BIGINT                       COMMENT 'Number of the most recent validated debit entry',"
			"	ACC_DEB_DATE        DATE                         COMMENT 'Effect date of the most recent validated debit entry',"
			"	ACC_DEB_AMOUNT      DECIMAL(20,5)                COMMENT 'Debit balance of validated entries',"
			"	ACC_CRE_ENTRY       BIGINT                       COMMENT 'Number of the most recent validated credit entry',"
			"	ACC_CRE_DATE        DATE                         COMMENT 'Effect date of the most recent validated credit entry',"
			"	ACC_CRE_AMOUNT      DECIMAL(20,5)                COMMENT 'Credit balance of validated entries',"
			"	ACC_DAY_DEB_ENTRY   BIGINT                       COMMENT 'Number of the most recent rough debit entry',"
			"	ACC_DAY_DEB_DATE    DATE                         COMMENT 'Effect date of the most recent rough debit entry',"
			"	ACC_DAY_DEB_AMOUNT  DECIMAL(20,5)                COMMENT 'Debit balance of rough entries',"
			"	ACC_DAY_CRE_ENTRY   BIGINT                       COMMENT 'Number of the most recent rough credit entry',"
			"	ACC_DAY_CRE_DATE    DATE                         COMMENT 'Effect date of the most recent rough credit entry',"
			"	ACC_DAY_CRE_AMOUNT  DECIMAL(20,5)                COMMENT 'Credit balance of rough entries',"
			"	ACC_OPEN_DEB_ENTRY  BIGINT                       COMMENT 'Number of the most recent debit entry at the exercice opening',"
			"	ACC_OPEN_DEB_DATE   DATE                         COMMENT 'Effect date of the most recent debit entry at the exercice opening',"
			"	ACC_OPEN_DEB_AMOUNT DECIMAL(20,5)                COMMENT 'Debit balance at the exercice opening',"
			"	ACC_OPEN_CRE_ENTRY  BIGINT                       COMMENT 'Number of the most recent credit entry at the exercice opening',"
			"	ACC_OPEN_CRE_DATE   DATE                         COMMENT 'Effect date of the most recent credit entry at the exercice opening',"
			"	ACC_OPEN_CRE_AMOUNT DECIMAL(20,5)                COMMENT 'Credit balance at the exercice opening'"
			")", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofo_sgbd_query( sgbd,
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
			")", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofo_sgbd_query( sgbd,
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
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT ("
			"	BAT_ID        BIGINT      NOT NULL UNIQUE COMMENT 'Intern import identifier',"
			"	BAT_URI       VARCHAR(128)                COMMENT 'Imported URI',"
			"	BAT_FORMAT    VARCHAR(80)                 COMMENT 'Identified file format',"
			"	BAT_COUNT     INTEGER                     COMMENT 'Imported lines count',"
			"	BAT_BEGIN     DATE                        COMMENT 'Begin date of the transaction list',"
			"	BAT_END       DATE                        COMMENT 'End date of the transaction list',"
			"	BAT_RIB       VARCHAR(80)                 COMMENT 'Bank provided RIB',"
			"	BAT_CURRENCY  VARCHAR(3)                  COMMENT 'Account currency',"
			"	BAT_SOLDE     DECIMAL(20,5)               COMMENT 'Signed balance of the account',"
			"	BAT_NOTES     VARCHAR(4096)               COMMENT 'Import notes',"
			"	BAT_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of import',"
			"	BAT_UPD_STAMP TIMESTAMP                   COMMENT 'Import timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT_LINES ("
			"	BAT_ID             BIGINT   NOT NULL      COMMENT 'Intern import identifier',"
			"	BAT_LINE_ID        BIGINT   NOT NULL UNIQUE COMMENT 'Intern imported line identifier',"
			"	BAT_LINE_DEFFECT   DATE                   COMMENT 'Effect date',"
			"	BAT_LINE_DOPE      DATE                   COMMENT 'Operation date',"
			"	BAT_LINE_REF       VARCHAR(80)            COMMENT 'Bank reference',"
			"	BAT_LINE_LABEL     VARCHAR(80)            COMMENT 'Line label',"
			"	BAT_LINE_CURRENCY  VARCHAR(3)             COMMENT 'Line currency',"
			"	BAT_LINE_AMOUNT    DECIMAL(20,5)          COMMENT 'Signed amount of the line',"
			"	BAT_LINE_ENTRY     BIGINT                 COMMENT 'Reciliated entry',"
			"	BAT_LINE_UPD_USER  VARCHAR(20)            COMMENT 'User responsible of the reconciliation',"
			"	BAT_LINE_UPD_STAMP TIMESTAMP              COMMENT 'Reconciliation timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_CLASSES ("
			"	CLA_NUMBER       INTEGER     NOT NULL UNIQUE   COMMENT 'Class number',"
			"	CLA_LABEL        VARCHAR(80) NOT NULL          COMMENT 'Class label',"
			"	CLA_NOTES        VARCHAR(4096)                 COMMENT 'Class notes',"
			"	CLA_UPD_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
			"	CLA_UPD_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (1,'Comptes de capitaux')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (2,'Comptes d\\'immobilisations')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (3,'Comptes de stocks et en-cours')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (4,'Comptes de tiers')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (5,'Comptes financiers')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (6,'Comptes de charges')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (7,'Comptes de produits')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (8,'Comptes spéciaux')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (9,'Comptes analytiques')", TRUE )){
		return( FALSE );
	}

	/*
	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ACC_CLOSINGS ("
			"	CPT_NUMBER       VARCHAR(20) BINARY NOT NULL   COMMENT 'Account number',"
			"	CPT_CLOSING      DATE                          COMMENT 'Closing date',"
			"	CPT_EXE_ID       INTEGER                       COMMENT 'Exercice identifier',"
			"	CPT_DEB_MNT      DECIMAL(20,5)                 COMMENT 'Validated entries debit balance',"
			"	CPT_CRE_MNT      DECIMAL(20,5)                 COMMENT 'Validated entries credit balance',"
			"	CONSTRAINT PRIMARY KEY (CPT_NUMBER,CPT_CLOSING)"
			")", TRUE )){
		return( FALSE );
	}
	*/

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_CURRENCIES ("
			"	CUR_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	CUR_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	CUR_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	CUR_DIGITS    INTEGER     DEFAULT 2                  COMMENT 'Decimal digits on display',"
			"	CUR_NOTES     VARCHAR(4096)                          COMMENT 'Currency notes',"
			"	CUR_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	CUR_UPD_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_CURRENCIES "
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL) VALUES ('EUR','Euro','€')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID              INTEGER   NOT NULL UNIQUE COMMENT 'Row identifier',"
			"	DOS_DEF_CURRENCY    VARCHAR(3)                COMMENT 'Default currency identifier',"
			"	DOS_EXE_BEGIN       DATE                      COMMENT 'Exercice beginning date',"
			"	DOS_EXE_END         DATE                      COMMENT 'Exercice ending date',"
			"	DOS_EXE_LENGTH      INTEGER                   COMMENT 'Exercice length in months',"
			"	DOS_EXE_NOTES       VARCHAR(4096)             COMMENT 'Exercice notes',"
			"	DOS_LABEL           VARCHAR(80)               COMMENT 'Raison sociale',"
			"	DOS_NOTES           VARCHAR(4096)             COMMENT 'Dossier notes',"
			"	DOS_SIREN           VARCHAR(20)               COMMENT 'Siren identifier',"
			"	DOS_UPD_USER        VARCHAR(20)               COMMENT 'User responsible of properties last update',"
			"	DOS_UPD_STAMP       TIMESTAMP                 COMMENT 'Properties last update timestamp',"
			"	DOS_EXE_ID          INTEGER                   COMMENT 'Exercice identifier',"
			"	DOS_LAST_EXE_ID     INTEGER                   COMMENT 'Last exercice identifier opened for the dossier',"
			"	DOS_LAST_BAT        BIGINT  DEFAULT 0         COMMENT 'Last BAT file number used',"
			"	DOS_LAST_BATLINE    BIGINT  DEFAULT 0         COMMENT 'Last BAT line number used',"
			"	DOS_LAST_ENTRY      BIGINT  DEFAULT 0         COMMENT 'Last entry number used',"
			"	DOS_LAST_SETTLEMENT BIGINT  DEFAULT 0         COMMENT 'Last settlement number used',"
			"	DOS_STATUS          CHAR(1)                   COMMENT 'Status of this exercice'"
			")", TRUE )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_DOSSIER "
			"	(DOS_ID,DOS_LABEL,DOS_EXE_LENGTH,DOS_DEF_CURRENCY,DOS_LAST_EXE_ID,DOS_EXE_ID,DOS_STATUS) "
			"	VALUES (1,'%s',%u,'EUR',1,1,'C')", name, DOS_DEFAULT_LENGTH );
	if( !ofo_sgbd_query( sgbd, query, TRUE )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_ENTRIES ("
			"	ENT_DEFFECT      DATE NOT NULL            COMMENT 'Imputation effect date',"
			"	ENT_NUMBER       BIGINT  NOT NULL UNIQUE  COMMENT 'Entry number',"
			"	ENT_DOPE         DATE NOT NULL            COMMENT 'Operation date',"
			"	ENT_LABEL        VARCHAR(80)              COMMENT 'Entry label',"
			"	ENT_REF          VARCHAR(20)              COMMENT 'Piece reference',"
			"	ENT_ACCOUNT      VARCHAR(20)              COMMENT 'Account number',"
			"	ENT_CURRENCY     VARCHAR(3)               COMMENT 'ISO 3A identifier of the currency',"
			"	ENT_DEBIT        DECIMAL(20,5) DEFAULT 0  COMMENT 'Debiting amount',"
			"	ENT_CREDIT       DECIMAL(20,5) DEFAULT 0  COMMENT 'Crediting amount',"
			"	ENT_LEDGER       VARCHAR(6)               COMMENT 'Mnemonic identifier of the ledger',"
			"	ENT_OPE_TEMPLATE VARCHAR(6)               COMMENT 'Mnemonic identifier of the operation template',"
			"	ENT_STATUS       INTEGER       DEFAULT 1  COMMENT 'Is the entry validated or deleted ?',"
			"	ENT_UPD_USER     VARCHAR(20)              COMMENT 'User responsible of last update',"
			"	ENT_UPD_STAMP    TIMESTAMP                COMMENT 'Last update timestamp',"
			"	ENT_CONCIL_DVAL  DATE                     COMMENT 'Reconciliation value date',"
			"	ENT_CONCIL_USER  VARCHAR(20)              COMMENT 'User responsible of the reconciliation',"
			"	ENT_CONCIL_STAMP TIMESTAMP                COMMENT 'Reconciliation timestamp',"
			"	ENT_STLMT_NUMBER BIGINT                   COMMENT 'Settlement number',"
			"	ENT_STLMT_USER   VARCHAR(20)              COMMENT 'User responsible of the settlement',"
			"	ENT_STLMT_STAMP  TIMESTAMP                COMMENT 'Settlement timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS ("
			"	LED_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the ledger',"
			"	LED_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Ledger label',"
			"	LED_NOTES     VARCHAR(4096)               COMMENT 'Ledger notes',"
			"	LED_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	LED_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	LED_LAST_CLO  DATE                        COMMENT 'Last closing date'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS_CUR ("
			"	LED_MNEMO        VARCHAR(6) NOT NULL      COMMENT 'Internal ledger identifier',"
			"	LED_CUR_CODE     VARCHAR(3) NOT NULL      COMMENT 'Internal currency identifier',"
			"	LED_CUR_CLO_DEB  DECIMAL(20,5)            COMMENT 'Debit balance at last closing',"
			"	LED_CUR_CLO_CRE  DECIMAL(20,5)            COMMENT 'Credit balance at last closing',"
			"	LED_CUR_DEB      DECIMAL(20,5)            COMMENT 'Current debit balance',"
			"	LED_CUR_CRE      DECIMAL(20,5)            COMMENT 'Current credit balance',"
			"	CONSTRAINT PRIMARY KEY (LED_MNEMO,LED_CUR_CODE)"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('ACH','Journal des achats','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('VEN','Journal des ventes','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('EXP','Journal de l\\'exploitant','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('OD','Journal des opérations diverses','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('BQ','Journal de banque','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES ("
			"	OTE_MNEMO      VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Operation template mnemonic',"
			"	OTE_LABEL      VARCHAR(80)       NOT NULL        COMMENT 'Template label',"
			"	OTE_LED_MNEMO  VARCHAR(6)                        COMMENT 'Generated entries imputation ledger',"
			"	OTE_LED_LOCKED INTEGER                           COMMENT 'Ledger is locked',"
			"	OTE_NOTES      VARCHAR(4096)                     COMMENT 'Template notes',"
			"	OTE_UPD_USER   VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	OTE_UPD_STAMP  TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES_DET ("
			"	OTE_MNEMO              VARCHAR(6) NOT NULL     COMMENT 'Operation template menmonic',"
			"	OTE_DET_ROW            INTEGER    NOT NULL     COMMENT 'Detail line number',"
			"	OTE_DET_COMMENT        VARCHAR(80)             COMMENT 'Detail line comment',"
			"	OTE_DET_ACCOUNT        VARCHAR(20)             COMMENT 'Account number',"
			"	OTE_DET_ACCOUNT_LOCKED INTEGER                 COMMENT 'Account number is locked',"
			"	OTE_DET_LABEL          VARCHAR(80)             COMMENT 'Entry label',"
			"	OTE_DET_LABEL_LOCKED   INTEGER                 COMMENT 'Entry label is locked',"
			"	OTE_DET_DEBIT          VARCHAR(80)             COMMENT 'Debit amount',"
			"	OTE_DET_DEBIT_LOCKED   INTEGER                 COMMENT 'Debit amount is locked',"
			"	OTE_DET_CREDIT         VARCHAR(80)             COMMENT 'Credit amount',"
			"	OTE_DET_CREDIT_LOCKED  INTEGER                 COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (OTE_MNEMO, OTE_DET_ROW)"
			")", TRUE )){
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
			"	REC_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	REC_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	REC_LAST      DATE                        COMMENT 'Effect date of the last generation'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_LABEL         VARCHAR(80)       NOT NULL        COMMENT 'Rate label',"
			"	RAT_NOTES         VARCHAR(4096)                     COMMENT 'Rate notes',"
			"	RAT_UPD_USER      VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	RAT_UPD_STAMP     TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			")", TRUE )){
		return( FALSE );
	}

	if( !ofo_sgbd_query( sgbd,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES_VAL ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL        COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_VAL_BEG       DATE    DEFAULT NULL              COMMENT 'Validity begin date',"
			"	RAT_VAL_END       DATE    DEFAULT NULL              COMMENT 'Validity end date',"
			"	RAT_VAL_RATE      DECIMAL(20,5)                     COMMENT 'Rate value',"
			"	ADD UNIQUE INDEX (RAT_MNEMO,RAT_VAL_BEG,RAT_VAL_END)"
			")", TRUE )){
		return( FALSE );
	}

	/* we do this only at the end of the model creation
	 * as a mark that all has been successfully done
	 */
	if( !ofo_sgbd_query( sgbd,
			"UPDATE OFA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=1", TRUE )){
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_dossier_get_name:
 *
 * Returns: the name (short label) of the dossier as it appeats in the
 * selection dialogs. The name is not stored in the DBMS, but at an
 * external level by the program.
 */
const gchar *
ofo_dossier_get_name( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->name );
	}

	g_return_val_if_reached( NULL );
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

		return(( const gchar * ) dossier->priv->userid );
	}

	g_return_val_if_reached( NULL );
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

		return(( const ofoSgbd * ) dossier->priv->sgbd );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_dbname:
 *
 * Returns: the name of the database of the opened dossier.
 */
gchar *
ofo_dossier_get_dbname( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;

		if( priv->sgbd && OFO_IS_SGBD( priv->sgbd )){

			return( ofo_sgbd_get_dbname( priv->sgbd ));
		}
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_use_currency:
 *
 * Returns: %TRUE if the dossier makes use of this currency, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	const gchar *default_dev;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		default_dev = ofo_dossier_get_default_currency( dossier );

		if( default_dev && g_utf8_strlen( default_dev, -1 )){
			return( g_utf8_collate( default_dev, currency ) == 0 );
		}
	}

	return( FALSE );
}

/**
 * ofo_dossier_get_default_currency:
 *
 * Returns: the default currency of the dossier.
 */
const gchar *
ofo_dossier_get_default_currency( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->currency );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_exe_begin:
 *
 * Returns: the date of the beginning of the exercice.
 */
const GDate *
ofo_dossier_get_exe_begin( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->priv->exe_begin );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_exe_end:
 *
 * Returns: the date of the end of the exercice.
 */
const GDate *
ofo_dossier_get_exe_end( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->priv->exe_end );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_exe_length:
 *
 * Returns: the length of the exercice, in months.
 */
gint
ofo_dossier_get_exe_length( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), -1 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->exe_length );
	}

	g_return_val_if_reached( -1 );
	return( -1 );
}

/**
 * ofo_dossier_get_exe_notes:
 *
 * Returns: the notes associated to the exercice.
 */
const gchar *
ofo_dossier_get_exe_notes( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->exe_notes );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_label:
 *
 * Returns: the label of the dossier. This is the 'raison sociale' for
 * the dossier.
 */
const gchar *
ofo_dossier_get_label( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->label );
	}

	g_return_val_if_reached( NULL );
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

		return(( const gchar * ) dossier->priv->notes );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_siren:
 *
 * Returns: the siren of the dossier.
 */
const gchar *
ofo_dossier_get_siren( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->siren );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_upd_user:
 *
 * Returns: the identifier of the user who has last updated the
 * properties of the dossier.
 */
const gchar *
ofo_dossier_get_upd_user( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->upd_user );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_stamp:
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const GTimeVal *
ofo_dossier_get_upd_stamp( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &dossier->priv->upd_stamp );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_status:
 */
const gchar *
ofo_dossier_get_status( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_status";
	ofoDossierPrivate *priv;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;

		if( !g_utf8_collate( priv->status, DOSSIER_CURRENT )){
			return( _( "Opened" ));
		}
		if( !g_utf8_collate( priv->status, DOSSIER_ARCHIVED )){
			return( _( "Archived" ));
		}
		g_warning( "%s: unknown status: %s", thisfn, priv->status );
	}

	return( NULL );
}

/**
 * ofo_dossier_get_this_exe_id:
 *
 * Returns: the exercice identifier for this exercice.
 */
gint
ofo_dossier_get_this_exe_id( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->exe_id );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_last_exe_id:
 *
 * Returns: the last exercice identifier used in the dossier.
 */
gint
ofo_dossier_get_last_exe_id( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_exe_id );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_last_bat:
 *
 * Returns: the last bat number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_bat( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_bat );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_last_batline:
 *
 * Returns: the last bat_line number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_batline( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_batline );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_last_entry:
 *
 * Returns: the last entry number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_entry( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_entry );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_last_settlement:
 *
 * Returns: the last settlement number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_settlement( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_settlement );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_next_bat:
 */
ofxCounter
ofo_dossier_get_next_bat( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_bat += 1;
		next = priv->last_bat;
		dossier_update_next( dossier, "DOS_LAST_BAT", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_next_batline:
 */
ofxCounter
ofo_dossier_get_next_batline( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_batline += 1;
		next = priv->last_batline;
		dossier_update_next( dossier, "DOS_LAST_BATLINE", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_next_entry:
 *
 * Returns: the next entry number to be allocated in the dossier.
 */
ofxCounter
ofo_dossier_get_next_entry( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_entry += 1;
		next = priv->last_entry;
		dossier_update_next( dossier, "DOS_LAST_ENTRY", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofo_dossier_get_next_settlement:
 */
ofxCounter
ofo_dossier_get_next_settlement( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_settlement += 1;
		next = priv->last_settlement;
		dossier_update_next( dossier, "DOS_LAST_SETTLEMENT", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/*
 * ofo_dossier_update_next_number:
 */
static void
dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number )
{
	gchar *query;

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER "
			"	SET %s=%ld "
			"	WHERE DOS_ID=%d",
					field, next_number, THIS_DOS_ID );

	ofo_sgbd_query( dossier->priv->sgbd, query, TRUE );
	g_free( query );
}

/**
 * ofo_dossier_get_exercices_list:
 *
 * Returns: the list of known exercices identifiers, sorted from most
 * recent to most old.
 * The returned list should be g_list_free() by the caller.
 * The exercice identifiers must be accessed via the
 * GPOINTER_TO_INT( it->data ) macro.
 */
GList *
ofo_dossier_get_exercices_list( const ofoDossier *dossier )
{
#if 0
	GList *list, *exe;
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		list = NULL;
		for( exe=dossier->priv->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;
			list = g_list_prepend( list, GINT_TO_POINTER( sexe->exe_id ));
		}
		return( g_list_reverse( list ));
	}
#endif
	return( NULL );
}

/**
 * ofo_dossier_get_last_closed_exercice:
 *
 * Returns: the last exercice closing date, as a newly allocated
 * #GDate structure which should be g_free() by the caller,
 * or %NULL if the dossier has never been closed.
 */
/*
GDate *
ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_last_closed_exercice";
	GList *exe;
	sDetailExe *sexe;
	GDate *dmax;
	gchar *str;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	dmax = NULL;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		for( exe=dossier->priv->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;
			if( sexe->status == DOS_STATUS_CLOSED ){
				g_return_val_if_fail( my_date_is_valid( &sexe->exe_end ), NULL );
				if( dmax ){
					g_return_val_if_fail( my_date_is_valid( dmax ), NULL );
					if( my_date_compare( &sexe->exe_end, dmax ) > 0 ){
						my_date_set_from_date( dmax, &sexe->exe_end );
					}
				} else {
					dmax = g_new0( GDate, 1 );
					my_date_set_from_date( dmax, &sexe->exe_end );
				}
			}
		}
	}

	str = my_date_to_str( dmax, MY_DATE_DMYY );
	g_debug( "%s: last_closed_exercice=%s", thisfn, str );
	g_free( str );

	return( dmax );
}
*/

/**
 * ofo_dossier_is_current:
 *
 * Returns: %TRUE if the exercice if the current one.
 */
gboolean
ofo_dossier_is_current( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( g_utf8_collate( dossier->priv->status, DOSSIER_CURRENT ) == 0 );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * ofo_dossier_is_entries_allowed:
 *
 * No new entry is allowed in the dossier while the exercice beginning
 * date is not set (and valid)
 */
gboolean
ofo_dossier_is_entries_allowed( const ofoDossier *dossier )
{
	const GDate *begin;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		begin = ofo_dossier_get_exe_begin( dossier );
		return( my_date_is_valid( begin ));
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * ofo_dossier_is_valid:
 */
gboolean
ofo_dossier_is_valid( const gchar *label, gint nb_months, const gchar *currency,
								const GDate *begin, const GDate *end )
{
	gboolean valid;

	valid = label && g_utf8_strlen( label, -1 );
	valid &= nb_months > 0;
	valid &= currency && g_utf8_strlen( currency, -1 );

	if( my_date_is_valid( begin ) && my_date_is_valid( end )){
		valid &= my_date_compare( begin, end ) < 0;
	}

	return( valid );
}

/**
 * ofo_dossier_set_default_currency:
 */
void
ofo_dossier_set_default_currency( ofoDossier *dossier, const gchar *currency )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->currency );
		dossier->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_dossier_set_exe_begin:
 */
void
ofo_dossier_set_exe_begin( const ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_date_set_from_date( &dossier->priv->exe_begin, date );
	}
}

/**
 * ofo_dossier_set_exe_end:
 */
void
ofo_dossier_set_exe_end( const ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_date_set_from_date( &dossier->priv->exe_end, date );
	}
}

/**
 * ofo_dossier_set_exe_length:
 */
void
ofo_dossier_set_exe_length( ofoDossier *dossier, gint nb_months )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( nb_months > 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->exe_length = nb_months;
	}
}

/**
 * ofo_dossier_set_exe_notes:
 * @dossier: this dossier
 * @notes : the notes to be set
 *
 * Attach the given @notes to the exercice.
 */
void
ofo_dossier_set_exe_notes( const ofoDossier *dossier, const gchar *notes )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		g_free( priv->exe_notes );
		priv->exe_notes = g_strdup( notes );
	}
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

		g_free( dossier->priv->label );
		dossier->priv->label = g_strdup( label );
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

		g_free( dossier->priv->notes );
		dossier->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_dossier_set_siren:
 */
void
ofo_dossier_set_siren( ofoDossier *dossier, const gchar *siren )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->siren );
		dossier->priv->siren = g_strdup( siren );
	}
}

/*
 * ofo_dossier_set_upd_user:
 */
static void
dossier_set_upd_user( ofoDossier *dossier, const gchar *user )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( user && g_utf8_strlen( user, -1 ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->upd_user );
		dossier->priv->upd_user = g_strdup( user );
	}
}

/*
 * ofo_dossier_set_upd_stamp:
 */
static void
dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( stamp );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &dossier->priv->upd_stamp, stamp );
	}
}

static void
dossier_set_this_exe_id( ofoDossier *dossier, gint exe_id )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->exe_id = exe_id;
	}
}

static void
dossier_set_last_exe_id( ofoDossier *dossier, gint exe_id )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_exe_id = exe_id;
	}
}

static void
dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_bat = counter;
	}
}

static void
dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_batline = counter;
	}
}

static void
dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_entry = counter;
	}
}

static void
dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_settlement = counter;
	}
}

static void
dossier_set_status( ofoDossier *dossier, const gchar *status )
{
	static const gchar *thisfn = "ofo_dossier_set_status";
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		g_free( priv->status );
		priv->status = NULL;

		if( !g_utf8_collate( status, DOSSIER_CURRENT ) ||
			!g_utf8_collate( status, DOSSIER_ARCHIVED )){

			priv->status = g_strdup( status );

		} else {
			g_warning( "%s: unknown status: %s", thisfn, status );
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
	return( dossier_read_properties( dossier ));
}

static gboolean
dossier_read_properties( ofoDossier *dossier )
{
	gchar *query;
	GSList *result, *icol;
	gboolean ok;
	const gchar *cstr;
	GTimeVal timeval;
	GDate date;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_DEF_CURRENCY,"
			"	DOS_EXE_BEGIN,DOS_EXE_END,DOS_EXE_LENGTH,DOS_EXE_NOTES,"
			"	DOS_LABEL,DOS_NOTES,DOS_SIREN,"
			"	DOS_UPD_USER,DOS_UPD_STAMP,"
			"	DOS_EXE_ID,DOS_LAST_EXE_ID,"
			"	DOS_LAST_BAT,DOS_LAST_BATLINE,DOS_LAST_ENTRY,DOS_LAST_SETTLEMENT,"
			"	DOS_STATUS "
			"FROM OFA_T_DOSSIER "
			"WHERE DOS_ID=%d", THIS_DOS_ID );

	result = ofo_sgbd_query_ex( dossier->priv->sgbd, query, TRUE );

	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_default_currency( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_exe_begin( dossier, my_date_set_from_sql( &date, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_exe_end( dossier, my_date_set_from_sql( &date, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_exe_length( dossier, atoi( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_exe_notes( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_label( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_notes( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			ofo_dossier_set_siren( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_upd_user( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_upd_stamp( dossier, my_utils_stamp_set_from_sql( &timeval, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_this_exe_id( dossier, atoi( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_last_exe_id( dossier, atoi( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_last_bat( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_last_batline( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_last_entry( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_last_settlement( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( cstr && g_utf8_strlen( cstr, -1 )){
			dossier_set_status( dossier, cstr );
		}

		ok = TRUE;
		ofo_sgbd_free_result( result );
	}

	return( ok );
}

/**
 * ofo_dossier_update:
 *
 * Update the properties of the dossier.
 */
gboolean
ofo_dossier_update( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

		return( dossier_do_update(
					dossier, dossier->priv->sgbd, dossier->priv->userid ));
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
dossier_do_update( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user )
{
	return( do_update_properties( dossier, sgbd, user ));
}

static gboolean
do_update_properties( ofoDossier *dossier, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp_str, *sdate;
	GTimeVal stamp;
	gboolean ok;
	const gchar *cstr;
	const GDate *date;

	ok = FALSE;
	query = g_string_new( "UPDATE OFA_T_DOSSIER SET " );

	cstr = ofo_dossier_get_default_currency( dossier );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_string_append_printf( query, "DOS_DEF_CURRENCY='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_DEF_CURRENCY=NULL," );
	}

	date = ofo_dossier_get_exe_begin( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_BEGIN='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_BEGIN=NULL," );
	}

	date = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_END='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_END=NULL," );
	}

	g_string_append_printf( query,
			"DOS_EXE_LENGTH=%d,", ofo_dossier_get_exe_length( dossier ));

	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	if( label && g_utf8_strlen( label, -1 )){
		g_string_append_printf( query, "DOS_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "DOS_LABEL=NULL," );
	}
	g_free( label );

	notes = my_utils_quote( ofo_dossier_get_notes( dossier ));
	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "DOS_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_siren( dossier );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_string_append_printf( query, "DOS_SIREN='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SIREN=NULL," );
	}

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query,
			"DOS_UPD_USER='%s',DOS_UPD_STAMP='%s',", user, stamp_str );
	g_free( stamp_str );

	g_string_append_printf( query,
			"DOS_EXE_ID=%d,DOS_LAST_EXE_ID=%d,"
			"DOS_LAST_BAT=%ld,DOS_LAST_BATLINE=%ld,DOS_LAST_ENTRY=%ld,DOS_LAST_SETTLEMENT=%ld ",
				ofo_dossier_get_this_exe_id( dossier ), ofo_dossier_get_last_exe_id( dossier ),
				ofo_dossier_get_last_bat( dossier ), ofo_dossier_get_last_batline( dossier ),
				ofo_dossier_get_last_entry( dossier ), ofo_dossier_get_last_settlement( dossier ));

	g_string_append_printf( query, "WHERE DOS_ID=%d", THIS_DOS_ID );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		dossier_set_upd_user( dossier, user );
		dossier_set_upd_stamp( dossier, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	return( ok );
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, const ofoDossier *dossier )
{
	GSList *lines;
	gchar *str, *stamp;
	const gchar *currency, *muser, *siren;
	gchar *sbegin, *send, *notes, *exenotes, *label;
	gboolean ok, with_headers;
	gulong count;

	with_headers = ofa_export_settings_get_headers( settings );

	count = ( gulong ) 1;
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "DefCurrency;ExeBegin;ExeEnd;ExeLength;ExeNotes;Label;Notes;Siren;MajUser;MajStamp;ThisExeId;LastExeId;LastBat;LastBatLine;LastEntry;LastSettlement;Status" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	sbegin = my_date_to_str( ofo_dossier_get_exe_begin( dossier ), MY_DATE_SQL );
	send = my_date_to_str( ofo_dossier_get_exe_end( dossier ), MY_DATE_SQL );
	exenotes = my_utils_export_multi_lines( ofo_dossier_get_exe_notes( dossier ));
	notes = my_utils_export_multi_lines( ofo_dossier_get_notes( dossier ));
	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	muser = ofo_dossier_get_upd_user( dossier );
	stamp = my_utils_stamp_to_str( ofo_dossier_get_upd_stamp( dossier ), MY_STAMP_YYMDHMS );
	currency = ofo_dossier_get_default_currency( dossier );
	siren = ofo_dossier_get_siren( dossier );

	str = g_strdup_printf( "%s;%s;%s;%d;%s;%s;%s;%s;%s;%s;%d;%d;%ld;%ld;%ld;%ld",
			currency ? currency : "",
			sbegin,
			send,
			ofo_dossier_get_exe_length( dossier ),
			exenotes ? exenotes : "",
			label ? label : "",
			notes ? notes : "",
			siren ? siren : "",
			muser ? muser : "",
			muser ? stamp : "",
			ofo_dossier_get_this_exe_id( dossier ),
			ofo_dossier_get_last_exe_id( dossier ),
			ofo_dossier_get_last_bat( dossier ),
			ofo_dossier_get_last_batline( dossier ),
			ofo_dossier_get_last_entry( dossier ),
			ofo_dossier_get_last_settlement( dossier ));

	g_free( sbegin );
	g_free( send );
	g_free( label );
	g_free( exenotes );
	g_free( notes );
	g_free( stamp );

	lines = g_slist_prepend( NULL, str );
	ok = ofa_iexportable_export_lines( exportable, lines );
	g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	if( !ok ){
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_dossier_backup:
 *
 * Backup the database behind the dossier
 */
gboolean
ofo_dossier_backup( const ofoDossier *dossier, const gchar *fname )
{
	ofoDossierPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( fname && g_utf8_strlen( fname, -1 ), FALSE );

	ok = FALSE;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;

		ok = ofo_sgbd_backup( priv->sgbd, fname );
	}

	return( ok );
}
