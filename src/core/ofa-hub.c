/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include "my/my-icollector.h"
#include "my/my-isettings.h"
#include "my/my-settings.h"
#include "my/my-signal.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-class.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-paimean.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* private instance data
 */
typedef struct {
	gboolean               dispose_has_run;

	/* initialization
	 */
	GApplication          *application;
	gchar                 *argv_0;

	/* to be deprecated
	 */
	ofaIGetter            *getter;

	/* global data
	 */
	ofaExtenderCollection *extenders;
	ofaDossierCollection  *dossier_collection;
	GList                 *core_objects;
	mySettings            *dossier_settings;
	mySettings            *user_settings;
	ofaOpenbookProps      *openbook_props;
	gchar                 *runtime_dir;

	/* dossier
	 */
	ofaIDBConnect         *connect;
	ofoDossier            *dossier;
	gboolean               read_only;
}
	ofaHubPrivate;

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETABLE_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	DOSSIER_OPENED,
	DOSSIER_CLOSED,
	DOSSIER_CHANGED,
	DOSSIER_PREVIEW,
	ENTRY_STATUS_COUNT,
	ENTRY_STATUS_CHANGE,
	EXE_DATES_CHANGED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ]     = { 0 };

static void     register_types( ofaHub *self );
static void     init_signaling_system( ofaHub *self );
static void     init_signaling_system_connect_to( ofaHub *self, GType type );
static void     setup_settings( ofaHub *self );
static gboolean on_deletable_default_handler( ofaHub *self, GObject *object );
static void     on_dossier_changed( ofaHub *hub, void *empty );
static gboolean remediate_dossier_settings( ofaHub *self );
static void     icollector_iface_init( myICollectorInterface *iface );
static guint    icollector_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaHub, ofa_hub, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaHub )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTOR, icollector_iface_init ))

static void
hub_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_hub_finalize";
	ofaHubPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_HUB( instance ));

	/* free data members here */
	priv = ofa_hub_get_instance_private( OFA_HUB( instance ));

	g_free( priv->argv_0 );
	g_free( priv->runtime_dir );

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

		/* close the opened dossier (if any) before disposing hub */
		ofa_hub_close_dossier( OFA_HUB( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->dossier_settings );
		g_clear_object( &priv->user_settings );
		g_clear_object( &priv->dossier_collection );
		g_clear_object( &priv->extenders );
		g_clear_object( &priv->openbook_props );

		g_list_free_full( priv->core_objects, ( GDestroyNotify ) g_object_unref );
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
	priv->dossier_collection = NULL;
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
	 * ofaHub::hub-object-deletable:
	 *
	 * The signal is emitted when the application wants to know if a
	 * particular object is deletable.
	 *
	 * The handlers should return %TRUE to abort the deletion of the
	 * proposed object.
	 *
	 * Handler is of type:
	 * 		gboolean user_handler( ofaHub    *hub,
	 * 								ofoBase  *object,
	 * 								gpointer  user_data );
	 */
	st_signals[ DELETABLE_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_DELETABLE,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				G_CALLBACK( on_deletable_default_handler ),
				my_signal_accumulator_false_handled,
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_BOOLEAN,
				1,
				G_TYPE_OBJECT );

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
	 * ofaHub::hub-dossier-changed:
	 *
	 * This signal is sent on the hub when the properties of the dossier
	 * has been modified (or may have been modified) by the user. This
	 * strictly corresponds to the OFA_T_DOSSIER table content.
	 *
	 * The #ofaHub itself is connected to the signal and is the very
	 * first handler triggered. It takes advantage of this signal to
	 * remediate the dossier settings.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 * 							gpointer user_data );
	 */
	st_signals[ DOSSIER_CHANGED ] = g_signal_new_class_handler(
				SIGNAL_HUB_DOSSIER_CHANGED,
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
	 * ofaHub::hub-dossier-preview:
	 * @uri: [allow-none]:
	 *
	 * This signal is sent on the hub to set a new background image.
	 * The sender is responsible to restore the original image if the
	 * user cancels the update.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub       *hub,
	 * 							const gchar *uri,
	 * 							gpointer     user_data );
	 */
	st_signals[ DOSSIER_PREVIEW ] = g_signal_new_class_handler(
				SIGNAL_HUB_DOSSIER_PREVIEW,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );

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
	 * status. This is an ACTION signal.
	 *
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

/**
 * ofa_hub_new:
 * @application: the #GApplication which has created and owns this object.
 *
 * Allocates and initializes the #ofaHub object of the application.
 *
 * Returns: a new #ofaHub object.
 */
