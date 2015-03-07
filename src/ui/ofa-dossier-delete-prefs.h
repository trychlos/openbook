/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_DOSSIER_DELETE_PREFS_H__
#define __OFA_DOSSIER_DELETE_PREFS_H__

/**
 * SECTION: ofa_dossier_delete_prefs
 * @short_description: #ofaDossierDeletePrefs class definition.
 * @include: ui/ofa-dossier-delete-prefs.h
 *
 * Manage the preferences when deleting a dossier.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_DELETE_PREFS                ( ofa_dossier_delete_prefs_get_type())
#define OFA_DOSSIER_DELETE_PREFS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_DELETE_PREFS, ofaDossierDeletePrefs ))
#define OFA_DOSSIER_DELETE_PREFS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_DELETE_PREFS, ofaDossierDeletePrefsClass ))
#define OFA_IS_DOSSIER_DELETE_PREFS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_DELETE_PREFS ))
#define OFA_IS_DOSSIER_DELETE_PREFS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_DELETE_PREFS ))
#define OFA_DOSSIER_DELETE_PREFS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_DELETE_PREFS, ofaDossierDeletePrefsClass ))

typedef struct _ofaDossierDeletePrefsPrivate         ofaDossierDeletePrefsPrivate;

typedef struct {
	/*< public members >*/
	GObject                       parent;

	/*< private members >*/
	ofaDossierDeletePrefsPrivate *priv;
}
	ofaDossierDeletePrefs;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaDossierDeletePrefsClass;

GType                  ofa_dossier_delete_prefs_get_type        ( void ) G_GNUC_CONST;

ofaDossierDeletePrefs *ofa_dossier_delete_prefs_new             ( void );

void                   ofa_dossier_delete_prefs_init_dialog     ( ofaDossierDeletePrefs *prefs,
																		GtkContainer *container );

gint                   ofa_dossier_delete_prefs_get_db_mode     ( ofaDossierDeletePrefs *prefs );
gboolean               ofa_dossier_delete_prefs_get_account_mode( ofaDossierDeletePrefs *prefs );

void                   ofa_dossier_delete_prefs_set_settings    ( ofaDossierDeletePrefs *prefs );

G_END_DECLS

#endif /* __OFA_DOSSIER_DELETE_PREFS_H__ */
