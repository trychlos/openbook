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

#ifndef __OPENBOOK_API_OFA_IGETTER_H__
#define __OPENBOOK_API_OFA_IGETTER_H__

/**
 * SECTION: igetter
 * @title: ofaIGetter
 * @short_description: The IGetter Interface
 * @include: openbook/ofa-igetter.h
 *
 * The #ofaIGetter interface lets plugins, external modules and more
 * generally all parts of application access to some global interest
 * variables.
 *
 * The #ofaIGetter interface is mainly the external API to the #ofaHub
 * object of the application.
 *
 * As such, it manages some UI-related and some not-UI-related properties.
 * In a command-line program without user interface, the caller should
 * be prepared to not have all properties set.
 */

#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "my/my-icollector.h"
#include "my/my-isettings.h"
#include "my/my-scope-mapper.h"

#include "api/ofa-dossier-collection-def.h"
#include "api/ofa-dossier-store-def.h"
#include "api/ofa-extender-collection-def.h"
#include "api/ofa-hub-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-ipage-manager-def.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-openbook-props-def.h"
#include "api/ofo-counter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IGETTER                      ( ofa_igetter_get_type())
#define OFA_IGETTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IGETTER, ofaIGetter ))
#define OFA_IS_IGETTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IGETTER ))
#define OFA_IGETTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IGETTER, ofaIGetterInterface ))

#if 0
typedef struct _ofaIGetter                    ofaIGetter;
#endif

/**
 * ofaIGetterInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_application: [should]: return the #GApplication instance.
 * @get_auth_settings: [should]: return the authentification settings.
 * @get_collector: [should]: return the #myICollector object.
 * @get_counters: [should]: return the #ofoCounter singleton.
 * @get_dossier_collection: [should]: return the #ofaDossierCollection object.
 * @get_dossier_settings: [should]: return the dossier settings.
 * @get_dossier_store: [should]: return the dossier store.
 * @get_extender_collection: [should]: return the #ofaExtenderCollection object.
 * @get_for_type: [should]: return the list of objects for a type..
 * @get_hub: [should]: return the #ofaHub instance.
 * @get_openbook_props: [should]: return the #ofaOpenbookProps instance.
 * @get_runtime_dir: [should]: return the running directory.
 * @get_signaler: [should]: return the #ofaISignaler instance.
 * @get_user_settings: [should]: return the user settings.
 * @get_main_window: [should]: return the #ofaMainWindow instance.
 * @get_page_manager: [should]: return the #ofaIPageManager instance.
 * @get_scope_mapper: [should]: return the #myScopeMapper instance.
 *
 * This defines the interface that an #ofaIGetter must/should/may implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint                   ( *get_interface_version )  ( void );

	/*** instance-wide non-UI related ***/
	/**
	 * get_application:
	 * @instance: this #ofaIGetter instance.
	 *
	 * Returns: the GApplication.
	 *
	 * Since: version 1
	 */
	GApplication *          ( *get_application )        ( const ofaIGetter *getter );

	/**
	 * get_auth_settings:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #myISettings interface which manages the #ofaIAuth data.
	 *
	 * Since: version 1
	 */
	myISettings *           ( *get_auth_settings )      ( const ofaIGetter *getter );

	/**
	 * get_collector:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #myICollector interface.
	 *
	 * Since: version 1
	 */
	myICollector *          ( *get_collector )          ( const ofaIGetter *getter );

	/**
	 * get_counters:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #ofoCounter singleton.
	 *
	 * Since: version 1
	 */
	ofoCounter *            ( *get_counters )           ( ofaIGetter *getter );

	/**
	 * get_dossier_collection:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the dossier collection.
	 *
	 * Since: version 1
	 */
	ofaDossierCollection *  ( *get_dossier_collection ) ( const ofaIGetter *getter );

	/**
	 * get_dossier_settings:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #myISettings interface to the dossier settings.
	 *
	 * Since: version 1
	 */
	myISettings *           ( *get_dossier_settings )   ( const ofaIGetter *getter );

	/**
	 * get_dossier_store:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #ofaDossierStore instance.
	 *
	 * Since: version 1
	 */
	ofaDossierStore *       ( *get_dossier_store )      ( const ofaIGetter *getter );

	/**
	 * get_extender_collection:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the extenders collection.
	 *
	 * Since: version 1
	 */
	ofaExtenderCollection * ( *get_extender_collection )( const ofaIGetter *getter );

	/**
	 * get_for_type:
	 * @getter: this #ofaIGetter getter.
	 * @type: the requested #GType.
	 *
	 * Returns: the list of objects which implements the @type.
	 *
	 * Since: version 1
	 */
	GList *                 ( *get_for_type )           ( const ofaIGetter *getter,
																GType type );

	/**
	 * get_hub:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the main hub object of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	ofaHub *                ( *get_hub )                ( const ofaIGetter *getter );

	/**
	 * get_openbook_props:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #ofaOpenbookProps object.
	 *
	 * Since: version 1
	 */
	ofaOpenbookProps *      ( *get_openbook_props )     ( const ofaIGetter *getter );

	/**
	 * get_runtime_dir:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the runtime directory.
	 *
	 * Since: version 1
	 */
	const gchar *           ( *get_runtime_dir )        ( const ofaIGetter *getter );

	/**
	 * get_signaler:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #ofaISignaler instance.
	 *
	 * Since: version 1
	 */
	ofaISignaler *          ( *get_signaler )           ( const ofaIGetter *getter );

	/**
	 * get_user_settings:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the #myISettings interface which manages the user preferences.
	 *
	 * Since: version 1
	 */
	myISettings *           ( *get_user_settings )      ( const ofaIGetter *getter );

	/*** instance-wide UI related ***/
	/**
	 * get_main_window:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the main window of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	GtkApplicationWindow *  ( *get_main_window )        ( const ofaIGetter *getter );

	/**
	 * get_page_manager:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the page manager of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	ofaIPageManager *      ( *get_page_manager )        ( const ofaIGetter *getter );

	/**
	 * get_scope_mapper:
	 * @getter: this #ofaIGetter getter.
	 *
	 * Returns: the instanciated #myScopeMapper, or %NULL.
	 *
	 * Since: version 1
	 */
	myScopeMapper *        ( *get_scope_mapper )        ( const ofaIGetter *getter );
}
	ofaIGetterInterface;

