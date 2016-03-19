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
#include <glib/gi18n.h>
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-iexportable.h"
#include "api/ofo-dossier.h"

/* data set against the exported object
 */
typedef struct {

	/* initialization
	 */
	const ofaFileFormat *settings;
	const void          *instance;

	/* runtime data
	 */
	GOutputStream       *stream;
	gulong               count;
	gulong               progress;
}
	sIExportable;

/* signals defined here
 */
enum {
	PROGRESS = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

#define IEXPORTABLE_LAST_VERSION        1
#define IEXPORTABLE_DATA                "ofa-iexportable-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIExportableInterface *klass );
static void          interface_base_finalize( ofaIExportableInterface *klass );
static gboolean      iexportable_export_to_stream( ofaIExportable *exportable, GOutputStream *stream, const ofaFileFormat *settings, ofaHub *hub );
static sIExportable *get_iexportable_data( ofaIExportable *exportable );
static void          on_exportable_finalized( sIExportable *sdata, GObject *finalized_object );

/**
 * ofa_iexportable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexportable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexportable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexportable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExportableInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExportable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iexportable_interface_base_init";
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIExportable::progress:
		 *
		 * This signal is to be sent on the exportable
		 * in order to visually render the export
		 * progression.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIExportable *exporter,
		 * 						gdouble       progress,
		 * 						const gchar  *text,
		 * 						gpointer      user_data );
		 */
		st_signals[ PROGRESS ] = g_signal_new_class_handler(
					"ofa-progress",
					interface_type,
					G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_DOUBLE, G_TYPE_STRING );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iexportable_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexportable_get_interface_last_version:
 * @instance: this #ofaIExportable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexportable_get_interface_last_version( void )
{
	return( IEXPORTABLE_LAST_VERSION );
}

/**
 * ofa_iexportable_export_to_path:
 * @exportable: this #ofaIExportable instance.
 * @fname: the output filename, will be overriden without requering
 *  any confirmation if already exists.
 * @settings: a #ofaFileFormat object.
 * @hub: the current  #ofaHub object.
 * @fn_double: the callback to be triggered on progress with a double.
 * @fn_text: the callback to be triggered on progress with a text.
 * @instance: the instance to be set for calling the callbacks.
 *
 * Export the specified dataset to the named file.
 *
 * Returns: %TRUE if the export has successfully completed.
 */
gboolean
ofa_iexportable_export_to_path( ofaIExportable *exportable,
									const gchar *uri, const ofaFileFormat *settings,
									ofaHub *hub, const void *instance )
{
	GFile *output_file;
	sIExportable *sdata;
	GOutputStream *output_stream;
	gboolean ok;

	g_return_val_if_fail( OFA_IS_IEXPORTABLE( exportable ), FALSE );

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->settings = settings;
	sdata->instance = instance;

	if( !my_utils_output_stream_new( uri, &output_file, &output_stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( output_stream ), FALSE );

	ok = iexportable_export_to_stream( exportable, output_stream, settings, hub );

	g_output_stream_close( output_stream, NULL, NULL );
	g_object_unref( output_file );

	return( ok );
}

static gboolean
iexportable_export_to_stream( ofaIExportable *exportable,
									GOutputStream *stream, const ofaFileFormat *settings,
									ofaHub *hub )
{
	sIExportable *sdata;

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->stream = stream;

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export( exportable, settings, hub ));
	}

	return( FALSE );
}

/**
 * ofa_iexportable_export_lines:
 * @exportable: this #ofaIExportable instance.
 * @GSList: a #GSList of exported lines.
 *
 * The #ofaIExportable exportable instance must have taken into account
 * the decimal dot and the field separators specified in provided
 * #ofaFileFormat settings.
 *
 * The #ofaIExportable interface takes care here of charset conversions.
 *
 * Returns: %TRUE if the lines have been successfully written to the
 * output stream.
 */
gboolean
ofa_iexportable_export_lines( ofaIExportable *exportable, GSList *lines )
{
	sIExportable *sdata;
	GSList *it;
	gchar *str, *converted, *msg;
	GError *error;
	gint ret;
	gdouble progress;

	g_return_val_if_fail( OFA_IS_IEXPORTABLE( exportable ), FALSE );

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	error = NULL;

	for( it=lines ; it ; it=it->next ){

		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( !sdata->count || sdata->count < 100 ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		str = g_strdup_printf( "%s\n", ( const gchar * ) it->data );
		converted = g_convert( str, -1,
								ofa_file_format_get_charmap( sdata->settings ),
								"UTF-8", NULL, NULL, &error );
		g_free( str );
		if( !converted ){
			msg = g_strdup_printf( _( "Charset conversion error: %s" ), error->message );
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, msg );
			g_free( msg );
			return( FALSE );
		}

		/* use strlen() rather than g_utf8_strlen() as we want a count
		 * of bytes rather than a count of chars */
		ret = g_output_stream_write( sdata->stream, converted, strlen( converted), NULL, &error );
		g_free( converted );
		if( ret == -1 ){
			msg = g_strdup_printf( _( "Write error: %s" ), error->message );
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, msg );
			g_free( msg );
			return( FALSE );
		}

		sdata->progress += 1;

		/* only try to display the progress if the exportable has first
		 * set the total count of lines */
		if( sdata->count ){
			progress = ( gdouble ) sdata->progress / ( gdouble ) sdata->count;
			str = g_strdup_printf( "%ld/%ld", sdata->progress, sdata->count );
		} else {
			progress = ( gdouble ) sdata->progress;
			str = g_strdup_printf( "%ld", sdata->progress );
		}
		g_signal_emit_by_name( exportable, "ofa-progress", progress, str );
		g_free( str );
	}

	return( TRUE );
}

/**
 * ofa_iexportable_get_count:
 * @exportable: this #ofaIExportable instance.
 *
 * Returns: the count of lines as set by the exportable instance.
 */
gulong
ofa_iexportable_get_count( ofaIExportable *exportable )
{
	sIExportable *sdata;

	g_return_val_if_fail( OFA_IS_IEXPORTABLE( exportable ), 0 );

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->count );
}

/**
 * ofa_iexportable_set_count:
 * @exportable: this #ofaIExportable instance.
 * @count: the count of lines the exportable plans to export.
 *
 * The #ofaIExportable exportable instance may use this function to set
 * the count of the full dataset.
 * This count will be later used to update the initial caller of the
 * export progress.
 */
void
ofa_iexportable_set_count( ofaIExportable *exportable, gulong count )
{
	sIExportable *sdata;

	g_return_if_fail( OFA_IS_IEXPORTABLE( exportable ));

	sdata = get_iexportable_data( exportable );
	g_return_if_fail( sdata );

	sdata->count = count;
}

static sIExportable *
get_iexportable_data( ofaIExportable *exportable )
{
	sIExportable *sdata;

	sdata = ( sIExportable * ) g_object_get_data( G_OBJECT( exportable ), IEXPORTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIExportable, 1 );
		g_object_set_data( G_OBJECT( exportable ), IEXPORTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( exportable ), ( GWeakNotify ) on_exportable_finalized, sdata );
	}

	return( sdata );
}

static void
on_exportable_finalized( sIExportable *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_iexportable_on_exportable_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata );
}
