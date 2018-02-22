/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>

#include "my/my-char.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"

#define IIMPORTER_LAST_VERSION            1

static guint st_initializations = 0;	/* interface initialization count */

static GType            register_type( void );
static void             interface_base_init( ofaIImporterInterface *klass );
static void             interface_base_finalize( ofaIImporterInterface *klass );
static ofaStreamFormat *get_default_stream_format( ofaIGetter *getter );
static void             free_fields( GSList *fields );

/**
 * ofa_iimporter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iimporter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iimporter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iimporter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIImporterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIImporter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIImporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iimporter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIImporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iimporter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iimporter_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iimporter_get_interface_last_version( void )
{
	return( IIMPORTER_LAST_VERSION );
}

/**
 * ofa_iimporter_find_willing_to:
 * @getter: a #ofaIGetter instance.
 * @uri: [allow-none]: the uri of the stream to be imported.
 * @type: [allow-none]: the candidate target GType.
 *
 * Returns: a #GList of willing-to importers, as new references which
 * should be #g_list_free_full( list, ( GDestroyNotify ) g_object_unref )
 * by the caller.
 */
GList *
ofa_iimporter_find_willing_to( ofaIGetter *getter, const gchar *uri, GType type )
{
	static const gchar *thisfn = "ofa_iimporter_find_willing_to";
	GList *willing_to, *importers, *it;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	willing_to = NULL;

	importers = ofa_igetter_get_for_type( getter, OFA_TYPE_IIMPORTER );
	for( it=importers ; it ; it=it->next ){
		g_debug( "%s: importer=%p (%s)", thisfn, it->data, G_OBJECT_TYPE_NAME( it->data ));
		if( ofa_iimporter_is_willing_to( OFA_IIMPORTER( it->data ), getter, uri, type )){
			willing_to = g_list_prepend( willing_to, g_object_ref( it->data ));
		}
	}
	g_list_free( importers );

	return( willing_to );
}

/**
 * ofa_iimporter_get_interface_version:
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
ofa_iimporter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IIMPORTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIImporterInterface * ) iface )->get_interface_version ){
		version = (( ofaIImporterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIImporter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iimporter_get_canon_name:
 * @instance: this #ofaIImporter instance.
 *
 * Returns: the canonical name to be associated with the @instance
 * provided that it implements the myIIdent interface, or %NULL
 */
