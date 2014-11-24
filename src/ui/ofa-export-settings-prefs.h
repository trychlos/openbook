/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFA_EXPORT_SETTINGS_PREFS_H__
#define __OFA_EXPORT_SETTINGS_PREFS_H__

/**
 * SECTION: ofa_export_settings_prefs
 * @short_description: #ofaExportSettingsPrefs class definition.
 * @include: ui/ofa-export-settings-prefs.h
 *
 * A convenience class which let the user manages its own export
 * settings. It is to be used as a piece of user preferences.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXPORT_SETTINGS_PREFS                ( ofa_export_settings_prefs_get_type())
#define OFA_EXPORT_SETTINGS_PREFS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXPORT_SETTINGS_PREFS, ofaExportSettingsPrefs ))
#define OFA_EXPORT_SETTINGS_PREFS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXPORT_SETTINGS_PREFS, ofaExportSettingsPrefsClass ))
#define OFA_IS_EXPORT_SETTINGS_PREFS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXPORT_SETTINGS_PREFS ))
#define OFA_IS_EXPORT_SETTINGS_PREFS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXPORT_SETTINGS_PREFS ))
#define OFA_EXPORT_SETTINGS_PREFS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXPORT_SETTINGS_PREFS, ofaExportSettingsPrefsClass ))

typedef struct _ofaExportSettingsPrefsPrivate         ofaExportSettingsPrefsPrivate;

typedef struct {
	/*< public members >*/
	GObject                        parent;

	/*< private members >*/
	ofaExportSettingsPrefsPrivate *priv;
}
	ofaExportSettingsPrefs;

typedef struct {
	/*< public members >*/
	GObjectClass                   parent;
}
	ofaExportSettingsPrefsClass;

GType                   ofa_export_settings_prefs_get_type   ( void ) G_GNUC_CONST;

ofaExportSettingsPrefs *ofa_export_settings_prefs_new        ( void );

void                    ofa_export_settings_prefs_attach_to  ( ofaExportSettingsPrefs *settings,
																		GtkContainer *parent );

void                    ofa_export_settings_prefs_init_dialog( ofaExportSettingsPrefs *settings );

void                    ofa_export_settings_prefs_show_folder( ofaExportSettingsPrefs *settings,
																		gboolean show );

gboolean                ofa_export_settings_prefs_apply      ( ofaExportSettingsPrefs *settings );

G_END_DECLS

#endif /* __OFA_EXPORT_SETTINGS_PREFS_H__ */