ofaHub *
ofa_hub_new( GApplication *application, const gchar *argv_0 )
{
	static const gchar *thisfn = "ofa_hub_new";
	ofaHub *hub;
	ofaHubPrivate *priv;

	g_debug( "%s: application=%p, argv_0=%s", thisfn, ( void * ) application, argv_0 );

	g_return_val_if_fail( application && G_IS_APPLICATION( application ), NULL );

	hub = g_object_new( OFA_TYPE_HUB, NULL );

	priv = ofa_hub_get_instance_private( hub );

	priv->application = application;
	priv->argv_0 = g_strdup( argv_0 );

	ofa_box_register_types();
	register_types( hub );
	init_signaling_system( hub );
	setup_settings( hub );

	/* as of 2017-02-01 (v0.66), the IGetter interface becomes useless
	 * as long as we always have a #ofaHub object */
	priv->getter = ofa_igetter_get_permanent_getter( OFA_IGETTER( application ));

	priv->extenders = ofa_extender_collection_new( priv->getter, PKGLIBDIR );
	priv->dossier_collection = ofa_dossier_collection_new( hub );
	priv->openbook_props = ofa_openbook_props_new( hub );
	priv->runtime_dir = g_path_get_dirname( priv->argv_0 );

	g_signal_connect( hub, SIGNAL_HUB_DOSSIER_CHANGED, G_CALLBACK( on_dossier_changed ), NULL );

	return( hub );
}

/*
 * ofa_hub_register_types:
 * @hub: this #ofaHub instance.
 *
 * Registers all #ofoBase derived types provided by the core library
 * (aka "core types") so that @hub will be able to dynamically request
 * them on demand.
 *
 * This method, plus #ofa_hub_get_for_type() below, are for example
 * used to get a dynamic list of importable or exportable types, and
 * more generally to be able to get a dynamic list of any known (and
 * registered) type.
 *
 * Plugins-provided types (aka "plugin types") do not need to register
 * here. It is enough they implement the desired interface to be
 * dynamically requested on demand.
 */
static void
register_types( ofaHub *self )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( self );

	/* this is also the order of IExportable/IImportable classes in the
	 * assistants: do not change this order */
	priv->core_objects = NULL;
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_DOSSIER, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CLASS, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CURRENCY, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_ACCOUNT, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CONCIL, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_ENTRY, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_LEDGER, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_PAIMEAN, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_OPE_TEMPLATE, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_RATE, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_BAT, NULL ));
}

/*
 * ofa_hub_init_signaling_system:
 * @hub: this #ofaHub instance.
 *
 * Initialize the hub signaling system.
 *
 * Be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself.
 *
 * Example of a use case: the intermediate closing by ledger may be run
 * without having first loaded the accounts, but the accounts should be
 * connected in order to update themselves.
 *
 * This method must be called after having registered core types.
 */
static void
init_signaling_system( ofaHub *self )
{
	GList *list, *it;

	list = ofa_hub_get_for_type( self, OFA_TYPE_ISIGNAL_HUB );

	for( it=list ; it ; it=it->next ){
		init_signaling_system_connect_to( self, G_OBJECT_TYPE( it->data ));
	}

	g_list_free_full( list, ( GDestroyNotify ) g_object_unref );
}

static void
init_signaling_system_connect_to( ofaHub *self, GType type )
{
	gpointer klass, iface;

	klass = g_type_class_ref( type );
	g_return_if_fail( klass );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISIGNAL_HUB );

	if( iface && (( ofaISignalHubInterface * ) iface )->connect ){
		(( ofaISignalHubInterface * ) iface )->connect( self );
	} else {
		g_info( "%s implementation does not provide 'ofaISignalHub::connect()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );
}

static void
setup_settings( ofaHub *self )
{
	ofaHubPrivate *priv;
	gchar *name;

	priv = ofa_hub_get_instance_private( self );

	priv->dossier_settings = my_settings_new_user_config( "dossier.conf", "OFA_DOSSIER_CONF" );

	name = g_strdup_printf( "%s.conf", PACKAGE );
	priv->user_settings = my_settings_new_user_config( name, "OFA_USER_CONF" );
	g_free( name );
}

/**
 * ofa_hub_get_application:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #GApplication which has created and owns this @hub.
 *
 * The returned reference is owned by the @hub instance, and should not
 * be released by the caller.
 */
GApplication *
ofa_hub_get_application( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->application );
}

/**
 * ofa_hub_get_extender_collection:
 * @hub: this #ofaHub instance.
 *
 * Returns: the extenders collection.
 *
 * The returned reference is owned by the @hub instance, and should not
 * be released by the caller.
 */
