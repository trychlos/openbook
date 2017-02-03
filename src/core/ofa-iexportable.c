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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"

/* data set against the exported object
 */
typedef struct {

	/* initialization
	 */
	ofaStreamFormat *settings;
	myIProgress     *instance;

	/* runtime data
	 */
	GOutputStream   *stream;
	gulong           count;
	gulong           progress;
}
	sIExportable;

#define IEXPORTABLE_LAST_VERSION        1
#define IEXPORTABLE_DATA                "ofa-iexportable-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIExportableInterface *klass );
static void          interface_base_finalize( ofaIExportableInterface *klass );
static gboolean      iexportable_export_to_stream( ofaIExportable *exportable, GOutputStream *stream, ofaStreamFormat *settings, ofaIGetter *getter );
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

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iexportable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexportable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexportable_get_interface_last_version( void )
{
	return( IEXPORTABLE_LAST_VERSION );
}

/**
 * ofa_iexporter_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_iexportable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXPORTABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExportableInterface * ) iface )->get_interface_version ){
		version = (( ofaIExportableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExportable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iexporter_get_label:
 * @instance: this #ofaIExportable instance.
 *
 * Returns: the displayable label to be associated with @instance, as a
 * newly allocated string which should be g_free() by the caller.
 *
 * The returned string may contain one '_' underline, to be used as a
 * keyboard shortcut. This underline will be automatically removed when
 * the target display does not manage them.
 *
 * As this label is to be used as a selection label among a list
 * provided by the user interface, a #ofaIExportable class which would
 * not implement this method, would just not be proposed for export to
 * the user.
 *
 * It is so a strong suggestion to implement this method.
 */
gchar *
ofa_iexportable_get_label( const ofaIExportable *instance )
{
	static const gchar *thisfn = "ofa_iexportable_get_label";

	g_return_val_if_fail( instance && OFA_IS_IEXPORTABLE( instance ), NULL );

	if( OFA_IEXPORTABLE_GET_INTERFACE( instance )->get_label ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( instance )->get_label( instance ));
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'get_label()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iexportable_export_to_uri:
 * @exportable: a #ofaIExportable instance.
 * @uri: the output URI,
 *  will be overriden without any further confirmation if already exists.
 * @settings: a #ofaStreamFormat object.
 * @getter: a #ofaIGetter instance.
 * @progress: the #myIProgress instance which display the export progress.
 *
 * Export the specified dataset to the named file.
 *
 * Returns: %TRUE if the export has successfully completed.
 */
gboolean
ofa_iexportable_export_to_uri( ofaIExportable *exportable,
									const gchar *uri, ofaStreamFormat *settings,
									ofaIGetter *getter, myIProgress *progress )
{
	GFile *output_file;
	sIExportable *sdata;
	GOutputStream *output_stream;
	gboolean ok;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->settings = settings;
	sdata->instance = progress;

	if( !my_utils_output_stream_new( uri, &output_file, &output_stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( output_stream ), FALSE );

	ok = iexportable_export_to_stream( exportable, output_stream, settings, getter );

	g_output_stream_close( output_stream, NULL, NULL );
	g_object_unref( output_file );

	return( ok );
}

static gboolean
iexportable_export_to_stream( ofaIExportable *exportable,
									GOutputStream *stream, ofaStreamFormat *settings,
									ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iexportable_export_to_stream";
	sIExportable *sdata;

	sdata = get_iexportable_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->stream = stream;

	my_iprogress_start_work( sdata->instance, exportable, NULL );

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export( exportable, settings, getter ));
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'export()' method",
			thisfn, G_OBJECT_TYPE_NAME( exportable ));
	return( FALSE );
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

/**
 * ofa_iexportable_set_line:
 * @exportable: this #ofaIExportable instance.
 * @line: the line to be outputed, silently ignored if empty.
 *
 * The #ofaIExportable implementation code must have taken into account
 * the decimal dot and the field separators specified in provided
 * #ofaStreamFormat settings.
 *
 * The #ofaIExportable interface takes care here of charset conversions.
 *
 * Returns: %TRUE if the line has been successfully written to the
 * output stream.
 */
gboolean
ofa_iexportable_set_line( ofaIExportable *exportable, const gchar *line )
{
	sIExportable *sdata;
	gchar *str, *converted, *msg;
	GError *error;
	gint ret;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );

	if( my_strlen( line )){
		sdata = get_iexportable_data( exportable );
		g_return_val_if_fail( sdata, FALSE );

		error = NULL;

		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( !sdata->count || sdata->count < 100 ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		str = g_strdup_printf( "%s\n", line );
		converted = g_convert( str, -1,
								ofa_stream_format_get_charmap( sdata->settings ),
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

		my_iprogress_pulse( sdata->instance, exportable, sdata->progress, sdata->count );
	}

	return( TRUE );
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
