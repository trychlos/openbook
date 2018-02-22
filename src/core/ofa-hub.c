/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-icollector.h"
#include "my/my-isettings.h"
#include "my/my-scope-mapper.h"
#include "my/my-settings.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-dossier-collection.h"
#include "api/ofa-dossier-store.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-openbook-props.h"
#include "api/ofa-prefs.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-class.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-counters.h"
#include "api/ofo-paimean.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-account-balance.h"

/* private instance data
 */
typedef struct {
	gboolean               dispose_has_run;

	/* runtime
	 */
	GApplication          *application;
	gchar                 *argv_0;
	ofaExtenderCollection *extenders_collection;
	ofaDossierCollection  *dossiers_collection;
	GList                 *core_objects;
	mySettings            *auth_settings;
	mySettings            *dossier_settings;
	ofaPrefs              *user_prefs;
	mySettings            *user_settings;
	ofaOpenbookProps      *openbook_props;
	gchar                 *runtime_dir;
	ofaDossierStore       *dossier_store;

	/* UI related
	 */
	GtkApplicationWindow  *main_window;
	ofaIPageManager       *page_manager;
	myScopeMapper         *scope_mapper;

	/* dossier
	 */
	ofaIDBConnect         *connect;
	ofoDossier            *dossier;
	gboolean               read_only;
	ofoCounters           *counters;
}
	ofaHubPrivate;

static void                   hub_register_types( ofaHub *self );
static void                   hub_setup_settings( ofaHub *self );
static void                   on_properties_dossier_changed( ofaISignaler *signaler, void *empty );
static gboolean               remediate_dossier_settings( ofaIGetter *getter );
static void                   icollector_iface_init( myICollectorInterface *iface );
static guint                  icollector_get_interface_version( void );
static void                   igetter_iface_init( ofaIGetterInterface *iface );
static GApplication          *igetter_get_application( const ofaIGetter *getter );
static myISettings           *igetter_get_auth_settings( const ofaIGetter *getter );
static myICollector          *igetter_get_collector( const ofaIGetter *getter );
static ofaDossierCollection  *igetter_get_dossier_collection( const ofaIGetter *getter );
static myISettings           *igetter_get_dossier_settings( const ofaIGetter *getter );
static ofaDossierStore       *igetter_get_dossier_store( const ofaIGetter *getter );
static ofaExtenderCollection *igetter_get_extender_collection( const ofaIGetter *getter );
static GList                 *igetter_get_for_type( const ofaIGetter *getter, GType type );
static ofaHub                *igetter_get_hub( const ofaIGetter *getter );
static ofaOpenbookProps      *igetter_get_openbook_props( const ofaIGetter *getter );
static const gchar           *igetter_get_runtime_dir( const ofaIGetter *getter );
static ofaISignaler          *igetter_get_signaler( const ofaIGetter *getter );
static ofaPrefs              *igetter_get_user_prefs( const ofaIGetter *getter );
static myISettings           *igetter_get_user_settings( const ofaIGetter *getter );
static GtkApplicationWindow  *igetter_get_main_window( const ofaIGetter *getter );
static ofaIPageManager       *igetter_get_page_manager( const ofaIGetter *getter );
static myScopeMapper         *igetter_get_scope_mapper( const ofaIGetter *getter );
static void                   isignaler_iface_init( ofaISignalerInterface *iface );

G_DEFINE_TYPE_EXTENDED( ofaHub, ofa_hub, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaHub )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTOR, icollector_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IGETTER, igetter_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALER, isignaler_iface_init ))

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

/* (openbook:21666): Gtk-WARNING **: A floating object was finalized.
 * This means that someone called g_object_unref() on an object that
 * had only a floating reference; the initial floating reference is not
 * owned by anyone and must be removed with g_object_ref_sink().
 */
