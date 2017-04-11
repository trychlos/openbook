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

#include "my/my-signal.h"

#include "api/ofa-igetter.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"

#define ISIGNALER_DATA                     "ofa-isignaler-data"
#define ISIGNALER_LAST_VERSION              1

/* a structure attached to the #ofaISignaler instance
 */
typedef struct {
	ofaIGetter *getter;
}
	sData;

/* signals defined here
 */
enum {
	/* non-UI related */
	BASE_NEW = 0,
	BASE_UPDATED,
	BASE_DELETABLE,
	BASE_DELETED,
	COLLECTION_RELOAD,
	DOSSIER_OPENED,
	DOSSIER_CLOSED,
	DOSSIER_CHANGED,
	DOSSIER_PREVIEW,
	PERIOD_CLOSING,
	PERIOD_CLOSED,
	EXE_DATES_CHANGED,
	STATUS_COUNT,
	STATUS_CHANGE,

	/* UI-related */
	UI_RESTART,
	MENU_AVAILABLE,
	PAGE_MANAGER_AVAILABLE,

	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ]     = { 0 };

static guint st_initializations         =   0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaISignalerInterface *klass );
static void     interface_base_finalize( ofaISignalerInterface *klass );
static gboolean on_deletable_default_handler( ofaISignaler *signaler, GObject *object );
static sData   *get_instance_data( const ofaISignaler *self );
static void     on_instance_finalized( sData *sdata, GObject *finalized_object );

