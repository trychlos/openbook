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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iregister.h"
#include "api/ofa-isingle-keeper.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-class.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* global data
	 */
	ofaExtenderCollection  *extenders;
	ofaPortfolioCollection *portfolios;
	GList                  *ofofakes;

	/* dossier
	 */
	ofaIDBConnect          *connect;
	ofoDossier             *dossier;
	ofaDossierPrefs        *dossier_prefs;
}
	ofaHubPrivate;

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	DOSSIER_OPENED,
	DOSSIER_CLOSED,
	ENTRY_STATUS_COUNT,
	ENTRY_STATUS_CHANGE,
	EXE_DATES_CHANGED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ]     = { 0 };

static void    iregister_iface_init( ofaIRegisterInterface *iface );
static GList  *iregister_get_for_type( ofaIRegister *instance, GType type );
static void    icollector_iface_init( ofaICollectorInterface *iface );
static guint   icollector_get_interface_version( const ofaICollector *instance );
static void    isingle_keeper_iface_init( ofaISingleKeeperInterface *iface );
static void    dossier_do_close( ofaHub *hub );
static void    init_signaling_system( ofaHub *hub );
static void    check_db_vs_settings( const ofaHub *hub );
static GSList *get_lines_from_content( const gchar *content, const ofaFileFormat *settings, guint *errors );
static void    free_fields( GSList *fields );
static void    free_lines( GSList *lines );

G_DEFINE_TYPE_EXTENDED( ofaHub, ofa_hub, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaHub )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IREGISTER, iregister_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTOR, icollector_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISINGLE_KEEPER, isingle_keeper_iface_init ))

static void
hub_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_hub_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_HUB( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_hub_parent_class )->finalize( instance );
}

static void
hub_dispose( GObject *instance )
{
	ofaHubPrivate *priv;

	g_return_if_fail( instance && OFA_IS_HUB( instance ));

	priv = ofa_hub_get_instance_private( OFA_HUB( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->extenders );
		g_clear_object( &priv->portfolios );
		g_list_free_full( priv->ofofakes, ( GDestroyNotify ) g_object_unref );

		dossier_do_close( OFA_HUB( instance ));
	}

	G_OBJECT_CLASS( ofa_hub_parent_class )->dispose( instance );
}

static void
ofa_hub_init( ofaHub *self )
{
	static const gchar *thisfn = "ofa_hub_init";
	ofaHubPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_HUB( self ));

	priv = ofa_hub_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->extenders = NULL;
	priv->portfolios = NULL;
}

