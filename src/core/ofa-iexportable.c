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
#include <stdlib.h>
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
	ofaIGetter      *getter;
	ofaStreamFormat *stformat;
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
static gboolean      iexportable_export_to_stream( ofaIExportable *exportable, GOutputStream *stream, const gchar *format_id );
static sIExportable *get_instance_data( const ofaIExportable *exportable );
static void          on_instance_finalized( sIExportable *sdata, GObject *finalized_object );

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
 * @exportable: this #ofaIExportable instance.
 *
 * Returns: the displayable label to be associated with @exportable, as a
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
ofa_iexportable_get_label( const ofaIExportable *exportable )
{
	static const gchar *thisfn = "ofa_iexportable_get_label";

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), NULL );

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->get_label ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->get_label( exportable ));
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'get_label()' method",
			thisfn, G_OBJECT_TYPE_NAME( exportable ));
	return( NULL );
}

/**
 * ofa_iexporter_get_formats:
 * @exportable: this #ofaIExportable instance.
 *
 * Returns: %NULL, or a null-terminated array of #ofsIExportableFormat
 * structures.
 */
ofsIExportableFormat *
ofa_iexportable_get_formats( ofaIExportable *exportable )
{
	static const gchar *thisfn = "ofa_iexportable_get_formats";

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), NULL );

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->get_formats ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->get_formats( exportable ));
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'get_formats()' method",
			thisfn, G_OBJECT_TYPE_NAME( exportable ));
	return( NULL );
}

/**
 * ofa_iexporter_free_formats:
 * @exportable: this #ofaIExportable instance.
 * @formats: [allow-none]: the #ofsIExportableFormat array as returned
 *  by ofa_iexportable_get_formats() function.
 *
 * Let the implementation release the @formats resources.
 */
void
ofa_iexportable_free_formats( ofaIExportable *exportable, ofsIExportableFormat *formats )
{
	static const gchar *thisfn = "ofa_iexportable_free_formats";

	g_return_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ));

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->free_formats ){
		OFA_IEXPORTABLE_GET_INTERFACE( exportable )->free_formats( exportable, formats );
		return;
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'free_formats()' method",
			thisfn, G_OBJECT_TYPE_NAME( exportable ));
}

/**
 * ofa_iexportable_export_to_uri:
 * @exportable: a #ofaIExportable instance.
 * @uri: the output URI,
 *  will be overriden without any further confirmation if already exists.
 * @format_id: the identifier of the export format.
 * @stformat: a #ofaStreamFormat object.
 * @getter: a #ofaIGetter instance.
 * @progress: the #myIProgress instance which display the export progress.
 *
 * Export the specified dataset to the named file.
 *
 * Returns: %TRUE if the export has successfully completed.
 */
gboolean
ofa_iexportable_export_to_uri( ofaIExportable *exportable,
									const gchar *uri, const gchar *format_id, ofaStreamFormat *stformat,
									ofaIGetter *getter, myIProgress *progress )
{
	GFile *output_file;
	sIExportable *sdata;
	GOutputStream *output_stream;
	gboolean ok;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );
	g_return_val_if_fail( stformat && OFA_IS_STREAM_FORMAT( stformat ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	sdata = get_instance_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->getter = getter;
	sdata->stformat = stformat;
	sdata->instance = progress;
	sdata->count = 0;
	sdata->progress = 0;

	if( !my_utils_output_stream_new( uri, &output_file, &output_stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( output_stream ), FALSE );

	ok = iexportable_export_to_stream( exportable, output_stream, format_id );

	g_output_stream_close( output_stream, NULL, NULL );
	g_object_unref( output_file );

	return( ok );
}

static gboolean
iexportable_export_to_stream( ofaIExportable *exportable, GOutputStream *stream, const gchar *format_id )
{
	static const gchar *thisfn = "ofa_iexportable_export_to_stream";
	sIExportable *sdata;

	sdata = get_instance_data( exportable );
	g_return_val_if_fail( sdata, FALSE );

	sdata->stream = stream;

	my_iprogress_start_work( sdata->instance, exportable, NULL );

	if( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export ){
		return( OFA_IEXPORTABLE_GET_INTERFACE( exportable )->export( exportable, format_id ));
	}

	g_info( "%s: ofaIExportable's %s implementation does not provide 'export()' method",
			thisfn, G_OBJECT_TYPE_NAME( exportable ));
	return( FALSE );
}

/**
 * ofa_iexporter_get_getter:
 * @exportable: this #ofaIExportable instance.
 *
 * Returns: the #ofaIGetter instance provided to ofa_iexportable_export_to_uri().
 */
ofaIGetter *
ofa_iexportable_get_getter( const ofaIExportable *exportable )
{
	sIExportable *sdata;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), NULL );

	sdata = get_instance_data( exportable );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->getter );
}

