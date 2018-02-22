/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_OPEN_PREFS_H__
#define __OFA_OPEN_PREFS_H__

/**
 * SECTION: ofa_open_prefs
 * @short_description: #ofaOpenPrefs class definition.
 * @include: core/ofa-open-prefs.h
 *
 * An object which manages the actions which may be run when opening a
 * dossier:
 * - whether to display the notes
 * - whether to display the properties
 * - whether to check the balances
 * - whether to check the DBMS integrity.
 *
 * This object may be used:
 * - as the default preferences for a new dossier
 * - as a dossier-wide preferences for existing dossier(s).
 *
 * The object reads its settings at initialization time with the
 * provided settings characteristics.
 * The settings are written on demand only.
 *
 * This object can be updated though #ofaOpenPrefsBin class.
 */

#include "my/my-isettings.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPEN_PREFS                ( ofa_open_prefs_get_type())
#define OFA_OPEN_PREFS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPEN_PREFS, ofaOpenPrefs ))
#define OFA_OPEN_PREFS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPEN_PREFS, ofaOpenPrefsClass ))
#define OFA_IS_OPEN_PREFS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPEN_PREFS ))
#define OFA_IS_OPEN_PREFS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPEN_PREFS ))
#define OFA_OPEN_PREFS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPEN_PREFS, ofaOpenPrefsClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaOpenPrefs;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaOpenPrefsClass;

/**
 * The keys for open preferences in user and dossier settings.
 */
#define OPEN_PREFS_USER_KEY             "ofaPreferences-OpenPrefs"
#define OPEN_PREFS_DOSSIER_KEY          "ofa-OpenPrefs"

GType         ofa_open_prefs_get_type              ( void ) G_GNUC_CONST;

ofaOpenPrefs *ofa_open_prefs_new                   ( myISettings *settings,
														const gchar *group,
														const gchar *key );

gboolean      ofa_open_prefs_get_display_notes     ( ofaOpenPrefs *prefs );

void          ofa_open_prefs_set_display_notes     ( ofaOpenPrefs *prefs,
														gboolean display_notes );

gboolean      ofa_open_prefs_get_non_empty_notes   ( ofaOpenPrefs *prefs );

void          ofa_open_prefs_set_non_empty_notes   ( ofaOpenPrefs *prefs,
														gboolean non_empty_notes );

gboolean      ofa_open_prefs_get_display_properties( ofaOpenPrefs *prefs );

void          ofa_open_prefs_set_display_properties( ofaOpenPrefs *prefs,
														gboolean display_properties );

gboolean      ofa_open_prefs_get_check_balances    ( ofaOpenPrefs *prefs );

void          ofa_open_prefs_set_check_balances    ( ofaOpenPrefs *prefs,
														gboolean check_balances );

gboolean      ofa_open_prefs_get_check_integrity   ( ofaOpenPrefs *prefs );

void          ofa_open_prefs_set_check_integrity   ( ofaOpenPrefs *prefs,
														gboolean check_integrity );

void          ofa_open_prefs_apply_settings        ( ofaOpenPrefs *prefs );

void          ofa_open_prefs_change_settings       ( ofaOpenPrefs *prefs,
														myISettings *settings,
														const gchar *group,
														const gchar *key );

G_END_DECLS

#endif /* __OFA_OPEN_PREFS_H__ */
