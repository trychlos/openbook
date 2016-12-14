/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_EXERCICE_TREEVIEW_H__
#define __OFA_EXERCICE_TREEVIEW_H__

/**
 * SECTION: ofa_exercice_treeview
 * @short_description: #ofaExerciceTreeview class definition.
 * @include: ui/ofa-exercice-treeview.h
 *
 * Manage a treeview with the list of the exercices which are defined
 * in the settings.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class:
 *    +------------------+--------------+
 *    | Signal           | Argument may |
 *    |                  | be %NULL     |
 *    +------------------+--------------+
 *    | ofa-exechanged   |      Yes     |
 *    | ofa-exeactivated |       No     |
 *    | ofa-exedelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-tvbin.h"

#include "ui/ofa-exercice-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_TREEVIEW                ( ofa_exercice_treeview_get_type())
#define OFA_EXERCICE_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeview ))
#define OFA_EXERCICE_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeviewClass ))
#define OFA_IS_EXERCICE_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_TREEVIEW ))
#define OFA_IS_EXERCICE_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_TREEVIEW ))
#define OFA_EXERCICE_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaExerciceTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaExerciceTreeviewClass;

GType                ofa_exercice_treeview_get_type    ( void ) G_GNUC_CONST;

ofaExerciceTreeview *ofa_exercice_treeview_new         ( ofaHub *hub,
																const gchar *settings_prefix );

void                 ofa_exercice_treeview_set_dossier ( ofaExerciceTreeview *view,
																ofaIDBDossierMeta *meta );

gboolean             ofa_exercice_treeview_get_selected( ofaExerciceTreeview *view,
																ofaIDBExerciceMeta **period );

G_END_DECLS

#endif /* __OFA_EXERCICE_TREEVIEW_H__ */