/**
 * ofa_iexporter_get_stream_format:
 * @exportable: this #ofaIExportable instance.
 *
 * Returns: the #ofaStreamformat object provided to ofa_iexportable_export_to_uri().
 */
ofaStreamFormat *
ofa_iexportable_get_stream_format( const ofaIExportable *exportable )
{
	sIExportable *sdata;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), NULL );

	sdata = get_instance_data( exportable );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->stformat );
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

	sdata = get_instance_data( exportable );
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

	sdata = get_instance_data( exportable );
	g_return_if_fail( sdata );

	sdata->count = count;
}

/**
 * ofa_iexportable_append_headers:
 * @exportable: this #ofaIExportable instance.
 * @tables_count: the count of tables.
 *
 * Export the headers corresponding to the provided table definitions.
 *
 * Exactly @tables_count #ofsBoxDef arrays must be provided after the
 * @tables_count argument.
 *
 * Returns: %TRUE if the headers have been successfully written to the
 * output stream.
 */
gboolean
ofa_iexportable_append_headers( ofaIExportable *exportable, guint tables_count, ... )
{
	sIExportable *sdata;
	gchar *str1, *str2;
	va_list ap;
	guint i;
	ofsBoxDef *box_def;
	gboolean ok;
	gchar field_sep;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );

	sdata = get_instance_data( exportable );
	g_return_val_if_fail( sdata->stformat && OFA_IS_STREAM_FORMAT( sdata->stformat ), FALSE );

	ok = TRUE;

	if( ofa_stream_format_get_with_headers( sdata->stformat )){
		field_sep = ofa_stream_format_get_field_sep( sdata->stformat );
		va_start( ap, tables_count );

		for( i=0 ; i<tables_count && ok ; ++i ){
			box_def = ( ofsBoxDef * ) va_arg( ap, ofsBoxDef * );
			str1 = ofa_box_csv_get_header( box_def, sdata->stformat );
			str2 = g_strdup_printf( "0%c%u%c%s", field_sep, i+1, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		va_end( ap );
	}

	return( ok );
}

/**
 * ofa_iexportable_append_line:
 * @exportable: this #ofaIExportable instance.
 * @line: the line to be outputed, silently ignored if empty.
 *
 * The #ofaIExportable implementation code must have taken into account
 * the decimal dot and the field separators specified in provided
 * #ofaStreamFormat stformat.
 *
 * The #ofaIExportable interface takes care here of charset conversions.
 *
 * Returns: %TRUE if the line has been successfully written to the
 * output stream.
 */
gboolean
ofa_iexportable_append_line( ofaIExportable *exportable, const gchar *line )
{
	sIExportable *sdata;
	gchar *str, *converted, *msg, *str2;
	GError *error;
	gint ret;
	gsize bytes_read;
	const gchar *dest_codeset;

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );

	if( my_strlen( line )){
		sdata = get_instance_data( exportable );
		g_return_val_if_fail( sdata, FALSE );

		error = NULL;

		/* a small delay so that user actually see the progression
		 * else it is too fast and we just see the end */
		if( !sdata->count || sdata->count < 100 ){
			g_usleep( 0.01*G_USEC_PER_SEC );
		}

		str = g_strdup_printf( "%s\n", line );
		dest_codeset = ofa_stream_format_get_charmap( sdata->stformat );

		/* pwi 2017- 3-23 It happens that g_convert doesn't know how to
		 * convert from long dash (utf8) to dash (iso-8859-15)
		 * help it in this matter */
		if( !g_str_has_prefix( dest_codeset, "UTF" )){
			str2 = my_utils_subst_long_dash( str );
			g_free( str );
			str = str2;
		}

		converted = g_convert( str, -1, dest_codeset, "UTF-8", &bytes_read, NULL, &error );
		if( !converted ){
			msg = g_strdup_printf(
					_( "Charset conversion error: %s, str='%s', bytes_read=%lu" ),
					error->message, str, bytes_read );
			//my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, msg );
			g_warning( "%s", msg );
			g_free( msg );
		}
		if( !converted ){
			converted = g_strdup( str );
		}
		g_free( str );
#if 0
		if( !converted ){
			return( FALSE );
		}
#endif

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
get_instance_data( const ofaIExportable *exportable )
{
	sIExportable *sdata;

	sdata = ( sIExportable * ) g_object_get_data( G_OBJECT( exportable ), IEXPORTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIExportable, 1 );
		g_object_set_data( G_OBJECT( exportable ), IEXPORTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( exportable ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIExportable *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_iexportable_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata );
}