/**
 * The group name for user preferences.
 */
#define HUB_USER_SETTINGS_GROUP         "General"

/**
 * The default decimals count for a rate.
 * The default decimals count for an amount.
 */
#define HUB_DEFAULT_DECIMALS_AMOUNT      2
#define HUB_DEFAULT_DECIMALS_RATE        3

/*
 * Interface-wide
 */
GType                  ofa_igetter_get_type                  ( void );

guint                  ofa_igetter_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint                  ofa_igetter_get_interface_version     ( GType type );

/*
 * Instance-wide non-UI related
 */
GApplication          *ofa_igetter_get_application           ( const ofaIGetter *getter );

myISettings           *ofa_igetter_get_auth_settings         ( const ofaIGetter *getter );

myICollector          *ofa_igetter_get_collector             ( const ofaIGetter *getter );

ofoCounter            *ofa_igetter_get_counters              ( ofaIGetter *getter );

ofaDossierCollection  *ofa_igetter_get_dossier_collection    ( const ofaIGetter *getter );

myISettings           *ofa_igetter_get_dossier_settings      ( const ofaIGetter *getter );

ofaDossierStore       *ofa_igetter_get_dossier_store         ( const ofaIGetter *getter );

ofaExtenderCollection *ofa_igetter_get_extender_collection   ( const ofaIGetter *getter );

GList                 *ofa_igetter_get_for_type              ( const ofaIGetter *getter,
																	GType type );

ofaHub                *ofa_igetter_get_hub                   ( const ofaIGetter *getter );

ofaOpenbookProps      *ofa_igetter_get_openbook_props        ( const ofaIGetter *getter );

const gchar           *ofa_igetter_get_runtime_dir           ( const ofaIGetter *getter );

ofaISignaler          *ofa_igetter_get_signaler              ( const ofaIGetter *getter );

myISettings           *ofa_igetter_get_user_settings         ( const ofaIGetter *getter );

/*
 * Instance-wide UI related
 */
GtkApplicationWindow  *ofa_igetter_get_main_window           ( const ofaIGetter *getter );

ofaIPageManager       *ofa_igetter_get_page_manager          ( const ofaIGetter *getter );

myScopeMapper         *ofa_igetter_get_scope_mapper          ( const ofaIGetter *getter );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IGETTER_H__ */