/**
 * ofa_isignaler_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isignaler_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isignaler_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isignaler_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISignalerInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISignaler", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISignalerInterface *klass )
{
	static const gchar *thisfn = "ofa_isignaler_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaISignaler::ofa-signaler-base-new:
		 *
		 * The signal is emitted after a new ofoBase-derived object has
		 * been successfully inserted in the database. A connected handler
		 * may take advantage of this signal e.g. to update its own list
		 * of displayed objects.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							ofoBase     *object,
		 * 							gpointer     user_data );
		 */
		st_signals[ BASE_NEW ] = g_signal_new_class_handler(
					SIGNALER_BASE_NEW,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_OBJECT );

		/**
		 * ofaISignaler::ofa-signaler-base-updated:
		 *
		 * The signal is emitted just after an ofoBase-derived object
		 * has been successfully updated in the DBMS. A connected
		 * handler may take advantage of this signal e.g. for updating
		 * its own list of displayed objects, or for updating its
		 * internal links, or so.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							ofoBase     *object,
		 * 							const gchar *prev_id,
		 * 							gpointer     user_data );
		 */
		st_signals[ BASE_UPDATED ] = g_signal_new_class_handler(
					SIGNALER_BASE_UPDATED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_OBJECT, G_TYPE_STRING );

		/**
		 * ofaISignaler::ofa-signaler-base-is-deletable:
		 *
		 * The signal is emitted when the application wants to know if a
		 * particular ofoBase-derived object is deletable.
		 *
		 * Any handler returning %FALSE will stop the emission of this
		 * message, and abort the deleting of the object.
		 *
		 * Handler is of type:
		 * 		gboolean user_handler( ofaISignaler *signaler,
		 * 								ofoBase     *object,
		 * 								gpointer     user_data );
		 */
		st_signals[ BASE_DELETABLE ] = g_signal_new_class_handler(
					SIGNALER_BASE_IS_DELETABLE,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					G_CALLBACK( on_deletable_default_handler ),
					my_signal_accumulator_false_handled,
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_BOOLEAN,
					1,
					G_TYPE_OBJECT );

		/**
		 * ofaISignaler::ofa-signaler-base-deleted:
		 *
		 * The signal is emitted just after an ofoBase-object has been
		 * successfully deleted from the DBMS. A connected handler may
		 * take advantage of this signal e.g. for updating its own list
		 * of displayed objects.
		 *
		 * Note that the emitter of this signal should attach a new reference
		 * to the deleted object in order to keep it alive during for the
		 * handlers execution.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							ofoBase     *object,
		 * 							gpointer     user_data );
		 */
		st_signals[ BASE_DELETED ] = g_signal_new_class_handler(
					SIGNALER_BASE_DELETED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_OBJECT );

		/**
		 * ofaISignaler::ofa-signaler-collection-reload:
		 *
		 * The signal is emitted when such an update has been made in
		 * the DBMS that it is considered easier for a connected handler
		 * just to reload the whole dataset if this later whishes keep
		 * synchronized with the database.
		 *
		 * This signal is so less an information signal that an action
		 * hint.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							GType        class_type,
		 * 							gpointer     user_data );
		 */
		st_signals[ COLLECTION_RELOAD ] = g_signal_new_class_handler(
					SIGNALER_COLLECTION_RELOAD,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_GTYPE );

		/**
		 * ofaISignaler::ofa-signaler-dossier-opened:
		 *
		 * This signal is sent on the signaler jusr after a dossier has
		 * been opened.
		 *
		 * At this time, the #ofaHub object has a connection on the
		 * dossier; dossier properties have been read from the DBMS.
		 * Dossier settings have not yet been remediated.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							gpointer     user_data );
		 */
		st_signals[ DOSSIER_OPENED ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_OPENED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );

		/**
		 * ofaISignaler::ofa-signaler-dossier-closed:
		 *
		 * This signal is sent on the signaler when it is about to
		 * (just before) close a dossier.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							gpointer     user_data );
		 */
		st_signals[ DOSSIER_CLOSED ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_CLOSED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );

		/**
		 * ofaISignaler::ofa-signaler-dossier-changed:
		 *
		 * This signal is sent on the signaler when the properties of
		 * the currently opened dossier have been modified (or may have
		 * been modified) by the user. This strictly corresponds to the
		 * OFA_T_DOSSIER table content.
		 *
		 * The #ofaHub itself is connected to the signal and is the very
		 * first handler triggered. It takes advantage of this signal to
		 * remediate the dossier settings.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							gpointer     user_data );
		 */
		st_signals[ DOSSIER_CHANGED ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_CHANGED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );

		/**
		 * ofaISignaler::ofa-signaler-dossier-preview:
		 * @uri: [allow-none]:
		 *
		 * This signal is sent on the signaler to set a new background image.
		 * The sender is responsible to restore the original image if the
		 * user cancels the update.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							const gchar *uri,
		 * 							gpointer     user_data );
		 */
		st_signals[ DOSSIER_PREVIEW ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_PREVIEW,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );

		/**
		 * ofaISignaler::ofa-signaler-dossier-period-closing:
		 * @closing_ind: an indicator of the period being closed.
		 * @closing_date: the closing date.
		 *
		 * This signal is sent on the signaler when a period is about to
		 * be closed.
		 *
		 * For the closing of an intermediate period as well as when
		 * closing an exercice, the signal is sent in the very beginning,
		 * before any other work could have take place.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler       *signaler,
		 * 							ofeSignalerClosing closing_ind,
		 * 							const GDate       *closing_date,
		 * 							gpointer           user_data );
		 */
		st_signals[ PERIOD_CLOSING ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_PERIOD_CLOSING,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_UINT, G_TYPE_POINTER );

		/**
		 * ofaISignaler::ofa-signaler-dossier-period-closed:
		 * @closing_ind: an indicator of the period being closed.
		 * @closed_date: the closing date.
		 *
		 * This signal is sent on the signaler when a period has just
		 * been closed.
		 *
		 * For the closing of an intermediate period, the signal is sent
		 * at the end of the work: ledgers have been closed at @closed_date,
		 * and the accounts balances have been archived if asked for.
		 *
		 * When closing an exercice, the signal is sent at the very end
		 * of the operation, when the new exercice has been opened and
		 * initialized. The @closed_date is then a past date here.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler       *signaler,
		 * 							ofeSignalerClosing closing_ind,
		 * 							const GDate       *closing_date,
		 * 							gpointer           user_data );
		 */
		st_signals[ PERIOD_CLOSED ] = g_signal_new_class_handler(
					SIGNALER_DOSSIER_PERIOD_CLOSED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_UINT, G_TYPE_POINTER );

		/**
		 * ofaISignaler::ofa-signaler-exercice-dates-changed:
		 *
		 * Beginning and or ending exercice dates of the dossier have
		 * been modified.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler
		 * 							const GDate *prev_begin,
		 * 							const GDate *prev_end,
		 * 							gpointer     user_data );
		 */
		st_signals[ EXE_DATES_CHANGED ] = g_signal_new_class_handler(
					SIGNALER_EXERCICE_DATES_CHANGED,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_POINTER, G_TYPE_POINTER );

		/**
		 * ofaISignaler::ofa-signaler-entry-change-count:
		 * @new_period: the targeted period indicator, or -1 if does not change.
		 * @new_status: the targeted status, or -1 if does not change.
		 * @count: the count of entries to be changed.
		 *
		 * This signal is sent on the signaler before each batch of
		 * entry period or status changes.
		 *
		 * A signal handler may so initialize e.g. a progression bar
		 * about the period/status change.
		 *
		 * The arguments may be read as: I am about to change the period
		 * or the status of '@count' entries to '@new_period' (-1 if
		 * unchanged) and @new_status (-1 if unchanged).
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 *							gint         new_period,
		 *							gint         new_status,
		 *							gulong       count,
		 * 							gpointer     user_data );
		 */
		st_signals[ STATUS_COUNT ] = g_signal_new_class_handler(
					SIGNALER_CHANGE_COUNT,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_INT, G_TYPE_INT, G_TYPE_ULONG );

		/**
		 * ofaISignaler::ofa-signaler-entry-status-change:
		 * @entry: the entry to be changed.
		 * @prev_period: the original period indicator, or -1 if does not change.
		 * @prev_status: the original status, or -1 of does not change.
		 * @new_period: the new period indicator, to be ignored if @prev_period is -1.
		 * @new_status: the new status, to be ignored if @prev_status is -1.
		 *
		 * This signal is sent of the @signaler to ask an antry to change its
		 * period indicator and/or its status.
		 *
		 * The #ofoEntry class signal handler will update the @entry with
		 * its new @new_period period indicator and its new @new_status status,
		 * and update the database accordingly.
		 *
		 * Other signal handlers may, e.g. update balances, progression
		 * bars, and so on.
		 *
		 * This is an ACTION signal.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler    *signaler,
		 * 							const ofoEntry *entry
		 *							gint            prev_period,
		 *							gint            prev_status,
		 *							gint            new_period,
		 *							gint            new_status,
		 * 							gpointer        user_data );
		 */
		st_signals[ STATUS_CHANGE ] = g_signal_new_class_handler(
					SIGNALER_PERIOD_STATUS_CHANGE,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					5,
					G_TYPE_OBJECT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT );

		/**
		 * ofaISignaler::ofa-signaler-ui-restart:
		 *
		 * The signal is emitted to ask the application to restart the
		 * user interface.
		 *
		 * Use case: taking into account new user preferences.
		 *
		 * This is an ACTION signal.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							gpointer     user_data );
		 */
		st_signals[ UI_RESTART ] = g_signal_new_class_handler(
					SIGNALER_UI_RESTART,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );

		/**
		 * ofaISignaler::ofa-signaler-menu-available:
		 *
		 * The signal is emitted each time a new menu has been registered
		 * by the #myMenuManager.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler *signaler,
		 * 							const gchar *scope,
		 * 							GActionMap  *action_map,
		 * 							gpointer     user_data );
		 */
		st_signals[ MENU_AVAILABLE ] = g_signal_new_class_handler(
					SIGNALER_MENU_AVAILABLE,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_STRING, G_TYPE_POINTER );

		/**
		 * ofaISignaler::ofa-signaler-page-manager-available:
		 *
		 * The signal is emitted when the #ofaIPageManager is available
		 * to register new themes.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaISignaler     *signaler,
		 * 							ofaIPageManager *manager,
		 * 							gpointer         user_data );
		 */
		st_signals[ PAGE_MANAGER_AVAILABLE ] = g_signal_new_class_handler(
					SIGNALER_PAGE_MANAGER_AVAILABLE,
					OFA_TYPE_ISIGNALER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_POINTER );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISignalerInterface *klass )
{
	static const gchar *thisfn = "ofa_isignaler_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isignaler_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isignaler_get_interface_last_version( void )
{
	return( ISIGNALER_LAST_VERSION );
}

/**
 * ofa_isignaler_get_interface_version:
 * @instance: this #ofaISignaler instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_isignaler_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISIGNALER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISignalerInterface * ) iface )->get_interface_version ){
		version = (( ofaISignalerInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISignaler::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_isignaler_init_signaling_system:
 * @signaler: this #ofaISignaler instance.
 * @getter: a #ofaIGetter instance.
 *
 * Initialize the #ofaISignaler signaling system, letting all
 * #ofaISignalable known instances to connect themselves to this
 * system.
 *
 * Be sure object class handlers are connected to the #ofaISignaler
 * signaling system, as they may be needed before the class has the
 * opportunity to initialize itself.
 *
 * Example of a use case: the intermediate closing by ledger may be run
 * without having first loaded the accounts, but the accounts should be
 * connected in order to update themselves.
 *
 * This method must be called after having registered core types.
 */
void
ofa_isignaler_init_signaling_system( ofaISignaler *signaler, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_isignaler_init_signaling_system";
	GList *list, *it;
	sData *sdata;

	g_debug( "%s: signaler=%p, getter=%p", thisfn, ( void * ) signaler, ( void * ) getter );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_instance_data( signaler );
	sdata->getter = getter;

	list = ofa_igetter_get_for_type( getter, OFA_TYPE_ISIGNALABLE );

	for( it=list ; it ; it=it->next ){
		ofa_isignalable_connect_to( G_OBJECT_TYPE( it->data ), signaler );
	}

	g_list_free( list );
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
on_deletable_default_handler( ofaISignaler *signaler, GObject *object )
{
	static const gchar *thisfn = "ofa_isignaler_on_deletable_default_handler";

	g_debug( "%s: signaler=%p, object=%p (%s)",
			thisfn, ( void * ) signaler, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	return( TRUE );
}

/**
 * ofa_isignaler_get_getter:
 * @instance: this #ofaISignaler instance.
 *
 * Returns: the #ofaIGetter instance which was set at initialization time.
 */
ofaIGetter *
ofa_isignaler_get_getter( ofaISignaler *signaler )
{
	sData *sdata;

	g_return_val_if_fail( signaler && OFA_IS_ISIGNALER( signaler ), NULL );

	sdata = get_instance_data( signaler );

	return( sdata->getter );
}

/**
 * ofa_isignaler_disconnect_handlers:
 * @signaler: this #ofaISignaler instance.
 * @handlers: a #GList of the handler identifiers got when connecting
 *  to the #ofaISignaler signaling system
 *
 * Disconnect the specified @handlers from the signaling system.
 * Free the list and clear the @handlers pointer.
 *
 * Rationale: an object should disconnect its signals when it disappears
 * while the signal emitter may still occurs, i.e. to prevent the signal
 * emitter to keep sending signals to a now-disappeared object.
 */
void
ofa_isignaler_disconnect_handlers( ofaISignaler *signaler, GList **handlers )
{
	static const gchar *thisfn = "ofa_isignaler_disconnect_handlers";
	GList *it;

	g_debug( "%s: signaler=%p, handlers=%p (count=%d)",
			thisfn, ( void * ) signaler, ( void * ) handlers, g_list_length( *handlers ));

	for( it=( *handlers ) ; it ; it=it->next ){
		g_signal_handler_disconnect( signaler, ( gulong ) it->data );
	}

	if( *handlers ){
		g_list_free( *handlers );
		*handlers = NULL;
	}
}

static sData *
get_instance_data( const ofaISignaler *self )
{
	sData *sdata;

	sdata = ( sData * ) g_object_get_data( G_OBJECT( self ), ISIGNALER_DATA );

	if( !sdata ){
		sdata = g_new0( sData, 1 );
		g_object_set_data( G_OBJECT( self ), ISIGNALER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sData *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_isignaler_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p", thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata );
}
