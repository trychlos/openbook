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

#ifndef __OFA_DOSSIER_ACTIONS_BIN_H__
#define __OFA_DOSSIER_ACTIONS_BIN_H__

/**
 * SECTION: ofa_dossier_actions_bin
 * @short_description: #ofaDossierActionsBin class definition.
 * @include: ui/ofa-dossier-actions-bin.h
 *
 * Let the user choose if the dossier/exercice should be opened after
 * the current action.
 *
 * This widget is used (and the label is automatically updated):
 * - from restore assistant: do we open the restored archived ?
 * - in new dossier dialog: do we open the newly created dossier ?
 * - from exercice closing assistant: do we open the new exercice ?
 *
 * The widget implements the #myIBin interface, but does not provide
 * any code for the apply() method. Instead, the caller should get the
 * current status of the check buttons and act accordingly.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

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

GType                 ofa_dossier_actions_bin_get_type ( void ) G_GNUC_CONST;

ofaDossierActionsBin *ofa_dossier_actions_bin_new      ( ofaIGetter *getter,
															const gchar *settings_prefix,
															guint rule );

gboolean              ofa_dossier_actions_bin_get_open ( ofaDossierActionsBin *bin );

gboolean              ofa_dossier_actions_bin_get_apply( ofaDossierActionsBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_ACTIONS_BIN_H__ */
