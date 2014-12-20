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
#include "api/ofa-dbms.h"
#include "api/ofa-idataset.h"
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

#include "core/ofo-marshal.h"

/* priv instance data
 */
struct _ofoDossierPrivate {

	/* internals
	 */
	gchar      *dname;					/* the name of the dossier in settings */
	ofaDbms    *dbms;
	gchar      *userid;

	/* row id 1
	 */
	gchar      *currency;
	GDate       exe_begin;
	GDate       exe_end;
	gint        exe_length;				/* exercice length (in month) */
	gchar      *exe_notes;
	gchar      *forward_ope;
	gchar      *label;					/* raison sociale */
	gchar      *notes;					/* notes */
	gchar      *siren;
	gchar      *sld_ope;
	gchar      *upd_user;
	GTimeVal    upd_stamp;
	ofxCounter  last_bat;
	ofxCounter  last_batline;
	ofxCounter  last_entry;
	ofxCounter  last_settlement;
	gchar      *status;

	GList      *cur_details;			/* a list of details per currency */
	GList      *datasets;				/* a list of datasets managed by ofaIDataset */
};

typedef struct {
	gchar *currency;
	gchar *sld_account;
}
	sCurrency;

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	VALIDATED_ENTRY,
	EXE_DATE_CHANGED,
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

static ofoBaseClass *ofo_dossier_parent_class = NULL;

