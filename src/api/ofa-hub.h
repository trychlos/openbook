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

#ifndef __OPENBOOK_API_OFA_HUB_H__
#define __OPENBOOK_API_OFA_HUB_H__

/**
 * SECTION: ofahub
 * @title: ofaHub
 * @short_description: The #ofaHub Class Definition
 * @include: openbook/ofa-hub.h
 *
 * The #ofaHub class manages the currently opened dossier/exercice.
 * Beginning with a valid connection, it points to the unique dossier
 * properties record, and maintains the different collections.
 * When set, the #ofaHub object is unique inside of the running
 * application.
 *
 * Instanciating a new hub with its valid connection is the same
 * than opening the dossier (though a graphical application may need
 * some more initializations). The process is as follow:
 *
 * - get a valid #ofaIDBConnect object instance;
 *   here "valid" means that either #ofa_idbconnect_open_with_editor()
 *   or #ofa_idbconnect_open_with_meta() has returned a %TRUE value.
 *
 * - ask the #ofaIHubber implementation for a new #ofaHub object;
 *   the #ofaIHubber implementation is responsible for instanciating
 *   the #ofaHub object, and maintaining this primary reference in its
 *   private data space.
 *
 * - the #ofaHub instanciation takes a reference on the provided
 *   #ofaIDBConnect instance, then taking care of opening the dossier
 *   (which mainly means reading its internal properties) and
 *   initializing its signaling system.
 *
 * - at last the #ofaIHubber interface emits the SIGNAL_HUBBER_NEW
 *   signal to advertize about the new #ofaHub instanciation.
 *
 * At last, unreffing the hub is the same than closing the dossier.
 * The process is as follow:
 *
 * - just call #ofa_ihubber_clear_hub() which will release the
 *   #ofaIHubber reference on its #ofaHub object;
 *   this reference is expected to be the only reference (or the last)
 *   on the object, so that releasing it will trigger the object
 *   finalization.
 *
 * - the #ofaHub object dispose code just releases its reference on
 *   the initially provided #ofaIDBConnect connection, and on the
 *   dossier.
 *
 * - the #ofaIHubber interface emits the SIGNAL_HUBBER_CLOSED signal
 *   after #ofaHub finalization.
 *
 * The #ofaHub class defines a "hub signaling system" which emits
 * dedicated messages on new, updated or deleted objects.
 *
 * Getting the #ofaHub object
 *
 * Several paths are available to get the current #ofaHub object:
 *
 * - via the #ofaIHubber interface:
 *   > get a pointer to the #ofaIHubber interface or its implementation
 *   > ask it for its #ofaHub object (as the implementation is
 *     responsible for maintaining the primary reference on the
 *     #ofaHub object)
 *   > see #ofa_ihubber_get_hub()
 *
 * - via the main window:
 *   > the main window has a direct access to its #GtkApplication,
 *     which happens to be our #ofaIHubber implementation
 *   > the main window takes itself its own reference on the #ofaHub
 *     object on dossier opening, releasing it on dossier closing.
 *   > see #ofa_main_window_get_hub()
 *
 * - from a #ofaPage:
 *   > because the #ofaPage has itself a direct access to the
 *     #ofaMainWindow
 *   > see #ofa_page_get_hub()
 *
 * - from a dialog box (a myDialog-derived object), or from an assistant
 *   (a myAssistant-derived object):
 *   > as soon as the #ofaMainWindow main window is provided at dialog
 *     instanciation time, the #my_window_get_application() method is
 *     able to return our #GtkApplication, which happens to actually be
 *     the #GtkApplication to which the #ofaMainWindow is attached to.
 */

#include "api/ofa-dossier-prefs.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iimportable.h"
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
typedef struct _ofaHubPrivate       ofaHubPrivate;
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
 * Signals defined here:
 */
#define SIGNAL_HUB_NEW                  "hub-object-new"
#define SIGNAL_HUB_UPDATED              "hub-object-updated"
#define SIGNAL_HUB_DELETED              "hub-object-deleted"
#define SIGNAL_HUB_RELOAD               "hub-dataset-reload"
#define SIGNAL_HUB_STATUS_COUNT         "hub-status-count"
#define SIGNAL_HUB_STATUS_CHANGE        "hub-status-change"
#define SIGNAL_HUB_EXE_DATES_CHANGED    "hub-exe-dates-changed"

GType                ofa_hub_get_type           ( void ) G_GNUC_CONST;

ofaHub              *ofa_hub_new_with_connect   ( const ofaIDBConnect *connect,
														GtkWindow *parent );

const ofaIDBConnect *ofa_hub_get_connect        ( const ofaHub *hub );

ofoDossier          *ofa_hub_get_dossier        ( const ofaHub *hub );

ofaDossierPrefs     *ofa_hub_get_dossier_prefs  ( const ofaHub *hub );

void                 ofa_hub_remediate_settings ( const ofaHub *hub );

guint                ofa_hub_import_csv         ( ofaHub *hub,
														ofaIImportable *object,
														const gchar *uri,
														const ofaFileFormat *settings,
														void *caller,
														guint *errors );

void                 ofa_hub_disconnect_handlers( ofaHub *hub,
														GList *handlers );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_H__ */
