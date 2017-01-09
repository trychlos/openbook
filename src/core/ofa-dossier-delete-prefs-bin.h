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

#ifndef __OFA_DOSSIER_DELETE_PREFS_BIN_H__
#define __OFA_DOSSIER_DELETE_PREFS_BIN_H__

/**
 * SECTION: ofa_dossier_delete_prefs_bin
 * @short_description: #ofaDossierDeletePrefsBin class definition.
 * @include: core/ofa-dossier-delete-prefs-bin.h
 *
 * Manage the preferences when deleting a dossier.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no  (has 'ofa-changed' signal)
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_DELETE_PREFS_BIN                ( ofa_dossier_delete_prefs_bin_get_type())
#define OFA_DOSSIER_DELETE_PREFS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_DELETE_PREFS_BIN, ofaDossierDeletePrefsBin ))
#define OFA_DOSSIER_DELETE_PREFS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_DELETE_PREFS_BIN, ofaDossierDeletePrefsBinClass ))
#define OFA_IS_DOSSIER_DELETE_PREFS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_DELETE_PREFS_BIN ))
#define OFA_IS_DOSSIER_DELETE_PREFS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_DELETE_PREFS_BIN ))
#define OFA_DOSSIER_DELETE_PREFS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_DELETE_PREFS_BIN, ofaDossierDeletePrefsBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaDossierDeletePrefsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaDossierDeletePrefsBinClass;

/**
 * ofnDBMode:
 *
 * What to do when the database already exists while defining a new
 * dossier.
 */
typedef enum {
	DBMODE_REINIT = 1,
	DBMODE_LEAVE_AS_IS
}
	ofnDBMode;

/**
 * ofnDBMode:
 *
 * What to do about the database when deleting a dossier.
 */
typedef enum {
	DBMODE_DROP = 1,
	DBMODE_KEEP
}
	ofnDBDeleteMode;

GType                     ofa_dossier_delete_prefs_bin_get_type        ( void ) G_GNUC_CONST;

ofaDossierDeletePrefsBin *ofa_dossier_delete_prefs_bin_new             ( ofaHub *hub );

gint                      ofa_dossier_delete_prefs_bin_get_db_mode     ( ofaDossierDeletePrefsBin *bin );

void                      ofa_dossier_delete_prefs_bin_set_db_mode     ( ofaDossierDeletePrefsBin *bin,
																				gint mode );

gboolean                  ofa_dossier_delete_prefs_bin_get_account_mode( ofaDossierDeletePrefsBin *bin );

void                      ofa_dossier_delete_prefs_bin_set_account_mode( ofaDossierDeletePrefsBin *bin,
																				gboolean drop_account );

void                      ofa_dossier_delete_prefs_bin_apply           ( ofaDossierDeletePrefsBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_DELETE_PREFS_BIN_H__ */