static GType       register_type( void );
static void        dossier_instance_init( ofoDossier *self );
static void        dossier_class_init( ofoDossierClass *klass );
static void        connect_objects_handlers( const ofoDossier *dossier );
static void        on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void        on_exe_dates_changed( const ofoDossier *dossier, void *empty );
static gboolean    dbmodel_update( const ofoDossier *dossier );
static gint        dbmodel_get_version( const ofoDossier *dossier );
static gboolean    dbmodel_to_v1( const ofoDossier *dossier );
static gint        dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static gint        dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static void        dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number );
static sCurrency  *get_currency_detail( ofoDossier *dossier, const gchar *currency, gboolean create );
static gint        cmp_currency_detail( sCurrency *a, sCurrency *b );
static void        dossier_set_upd_user( ofoDossier *dossier, const gchar *user );
static void        dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp );
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
static gboolean    dossier_do_update( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user );
static gboolean    do_update_properties( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user );
static gboolean    dossier_do_update_currencies( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user );
static gboolean    do_update_currency_properties( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static void        idataset_iface_init( ofaIDatasetInterface *iface );
static guint       idataset_get_interface_version( const ofaIDataset *instance );
static GList      *idataset_get_datasets( const ofaIDataset *instance );
static void        idataset_set_datasets( ofaIDataset *instance, GList *list );
static void        free_datasets( GList *datasets );
static void        free_cur_detail( sCurrency *details );
static void        free_cur_details( GList *details );

GType
ofo_dossier_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
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
		( GClassInitFunc ) dossier_class_init,
		NULL,
		NULL,
		sizeof( ofoDossier ),
		0,
		( GInstanceInitFunc ) dossier_instance_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo idataset_iface_info = {
		( GInterfaceInitFunc ) idataset_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoDossier", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IDATASET, &idataset_iface_info );

	return( type );
}

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

	g_free( priv->dname );
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

		if( priv->cur_details ){
			free_cur_details( priv->cur_details );
		}
		if( priv->datasets ){
			free_datasets( priv->datasets );
		}
		if( priv->dbms ){
			g_clear_object( &priv->dbms );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->dispose( instance );
}

static void
dossier_instance_init( ofoDossier *self )
{
	static const gchar *thisfn = "ofo_dossier_instance_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_DOSSIER, ofoDossierPrivate );
}

static void
dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_dossier_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_finalize;

	g_type_class_add_private( klass, sizeof( ofoDossierPrivate ));

	/**
	 * ofoDossier::ofa-signal-new-object:
	 *
	 * The signal is emitted after a new object has been successfully
	 * inserted in the DBMS. A connected handler may take advantage of
	 * this signal e.g. to update its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoBase      *object,
	 * 								gpointer      user_data );
	 */
	st_signals[ NEW_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_DOSSIER_NEW_OBJECT,
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
	 * updated in the DBMS. A connected handler may take advantage of
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
				SIGNAL_DOSSIER_UPDATED_OBJECT,
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
	 * deleted from the DBMS. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier *dossier,
	 * 								ofoBase      *object,
	 * 								gpointer      user_data );
	 */
	st_signals[ DELETED_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_DOSSIER_DELETED_OBJECT,
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
	 * DBMS that it is considered easier for a connected handler just
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
				SIGNAL_DOSSIER_RELOAD_DATASET,
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
				SIGNAL_DOSSIER_VALIDATED_ENTRY,
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
	 * ofoDossier::ofa-signal-exe-date-changed:
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofoDossier  *dossier,
	 * 								gpointer user_data );
	 */
	st_signals[ EXE_DATE_CHANGED ] = g_signal_new_class_handler(
				SIGNAL_DOSSIER_EXE_DATE_CHANGED,
				OFO_TYPE_DOSSIER,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofo_dossier_new:
 */
ofoDossier *
ofo_dossier_new( void )
{
	ofoDossier *dossier;

	dossier = g_object_new( OFO_TYPE_DOSSIER, NULL );

	return( dossier );
}

#if 0
static gboolean
check_user_exists( ofaDbms *dbms, const gchar *account )
{
	gchar *query;
	GSList *res;
	gboolean exists;

	exists = FALSE;
	query = g_strdup_printf( "SELECT ROL_USER FROM OFA_T_ROLES WHERE ROL_USER='%s'", account );
	res = ofa_dbms_query_ex( dbms, query );
	if( res ){
		gchar *s = ( gchar * )(( GSList * ) res->data )->data;
		if( !g_utf8_collate( account, s )){
			exists = TRUE;
		}
		ofa_dbms_free_results( res );
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
			account, dossier->priv->dname );

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
 * @dossier: this #ofoDossier object
 * @dname: the name of the dossier, as read from settings
 * @dbname: [allow-none]: the exercice database to be selected
 * @account:
 * @password:
 */
gboolean
ofo_dossier_open( ofoDossier *dossier,
						const gchar *dname, const gchar *dbname,
						const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofo_dossier_open";
	ofoDossierPrivate *priv;
	ofaDbms *dbms;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( dname && g_utf8_strlen( dname, -1 ), FALSE );
	g_return_val_if_fail( account && g_utf8_strlen( account, -1 ), FALSE );
	g_return_val_if_fail( password && g_utf8_strlen( password, -1 ), FALSE );

	g_debug( "%s: dossier=%p, dname=%s, dbname=%s, account=%s, password=%s",
			thisfn,
			( void * ) dossier, dname, dbname, account, password );

	priv = dossier->priv;
	ok = FALSE;

	dbms = ofa_dbms_new();;
	if( !ofa_dbms_connect( dbms, dname, dbname, account, password, TRUE )){
		g_object_unref( dbms );
		return( FALSE );
	}

	priv->dname = g_strdup( dname );
	priv->dbms = dbms;
	priv->userid = g_strdup( account );

	if( dbmodel_update( dossier )){
		connect_objects_handlers( dossier );
		ok = dossier_do_read( dossier );
	}

	return( ok );
}

/*
 * be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself
 *
 * example of a use case: the intermediate closing by ledger may be run
 * without having first loaded the accounts, but the accounts should be
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
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_EXE_DATE_CHANGED, G_CALLBACK( on_exe_dates_changed ), NULL );
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

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );

	g_free( query );
}

static void
on_exe_dates_changed( const ofoDossier *dossier, void *empty )
{
	ofa_dbms_set_current_exercice(
			( ofaDbms * ) ofo_dossier_get_dbms( dossier ),
			ofo_dossier_get_name( dossier ),
			ofo_dossier_get_exe_begin( dossier ),
			ofo_dossier_get_exe_end( dossier ));
}

/**
 * ofo_dossier_dbmodel_update:
 * @dossier: this #ofoDossier instance, with an already opened
 *  connection
 *
 * Update the DB model in the DBMS
 */
static gboolean
dbmodel_update( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_update";
	gint cur_version;

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	cur_version = dbmodel_get_version( dossier );
	g_debug( "%s: cur_version=%d, THIS_DBMODEL_VERSION=%d", thisfn, cur_version, THIS_DBMODEL_VERSION );

	if( cur_version < THIS_DBMODEL_VERSION ){
		if( cur_version < 1 ){
			dbmodel_to_v1( dossier );
		}
	}

	return( TRUE );
}

/*
 * returns the last complete version
 * i.e. une version where the version date is set
 */
static gint
dbmodel_get_version( const ofoDossier *dossier )
{
	gint vmax;

	ofa_dbms_query_int(
			dossier->priv->dbms,
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0", &vmax, FALSE );

	return( vmax );
}

/**
 * ofo_dossier_dbmodel_to_v1:
 * @dbms: an already opened #ofaDbms connection
 * @dname: the name of the dossier from settings, will be used as
 *  default label
 * @account: the current connected account.
 */
static gboolean
dbmodel_to_v1( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_to_v1";
	ofoDossierPrivate *priv;
	gchar *query;

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	priv = dossier->priv;

	/* default value for timestamp cannot be null */
	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER NOT NULL UNIQUE DEFAULT 0     COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP DEFAULT 0                   COMMENT 'Version application timestamp') "
			"CHARACTER SET utf8",
			TRUE)){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (1, 0)",
			TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
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
			"	ACC_VAL_DEBIT       DECIMAL(20,5)                COMMENT 'Debit balance of validated entries',"
			"	ACC_VAL_CREDIT      DECIMAL(20,5)                COMMENT 'Credit balance of validated entries',"
			"	ACC_ROUGH_DEBIT     DECIMAL(20,5)                COMMENT 'Debit balance of rough entries',"
			"	ACC_ROUGH_CREDIT    DECIMAL(20,5)                COMMENT 'Credit balance of rough entries',"
			"	ACC_OPEN_DEBIT      DECIMAL(20,5)                COMMENT 'Debit balance at the exercice opening',"
			"	ACC_OPEN_CREDIT     DECIMAL(20,5)                COMMENT 'Credit balance at the exercice opening',"
			"	ACC_FUT_DEBIT       DECIMAL(20,5)                COMMENT 'Debit balance of future entries',"
			"	ACC_FUT_CREDIT      DECIMAL(20,5)                COMMENT 'Credit balance of future entries'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofa_dbms_query( priv->dbms,
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
	if( !ofa_dbms_query( priv->dbms,
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

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT ("
			"	BAT_ID        BIGINT      NOT NULL UNIQUE COMMENT 'Intern import identifier',"
			"	BAT_URI       VARCHAR(256)                COMMENT 'Imported URI',"
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
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
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
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_CLASSES ("
			"	CLA_NUMBER       INTEGER     NOT NULL UNIQUE   COMMENT 'Class number',"
			"	CLA_LABEL        VARCHAR(80) NOT NULL          COMMENT 'Class label',"
			"	CLA_NOTES        VARCHAR(4096)                 COMMENT 'Class notes',"
			"	CLA_UPD_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
			"	CLA_UPD_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (1,'Comptes de capitaux')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (2,'Comptes d\\'immobilisations')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (3,'Comptes de stocks et en-cours')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (4,'Comptes de tiers')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (5,'Comptes financiers')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (6,'Comptes de charges')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (7,'Comptes de produits')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (8,'Comptes spéciaux')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (9,'Comptes analytiques')", TRUE )){
		return( FALSE );
	}

	/*
	if( !ofa_dbms_query( priv->dbms,
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

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_CURRENCIES ("
			"	CUR_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	CUR_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	CUR_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	CUR_DIGITS    INTEGER     DEFAULT 2                  COMMENT 'Decimal digits on display',"
			"	CUR_NOTES     VARCHAR(4096)                          COMMENT 'Currency notes',"
			"	CUR_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	CUR_UPD_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_CURRENCIES "
			"	(CUR_CODE,CUR_LABEL,CUR_SYMBOL,CUR_DIGITS) VALUES ('EUR','Euro','€',2)", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID               INTEGER   NOT NULL UNIQUE COMMENT 'Row identifier',"
			"	DOS_DEF_CURRENCY     VARCHAR(3)                COMMENT 'Default currency identifier',"
			"	DOS_EXE_BEGIN        DATE                      COMMENT 'Exercice beginning date',"
			"	DOS_EXE_END          DATE                      COMMENT 'Exercice ending date',"
			"	DOS_EXE_LENGTH       INTEGER                   COMMENT 'Exercice length in months',"
			"	DOS_EXE_NOTES        VARCHAR(4096)             COMMENT 'Exercice notes',"
			"	DOS_FORW_OPE         VARCHAR(6)                COMMENT 'Operation mnemo for carried forward entries',"
			"	DOS_LABEL            VARCHAR(80)               COMMENT 'Raison sociale',"
			"	DOS_NOTES            VARCHAR(4096)             COMMENT 'Dossier notes',"
			"	DOS_SIREN            VARCHAR(9)                COMMENT 'Siren identifier',"
			"	DOS_SLD_OPE          VARCHAR(6)                COMMENT 'Operation mnemo for balancing entries',"
			"	DOS_UPD_USER         VARCHAR(20)               COMMENT 'User responsible of properties last update',"
			"	DOS_UPD_STAMP        TIMESTAMP                 COMMENT 'Properties last update timestamp',"
			"	DOS_LAST_BAT         BIGINT  DEFAULT 0         COMMENT 'Last BAT file number used',"
			"	DOS_LAST_BATLINE     BIGINT  DEFAULT 0         COMMENT 'Last BAT line number used',"
			"	DOS_LAST_ENTRY       BIGINT  DEFAULT 0         COMMENT 'Last entry number used',"
			"	DOS_LAST_SETTLEMENT  BIGINT  DEFAULT 0         COMMENT 'Last settlement number used',"
			"	DOS_STATUS           CHAR(1)                   COMMENT 'Status of this exercice'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_DOSSIER "
			"	(DOS_ID,DOS_LABEL,DOS_EXE_LENGTH,DOS_DEF_CURRENCY,DOS_STATUS) "
			"	VALUES (1,'%s',%u,'EUR','C')", priv->dname, DOS_DEFAULT_LENGTH );
	if( !ofa_dbms_query( priv->dbms, query, TRUE )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER_CUR ("
			"	DOS_ID               INTEGER   NOT NULL        COMMENT 'Row identifier',"
			"	DOS_CURRENCY         VARCHAR(3)                COMMENT 'Currency identifier',"
			"	DOS_SLD_ACCOUNT      VARCHAR(20)               COMMENT 'Balancing account when closing the exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_CURRENCY)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
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
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS ("
			"	LED_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the ledger',"
			"	LED_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Ledger label',"
			"	LED_NOTES     VARCHAR(4096)               COMMENT 'Ledger notes',"
			"	LED_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	LED_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	LED_LAST_CLO  DATE                        COMMENT 'Last closing date'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS_CUR ("
			"	LED_MNEMO            VARCHAR(6) NOT NULL  COMMENT 'Internal ledger identifier',"
			"	LED_CUR_CODE         VARCHAR(3) NOT NULL  COMMENT 'Internal currency identifier',"
			"	LED_CUR_VAL_DEBIT    DECIMAL(20,5)        COMMENT 'Validated debit total for this exercice on this journal',"
			"	LED_CUR_VAL_CREDIT   DECIMAL(20,5)        COMMENT 'Validated credit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_DEBIT  DECIMAL(20,5)        COMMENT 'Rough debit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_CREDIT DECIMAL(20,5)        COMMENT 'Rough credit total for this exercice on this journal',"
			"	LED_CUR_FUT_DEBIT    DECIMAL(20,5)        COMMENT 'Futur debit total on this journal',"
			"	LED_CUR_FUT_CREDIT   DECIMAL(20,5)        COMMENT 'Futur credit total on this journal',"
			"	CONSTRAINT PRIMARY KEY (LED_MNEMO,LED_CUR_CODE)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('ACH','Journal des achats','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('VEN','Journal des ventes','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('EXP','Journal de l\\'exploitant','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('OD','Journal des opérations diverses','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('BQ','Journal de banque','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES ("
			"	OTE_MNEMO      VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Operation template mnemonic',"
			"	OTE_LABEL      VARCHAR(80)       NOT NULL        COMMENT 'Template label',"
			"	OTE_LED_MNEMO  VARCHAR(6)                        COMMENT 'Generated entries imputation ledger',"
			"	OTE_LED_LOCKED INTEGER                           COMMENT 'Ledger is locked',"
			"	OTE_NOTES      VARCHAR(4096)                     COMMENT 'Template notes',"
			"	OTE_UPD_USER   VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	OTE_UPD_STAMP  TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES_DET ("
			"	OTE_MNEMO              VARCHAR(6) NOT NULL     COMMENT 'Operation template menmonic',"
			"	OTE_DET_ROW            INTEGER    NOT NULL     COMMENT 'Detail line number',"
			"	OTE_DET_COMMENT        VARCHAR(80)             COMMENT 'Detail line comment',"
			"	OTE_DET_REF            VARCHAR(20)             COMMENT 'Line reference',"
			"	OTE_DET_REF_LOCKED     INTEGER                 COMMENT 'Line reference is locked',"
			"	OTE_DET_ACCOUNT        VARCHAR(20)             COMMENT 'Account number',"
			"	OTE_DET_ACCOUNT_LOCKED INTEGER                 COMMENT 'Account number is locked',"
			"	OTE_DET_LABEL          VARCHAR(80)             COMMENT 'Entry label',"
			"	OTE_DET_LABEL_LOCKED   INTEGER                 COMMENT 'Entry label is locked',"
			"	OTE_DET_DEBIT          VARCHAR(80)             COMMENT 'Debit amount',"
			"	OTE_DET_DEBIT_LOCKED   INTEGER                 COMMENT 'Debit amount is locked',"
			"	OTE_DET_CREDIT         VARCHAR(80)             COMMENT 'Credit amount',"
			"	OTE_DET_CREDIT_LOCKED  INTEGER                 COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (OTE_MNEMO, OTE_DET_ROW)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofa_dbms_query( priv->dbms,
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

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_LABEL         VARCHAR(80)       NOT NULL        COMMENT 'Rate label',"
			"	RAT_NOTES         VARCHAR(4096)                     COMMENT 'Rate notes',"
			"	RAT_UPD_USER      VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	RAT_UPD_STAMP     TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( priv->dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES_VAL ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL        COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_VAL_BEG       DATE    DEFAULT NULL              COMMENT 'Validity begin date',"
			"	RAT_VAL_END       DATE    DEFAULT NULL              COMMENT 'Validity end date',"
			"	RAT_VAL_RATE      DECIMAL(20,5)                     COMMENT 'Rate value',"
			"	CONSTRAINT PRIMARY KEY (RAT_MNEMO,RAT_VAL_BEG,RAT_VAL_END)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* we do this only at the end of the model creation
	 * as a mark that all has been successfully done
	 */
	if( !ofa_dbms_query( priv->dbms,
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

		return(( const gchar * ) dossier->priv->dname );
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
 * ofo_dossier_get_dbms:
 *
 * Returns: the current DBMS handler.
 */
const ofaDbms *
ofo_dossier_get_dbms( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const ofaDbms * ) dossier->priv->dbms );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_use_account:
 *
 * Returns: %TRUE if the dossier makes use of this account, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_account( const ofoDossier *dossier, const gchar *account )
{
	gint count;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		count = dossier_cur_count_uses( dossier, "DOS_SLD_ACCOUNT", account );
		return( count > 0 );
	}

	return( FALSE );
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
	gint count;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		default_dev = ofo_dossier_get_default_currency( dossier );

		if( default_dev &&
				g_utf8_strlen( default_dev, -1 ) &&
				!g_utf8_collate( default_dev, currency )){
			return( TRUE );
		}

		count = dossier_cur_count_uses( dossier, "DOS_CURRENCY", currency );
		return( count > 0 );
	}

	return( FALSE );
}

/**
 * ofo_dossier_use_ope_template:
 *
 * Returns: %TRUE if the dossier makes use of this operation template,
 * thus preventing its deletion.
 */
gboolean
ofo_dossier_use_ope_template( const ofoDossier *dossier, const gchar *ope_template )
{
	gint forward, solde;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		forward = dossier_count_uses( dossier, "DOS_FORW_OPE", ope_template );
		solde = dossier_count_uses( dossier, "DOS_SLD_OPE", ope_template );
		return( forward+solde > 0 );
	}

	return( FALSE );
}

static gint
dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	gchar *query;
	gint count;

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, THIS_DOS_ID );

	ofa_dbms_query_int( ofo_dossier_get_dbms( dossier ), query, &count, TRUE );

	g_free( query );

	return( count );
}

static gint
dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	gchar *query;
	gint count;

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER_CUR WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, THIS_DOS_ID );

	ofa_dbms_query_int( ofo_dossier_get_dbms( dossier ), query, &count, TRUE );

	g_free( query );

	return( count );
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
 * ofo_dossier_get_forward_ope:
 *
 * Returns: the forward ope of the dossier.
 */
