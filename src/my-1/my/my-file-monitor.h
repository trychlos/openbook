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

#ifndef __MY_API_MY_FILE_MONITOR_H__
#define __MY_API_MY_FILE_MONITOR_H__

/* @title: myFileMonitor
 * @short_description: The myFileMonitor Class Definition
 * @include: my/my-file-monitor.h
 *
 * The #myFileMonitor class monitors files,
 * sending an ad-hoc message when they are changed.
 *
 * This class is mostly useful when working on settings file. This sort
 * of file often triggers many individual notifications on each change,
 * making difficult for an application to maintain itself up to date.
 *
 * Defining a #myFileMonitor on these files let the application only be
 * notified at the end of the notifications burst, thus letting it
 * reload the settings only once.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_FILE_MONITOR                ( my_file_monitor_get_type())
#define MY_FILE_MONITOR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_FILE_MONITOR, myFileMonitor ))
#define MY_FILE_MONITOR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_FILE_MONITOR, myFileMonitorClass ))
#define MY_IS_FILE_MONITOR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_FILE_MONITOR ))
#define MY_IS_FILE_MONITOR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_FILE_MONITOR ))
#define MY_FILE_MONITOR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_FILE_MONITOR, myFileMonitorClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	myFileMonitor;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	myFileMonitorClass;

GType          my_file_monitor_get_type( void ) G_GNUC_CONST;

myFileMonitor *my_file_monitor_new     ( const gchar *filename );

G_END_DECLS

#endif /* __MY_API_MY_FILE_MONITOR_H__ */
