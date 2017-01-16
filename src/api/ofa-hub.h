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

#ifndef __OPENBOOK_API_OFA_HUB_H__
#define __OPENBOOK_API_OFA_HUB_H__

/**
 * SECTION: ofahub
 * @title: ofaHub
 * @short_description: The #ofaHub Class Definition
 * @include: openbook/ofa-hub.h
 *
 * The #ofaHub class manages and maintains all objects which are globally
 * used by the application:
 * - the extenders collection (aka plugins),
 * - the opened dossier (if any),
 * - etc.
 *
 * The #ofaHub class defines a "hub signaling system" which emits
 * dedicated messages on new, updated or deleted objects.
 *
 * There is only one globally unique #ofaHub object, and it is
 * instanciated at application_new() time.
 *
 * The #ofaHub object is available through the #ofaIGetter interface.
 */

#include "my/my-icollector.h"
#include "my/my-isettings.h"

#include "api/ofa-dossier-collection.h"
#include "api/ofa-dossier-prefs.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-openbook-props.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_HUB                ( ofa_hub_get_type())
#define OFA_HUB( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_HUB, ofaHub ))
#define OFA_HUB_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_HUB, ofaHubClass ))
#define OFA_IS_HUB( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_HUB ))
#define OFA_IS_HUB_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_HUB ))
#define OFA_HUB_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_HUB, ofaHubClass ))

#if 0
typedef struct _ofaHub              ofaHub;
#endif

struct _ofaHub {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaHubClass;

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

/**
 * Rules when defining a new dossier and/or a new exercice:
 */
enum {
	HUB_RULE_DOSSIER_NEW = 1,
	HUB_RULE_DOSSIER_RESTORE
};

/**
 * Signals defined here:
 */
#define SIGNAL_HUB_NEW                  "hub-object-new"
#define SIGNAL_HUB_UPDATED              "hub-object-updated"
#define SIGNAL_HUB_DELETABLE            "hub-object-deletable"
#define SIGNAL_HUB_DELETED              "hub-object-deleted"
#define SIGNAL_HUB_RELOAD               "hub-dataset-reload"
#define SIGNAL_HUB_DOSSIER_OPENED       "hub-dossier-opened"
#define SIGNAL_HUB_DOSSIER_CLOSED       "hub-dossier-closed"
#define SIGNAL_HUB_DOSSIER_CHANGED      "hub-dossier-changed"
#define SIGNAL_HUB_DOSSIER_PREVIEW      "hub-dossier-preview"
#define SIGNAL_HUB_STATUS_COUNT         "hub-status-count"
#define SIGNAL_HUB_STATUS_CHANGE        "hub-status-change"
#define SIGNAL_HUB_EXE_DATES_CHANGED    "hub-exe-dates-changed"

GType                  ofa_hub_get_type                  ( void ) G_GNUC_CONST;

ofaHub                *ofa_hub_new                       ( ofaIGetter *getter );

ofaExtenderCollection *ofa_hub_get_extender_collection   ( ofaHub *hub );

myICollector          *ofa_hub_get_collector             ( const ofaHub *hub );

GList                 *ofa_hub_get_for_type              ( ofaHub *hub,
																GType type );

ofaDossierCollection  *ofa_hub_get_dossier_collection    ( ofaHub *hub );

myISettings           *ofa_hub_get_dossier_settings      ( ofaHub *hub );

myISettings           *ofa_hub_get_user_settings         ( ofaHub *hub );

ofaOpenbookProps      *ofa_hub_get_openbook_props        ( ofaHub *hub );

const gchar           *ofa_hub_get_runtime_dir           ( ofaHub *hub );

void                   ofa_hub_set_runtime_dir           ( ofaHub *hub,
																const gchar *dir );

const ofaIDBConnect   *ofa_hub_get_connect               ( ofaHub *hub );

ofoDossier            *ofa_hub_get_dossier               ( ofaHub *hub );

gboolean               ofa_hub_dossier_open              ( ofaHub *hub,
																GtkWindow *parent,
																ofaIDBConnect *connect,
																gboolean run_prefs,
																gboolean read_only );

void                   ofa_hub_dossier_close             ( ofaHub *hub );

gboolean               ofa_hub_dossier_is_writable       ( ofaHub *hub );

ofaDossierPrefs       *ofa_hub_dossier_get_prefs         ( ofaHub *hub );

gboolean               ofa_hub_dossier_remediate_settings( ofaHub *hub );

ofaIImporter          *ofa_hub_get_willing_to            ( ofaHub *hub,
																const gchar *uri,
																GType type );

void                   ofa_hub_disconnect_handlers       ( ofaHub *hub,
																GList **handlers );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_H__ */
