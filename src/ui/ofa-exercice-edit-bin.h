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

#ifndef __OFA_EXERCICE_EDIT_BIN_H__
#define __OFA_EXERCICE_EDIT_BIN_H__

/**
 * SECTION: ofa_exercice_edit_bin
 * @short_description: #ofaExerciceEditBin class definition.
 * @include: ui/ofa-exercice-edit-bin.h
 *
 * Let the user define a new exercice, and register it in dossier settings.
 *
 * This dialog is a composite of:
 *
 *   ofaExerciceEditBin
 *    |
 *    +- ofaExerciceMetaBin
 *    |
 *    +-------------------------------------- ofaIDBExerciceEditor
 *    |                                        |
 *    |                                        +- ofaMysqlExerciceEditor
 *    |                                            |
 *    |                                            +- ofaMysqlExerciceBin
 *    +
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'my-ibin-changed' signal)
 * - settings:   no
 * - current:    no
 *
 * Note that #ofaExerciceEditBin class implements myIBin interface,
 * but the #appli() method, because this later returns the newly
 * created #ofaIDBExerciceMeta object, which does not fit into the
 * standard prototype.
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-idbprovider-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_EDIT_BIN                ( ofa_exercice_edit_bin_get_type())
#define OFA_EXERCICE_EDIT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_EDIT_BIN, ofaExerciceEditBin ))
#define OFA_EXERCICE_EDIT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_EDIT_BIN, ofaExerciceEditBinClass ))
#define OFA_IS_EXERCICE_EDIT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_EDIT_BIN ))
#define OFA_IS_EXERCICE_EDIT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_EDIT_BIN ))
#define OFA_EXERCICE_EDIT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_EDIT_BIN, ofaExerciceEditBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaExerciceEditBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaExerciceEditBinClass;

GType                 ofa_exercice_edit_bin_get_type             ( void ) G_GNUC_CONST;

ofaExerciceEditBin   *ofa_exercice_edit_bin_new                  ( ofaIGetter *getter,
																		const gchar *settings_prefix,
																		guint rule );

void                  ofa_exercice_edit_bin_set_provider         ( ofaExerciceEditBin *bin,
																		ofaIDBProvider *provider );

void                  ofa_exercice_edit_bin_set_dossier_meta     ( ofaExerciceEditBin *bin,
																		ofaIDBDossierMeta *dossier_meta );

ofaIDBExerciceMeta   *ofa_exercice_edit_bin_apply                ( ofaExerciceEditBin *bin );

G_END_DECLS

#endif /* __OFA_EXERCICE_EDIT_BIN_H__ */