static void
free_core_object( GObject *object )
{
	if( g_object_is_floating( object )){
		g_object_ref_sink( object );
	}
	g_object_unref( object );
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

		/* unref object members here,
		 * ending with collection, ofaExtenderCollection at very last */

		g_clear_object( &priv->auth_settings );
		g_clear_object( &priv->dossier_settings );
		g_clear_object( &priv->user_prefs );
		g_clear_object( &priv->user_settings );
		g_clear_object( &priv->openbook_props );
		g_clear_object( &priv->scope_mapper );
		g_clear_object( &priv->dossier_store );

		g_list_free_full( priv->core_objects, ( GDestroyNotify ) free_core_object );

		g_clear_object( &priv->dossiers_collection );
		g_clear_object( &priv->extenders_collection );
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
	priv->extenders_collection = NULL;
	priv->dossiers_collection = NULL;
}

static void
ofa_hub_class_init( ofaHubClass *klass )
{
	static const gchar *thisfn = "ofa_hub_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = hub_dispose;
	G_OBJECT_CLASS( klass )->finalize = hub_finalize;
}

/**
 * ofa_hub_new:
 *
 * Allocates and initializes the #ofaHub object of the application.
 *
 * Returns: a new #ofaHub object.
 */
ofaHub *
ofa_hub_new( void )
{
	static const gchar *thisfn = "ofa_hub_new";
	ofaHub *hub;
	ofaHubPrivate *priv;

	g_debug( "%s:", thisfn );

	hub = g_object_new( OFA_TYPE_HUB, NULL );

	priv = ofa_hub_get_instance_private( hub );

	/* must have extenders_collection before the ISignaler be able
	 * to initialize itself */
	priv->extenders_collection = ofa_extender_collection_new( OFA_IGETTER( hub ), PKGLIBDIR );

	ofa_box_register_types();			/* register types */
	hub_register_types( hub );

	ofa_isignaler_init_signaling_system( OFA_ISIGNALER( hub ), OFA_IGETTER( hub ));

	hub_setup_settings( hub );
	priv->user_prefs = ofa_prefs_new( OFA_IGETTER( hub ));
	priv->dossiers_collection = ofa_dossier_collection_new( OFA_IGETTER( hub ));
	priv->openbook_props = ofa_openbook_props_new( OFA_IGETTER( hub ));
	priv->scope_mapper = my_scope_mapper_new();
	priv->dossier_store = ofa_dossier_store_new( OFA_IGETTER( hub ));

	/* after ofaDossierStore initialization, all dossiers and exercices
	 *  meta have a ref_count=2 (which is fine) */
	g_debug( "%s: dumping the dossiers collection after store initialization", thisfn );
	ofa_dossier_collection_dump( priv->dossiers_collection );

	/* remediate dossier settings when properties change */
	g_signal_connect( OFA_ISIGNALER( hub ), SIGNALER_DOSSIER_CHANGED, G_CALLBACK( on_properties_dossier_changed ), NULL );

	return( hub );
}

/*
 * hub_register_types:
 *
 * Registers all #ofoBase derived types provided by the core library
 * (aka "core types") so that @hub will be able to dynamically request
 * them on demand.
 *
 * This method, plus #ofa_igetter_get_for_type() below, are for example
 * used to get a dynamic list of importable or exportable types, and
 * more generally to be able to get a dynamic list of any known (and
 * registered) type.
 *
 * Plugins-provided types (aka "plugin types") do not need to register
 * here. It is enough they implement the desired interface to be
 * dynamically requested on demand.
 */
static void
hub_register_types( ofaHub *self )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( self );

	priv->core_objects = NULL;

	/* this is needed to be able to export from ofaTVBin */
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFA_TYPE_TVBIN, "ofa-tvbin-getter", self, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFA_TYPE_ACCOUNT_BALANCE, "ofa-getter", self, NULL ));

	/* it is or may be ISignalable */
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CONCIL, NULL ));

	/* this is also the order of IExportable/IImportable classes in the
	 * assistants: do not change this order */
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_DOSSIER, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CLASS, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_CURRENCY, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_ACCOUNT, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_LEDGER, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_PAIMEAN, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_OPE_TEMPLATE, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_RATE, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_ENTRY, NULL ));
	priv->core_objects = g_list_prepend( priv->core_objects, g_object_new( OFO_TYPE_BAT, NULL ));

	/* plugins exportables/importables will come at the end of the list */
}