const gchar *
ofo_dossier_get_forward_ope( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->forward_ope );
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
 * ofo_dossier_get_sld_ope:
 *
 * Returns: the sld ope of the dossier.
 */
const gchar *
ofo_dossier_get_sld_ope( const ofoDossier *dossier )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->sld_ope );
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

	ofa_dbms_query( dossier->priv->dbms, query, TRUE );
	g_free( query );
}

/**
 * ofo_dossier_get_min_deffect:
 * @date: [in-out]: the #GDate date to be set.
 * @dossier: this dossier.
 * @ledger: [allow-none]: the imputed ledger.
 *
 * Computes the minimal effect date valid for the considered dossier and
 * ledger.
 *
 * Returns: the @date #GDate pointer.
 */
GDate *
ofo_dossier_get_min_deffect( GDate *date, const ofoDossier *dossier, ofoLedger *ledger )
{
	const GDate *last_clo;
	gint to_add;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	if( ledger ){
		if( !OFO_IS_LEDGER( ledger )){
			g_return_val_if_reached( NULL );
			return( NULL );
		}
	}

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		last_clo = ledger ? ofo_ledger_get_last_close( ledger ) : NULL;
		my_date_set_from_date( date, ofo_dossier_get_exe_begin( dossier ));
		to_add = 0;

		if( my_date_is_valid( date )){
			if( my_date_is_valid( last_clo )){
				if( my_date_compare( date, last_clo ) < 0 ){
					my_date_set_from_date( date, last_clo );
					to_add = 1;
				}
			}
		} else if( my_date_is_valid( last_clo )){
			my_date_set_from_date( date, last_clo );
			to_add = 1;
		}

		if( my_date_is_valid( date )){
			g_date_add_days( date, to_add );
		}

		return( date );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_currencies:
 * @dossier: this dossier.
 *
 * Returns: an alphabetically sorted #GSList of the currencies defined
 * in the subtable.
 *
 * The returned list should be #ofo_dossier_free_currencies() by the
 * caller.
 */
GSList *
ofo_dossier_get_currencies( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	sCurrency *sdet;
	GSList *list;
	GList *it;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		list = NULL;

		for( it=priv->cur_details ; it ; it=it->next ){
			sdet = ( sCurrency * ) it->data;
			list = g_slist_insert_sorted( list, g_strdup( sdet->currency ), ( GCompareFunc ) g_utf8_collate );
		}

		return( list );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofo_dossier_get_sld_account:
 * @dossier: this dossier.
 * @currency: the serched currency.
 *
 * Returns: the account configured as balancing account for this
 * currency.
 *
 * The returned string is owned by the @dossier, and must not be freed
 * by the caller.
 */
const gchar *
ofo_dossier_get_sld_account( ofoDossier *dossier, const gchar *currency )
{
	sCurrency *sdet;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sdet = get_currency_detail( dossier, currency, FALSE );
		if( sdet ){
			return(( const gchar * ) sdet->sld_account );
		}
	}

	return( NULL );
}

static sCurrency *
get_currency_detail( ofoDossier *dossier, const gchar *currency, gboolean create )
{
	ofoDossierPrivate *priv;
	GList *it;
	sCurrency *sdet;

	priv = dossier->priv;
	sdet = NULL;

	for( it=priv->cur_details ; it ; it=it->next ){
		sdet = ( sCurrency * ) it->data;
		if( !g_utf8_collate( sdet->currency, currency )){
			return( sdet );
		}
	}

	if( create ){
		sdet = g_new0( sCurrency, 1 );
		sdet->currency = g_strdup( currency );
		priv->cur_details = g_list_insert_sorted( priv->cur_details, sdet, ( GCompareFunc ) cmp_currency_detail );
	}

	return( sdet );
}

static gint
cmp_currency_detail( sCurrency *a, sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
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
 * ofo_dossier_is_valid:
 */
gboolean
ofo_dossier_is_valid( const gchar *label, gint nb_months, const gchar *currency,
								const GDate *begin, const GDate *end, gchar **msg )
{
	if( !label || !g_utf8_strlen( label, -1 )){
		*msg = g_strdup( _( "Empty label" ));
		return( FALSE );
	}

	if( nb_months <= 0 ){
		*msg = g_strdup_printf( "Invalid length of exercice: %d", nb_months );
		return( FALSE );
	}

	if( !currency || !g_utf8_strlen( currency, -1 )){
		*msg = g_strdup( _( "Empty default currency"));
		return( FALSE );
	}

	if( my_date_is_valid( begin ) && my_date_is_valid( end )){
		if( my_date_compare( begin, end ) > 0 ){
			*msg = g_strdup( _( "Exercice is set to begin after it has ended" ));
			return( FALSE );
		}
	}

	return( TRUE );
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
ofo_dossier_set_exe_begin( ofoDossier *dossier, const GDate *date )
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
ofo_dossier_set_exe_end( ofoDossier *dossier, const GDate *date )
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
ofo_dossier_set_exe_notes( ofoDossier *dossier, const gchar *notes )
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
 * ofo_dossier_set_forward_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_forward_ope( ofoDossier *dossier, const gchar *ope )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->forward_ope );
		dossier->priv->forward_ope = g_strdup( ope );
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

/**
 * ofo_dossier_set_sld_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_sld_ope( ofoDossier *dossier, const gchar *ope )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->sld_ope );
		dossier->priv->sld_ope = g_strdup( ope );
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

/**
 * ofo_dossier_reset_currencies:
 * @dossier: this dossier.
 *
 * Reset the currencies (free all).
 *
 * This function should be called when updating the currencies
 * properties, as we are only able to set new elements in the list.
 */
void
ofo_dossier_reset_currencies( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		free_cur_details( priv->cur_details );
		priv->cur_details = NULL;
	}
}

/**
 * ofo_dossier_set_sld_account:
 * @dossier: this dossier.
 * @currency: the currency.
 * @account: the account.
 *
 * Set the balancing account for the currency.
 */
void
ofo_dossier_set_sld_account( ofoDossier *dossier, const gchar *currency, const gchar *account )
{
	sCurrency *sdet;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( currency && g_utf8_strlen( currency, -1 ));
	g_return_if_fail( account && g_utf8_strlen( account, -1 ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sdet = get_currency_detail( dossier, currency, TRUE );
		sdet->sld_account = g_strdup( account );
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
	GSList *result, *irow, *icol;
	gboolean ok;
	const gchar *cstr, *currency;
	GTimeVal timeval;
	GDate date;
	sCurrency *sdet;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_DEF_CURRENCY,"
			"	DOS_EXE_BEGIN,DOS_EXE_END,DOS_EXE_LENGTH,DOS_EXE_NOTES,"
			"	DOS_FORW_OPE,"
			"	DOS_LABEL,DOS_NOTES,DOS_SIREN,"
			"	DOS_SLD_OPE,"
			"	DOS_UPD_USER,DOS_UPD_STAMP,"
			"	DOS_LAST_BAT,DOS_LAST_BATLINE,DOS_LAST_ENTRY,DOS_LAST_SETTLEMENT,"
			"	DOS_STATUS "
			"FROM OFA_T_DOSSIER "
			"WHERE DOS_ID=%d", THIS_DOS_ID );

	if( ofa_dbms_query_ex( dossier->priv->dbms, query, &result, TRUE )){
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
			ofo_dossier_set_forward_ope( dossier, cstr );
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
			ofo_dossier_set_sld_ope( dossier, cstr );
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
		ofa_dbms_free_results( result );
	}

	g_free( query );

	query = g_strdup_printf(
			"SELECT DOS_CURRENCY,DOS_SLD_ACCOUNT "
			"	FROM OFA_T_DOSSIER_CUR "
			"	WHERE DOS_ID=%d ORDER BY DOS_CURRENCY ASC", THIS_DOS_ID );

	if( ofa_dbms_query_ex( dossier->priv->dbms, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			currency = icol->data;
			if( currency && g_utf8_strlen( currency, -1 )){
				icol = icol->next;
				cstr = icol->data;
				if( cstr && g_utf8_strlen( cstr, -1 )){
					sdet = get_currency_detail( dossier, currency, TRUE );
					sdet->sld_account = g_strdup( cstr );
				}
			}
		}
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
					dossier, dossier->priv->dbms, dossier->priv->userid ));
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
dossier_do_update( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user )
{
	return( do_update_properties( dossier, dbms, user ));
}

static gboolean
do_update_properties( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user )
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

	notes = my_utils_quote( ofo_dossier_get_exe_notes( dossier ));
	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "DOS_EXE_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_EXE_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_forward_ope( dossier );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_string_append_printf( query, "DOS_FORW_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_FORW_OPE=NULL," );
	}

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

	cstr = ofo_dossier_get_sld_ope( dossier );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_string_append_printf( query, "DOS_SLD_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SLD_OPE=NULL," );
	}

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query,
			"DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' ", user, stamp_str );
	g_free( stamp_str );

	g_string_append_printf( query, "WHERE DOS_ID=%d", THIS_DOS_ID );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		dossier_set_upd_user( dossier, user );
		dossier_set_upd_stamp( dossier, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	return( ok );
}

