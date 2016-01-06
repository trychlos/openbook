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

/* @title: ofaHub
 * @short_description: The #ofaHub Class Definition
 * @include: openbook/ofa-hub.h
 *
 * The ofaHub class manages the currently opened dossier/exercice.
 * Beginning with a valid connection, it points to the unique dossier
 * properties record, and maintains the different collections.
 *
 * Instanciating a new hub with its valid connection is the same
 * than opening the dossier (though a graphical application may need
 * some more initializations).
 *
 * At last, unreffing the hub is the same than closing the dossier.
 *
 * The #ofaHub class defines a "hub signaling system" which emits
 * dedicated messages on new, updated or deleted objects.
 */

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

/**
 * Signals defined here:
 */
#define SIGNAL_HUB_NEW                  "hub-object-new"
#define SIGNAL_HUB_UPDATED              "hub-object-updated"
#define SIGNAL_HUB_DELETED              "hub-object-deleted"
#define SIGNAL_HUB_RELOAD               "hub-dataset-reload"
#define SIGNAL_HUB_ENTRY_STATUS_CHANGED "hub-entry-status-changed"
#define SIGNAL_HUB_EXE_DATES_CHANGED    "hub-exe-dates-changed"

GType                ofa_hub_get_type          ( void ) G_GNUC_CONST;

ofaHub              *ofa_hub_new_with_connect  ( const ofaIDBConnect *connect );

const ofaIDBConnect *ofa_hub_get_connect       ( const ofaHub *hub );

ofoDossier          *ofa_hub_get_dossier       ( const ofaHub *hub );

void                 ofa_hub_remediate_settings( const ofaHub *hub );

guint                ofa_hub_import_csv        ( ofaHub *hub,
														ofaIImportable *object,
														const gchar *uri,
														const ofaFileFormat *settings,
														void *caller,
														guint *errors );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_H__ */