ofaExtenderCollection *
ofa_hub_get_extender_collection( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->extenders );
}

/**
 * ofa_hub_get_collector:
 * @hub: this #ofaHub instance.
 *
 * Returns: the object which implement #myICollector interface.
 */
myICollector *
ofa_hub_get_collector( const ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( MY_ICOLLECTOR( hub ));
}

/*
 * Without this class handler connected, the returned deletability is
 * False.
 *
 * With this class handler connected and returning %TRUE, then the
 * returned deletability is True (which is the expected default).
 *
 * With this class handler connected and returning %FALSE, then the
 * returned deletability is False.
 *
 * Any connected handler returning %FALSE stops the signal emission,
 * and the returned value is %FALSE.
 *
 * If all connected handlers return %TRUE, then the control reaches
 * this default handler which also returns %TRUE. The object is then
 * supposed to be deletable.
 */
static gboolean
on_deletable_default_handler( ofaHub *self, GObject *object )
{
	static const gchar *thisfn = "ofa_hub_on_deletable_default_handler";

	g_debug( "%s: self=%p, object=%p (%s)",
			thisfn, ( void * ) self, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	return( TRUE );
}

static void
on_dossier_changed( ofaHub *hub, void *empty )
{
	remediate_dossier_settings( hub );
}

/**
 * ofa_hub_get_for_type:
 * @hub: the #ofaHub object of the application.
 * @type: a GType.
 *
 * Returns: a list of new references to objects which implement the
 * @type, concatenating both those from the core library, and those
 * advertized by the plugins.
 *
 * It is expected that the caller takes ownership of the returned list,
 * and #g_list_free_full( list, ( GDestroyNotify ) g_object_unref )
 * after use.
 */
GList *
ofa_hub_get_for_type( ofaHub *hub, GType type )
{
	ofaHubPrivate *priv;
	GList *objects, *it, *extender_objects;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	/* requests first the objects registered from core library */
	objects = NULL;
	for( it=priv->core_objects ; it ; it=it->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( it->data ), type )){
			objects = g_list_prepend( objects, g_object_ref( it->data ));
		}
	}

	/* requests then same type from loaded modules */
	extender_objects = ofa_extender_collection_get_for_type( priv->extenders, type );
	objects = g_list_concat( objects, extender_objects );

	return( objects );
}

/**
 * ofa_hub_get_dossier_collection:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaDossierCollection object which manages the
 * collection of dossiers (aka portfolio) from the settings point of
 * view.
 *
 * The returned reference is owned by the @hub instance, and should not
 * be released by the caller.
 */
ofaDossierCollection *
ofa_hub_get_dossier_collection( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->dossier_collection );
}

/**
 * ofa_hub_get_dossier_settings:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #myISettings instance which manages the dossier settings.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
myISettings *
ofa_hub_get_dossier_settings( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( MY_ISETTINGS( priv->dossier_settings ));
}

/**
 * ofa_hub_get_user_settings:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #myISettings instance which manages the user settings.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
myISettings *
ofa_hub_get_user_settings( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( MY_ISETTINGS( priv->user_settings ));
}

/**
 * ofa_hub_get_openbook_props:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaOpenbookProps which has been initialized at startup
 * time.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
ofaOpenbookProps *
ofa_hub_get_openbook_props( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->openbook_props );
}

/**
 * ofa_hub_get_runtime_dir:
 * @hub: this #ofaHub instance.
 *
 * Returns: the directory where Openbook is executed from.
 */
const gchar *
ofa_hub_get_runtime_dir( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->runtime_dir );
}

/**
 * ofa_hub_open_dossier:
 * @hub: this #ofaHub instance.
 * @parent: [allow-none]: the #GtkWindow parent window.
 * @connect: a valid connection to the targeted database;
 *  the @hub object takes its own reference on the @connect instance,
 *  which thus can then be released by the caller.
 * @read_only: whether the dossier should be opened in read-only mode.
 * @remediate_settings: whether settings should be remediated to the
 *  properties read from database;
 *  though this is in general %TRUE, this is in particular %FALSE when
 *  about to open the new exercice after a closing, because the new
 *  properties have not yet been updated.
 *
 * Open the dossier and exercice pointed to by the @connect connection.
 * On success, the @hub object takes a reference on this @connect
 * connection, which thus may be then released by the caller.
 *
 * This method is the canonical way of opening a dossier in batch mode.
 * #ofaIDBConnect and #ofoDossier are expected to be %NULL.
 *
 * In user interface mode, see #ofa_dossier_open_run() function.
 *
 * Returns: %TRUE if the dossier has been successully opened, %FALSE
 * else.
 */