/**
 * ofo_dossier_update_currencies:
 *
 * Update the currency properties of the dossier.
 */
gboolean
ofo_dossier_update_currencies( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update_currencies";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

		return( dossier_do_update_currencies(
					dossier, dossier->priv->dbms, dossier->priv->userid ));
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
dossier_do_update_currencies( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user )
{
	return( do_update_currency_properties( dossier, dbms, user ));
}

static gboolean
do_update_currency_properties( ofoDossier *dossier, const ofaDbms *dbms, const gchar *user )
{
	ofoDossierPrivate *priv;
	gchar *query;
	sCurrency *sdet;
	gboolean ok;
	GList *it;
	GTimeVal stamp;
	gchar *stamp_str;

	ok = ofa_dbms_query( dbms, "DELETE FROM OFA_T_DOSSIER_CUR", TRUE );

	if( ok ){
		priv = dossier->priv;

		for( it=priv->cur_details ; it && ok ; it=it->next ){
			sdet = ( sCurrency * ) it->data;
			query = g_strdup_printf(
					"INSERT INTO OFA_T_DOSSIER_CUR (DOS_ID,DOS_CURRENCY,DOS_SLD_ACCOUNT) VALUES "
					"	(%d,'%s','%s')", THIS_DOS_ID, sdet->currency, sdet->sld_account );
			ok &= ofa_dbms_query( dbms, query, TRUE );
			g_free( query );
		}

		if( ok ){
			my_utils_stamp_set_now( &stamp );
			stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
			query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER SET "
					"	DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' "
					"	WHERE DOS_ID=%d", user, stamp_str, THIS_DOS_ID );
			g_free( stamp_str );

			if( !ofa_dbms_query( dbms, query, TRUE )){
				ok = FALSE;
			} else {
				dossier_set_upd_user( dossier, user );
				dossier_set_upd_stamp( dossier, &stamp );
			}
		}
	}

	return( ok );
}

