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
 *
 */

#ifndef __OFA_SETTINGS_MONITOR_H__
#define __OFA_SETTINGS_MONITOR_H__

/* @title: ofaSettingsMonitor
 * @short_description: The SettingsMonitor Class Definition
 * @include: core/ofa-settings-monitor.h
 *
 * The #ofaSettingsMonitor class monitors configuration files,
 * sending an ad-hoc messages when they are changed.
 */

#include <glib-object.h>

#include "api/ofa-settings.h"

G_BEGIN_DECLS

#define OFA_TYPE_SETTINGS_MONITOR                ( ofa_settings_monitor_get_type())
#define OFA_SETTINGS_MONITOR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SETTINGS_MONITOR, ofaSettingsMonitor ))
#define OFA_SETTINGS_MONITOR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SETTINGS_MONITOR, ofaSettingsMonitorClass ))
#define OFA_IS_SETTINGS_MONITOR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SETTINGS_MONITOR ))
#define OFA_IS_SETTINGS_MONITOR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SETTINGS_MONITOR ))
#define OFA_SETTINGS_MONITOR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SETTINGS_MONITOR, ofaSettingsMonitorClass ))

typedef struct _ofaSettingsMonitorPrivate        ofaSettingsMonitorPrivate;

typedef struct {
	/*< public members >*/
	GObject                    parent;

	/*< private members >*/
	ofaSettingsMonitorPrivate *priv;
}
	ofaSettingsMonitor;

typedef struct {
	/*< public members >*/
	GObjectClass               parent;
}
	ofaSettingsMonitorClass;

GType               ofa_settings_monitor_get_type       ( void ) G_GNUC_CONST;

ofaSettingsMonitor *ofa_settings_monitor_new            ( ofaSettingsTarget target );

gboolean            ofa_settings_monitor_is_target_empty( const ofaSettingsMonitor *monitor );

G_END_DECLS

#endif /* __OFA_SETTINGS_MONITOR_H__ */