static void
ofa_hub_class_init( ofaHubClass *klass )
{
	static const gchar *thisfn = "ofa_hub_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = hub_dispose;
	G_OBJECT_CLASS( klass )->finalize = hub_finalize;

	/**
	 * ofaHub::hub-object-new:
	 *
	 * The signal is emitted after a new object has been successfully
	 * inserted in the database. A connected handler may take advantage
	 * of this signal e.g. to update its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub    *hub,
	 * 							ofoBase  *object,
	 * 							gpointer  user_data );
	 */
	st_signals[ NEW_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_NEW,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaHub::hub-object-updated:
	 *
	 * The signal is emitted just after an object has been successfully
	 * updated in the DBMS. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects,
	 * or for updating its internal links, or so.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub       *hub,
	 * 							ofoBase     *object,
	 * 							const gchar *prev_id,
	 * 							gpointer     user_data );
	 */
	st_signals[ UPDATED_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_UPDATED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_STRING );

	/**
	 * ofaHub::hub-object-deleted:
	 *
	 * The signal is emitted just after an object has been successfully
	 * deleted from the DBMS. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects.
	 *
	 * Note that the emitter of this signal should attach a new reference
	 * to the deleted object in order to keep it alive during for the
	 * handlers execution.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub    *hub,
	 * 							ofoBase  *object,
	 * 							gpointer  user_data );
	 */
	st_signals[ DELETED_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_DELETED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaHub::hub-object-reload:
	 *
	 * The signal is emitted when such an update has been made in the
	 * DBMS that it is considered easier for a connected handler just
	 * to reload the whole dataset if this later whishes keep
	 * synchronized with the database.
	 *
	 * This signal is so less an information signal that an action hint.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 * 							GType    type_class,
	 * 							gpointer user_data );
	 */
	st_signals[ RELOAD_DATASET ] = g_signal_new_class_handler(
				SIGNAL_HUB_RELOAD,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_GTYPE );

	/**
	 * ofaHub::hub-dossier-opened:
	 *
	 * This signal is sent on the hub after it has opened a dossier.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 * 							gpointer user_data );
	 */
	st_signals[ DOSSIER_OPENED ] = g_signal_new_class_handler(
				SIGNAL_HUB_DOSSIER_OPENED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaHub::hub-dossier-closed:
	 *
	 * This signal is sent on the hub when it is about to close a
	 * dossier.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 * 							gpointer user_data );
	 */
	st_signals[ DOSSIER_CLOSED ] = g_signal_new_class_handler(
				SIGNAL_HUB_DOSSIER_CLOSED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaHub::hub-status-count:
	 *
	 * This signal is sent on the hub before each batch of entry status
	 * changes. A signal handler may so initialize e.g. a progression
	 * bar about the status change.
	 * The arguments may be read as: I am about to change the status of
	 * '@count' entries to '@new_status' status.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 *							gint     new_status,
	 *							gulong   count,
	 * 							gpointer user_data );
	 */
	st_signals[ ENTRY_STATUS_COUNT ] = g_signal_new_class_handler(
				SIGNAL_HUB_STATUS_COUNT,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_UINT, G_TYPE_ULONG );

	/**
	 * ofaHub::hub-status-change:
	 *
	 * This signal is sent of the @hub to ask an antry to change its
	 * status.
	 * The #ofoEntry class signal handler will update the @entry with
	 * its new @new_status status, and update the database accordingly.
	 * Other signal handlers may, e.g. update balances, progression
	 * bars, and so on.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub          *hub,
	 * 							const ofoEntry *entry
	 * 							gint            prev_status,
	 *							gint            new_status,
	 * 							gpointer        user_data );
	 */
	st_signals[ ENTRY_STATUS_CHANGE ] = g_signal_new_class_handler(
				SIGNAL_HUB_STATUS_CHANGE,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				3,
				G_TYPE_OBJECT, G_TYPE_UINT, G_TYPE_UINT );

	/**
	 * ofaHub::hub-exe-dates-changed:
	 *
	 * Beginning and or ending exercice dates of the dossier have been
	 * modified.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub       *hub
	 * 							const GDate *prev_begin,
	 * 							const GDate *prev_end,
	 * 							gpointer     user_data );
	 */
	st_signals[ EXE_DATES_CHANGED ] = g_signal_new_class_handler(
				SIGNAL_HUB_EXE_DATES_CHANGED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
}

/*
 * ofaIRegister interface management
 */
static void
iregister_iface_init( ofaIRegisterInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_iregister_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_for_type = iregister_get_for_type;
}

static GList *
iregister_get_for_type( ofaIRegister *instance, GType type )
{
	ofaHubPrivate *priv;
	GList *list, *it;

	priv = ofa_hub_get_instance_private( OFA_HUB( instance ));

	list = NULL;

	for( it=priv->ofofakes ; it ; it=it->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( it->data ), type )){
			list = g_list_prepend( list, g_object_ref( it->data ));
		}
	}

	return( list );
}

/*
 * ofaICollector interface management
 */
static void
icollector_iface_init( ofaICollectorInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_icollector_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollector_get_interface_version;
}

static guint
icollector_get_interface_version( const ofaICollector *instance )
{
	return( 1 );
}

/*
 * ofaISingleKeeper interface management
 */
static void
isingle_keeper_iface_init( ofaISingleKeeperInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_isingle_keeper_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/**
 * ofa_hub_new:
 *
 * Returns: a new #ofaHub object.
 */
ofaHub *
ofa_hub_new( void )
{
	ofaHub *hub;

	hub = g_object_new( OFA_TYPE_HUB, NULL );

	return( hub );
}

/**
 * ofa_hub_get_extender_collection:
 * @hub: this #ofaHub instance.
 *
 * Returns: the extenders collection.
 *
 * The returned reference is owned by the @hub instance, and should not
 * be unreffed by the caller.
 */
ofaExtenderCollection *
ofa_hub_get_extender_collection( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->extenders );
}

/**
 * ofa_hub_set_extender_collection:
 * @hub: this #ofaHub instance.
 * @collection: the extender collection.
 *
 * Set the extender collection.
 */
void
ofa_hub_set_extender_collection( ofaHub *hub, ofaExtenderCollection *collection )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->extenders = collection;
}

/**
 * ofa_hub_register_types:
 * @hub: this #ofaHub instance.
 *
 * Registers all #ofoBase derived types provided by the core library.
 */
