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

#ifndef __OFA_DOSSIER_ACTIONS_BIN_H__
#define __OFA_DOSSIER_ACTIONS_BIN_H__

/**
 * SECTION: ofa_dossier_actions_bin
 * @short_description: #ofaDossierActionsBin class definition.
 * @include: ui/ofa-dossier-actions-bin.h
 *
 * Let the user define actions which are to be taken on dossier
 * opening.
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

#define OFA_TYPE_DOSSIER_ACTIONS_BIN                ( ofa_dossier_actions_bin_get_type())
#define OFA_DOSSIER_ACTIONS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_ACTIONS_BIN, ofaDossierActionsBin ))
#define OFA_DOSSIER_ACTIONS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_ACTIONS_BIN, ofaDossierActionsBinClass ))
#define OFA_IS_DOSSIER_ACTIONS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_ACTIONS_BIN ))
#define OFA_IS_DOSSIER_ACTIONS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_ACTIONS_BIN ))
#define OFA_DOSSIER_ACTIONS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_ACTIONS_BIN, ofaDossierActionsBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaDossierActionsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaDossierActionsBinClass;

GType                 ofa_dossier_actions_bin_get_type          ( void ) G_GNUC_CONST;

ofaDossierActionsBin *ofa_dossier_actions_bin_new               ( ofaHub *hub,
																		const gchar *settings_prefix,
																		guint rule );

GtkSizeGroup         *ofa_dossier_actions_bin_get_size_group    ( ofaDossierActionsBin *bin,
																		guint column );

gboolean              ofa_dossier_actions_bin_is_valid          ( ofaDossierActionsBin *bin,
																		gchar **error_message );

gboolean              ofa_dossier_actions_bin_get_open_on_create( ofaDossierActionsBin *bin );

gboolean              ofa_dossier_actions_bin_get_apply_actions ( ofaDossierActionsBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_ACTIONS_BIN_H__ */