gboolean
ofa_hub_open_dossier( ofaHub *hub, GtkWindow *parent,
							ofaIDBConnect *connect, gboolean read_only, gboolean remediate_settings )
{
	static const gchar *thisfn = "ofa_hub_open_dossier";
	ofaHubPrivate *priv;
	gboolean ok;

	g_debug( "%s: hub=%p, parent=%p, connect=%p, read_only=%s, remediate_settings=%s",
			thisfn, ( void * ) hub, ( void * ) parent, ( void * ) connect,
			read_only ? "True":"False", remediate_settings ? "True":"False" );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	/* this is a programming error to have a dossier currently opened
	 *  at this time */
	g_return_val_if_fail( priv->connect == NULL, FALSE );
	g_return_val_if_fail( priv->dossier == NULL, FALSE );

	priv->connect = g_object_ref(( gpointer ) connect );
	ok = FALSE;

	if( ofa_idbmodel_update( priv->getter, parent )){
		priv->dossier = ofo_dossier_new( hub );
		if( priv->dossier ){
			priv->read_only = read_only;
			g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_OPENED );
			ok = TRUE;
			if( remediate_settings ){
				g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_CHANGED );
			}
		}
	}

	if( !ok ){
		ofa_hub_close_dossier( hub );
	}

	return( ok );
}

/*
 * When opening the dossier, make sure the settings are up to date
 * (this may not be the case when the dossier has just been restored
 *  or created)
 *
 * The datas found in the dossier database take precedence over those
 * read from dossier settings. This is because dossier database is
 * (expected to be) updated via the Openbook software suite and so be
 * controlled, while the dossier settings may easily be tweaked by the
 * user.
 */
static gboolean
remediate_dossier_settings( ofaHub *self )
{
	static const gchar *thisfn = "ofa_hub_remediate_dossier_settings";
	ofoDossier *dossier;
	gboolean db_current, settings_current, remediated;
	const GDate *db_begin, *db_end, *settings_begin, *settings_end;
	const ofaIDBConnect *cnx;
	ofaIDBExerciceMeta *period;
	gchar *sdbbegin, *sdbend, *ssetbegin, *ssetend;

	remediated = FALSE;
	dossier = ofa_hub_get_dossier( self );
	cnx = ofa_hub_get_connect( self );

	/* data from db */
	db_current = ofo_dossier_is_current( dossier );
	db_begin = ofo_dossier_get_exe_begin( dossier );
	db_end = ofo_dossier_get_exe_end( dossier );

	/* data from settings */
	period = ofa_idbconnect_get_exercice_meta( cnx );
	settings_current = ofa_idbexercice_meta_get_current( period );
	settings_begin = ofa_idbexercice_meta_get_begin_date( period );
	settings_end = ofa_idbexercice_meta_get_end_date( period );

	/* update settings if not equal */
	if( db_current != settings_current ||
			my_date_compare_ex( db_begin, settings_begin, TRUE ) != 0 ||
			my_date_compare_ex( db_end, settings_end, FALSE ) != 0 ){

		sdbbegin = my_date_to_str( db_begin, MY_DATE_SQL );
		sdbend = my_date_to_str( db_end, MY_DATE_SQL );
		ssetbegin = my_date_to_str( settings_begin, MY_DATE_SQL );
		ssetend = my_date_to_str( settings_end, MY_DATE_SQL );

		g_debug( "%s: db_current=%s, db_begin=%s, db_end=%s, settings_current=%s, settings_begin=%s, settings_end=%s: remediating settings",
				thisfn,
				db_current ? "True":"False", sdbbegin, sdbend,
				settings_current ? "True":"False", ssetbegin, ssetend );

		g_free( sdbbegin );
		g_free( sdbend );
		g_free( ssetbegin );
		g_free( ssetend );

		ofa_idbexercice_meta_set_current( period, db_current );
		ofa_idbexercice_meta_set_begin_date( period, db_begin );
		ofa_idbexercice_meta_set_end_date( period, db_end );
		ofa_idbexercice_meta_update_settings( period );

		remediated = TRUE;

	} else {
		g_debug( "%s: nothing to do", thisfn );
	}

	return( remediated );
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
ofaIDBConnect *
ofa_hub_get_connect( ofaHub *hub )
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
ofa_hub_get_dossier( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->dossier );
}

/**
 * ofa_hub_is_opened_dossier:
 * @hub: this #ofaHub instance.
 * @exercice_meta: the #ofaIDBExerciceMeta we would want open.
 *
 * Returns: %TRUE if @exercice_meta is currently opened.
 */
