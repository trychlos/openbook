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

#include "api/ofa-iexporter.h"
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
	sIExporter;

/* signals defined here
 */
enum {
	PROGRESS = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

#define IEXPORTER_LAST_VERSION        1
#define IEXPORTER_DATA                "ofa-iexporter-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIExporterInterface *klass );
static void          interface_base_finalize( ofaIExporterInterface *klass );
static gboolean      iexporter_export_to_stream( ofaIExporter *exportable, GOutputStream *stream, const ofaFileFormat *settings, ofaHub *hub );
static sIExporter *get_iexporter_data( ofaIExporter *exportable );
static void          on_exportable_finalized( sIExporter *sdata, GObject *finalized_object );

/**
 * ofa_iexporter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexporter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexporter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexporter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExporterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExporter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iexporter_interface_base_init";
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIExporter::progress:
		 *
		 * This signal is to be sent on the exportable
		 * in order to visually render the export
		 * progression.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIExporter *exporter,
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
interface_base_finalize( ofaIExporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iexporter_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexporter_get_interface_last_version:
 * @instance: this #ofaIExporter instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexporter_get_interface_last_version( void )
{
	return( IEXPORTER_LAST_VERSION );
}

/**
 * ofa_iexporter_export_to_path:
 * @exportable: this #ofaIExporter instance.
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
ofa_iexporter_export_to_path( ofaIExporter *exportable,
									const gchar *uri, const ofaFileFormat *settings,
									ofaHub *hub, const void *instance )
{
	GFile *output_file;
	sIExporter *sdata;
	GOutputStream *output_stream;
	gboolean ok;

	g_return_val_if_fail( OFA_IS_IEXPORTER( exportable ), FALSE );

	sdata = get_iexporter_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->settings = settings;
	sdata->instance = instance;

	if( !my_utils_output_stream_new( uri, &output_file, &output_stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( output_stream ), FALSE );

	ok = iexporter_export_to_stream( exportable, output_stream, settings, hub );

	g_output_stream_close( output_stream, NULL, NULL );
	g_object_unref( output_file );

	return( ok );
}

static gboolean
iexporter_export_to_stream( ofaIExporter *exportable,
									GOutputStream *stream, const ofaFileFormat *settings,
									ofaHub *hub )
{
	sIExporter *sdata;

	sdata = get_iexporter_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->stream = stream;

	if( OFA_IEXPORTER_GET_INTERFACE( exportable )->export ){
		return( OFA_IEXPORTER_GET_INTERFACE( exportable )->export( exportable, settings, hub ));
	}

	return( FALSE );
}

/**
 * ofa_iexporter_export_lines:
 * @exportable: this #ofaIExporter instance.
 * @GSList: a #GSList of exported lines.
 *
 * The #ofaIExporter exportable instance must have taken into account
 * the decimal dot and the field separators specified in provided
 * #ofaFileFormat settings.
 *
 * The #ofaIExporter interface takes care here of charset conversions.
 *
 * Returns: %TRUE if the lines have been successfully written to the
 * output stream.
 */
gboolean
ofa_iexporter_export_lines( ofaIExporter *exportable, GSList *lines )
{
	sIExporter *sdata;
	GSList *it;
	gchar *str, *converted, *msg;
	GError *error;
	gint ret;
	gdouble progress;

	g_return_val_if_fail( OFA_IS_IEXPORTER( exportable ), FALSE );

	sdata = get_iexporter_data( exportable );
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
 * ofa_iexporter_get_count:
 * @exportable: this #ofaIExporter instance.
 *
 * Returns: the count of lines as set by the exportable instance.
 */
gulong
ofa_iexporter_get_count( ofaIExporter *exportable )
{
	sIExporter *sdata;

	g_return_val_if_fail( OFA_IS_IEXPORTER( exportable ), 0 );

	sdata = get_iexporter_data( exportable );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->count );
}

/**
 * ofa_iexporter_set_count:
 * @exportable: this #ofaIExporter instance.
 * @count: the count of lines the exportable plans to export.
 *
 * The #ofaIExporter exportable instance may use this function to set
 * the count of the full dataset.
 * This count will be later used to update the initial caller of the
 * export progress.
 */
void
ofa_iexporter_set_count( ofaIExporter *exportable, gulong count )
{
	sIExporter *sdata;

	g_return_if_fail( OFA_IS_IEXPORTER( exportable ));

	sdata = get_iexporter_data( exportable );
	g_return_if_fail( sdata );

	sdata->count = count;
}

static sIExporter *
get_iexporter_data( ofaIExporter *exportable )
{
	sIExporter *sdata;

	sdata = ( sIExporter * ) g_object_get_data( G_OBJECT( exportable ), IEXPORTER_DATA );

	if( !sdata ){
		sdata = g_new0( sIExporter, 1 );
		g_object_set_data( G_OBJECT( exportable ), IEXPORTER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( exportable ), ( GWeakNotify ) on_exportable_finalized, sdata );
	}

	return( sdata );
}

static void
on_exportable_finalized( sIExporter *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_iexporter_on_exportable_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata );
}
