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

#ifndef __OFA_EXPORT_SETTINGS_H__
#define __OFA_EXPORT_SETTINGS_H__

/**
 * SECTION: ofa_export_settings
 * @short_description: #ofaExportSettings class definition.
 * @include: ui/ofa-export-settings.h
 *
 * A convenience class which manages the export settings.
 */

#include "api/my-date.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXPORT_SETTINGS                ( ofa_export_settings_get_type())
#define OFA_EXPORT_SETTINGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXPORT_SETTINGS, ofaExportSettings ))
#define OFA_EXPORT_SETTINGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXPORT_SETTINGS, ofaExportSettingsClass ))
#define OFA_IS_EXPORT_SETTINGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXPORT_SETTINGS ))
#define OFA_IS_EXPORT_SETTINGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXPORT_SETTINGS ))
#define OFA_EXPORT_SETTINGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXPORT_SETTINGS, ofaExportSettingsClass ))

typedef struct _ofaExportSettingsPrivate        ofaExportSettingsPrivate;

typedef struct {
	/*< public members >*/
	GObject                   parent;

	/*< private members >*/
	ofaExportSettingsPrivate *priv;
}
	ofaExportSettings;

typedef struct {
	/*< public members >*/
	GObjectClass              parent;
}
	ofaExportSettingsClass;

GType              ofa_export_settings_get_type       ( void ) G_GNUC_CONST;

ofaExportSettings *ofa_export_settings_new            ( const gchar *name );

const gchar       *ofa_export_settings_get_charmap    ( const ofaExportSettings *settings );

myDateFormat       ofa_export_settings_get_date_format( const ofaExportSettings *settings );

gchar              ofa_export_settings_get_decimal_sep( const ofaExportSettings *settings );

gchar              ofa_export_settings_get_field_sep  ( const ofaExportSettings *settings );

const gchar       *ofa_export_settings_get_folder     ( const ofaExportSettings *settings );

void               ofa_export_settings_set            ( ofaExportSettings *settings,
															const gchar *charmap,
															myDateFormat date_format,
															gchar decimal_sep,
															gchar field_sep,
															const gchar *folder );

G_END_DECLS

#endif /* __OFA_EXPORT_SETTINGS_H__ */