void
ofa_hub_register_types( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->ofofakes = NULL;
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_DOSSIER, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_CLASS, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_CURRENCY, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_ACCOUNT, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_BAT, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_CONCIL, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_LEDGER, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_OPE_TEMPLATE, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_RATE, NULL ));
	priv->ofofakes = g_list_prepend( priv->ofofakes, g_object_new( OFO_TYPE_ENTRY, NULL ));
}

/**
 * ofa_hub_get_portfolio_collection:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaPortfolioCollection object which manages the
 * collection of dossiers (aka portfolio) from the settings point of
 * view.
 *
 * The returned reference is owned by the @hub instance, and should not
 * be unreffed by the caller.
 */
ofaPortfolioCollection *
ofa_hub_get_portfolio_collection( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->portfolios );
}

/**
 * ofa_hub_set_portfolio_collection:
 * @hub: this #ofaHub instance.
 * @collection: the dossier collection.
 *
 * Set the portfolio collection.
 */
void
ofa_hub_set_portfolio_collection( ofaHub *hub, ofaPortfolioCollection *collection )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->portfolios = collection;
}

/**
 * ofa_hub_dossier_open:
 * @hub: this #ofaHub instance.
 * @connect: a valid connection to the targeted database.
 * @parent: the #GtkWindow parent window.
 *
 * Open the dossier and exercice pointed to by the @connect connection.
 * On success, the @hub object takes a reference on this @connect
 * connection, which thus may be then released by the caller.
 *
 * This method is the canonical way of opening a dossier.
 *
 * Returns: %TRUE if the dossier has been successully opened, %FALSE
 * else.
 */
gboolean
ofa_hub_dossier_open( ofaHub *hub, ofaIDBConnect *connect, GtkWindow *parent )
{
	ofaHubPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ofa_hub_dossier_close( hub );

	priv->connect = g_object_ref(( gpointer ) connect );
	ok = FALSE;

	if( ofa_idbmodel_update( hub, parent )){
		priv->dossier = ofo_dossier_new( hub );
		if( priv->dossier ){
			init_signaling_system( hub );
			priv->dossier_prefs = ofa_dossier_prefs_new( hub );
			ok = TRUE;
		}
	}

	if( !ok ){
		dossier_do_close( hub );
	} else {
		g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_OPENED );
	}

	return( ok );
}

/**
 * ofa_hub_dossier_close:
 * @hub: this #ofaHub instance.
 *
 * Close the currently opened dossier if any.
 *
 * This method is the canonical way of closing a dossier.
 */
void
ofa_hub_dossier_close( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_CLOSED );

	dossier_do_close( hub );
}

static void
dossier_do_close( ofaHub *hub )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( hub );

	g_clear_object( &priv->connect );
	g_clear_object( &priv->dossier );
	g_clear_object( &priv->dossier_prefs );
}

/*
 * Be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself
 *
 * Example of a use case: the intermediate closing by ledger may be run
 * without having first loaded the accounts, but the accounts should be
 * connected in order to update themselves.
 */
static void
init_signaling_system( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_init_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	ofo_account_connect_to_hub_signaling_system( hub );
	ofo_bat_connect_to_hub_signaling_system( hub );
	ofo_class_connect_to_hub_signaling_system( hub );
	ofo_concil_connect_to_hub_signaling_system( hub );
	ofo_currency_connect_to_hub_signaling_system( hub );
	ofo_entry_connect_to_hub_signaling_system( hub );
	ofo_ledger_connect_to_hub_signaling_system( hub );
	ofo_ope_template_connect_to_hub_signaling_system( hub );
	ofo_rate_connect_to_hub_signaling_system( hub );

	ofa_idbmodel_init_hub_signaling_system( hub );
}

/*
 * ofa_hub_get_connect:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaIDBConnect connection object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
const ofaIDBConnect *
ofa_hub_get_connect( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->connect );
}

/*
 * ofa_hub_get_dossier:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofoDossier object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
ofoDossier *
ofa_hub_get_dossier( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->dossier );
}

/*
 * ofa_hub_get_dossier_prefs:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaDossierPrefs object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
ofaDossierPrefs *
ofa_hub_get_dossier_prefs( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->dossier_prefs );
}

/*
 * ofa_hub_remediate_settings:
 * @hub: this #ofaHub instance.
 *
 * Make sure, and update them if needed, that dossier settings datas
 * are synchronized with the same data recorded in the database:
 * - opened or closed exercice,
 * - begin and end dates of the exercice.
 */
void
ofa_hub_remediate_settings( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	check_db_vs_settings( hub );
}

