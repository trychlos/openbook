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

#ifndef __OFA_PREFERENCES_H__
#define __OFA_PREFERENCES_H__

/**
 * SECTION: ofa_preferences
 * @short_description: #ofaPreferences class definition.
 * @include: core/ofa-preferences.h
 *
 * Manage the general user preferences,
 * i.e. user preferences which are not attached to any dossier.
 */

#include "api/my-date.h"

#include "core/my-dialog.h"
#include "core/ofa-main-window-def.h"
#include "core/ofa-plugin.h"

G_BEGIN_DECLS

#define OFA_TYPE_PREFERENCES                ( ofa_preferences_get_type())
#define OFA_PREFERENCES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PREFERENCES, ofaPreferences ))
#define OFA_PREFERENCES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PREFERENCES, ofaPreferencesClass ))
#define OFA_IS_PREFERENCES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PREFERENCES ))
#define OFA_IS_PREFERENCES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PREFERENCES ))
#define OFA_PREFERENCES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PREFERENCES, ofaPreferencesClass ))

typedef struct _ofaPreferencesPrivate        ofaPreferencesPrivate;

typedef struct {
	/*< public members >*/
	myDialog               parent;

	/*< private members >*/
	ofaPreferencesPrivate *priv;
}
	ofaPreferences;

typedef struct {
	/*< public members >*/
	myDialogClass parent;
}
	ofaPreferencesClass;

GType        ofa_preferences_get_type( void ) G_GNUC_CONST;

gboolean     ofa_preferences_run     (     ofaMainWindow *parent,     ofaPlugin *plugin );

/* these are helpers available to the rest of the application
 */
gboolean     ofa_prefs_assistant_quit_on_escape         ( void );
gboolean     ofa_prefs_assistant_confirm_on_escape      ( void );
gboolean     ofa_prefs_assistant_confirm_on_cancel      ( void );

gboolean     ofa_prefs_appli_confirm_on_quit            ( void );
gboolean     ofa_prefs_appli_confirm_on_altf4           ( void );

gboolean     ofa_prefs_account_delete_root_with_children( void );

myDateFormat ofa_prefs_date_display                     ( void );
myDateFormat ofa_prefs_date_check                       ( void );

const gchar *ofa_prefs_amount_decimal_sep               ( void );
const gchar *ofa_prefs_amount_thousand_sep              ( void );

G_END_DECLS

#endif /* __OFA_PREFERENCES_H__ */
