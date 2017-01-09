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

#ifndef __OFA_DOSSIER_PREFS_H__
#define __OFA_DOSSIER_PREFS_H__

/**
 * SECTION: ofa_dossier_prefs
 * @short_description: #ofaDossierPrefs functions.
 * @include: ui/ofa-dossier-prefs.h
 *
 * Get and set the dossier user preferences.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_PREFS                ( ofa_dossier_prefs_get_type())
#define OFA_DOSSIER_PREFS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_PREFS, ofaDossierPrefs ))
#define OFA_DOSSIER_PREFS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_PREFS, ofaDossierPrefsClass ))
#define OFA_IS_DOSSIER_PREFS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_PREFS ))
#define OFA_IS_DOSSIER_PREFS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_PREFS ))
#define OFA_DOSSIER_PREFS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_PREFS, ofaDossierPrefsClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaDossierPrefs;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaDossierPrefsClass;

GType            ofa_dossier_prefs_get_type          ( void ) G_GNUC_CONST;

ofaDossierPrefs *ofa_dossier_prefs_new               ( ofaHub *hub );

gboolean         ofa_dossier_prefs_get_open_notes    ( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_open_notes    ( ofaDossierPrefs *prefs,
															gboolean open );

gboolean         ofa_dossier_prefs_get_nonempty      ( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_nonempty      ( ofaDossierPrefs *prefs,
															gboolean nonempty );

gboolean         ofa_dossier_prefs_get_properties    ( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_properties    ( ofaDossierPrefs *prefs,
															gboolean properties );

gboolean         ofa_dossier_prefs_get_balances      ( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_balances      ( ofaDossierPrefs *prefs,
															gboolean balances );

gboolean         ofa_dossier_prefs_get_integrity     ( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_integrity     ( ofaDossierPrefs *prefs,
															gboolean integrity );

gchar           *ofa_dossier_prefs_get_background_img( ofaDossierPrefs *prefs );

void             ofa_dossier_prefs_set_background_img( ofaDossierPrefs *prefs,
															const gchar *uri );

G_END_DECLS

#endif /* __OFA_DOSSIER_PREFS_H__ */