/* when opening the dossier, make sure the settings are up to date
 * (this may not be the case when the dossier has just been restored
 *  or created)
 * The datas found in the dossier database take precedence over those
 * read from dossier settings. This is because dossier database is
 * (expected to be) updated via the Openbook software suite and so be
 * controlled, while the dossier settings may easily be tweaked by the
 * user.
 */
static void
check_db_vs_settings( const ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_check_db_vs_settings";
	ofoDossier *dossier;
	gboolean db_current, settings_current;
	const GDate *db_begin, *db_end, *settings_begin, *settings_end;
	const ofaIDBConnect *cnx;
	ofaIDBPeriod *period;
	ofaIDBMeta *meta;
	gchar *sdbbegin, *sdbend, *ssetbegin, *ssetend;

	dossier = ofa_hub_get_dossier( hub );
	cnx = ofa_hub_get_connect( hub );

	/* data from db */
	db_current = ofo_dossier_is_current( dossier );
	db_begin = ofo_dossier_get_exe_begin( dossier );
	db_end = ofo_dossier_get_exe_end( dossier );

	/* data from settings */
	period = ofa_idbconnect_get_period( cnx );
	settings_current = ofa_idbperiod_get_current( period );
	settings_begin = ofa_idbperiod_get_begin_date( period );
	settings_end = ofa_idbperiod_get_end_date( period );

	/* update settings if not equal */
	if( db_current != settings_current ||
			my_date_compare_ex( db_begin, settings_begin, TRUE ) != 0 ||
			my_date_compare_ex( db_end, settings_end, FALSE ) != 0 ){

		sdbbegin = my_date_to_str( db_begin, MY_DATE_SQL );
		sdbend = my_date_to_str( db_end, MY_DATE_SQL );
		ssetbegin = my_date_to_str( settings_begin, MY_DATE_SQL );
		ssetend = my_date_to_str( settings_end, MY_DATE_SQL );

		g_debug( "%s: db_current=%s, db_begin=%s, db_end=%s, settings_current=%s, settings_begin=%s, settings_end=%s: updating settings",
				thisfn,
				db_current ? "True":"False", sdbbegin, sdbend,
				settings_current ? "True":"False", ssetbegin, ssetend );

		g_free( sdbbegin );
		g_free( sdbend );
		g_free( ssetbegin );
		g_free( ssetend );

		meta = ofa_idbconnect_get_meta( cnx );
		ofa_idbmeta_update_period( meta, period, db_current, db_begin, db_end );
		g_object_unref( meta );
	}

	g_object_unref( period );
}

/**
 * ofa_hub_import_csv:
 * @dossier: the target #ofoDossier
 * @object: an empty object of the class to be imported.
 * @uri: the input file
 * @settings: the #ofaFileFormat instance which describes the @uri file.
 * @caller: [allow-none]: the #ofaIImporter instance
 * @errors: [allow-none][out]: errors count
 *
 * Import a CSV file into the dossier.
 *
 * Returns: the count of imported lines.
 */
guint
ofa_hub_import_csv( ofaHub *hub, ofaIImportable *object, const gchar *uri, const ofaFileFormat *settings, void *caller, guint *errors )
{
	guint count, local_errors;
	gchar *content;
	GSList *lines;
	gchar *str;
	gint headers_count;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );
	g_return_val_if_fail( object && OFA_IS_IIMPORTABLE( object ), 0 );
	g_return_val_if_fail( my_strlen( uri ), 0 );
	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	count = 0;
	local_errors = 0;
	headers_count = ofa_file_format_get_headers_count( settings );

	content = my_utils_uri_get_content( uri, ofa_file_format_get_charmap( settings ), &local_errors );
	if( !local_errors ){
		lines = get_lines_from_content( content, settings, &local_errors );
		if( !local_errors ){
			if( lines ){
				count = g_slist_length( lines ) - headers_count;

				if( count > 0 ){
					local_errors = ofa_iimportable_import( object, lines, settings, hub, caller );

				} else if( count < 0 ){
					count = 0;
					local_errors += 1;
					str = g_strdup_printf(
							_( "Expected headers count=%u greater than count of lines read from '%s' file" ),
							headers_count, uri );
					my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
					g_free( str );
				}

				free_lines( lines );
			}
		}
	}

	g_free( content );
	if( errors ){
		*errors = local_errors;
	}

	return( count );
}

/*
 * Returns a GSList of lines, where each lines->data is a GSList of
 * fields.
 * We have to explore the content to extract each field which
 * may or may not be quoted, considering only newlines which are not
 * inside a quoted field.
 */