static void
hub_setup_settings( ofaHub *self )
{
	ofaHubPrivate *priv;
	gchar *name;

	priv = ofa_hub_get_instance_private( self );

	priv->auth_settings = my_settings_new_user_config( "auth.conf", "OFA_AUTH_CONF" );

	priv->dossier_settings = my_settings_new_user_config( "dossier.conf", "OFA_DOSSIER_CONF" );

	name = g_strdup_printf( "%s.conf", PACKAGE );
	priv->user_settings = my_settings_new_user_config( name, "OFA_USER_CONF" );
	g_free( name );
}

/**
 * ofa_hub_set_application:
 * @hub: this #ofaHub instance.
 * @application: the #GApplication which has created and owns this @hub.
 *
 * Set the @application.
 */
void
ofa_hub_set_application( ofaHub *hub, GApplication *application )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( application && G_IS_APPLICATION( application ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->application = application;
}

/**
 * ofa_hub_set_runtime_command:
 * @hub: this #ofaHub instance.
 * @argv_0: the first argument of the command-line.
 *
 * Set the @argv_0.
 *
 * Simultaneously compute the runtime directory (the directory from
 * which the current program has been run).
 */
void
ofa_hub_set_runtime_command( ofaHub *hub, const gchar *argv_0 )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( my_strlen( argv_0 ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->argv_0 = g_strdup( argv_0 );
	priv->runtime_dir = g_path_get_dirname( priv->argv_0 );
}

/**
 * ofa_hub_set_main_window:
 * @hub: this #ofaHub instance.
 * @main_window: the main window of the application.
 *
 * Set the @main_window.
 */
void
ofa_hub_set_main_window( ofaHub *hub, GtkApplicationWindow *main_window )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( main_window && GTK_IS_APPLICATION_WINDOW( main_window ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->main_window = main_window;
}

/**
 * ofa_hub_set_page_manager:
 * @hub: this #ofaHub instance.
 * @page_manager: the page manager of the application.
 *
 * Set the @page_manager.
 */
void
ofa_hub_set_page_manager( ofaHub *hub, ofaIPageManager *page_manager )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	g_return_if_fail( page_manager && OFA_IS_IPAGE_MANAGER( page_manager ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	priv->page_manager = page_manager;
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
 * In user interface mode, see #ofa_dossier_open_run_modal() function.
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

	if( ofa_idbmodel_update( OFA_IGETTER( hub ), parent )){
		priv->dossier = ofo_dossier_new( OFA_IGETTER( hub ));
		if( priv->dossier ){
			priv->read_only = read_only;
			g_signal_emit_by_name( OFA_ISIGNALER( hub ), SIGNALER_DOSSIER_OPENED );
			ok = TRUE;
			if( remediate_settings ){
				g_signal_emit_by_name( OFA_ISIGNALER( hub ), SIGNALER_DOSSIER_CHANGED );
			}
			priv->counters = ofo_counters_new( OFA_IGETTER( hub ));
		}
	}

	if( !ok ){
		ofa_hub_close_dossier( hub );
	}

	return( ok );
}

static void
on_properties_dossier_changed( ofaISignaler *signaler, void *empty )
{
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	remediate_dossier_settings( getter );
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
remediate_dossier_settings( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_hub_remediate_dossier_settings";
	ofaHub *hub;
	ofaIDBConnect *cnx;
	ofoDossier *dossier;
	gboolean db_current, settings_current, remediated;
	const GDate *db_begin, *db_end, *settings_begin, *settings_end;
	ofaIDBExerciceMeta *period;
	gchar *sdbbegin, *sdbend, *ssetbegin, *ssetend;

	remediated = FALSE;

	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	cnx = ofa_hub_get_connect( hub );

	/* data from db */
	db_current = ofo_dossier_is_current( dossier );
	db_begin = ofo_dossier_get_exe_begin( dossier );
	db_end = ofo_dossier_get_exe_end( dossier );

	/* data from settings */
	period = ofa_idbconnect_get_exercice_meta( cnx );
	settings_current = ofa_idbexercice_meta_get_current( period );
	settings_begin = ofa_idbexercice_meta_get_begin_date( period );
	settings_end = ofa_idbexercice_meta_get_end_date( period );

	sdbbegin = my_date_to_str( db_begin, MY_DATE_SQL );
	sdbend = my_date_to_str( db_end, MY_DATE_SQL );
	ssetbegin = my_date_to_str( settings_begin, MY_DATE_SQL );
	ssetend = my_date_to_str( settings_end, MY_DATE_SQL );

	g_debug( "%s: db_current=%s, db_begin=%s, db_end=%s, settings_current=%s, settings_begin=%s, settings_end=%s",
			thisfn,
			db_current ? "True":"False", sdbbegin, sdbend,
			settings_current ? "True":"False", ssetbegin, ssetend );

	g_free( sdbbegin );
	g_free( sdbend );
	g_free( ssetbegin );
	g_free( ssetend );

	/* update settings if not equal */
	if( db_current != settings_current ||
			my_date_compare_ex( db_begin, settings_begin, TRUE ) != 0 ||
			my_date_compare_ex( db_end, settings_end, FALSE ) != 0 ){

		g_debug( "%s: remediating settings", thisfn );

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
 * ofa_hub_get_counters:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofoCounters object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
ofoCounters *
ofa_hub_get_counters( ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->counters );
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
	static const gchar *thisfn = "ofa_hub_is_writable_dossier";
	ofaHubPrivate *priv;
	gboolean dossier_is_current, is_writable;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	priv = ofa_hub_get_instance_private( hub );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	dossier_is_current = ofo_dossier_is_current( priv->dossier );

	is_writable = priv->dossier &&
			OFO_IS_DOSSIER( priv->dossier ) &&
			dossier_is_current &&
			!priv->read_only;

	g_debug( "%s: dossier_is_current=%s, opened_read_only=%s, is_writable=%s",
			thisfn, dossier_is_current ? "True":"False",
			priv->read_only ? "True":"False", is_writable ? "True":"False" );

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
	static const gchar *thisfn = "ofa_hub_close_dossier";
	ofaHubPrivate *priv;

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_hub_get_instance_private( hub );

	g_return_if_fail( !priv->dispose_has_run );

	if( priv->dossier || priv->connect ){

		/* emit the closing signal
		 *  at this time all datas are alive and valid */
		g_signal_emit_by_name( OFA_ISIGNALER( hub ), SIGNALER_DOSSIER_CLOSED );

		g_clear_object( &priv->connect );
		g_clear_object( &priv->counters );
		g_clear_object( &priv->dossier );

		my_icollector_free_all( ofa_igetter_get_collector( OFA_IGETTER( hub )));
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
	ofaExtenderCollection *extenders_collection;
	GList *importers, *it;

	found = NULL;
	extenders_collection = ofa_igetter_get_extender_collection( OFA_IGETTER( hub ));
	importers = ofa_extender_collection_get_for_type( extenders_collection, OFA_TYPE_IIMPORTER );

	for( it=importers ; it ; it=it->next ){
		if( ofa_iimporter_is_willing_to( OFA_IIMPORTER( it->data ), OFA_IGETTER( hub ), uri, type )){
			found = OFA_IIMPORTER( it->data );
			break;
		}
	}

	g_list_free( importers );

	return( found ? g_object_ref( G_OBJECT( found )) : NULL );
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

/*
 * ofaIGetter interface management
 */
static void
igetter_iface_init( ofaIGetterInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_igetter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	/* non-UI related */
	iface->get_application = igetter_get_application;
	iface->get_auth_settings = igetter_get_auth_settings;
	iface->get_collector = igetter_get_collector;
	iface->get_dossier_collection = igetter_get_dossier_collection;
	iface->get_dossier_settings = igetter_get_dossier_settings;
	iface->get_dossier_store = igetter_get_dossier_store;
	iface->get_extender_collection = igetter_get_extender_collection;
	iface->get_for_type = igetter_get_for_type;
	iface->get_hub = igetter_get_hub;
	iface->get_openbook_props = igetter_get_openbook_props;
	iface->get_runtime_dir = igetter_get_runtime_dir;
	iface->get_signaler = igetter_get_signaler;
	iface->get_user_prefs = igetter_get_user_prefs;
	iface->get_user_settings = igetter_get_user_settings;

	/* ui-related */
	iface->get_main_window = igetter_get_main_window;
	iface->get_page_manager = igetter_get_page_manager;
	iface->get_scope_mapper = igetter_get_scope_mapper;
}

static GApplication *
igetter_get_application( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->application );
}

static myISettings *
igetter_get_auth_settings( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( MY_ISETTINGS( priv->auth_settings ));
}

static myICollector *
igetter_get_collector( const ofaIGetter *getter )
{
	return( MY_ICOLLECTOR( getter ));
}

static ofaDossierCollection *
igetter_get_dossier_collection( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->dossiers_collection );
}

static myISettings *
igetter_get_dossier_settings( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( MY_ISETTINGS( priv->dossier_settings ));
}

static ofaDossierStore *
igetter_get_dossier_store( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->dossier_store );
}

static ofaExtenderCollection *
igetter_get_extender_collection( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->extenders_collection );
}

static GList *
igetter_get_for_type( const ofaIGetter *getter, GType type )
{
	ofaHubPrivate *priv;
	GList *objects, *it, *extender_objects;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	/* requests first the objects registered from core library */
	objects = NULL;
	for( it=priv->core_objects ; it ; it=it->next ){
		if( G_TYPE_CHECK_INSTANCE_TYPE( G_OBJECT( it->data ), type )){
			objects = g_list_prepend( objects, it->data );
		}
	}

	/* requests then same type from loaded modules */
	extender_objects = ofa_extender_collection_get_for_type( priv->extenders_collection, type );
	for( it=extender_objects ; it ; it=it->next ){
		objects = g_list_append( objects, it->data );
	}
	g_list_free( extender_objects );

	return( objects );
}

static ofaHub *
igetter_get_hub( const ofaIGetter *getter )
{
	return( OFA_HUB( getter ));
}

static ofaOpenbookProps *
igetter_get_openbook_props( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->openbook_props );
}

static const gchar *
igetter_get_runtime_dir( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return(( const gchar * ) priv->runtime_dir );
}

static ofaISignaler *
igetter_get_signaler( const ofaIGetter *getter )
{
	return( OFA_ISIGNALER( igetter_get_hub( getter )));
}

static ofaPrefs *
igetter_get_user_prefs( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->user_prefs );
}

static myISettings *
igetter_get_user_settings( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( MY_ISETTINGS( priv->user_settings ));
}

static GtkApplicationWindow *
igetter_get_main_window( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->main_window );
}

/*
 * the themes are managed by the main window
 */
static ofaIPageManager *
igetter_get_page_manager( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->page_manager );
}

static myScopeMapper *
igetter_get_scope_mapper( const ofaIGetter *getter )
{
	ofaHubPrivate *priv;

	priv = ofa_hub_get_instance_private( OFA_HUB( getter ));

	return( priv->scope_mapper );
}

/*
 * ofaISignaler interface management
 */
static void
isignaler_iface_init( ofaISignalerInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_isignaler_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}