gchar *
ofa_iimporter_get_canon_name( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_canon_name";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );

	if( MY_IS_IIDENT( instance )){
		return( my_iident_get_canon_name( MY_IIDENT( instance ), NULL ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not implement myIIdent interface",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iimporter_get_display_name:
 * @instance: this #ofaIImporter instance.
 *
 * Returns: the displayable name to be associated with the @instance
 * provided that it implements the myIIdent interface, or %NULL
 */
gchar *
ofa_iimporter_get_display_name( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_display_name";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );

	if( MY_IS_IIDENT( instance )){
		return( my_iident_get_display_name( MY_IIDENT( instance ), NULL ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not implement myIIdent interface",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iimporter_get_version:
 * @instance: this #ofaIImporter instance.
 *
 * Returns: the version to be associated with the @instance
 * provided that it implements the myIIdent interface, or %NULL
 */
gchar *
ofa_iimporter_get_version( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_version";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );

	if( MY_IS_IIDENT( instance )){
		return( my_iident_get_version( MY_IIDENT( instance ), NULL ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not implement myIIdent interface",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iimporter_get_accepted_contents:
 * @instance: this #ofaIImporter instance.
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of mimetypes the @instance importer is able to
 * deal with.
 */
const GList *
ofa_iimporter_get_accepted_contents( const ofaIImporter *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iimporter_get_accepted_contents";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->get_accepted_contents ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->get_accepted_contents( instance, getter ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'get_accepted_contents()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iimporter_get_accept_content:
 * @instance: this #ofaIImporter instance.
 * @getter: a #ofaIGetter instance.
 * @content: the (guessed) mimetype of the imported file.
 *
 * Returns: %TRUE if the @instance accepts the @content.
 */
gboolean
ofa_iimporter_get_accept_content( const ofaIImporter *instance, ofaIGetter *getter, const gchar *content )
{
	gboolean accept;
	const GList *contents, *it;

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	g_debug( "ofa_iimporter_get_accept_content: content=%s", content );
	accept = FALSE;
	contents = ofa_iimporter_get_accepted_contents( instance, getter );
	for( it=contents ; it ; it=it->next ){
		g_debug( "ofa_iimporter_get_accept_content: it_data=%s", ( const gchar * ) it->data );
		if( !my_collate(( const gchar * ) it->data, content )){
			accept = TRUE;
			break;
		}
	}

	return( accept );
}

/**
 * ofa_iimporter_get_default_format:
 * @instance: this #ofaIImporter instance.
 * @getter: a #ofaIGetter instance.
 * @is_user_modifiable: [allow-none][out]: whether the returned
 *  #ofaStreamFormat (if any) is modifiable by the user.
 *
 * Returns: a new #ofaStreamFormat instance, or %NULL.
 *
 * Since: version 1.
 */
ofaStreamFormat *
ofa_iimporter_get_default_format( const ofaIImporter *instance, ofaIGetter *getter, gboolean *is_user_modifiable )
{
	static const gchar *thisfn = "ofa_iimporter_get_default_format";
	ofaStreamFormat *format;

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->get_default_format ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->get_default_format( instance, getter, is_user_modifiable ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'get_default_format()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	format = get_default_stream_format( getter );

	return( format );
}

static ofaStreamFormat *
get_default_stream_format( ofaIGetter *getter )
{
	ofaStreamFormat *format;

	format = ofa_stream_format_new( getter, NULL, OFA_SFMODE_IMPORT );

	ofa_stream_format_set( format,
			TRUE,  "UTF-8",
			TRUE,  MY_DATE_SQL,
			FALSE, MY_CHAR_ZERO,
			FALSE, MY_CHAR_COMMA,
			FALSE, MY_CHAR_TAB,
			FALSE, MY_CHAR_ZERO,
			0 );

	ofa_stream_format_set_field_updatable( format, OFA_SFHAS_ALL, TRUE );

	return( format );
}

/**
 * ofa_iimporter_is_willing_to:
 * @instance: this #ofaIImporter instance.
 * @getter: a #ofaIGetter instance.
 * @uri: [allow-none]: the uri of the stream to be imported.
 * @type: [allow-none]: the candidate target GType.
 *
 * Returns: %TRUE if the @instance is willing to import @uri to @type.
 */
gboolean
ofa_iimporter_is_willing_to( const ofaIImporter *instance, ofaIGetter *getter, const gchar *uri, GType type )
{
	static const gchar *thisfn = "ofa_iimporter_is_willing_to";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->is_willing_to ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->is_willing_to( instance, getter, uri, type ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'is_willing_to()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( TRUE );
}

/**
 * ofa_iimporter_import:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 *
 * Returns: the total count of errors.
 */
guint
ofa_iimporter_import( ofaIImporter *instance, ofsImporterParms *parms )
{
	static const gchar *thisfn = "ofa_iimporter_import";
	GSList *lines;
	guint error_count, headers_count;
	gint count;
	gchar *msgerr;

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), 1 );

	if( parms->progress ){
		my_iprogress_start_work( parms->progress, instance, NULL );
	}

	lines = NULL;
	msgerr = NULL;
	error_count = 0;

	/* first parse the input stream to a GSList of lines, each line data
	 * being itself a GSList of fields
	 */
	if( OFA_IIMPORTER_GET_INTERFACE( instance )->parse ){
		lines = OFA_IIMPORTER_GET_INTERFACE( instance )->parse( instance, parms, &msgerr );
	} else {
		g_info( "%s: ofaIImporter's %s implementation does not provide 'parse()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}

	/* then import the parsed datas
	 */
	if( msgerr ){
		ofa_iimporter_progress_text( instance, parms, MY_PROGRESS_ERROR, msgerr );
		g_free( msgerr );
		error_count += 1;

	} else if( lines ){
		if( 0 ){
			my_utils_dump_gslist_str( lines );
		}
		parms->lines_count = g_slist_length( lines );
		headers_count = ofa_stream_format_get_headers_count( parms->format );
		count = parms->lines_count - headers_count;

		if( count > 0 ){
			error_count = ofa_iimportable_import( parms->type, instance, parms, g_slist_nth( lines, headers_count ));

		} else if( count < 0 ){
			error_count += 1;
			msgerr = g_strdup_printf(
							_( "Expected headers count=%u greater than count of lines=%u read from '%s' file" ),
								headers_count, parms->lines_count, parms->uri );
			ofa_iimporter_progress_text( instance, parms, MY_PROGRESS_ERROR, msgerr );
			g_free( msgerr );
		}

		g_slist_free_full( lines, ( GDestroyNotify ) free_fields );

	} else {
		msgerr = g_strdup( _( "empty parsed set" ));
		ofa_iimporter_progress_text( instance, parms, MY_PROGRESS_NORMAL, msgerr );
		g_free( msgerr );
	}

	return( error_count );
}

static void
free_fields( GSList *fields )
{
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );
}

/**
 * ofa_iimporter_progress_start:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 *
 * Acts as a proxy to #myIProgress::start_progress() method.
 */
void
ofa_iimporter_progress_start( ofaIImporter *instance, ofsImporterParms *parms )
{
	if( parms->progress ){
		my_iprogress_start_progress( parms->progress, instance, NULL, TRUE );
	}
}

/**
 * ofa_iimporter_progress_pulse:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 * @count: the current count.
 * @total: the total count.
 *
 * Acts as a proxy to #myIProgress::pulse() method.
 */
void
ofa_iimporter_progress_pulse( ofaIImporter *instance, ofsImporterParms *parms, gulong count, gulong total )
{
	if( parms->progress ){
		my_iprogress_pulse( parms->progress, instance, count, total );
	}
}

/**
 * ofa_iimporter_progress_num_text:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 * @text: the text to be displayed.
 *
 * Acts as a proxy to #myIProgress::set_text() method.
 */
void
ofa_iimporter_progress_num_text( ofaIImporter *instance, ofsImporterParms *parms, guint numline, const gchar *text )
{
	gchar *str;

	if( parms->progress ){
		str = g_strdup_printf( "[%u] %s\n", numline, text );
		ofa_iimporter_progress_text( instance, parms, MY_PROGRESS_NONE, str );
		g_free( str );
	}
}

/**
 * ofa_iimporter_progress_text:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 * @type: the type of the @text.
 * @text: the text to be displayed.
 *
 * Acts as a proxy to #myIProgress::set_text() method.
 */
void
ofa_iimporter_progress_text( ofaIImporter *instance, ofsImporterParms *parms, guint type, const gchar *text )
{
	if( parms->progress ){
		my_iprogress_set_text( parms->progress, instance, type, text );
	}
}
