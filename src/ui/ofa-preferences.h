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

#ifndef __OPENBOOK_API_OFA_PREFERENCES_H__
#define __OPENBOOK_API_OFA_PREFERENCES_H__

/**
 * SECTION: ofa_preferences
 * @short_description: #ofaPreferences class definition.
 * @include: ui/ofa-preferences.h
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

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PREFERENCES_H__ */