gboolean
ofa_hub_is_opened_dossier( ofaHub *hub, ofaIDBExerciceMeta *exercice_meta )
{
	ofaHubPrivate *priv;
	ofaIDBDossierMeta *current_dossier, *candidate_dossier;
	ofaIDBExerciceMeta *current_exercice;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ), FALSE );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( !priv->connect || !priv->dossier ){
		return( FALSE );
	}

	candidate_dossier = ofa_idbexercice_meta_get_dossier_meta( exercice_meta );
	current_dossier = ofa_idbconnect_get_dossier_meta( priv->connect );

	if( ofa_idbdossier_meta_compare( current_dossier, candidate_dossier ) != 0 ){
		return( FALSE );
	}

	current_exercice = ofa_idbconnect_get_exercice_meta( priv->connect );
	if( ofa_idbexercice_meta_compare( current_exercice, exercice_meta ) != 0 ){
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofa_hub_is_writable_dossier:
 * @hub: this #ofaHub instance.
 *
 * Returns: %TRUE if the dossier is writable, i.e. is a current exercice
 * which has not been opened in read-only mode.
 */
gboolean
ofa_hub_is_writable_dossier( ofaHub *hub )
{
	ofaHubPrivate *priv;
	gboolean is_writable;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	is_writable = priv->dossier &&
			OFO_IS_DOSSIER( priv->dossier ) &&
			ofo_dossier_is_current( priv->dossier ) &&
			!priv->read_only;

	return( is_writable );
}

/**
 * ofa_hub_close_dossier:
 * @hub: this #ofaHub instance.
 *
 * Close the currently opened dossier if any.
 *
 * This method is the canonical way of closing a dossier.
 */
void
ofa_hub_close_dossier( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	if( priv->dossier || priv->connect ){

		/* emit the closing signal
		 *  at this time all datas are alive and valid */
		g_signal_emit_by_name( hub, SIGNAL_HUB_DOSSIER_CLOSED );

		g_clear_object( &priv->connect );
		g_clear_object( &priv->dossier );

		my_icollector_free_all( ofa_hub_get_collector( hub ));
	}
}

/**
 * ofa_hub_get_willing_to_import:
 * @hub: this #ofaHub instance.
 * @uri: the URI of a file to be imported.
 * @type: the expected target GType.
 *
 * Returns: a new reference to the first found #ofaIImporter willing to
 * import the @uri, which should be #g_object_unref() by the caller.
 */
ofaIImporter *
ofa_hub_get_willing_to_import( ofaHub *hub, const gchar *uri, GType type )
{
	ofaIImporter *found;
	ofaExtenderCollection *extenders;
	GList *importers, *it;

	found = NULL;
	extenders = ofa_hub_get_extender_collection( hub );
	importers = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IIMPORTER );

	for( it=importers ; it ; it=it->next ){
		if( ofa_iimporter_is_willing_to( OFA_IIMPORTER( it->data ), hub, uri, type )){
			found = OFA_IIMPORTER( it->data );
			break;
		}
	}

	ofa_extender_collection_free_types( importers );

	return( found ? g_object_ref( G_OBJECT( found )) : NULL );
}

/**
 * ofa_hub_disconnect_handlers:
 * @hub: this #ofaHub object.
 * @handlers: a #GList of the handler identifiers got when connecting
 *  to the hub signaling system
 *
 * Disconnect the specified @handlers from the signaling system.
 * Free the list and clear the @handlers pointer.
 *
 * Rationale: an object should disconnect its signals when it disappears
 * while the signal emitter is still alive, i.e. to prevent the signal
 * emitter to keep sending signals to a now-disappeared object.
 */
void
ofa_hub_disconnect_handlers( ofaHub *hub, GList **handlers )
{
	static const gchar *thisfn = "ofa_hub_disconnect_handlers";
	ofaHubPrivate *priv;
	GList *it;

	g_debug( "%s: hub=%p, handlers=%p (count=%d)",
			thisfn, ( void * ) hub, ( void * ) handlers, g_list_length( *handlers ));

	priv = ofa_hub_get_instance_private( hub );

	if( !priv->dispose_has_run ){
		for( it=( *handlers ) ; it ; it=it->next ){
			g_signal_handler_disconnect( hub, ( gulong ) it->data );
		}
	}

	if( *handlers ){
		g_list_free( *handlers );
		*handlers = NULL;
	}
}

/*
 * myICollector interface management
 */
static void
icollector_iface_init( myICollectorInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_icollector_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollector_get_interface_version;
}

static guint
icollector_get_interface_version( void )
{
	return( 1 );
}
