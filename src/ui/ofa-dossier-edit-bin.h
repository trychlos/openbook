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

#ifndef __OFA_DOSSIER_EDIT_BIN_H__
#define __OFA_DOSSIER_EDIT_BIN_H__

/**
 * SECTION: ofa_dossier_edit_bin
 * @short_description: #ofaDossierEditBin class definition.
 * @include: ui/ofa-dossier-edit-bin.h
 *
 * Let the user define a new dossier, selecting the DBMS provider and
 * its connection properties, registering it in the settings.
 *
 * This is in particular the main component of the Dossier/New dialog
 * box.
 *
 * This dialog is a composite of:
 *
 *   ofaDossierEditBin
 *    |
 *    +- ofaDossierMetaBin
 *    |
 *    +- ofaIDBDossierEditor
 *    |
 *    +- ofaExerciceMetaBin
 *    |
 *    +- ofaIDBExerciceEditor
 *    |
 *    +- ofaAdminCredentialsBin
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_EDIT_BIN                ( ofa_dossier_edit_bin_get_type())
#define OFA_DOSSIER_EDIT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_EDIT_BIN, ofaDossierEditBin ))
#define OFA_DOSSIER_EDIT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_EDIT_BIN, ofaDossierEditBinClass ))
#define OFA_IS_DOSSIER_EDIT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_EDIT_BIN ))
#define OFA_IS_DOSSIER_EDIT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_EDIT_BIN ))
#define OFA_DOSSIER_EDIT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_EDIT_BIN, ofaDossierEditBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaDossierEditBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaDossierEditBinClass;

GType              ofa_dossier_edit_bin_get_type( void ) G_GNUC_CONST;

ofaDossierEditBin *ofa_dossier_edit_bin_new     ( ofaHub *hub,
														const gchar *settings_prefix,
														guint rule );

gboolean           ofa_dossier_edit_bin_is_valid( ofaDossierEditBin *bin,
														gchar **message );

gboolean           ofa_dossier_edit_bin_apply   ( ofaDossierEditBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_EDIT_BIN_H__ */