/*
 * ofaIExportable interface management
 */
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

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GSList *lines;
	gchar *str, *stamp;
	const gchar *currency, *muser, *siren;
	const gchar *fope;
	const gchar *bope;
	gchar *sbegin, *send, *notes, *exenotes, *label;
	gboolean ok, with_headers;
	gulong count;

	with_headers = ofa_file_format_has_headers( settings );

	count = ( gulong ) 1;
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "DefCurrency;ExeBegin;ExeEnd;ExeLength;ExeNotes;"
				"ForwardOpe;"
				"Label;Notes;Siren;"
				"SldOpe;"
				"MajUser;MajStamp;"
				"LastBat;LastBatLine;LastEntry;LastSettlement;"
				"Status" );
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
	fope = ofo_dossier_get_forward_ope( dossier );
	notes = my_utils_export_multi_lines( ofo_dossier_get_notes( dossier ));
	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	muser = ofo_dossier_get_upd_user( dossier );
	stamp = my_utils_stamp_to_str( ofo_dossier_get_upd_stamp( dossier ), MY_STAMP_YYMDHMS );
	currency = ofo_dossier_get_default_currency( dossier );
	siren = ofo_dossier_get_siren( dossier );
	bope = ofo_dossier_get_sld_ope( dossier );

	str = g_strdup_printf( "%s;%s;%s;%d;%s;%s;%s;%s;%s;%s;%s;%s;%ld;%ld;%ld;%ld;%s",
			currency ? currency : "",
			sbegin,
			send,
			ofo_dossier_get_exe_length( dossier ),
			exenotes ? exenotes : "",
			fope ? fope : "",
			label ? label : "",
			notes ? notes : "",
			siren ? siren : "",
			bope ? bope : "",
			muser ? muser : "",
			muser ? stamp : "",
			ofo_dossier_get_last_bat( dossier ),
			ofo_dossier_get_last_batline( dossier ),
			ofo_dossier_get_last_entry( dossier ),
			ofo_dossier_get_last_settlement( dossier ),
			ofo_dossier_get_status( dossier ));

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

		ok = ofa_dbms_backup( priv->dbms, fname );
	}

	return( ok );
}

/*
 * ofaIDataset interface management
 */
static void
idataset_iface_init( ofaIDatasetInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_idataset_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idataset_get_interface_version;
	iface->get_datasets = idataset_get_datasets;
	iface->set_datasets = idataset_set_datasets;
}

static guint
idataset_get_interface_version( const ofaIDataset *instance )
{
	return( 1 );
}

static GList *
idataset_get_datasets( const ofaIDataset *instance )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( instance && OFO_IS_DOSSIER( instance ), NULL );

	priv = OFO_DOSSIER( instance )->priv;

	return( priv->datasets );
}

static void
idataset_set_datasets( ofaIDataset *instance, GList *list )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( instance && OFO_IS_DOSSIER( instance ));

	priv = OFO_DOSSIER( instance )->priv;

	priv->datasets = list;
}

static void
free_datasets( GList *datasets )
{
	g_list_free_full( datasets, ( GDestroyNotify ) ofa_idataset_free_full );
}

static void
free_cur_detail( sCurrency *details )
{
	g_free( details->currency );
	g_free( details->sld_account );
	g_free( details );
}

static void
free_cur_details( GList *details )
{
	g_list_free_full( details, ( GDestroyNotify ) free_cur_detail );
}
