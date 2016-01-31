/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib.h>

#include "api/my-file-monitor.h"
#include "api/my-timeout.h"
#include "api/my-utils.h"

/* private instance data
 */
struct _myFileMonitorPrivate {
	gboolean      dispose_has_run;

	/* runtime data
	 */
	gchar        *filename;
	GFileMonitor *monitor;
	myTimeout    *timeout;
};

#define FILE_MONITOR_SIGNAL_CHANGED     "changed"
#define FILE_MONITOR_RATE_LIMIT         250		/* ms */

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( myFileMonitor, my_file_monitor, G_TYPE_OBJECT )

static void on_monitor_changed( GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, myFileMonitor *self );
static void on_monitor_changed_timeout( myFileMonitor *self );

static void
settings_monitor_finalize( GObject *object )
{
	static const gchar *thisfn = "my_file_monitor_finalize";
	myFileMonitorPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_FILE_MONITOR( object ));

	/* free data members here */
	priv = MY_FILE_MONITOR( object )->priv;

	g_free( priv->filename );
	g_clear_object( &priv->monitor );
	g_free( priv->timeout );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_file_monitor_parent_class )->finalize( object );
}

static void
settings_monitor_dispose( GObject *object )
{
	myFileMonitorPrivate *priv;

	g_return_if_fail( object && MY_IS_FILE_MONITOR( object ));

	priv = MY_FILE_MONITOR( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_file_monitor_cancel( priv->monitor );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_file_monitor_parent_class )->dispose( object );
}

static void
my_file_monitor_init( myFileMonitor *self )
{
	static const gchar *thisfn = "my_file_monitor_init";

	g_return_if_fail( self && MY_IS_FILE_MONITOR( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_FILE_MONITOR, myFileMonitorPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
my_file_monitor_class_init( myFileMonitorClass *klass )
{
	static const gchar *thisfn = "my_file_monitor_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settings_monitor_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_monitor_finalize;

	g_type_class_add_private( klass, sizeof( myFileMonitorPrivate ));

	/**
	 * myFileMonitor:
	 *
	 * This signal is sent when the monitored settings file changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( myFileMonitor *monitor,
	 * 						const gchar *filename,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				FILE_MONITOR_SIGNAL_CHANGED,
				MY_TYPE_FILE_MONITOR,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );
}

/**
 * my_file_monitor_new:
 * @filename: the full path of the file to be monitored.
 *
 * Returns: a new #myFileMonitor object which should be g_object_unref()
 * by the caller.
 */
myFileMonitor *
my_file_monitor_new( const gchar *filename )
{
	myFileMonitor *monitor;
	myFileMonitorPrivate *priv;
	GFile *file;

	g_return_val_if_fail( my_strlen( filename ), NULL );

	monitor = g_object_new( MY_TYPE_FILE_MONITOR, NULL );
	priv = monitor->priv;
	priv->filename = g_strdup( filename );
	file = g_file_new_for_path( filename );
	priv->monitor = g_file_monitor_file( file, G_FILE_MONITOR_NONE, NULL, NULL );

	/*g_file_monitor_set_rate_limit( priv->monitor, FILE_MONITOR_RATE_LIMIT );*/
	/* rather use our myTimeout struct */
	priv->timeout = g_new0( myTimeout, 1 );
	priv->timeout->timeout = FILE_MONITOR_RATE_LIMIT;
	priv->timeout->handler = ( myTimeoutFunc ) on_monitor_changed_timeout;
	priv->timeout->user_data = monitor;

	g_signal_connect( priv->monitor, "changed", G_CALLBACK( on_monitor_changed ), monitor );
	g_object_unref( file );

	return( monitor );
}

/*
 * for now, only sends a message to say that the list of dossier is
 * empty or not empty
 *
 * Without any rate limit, we are receiving four notifications when the
 * dossier.conf file is opened, :
 *   time=35891920971, event_type=1 (G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
 *   time=35891921737, event_type=3 (G_FILE_MONITOR_EVENT_CREATED)
 *   time=35891922172, event_type=1 (G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
 *   time=35891922555, event_type=1 (G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
 * All (but maybe the last) are obviously useless:
 * - at least because the file already existed
 * - because the file has not actually changed, but only the date of last
 *   access has been set; this is an attribute change we don't care about.
 *
 * With a 100ms rate limit, we receive the four same notifications :(
 * Note that the default is 800ms according to the doc, and is not more
 * respected.
 */
static void
on_monitor_changed( GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, myFileMonitor *self )
{
	static const gchar* thisfn = "my_file_monitor_on_monitor_changed";
	const gchar *event_str = "";

	switch( event_type ){
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
			event_str = "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED";
			break;
		case G_FILE_MONITOR_EVENT_CHANGED:
			event_str = "G_FILE_MONITOR_EVENT_CHANGED";
			break;
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
			event_str = "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT";
			break;
		case G_FILE_MONITOR_EVENT_CREATED:
			event_str = "G_FILE_MONITOR_EVENT_CREATED";
			break;
		case G_FILE_MONITOR_EVENT_DELETED:
			event_str = "G_FILE_MONITOR_EVENT_DELETED";
			break;
		case G_FILE_MONITOR_EVENT_MOVED:
			event_str = "G_FILE_MONITOR_EVENT_MOVED";
			break;
		case G_FILE_MONITOR_EVENT_MOVED_IN:
			event_str = "G_FILE_MONITOR_EVENT_MOVED_IN";
			break;
		case G_FILE_MONITOR_EVENT_MOVED_OUT:
			event_str = "G_FILE_MONITOR_EVENT_MOVED_OUT";
			break;
		case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
			event_str = "G_FILE_MONITOR_EVENT_PRE_UNMOUNT";
			break;
		case G_FILE_MONITOR_EVENT_RENAMED:
			event_str = "G_FILE_MONITOR_EVENT_RENAMED";
			break;
		case G_FILE_MONITOR_EVENT_UNMOUNTED:
			event_str = "G_FILE_MONITOR_EVENT_UNMOUNTED";
			break;
	}

	g_debug( "%s: time=%lu, event_type=%d (%s)",
			thisfn, g_get_monotonic_time(), event_type, event_str );

	my_timeout_event( self->priv->timeout );
}

static void
on_monitor_changed_timeout( myFileMonitor *monitor )
{
	static const gchar *thisfn = "my_file_monitor_on_monitor_changed_timeout";
	myFileMonitorPrivate *priv = monitor->priv;

	g_debug( "%s: emitting signal: filename=%s", thisfn, priv->filename );
	g_signal_emit_by_name( monitor, FILE_MONITOR_SIGNAL_CHANGED, priv->filename );
}
