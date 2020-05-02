/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
 * used by the dossier:
 * - the opened dossier (if any),
 * - its connection to the DBMS,
 * - its internal counters,
 * - etc.
 *
 * There is only one globally unique #ofaHub object, and it is
 * instanciated at application_new() time.
 *
 * The #ofaHub object is available through the #ofaIGetter interface.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-ipage-manager-def.h"
#include "api/ofo-counters-def.h"
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
 * Rules when defining a new dossier and/or a new exercice:
 */
enum {
	HUB_RULE_DOSSIER_NEW = 1,
	HUB_RULE_DOSSIER_RECOVERY,
	HUB_RULE_DOSSIER_RESTORE,
	HUB_RULE_EXERCICE_NEW,
	HUB_RULE_EXERCICE_DELETE,
	HUB_RULE_EXERCICE_CLOSE
};

GType          ofa_hub_get_type             ( void ) G_GNUC_CONST;

ofaHub        *ofa_hub_new                  ( void );

void           ofa_hub_set_application      ( ofaHub *hub,
													GApplication *application);

void           ofa_hub_set_runtime_command  ( ofaHub *hub,
													const gchar *argv_0 );

void           ofa_hub_set_main_window      ( ofaHub *hub,
													GtkApplicationWindow *main_window );

void           ofa_hub_set_page_manager     ( ofaHub *hub,
													ofaIPageManager *page_manager );

gboolean       ofa_hub_open_dossier         ( ofaHub *hub,
													GtkWindow *parent,
													ofaIDBConnect *connect,
													gboolean read_only,
													gboolean remediate_settings );

ofaIDBConnect *ofa_hub_get_connect          ( ofaHub *hub );

ofoCounters   *ofa_hub_get_counters         ( ofaHub *hub );

ofoDossier    *ofa_hub_get_dossier          ( ofaHub *hub );

gboolean       ofa_hub_is_opened_dossier    ( ofaHub *hub,
													ofaIDBExerciceMeta *exercice_meta );

gboolean       ofa_hub_is_writable_dossier  ( ofaHub *hub );

void           ofa_hub_close_dossier        ( ofaHub *hub );

ofaIImporter  *ofa_hub_get_willing_to_import( ofaHub *hub,
													const gchar *uri,
													GType type );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_H__ */