static GSList *
get_lines_from_content( const gchar *content, const ofaFileFormat *settings, guint *errors )
{
	static const gchar *thisfn = "ofa_hub_get_lines_from_content";
	GSList *tmp_list, *out_list, *it_list, *field_list;
	gchar *field_sep;
	gchar **lines, **it_line;
	gchar **fields, **it_field;
	gchar *prev, *temp, *str;
	guint numline;

	/* UTF-8 validation */
	if( !g_utf8_validate( content, -1, NULL )){
		str = g_strdup_printf( _( "The provided string is not UTF8-valide: '%s'" ), content );
		my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
		g_free( str );
		*errors += 1;
		return( NULL );
	}

	/* split on end-of-line
	 * then re-concatenate segments where end-of-line was backslashed */
	lines = g_strsplit( content, "\n", -1 );
	it_line = lines;
	prev = NULL;
	tmp_list = NULL;
	numline = 0;
	while( *it_line ){
		if( prev ){
			if( 1 ){
				g_debug( "num=%u line='%s'", numline, prev );
			}
			temp = g_utf8_substring( prev, 0, my_strlen( prev )-1 );
			g_free( prev );
			prev = temp;
			temp = g_strconcat( prev, "\n", *it_line, NULL );
			g_free( prev );
			prev = temp;
		} else {
			prev = g_strdup( *it_line );
		}
		if( my_strlen( prev ) && !g_str_has_suffix( prev, "\\" )){
			numline += 1;
			tmp_list = g_slist_prepend( tmp_list, g_strdup( prev ));
			g_free( prev );
			prev = NULL;
		} else if( !my_strlen( prev )){
			g_free( prev );
			prev = NULL;
		}
		it_line++;
	}
	g_strfreev( lines );
	tmp_list = g_slist_reverse( tmp_list );
	if( 0 ){
		g_debug( "%s: tmp_list count=%d", thisfn, g_slist_length( tmp_list ));
	}

	/* tmp_list is now a list of lines
	 * fields have to be splitted when field separator is not backslashed */
	numline = 0;
	out_list = NULL;
	field_sep = g_strdup_printf( "%c", ofa_file_format_get_field_sep( settings ));

	if( *errors == 0 ){
		for( it_list=tmp_list ; it_list ; it_list=it_list->next ){
			fields = g_strsplit(( const gchar * ) it_list->data, field_sep, -1 );
			it_field = fields;
			field_list = NULL;
			prev = NULL;
			numline += 1;
			while( *it_field ){
				if( prev ){
					temp = g_strconcat( prev, field_sep, *it_field, NULL );
					g_free( prev );
					prev = temp;
				} else {
					prev = g_strdup( *it_field );
				}
				if( !g_str_has_suffix( prev, "\\" )){
					if( 0 ){
						g_debug( "%s: numline=%u, field='%s'", thisfn, numline, prev );
					}
					field_list = g_slist_prepend( field_list, prev );
					prev = NULL;
				}
				it_field++;
			}
			out_list = g_slist_prepend( out_list, g_slist_reverse( field_list ));
			g_strfreev( fields );
		}
	}
	g_free( field_sep );
	g_slist_free_full( tmp_list, ( GDestroyNotify ) g_free );
	g_debug( "%s: out_list count=%d", thisfn, g_slist_length( out_list ));

	return( g_slist_reverse( out_list ));
}

static void
free_fields( GSList *fields )
{
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );
}

static void
free_lines( GSList *lines )
{
	g_slist_free_full( lines, ( GDestroyNotify ) free_fields );
}

/**
 * ofa_hub_disconnect_handlers:
 * @hub: this #ofaHub object.
 * @handlers: a #GList of the handler identifiers got when connecting
 *  to the hub signaling system
 *
 * Disconnect the specified @handlers from the signaling system.
 *
 * Rationale: an object should disconnect its signals when it disappears
 * while the signal emitter is still alive, i.e. to prevent the signal
 * emitter to keep sending signals to a now-disappeared object.
 */
void
ofa_hub_disconnect_handlers( ofaHub *hub, GList *handlers )
{
	static const gchar *thisfn = "ofa_hub_disconnect_handlers";
	ofaHubPrivate *priv;
	GList *it;

	g_debug( "%s: hub=%p, handlers=%p (count=%d)",
			thisfn, ( void * ) hub, ( void * ) handlers, g_list_length( handlers ));

	priv = ofa_hub_get_instance_private( hub );

	if( !priv->dispose_has_run ){
		for( it=handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( hub, ( gulong ) it->data );
		}
	}
}
