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

#ifndef __OPENBOOK_API_OFA_PREFERENCES_H__
#define __OPENBOOK_API_OFA_PREFERENCES_H__

/**
 * SECTION: ofa_preferences
 * @short_description: #ofaPreferences class definition.
 * @include: openbook/ofa-preferences.h
 *
 * Manage the general user preferences,
 * i.e. user preferences which are not attached to any dossier.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, maybe after having displayed an error message box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           yes
 * - current:            no
 */

#include <gtk/gtk.h>

#include "my/my-date.h"

#include "api/ofa-hub-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-extender-module.h"

G_BEGIN_DECLS

#define OFA_TYPE_PREFERENCES                ( ofa_preferences_get_type())
#define OFA_PREFERENCES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PREFERENCES, ofaPreferences ))
#define OFA_PREFERENCES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PREFERENCES, ofaPreferencesClass ))
#define OFA_IS_PREFERENCES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PREFERENCES ))
#define OFA_IS_PREFERENCES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PREFERENCES ))
#define OFA_PREFERENCES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PREFERENCES, ofaPreferencesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaPreferences;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaPreferencesClass;

GType        ofa_preferences_get_type                   ( void ) G_GNUC_CONST;

void         ofa_preferences_run                        ( ofaIGetter *getter,
																GtkWindow *parent,
																ofaExtenderModule *plugin );

/* these are helpers globally available
 */
gboolean     ofa_prefs_assistant_quit_on_escape         ( ofaHub *hub );
gboolean     ofa_prefs_assistant_confirm_on_escape      ( ofaHub *hub );
gboolean     ofa_prefs_assistant_confirm_on_cancel      ( ofaHub *hub );
gboolean     ofa_prefs_assistant_is_willing_to_quit     ( ofaHub *hub, guint keyval );

gboolean     ofa_prefs_appli_confirm_on_altf4           ( ofaHub *hub );
gboolean     ofa_prefs_appli_confirm_on_quit            ( ofaHub *hub );

gboolean     ofa_prefs_dossier_open_notes               ( ofaHub *hub );
gboolean     ofa_prefs_dossier_open_notes_if_empty      ( ofaHub *hub );
gboolean     ofa_prefs_dossier_open_properties          ( ofaHub *hub );
gboolean     ofa_prefs_dossier_open_balance             ( ofaHub *hub );
gboolean     ofa_prefs_dossier_open_integrity           ( ofaHub *hub );

gboolean     ofa_prefs_account_delete_root_with_children( ofaHub *hub );

myDateFormat ofa_prefs_date_display                     ( ofaHub *hub );
myDateFormat ofa_prefs_date_check                       ( ofaHub *hub );
gboolean     ofa_prefs_date_overwrite                   ( ofaHub *hub );

const gchar *ofa_prefs_amount_decimal_sep               ( ofaHub *hub );
const gchar *ofa_prefs_amount_thousand_sep              ( ofaHub *hub );
gboolean     ofa_prefs_amount_accept_dot                ( ofaHub *hub );
gboolean     ofa_prefs_amount_accept_comma              ( ofaHub *hub );

gchar       *ofa_prefs_export_default_folder            ( ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PREFERENCES_H__ */
