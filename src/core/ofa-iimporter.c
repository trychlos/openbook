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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"

#define IIMPORTER_LAST_VERSION            1

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIImporterInterface *klass );
static void  interface_base_finalize( ofaIImporterInterface *klass );

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
 * ofa_iimporter_get_interface_version:
 * @instance: this #ofaIImporter instance.
 *
 * Returns: the version number implemented.
 */
guint
ofa_iimporter_get_interface_version( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), 0 );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'get_interface_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 1 );
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
 *
 * Returns: the list of mimetypes the @instance importer is able to
 * deal with.
 */
const GList *
ofa_iimporter_get_accepted_contents( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_accepted_contents";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->get_accepted_contents ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->get_accepted_contents( instance ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'get_accepted_contents()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iimporter_get_accept_content:
 * @instance: this #ofaIImporter instance.
 * @content: the (guessed) mimetype of the imported file.
 *
 * Returns: %TRUE if the @instance accepts the @content.
 */
gboolean
ofa_iimporter_get_accept_content( const ofaIImporter *instance, const gchar *content )
{
	gboolean accept;
	const GList *contents, *it;

	g_debug( "ofa_iimporter_get_accept_content: content=%s", content );
	accept = FALSE;
	contents = ofa_iimporter_get_accepted_contents( instance );
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
 * ofa_iimporter_is_willing_to:
 * @instance: this #ofaIImporter instance.
 * @uri: [allow-none]: the uri of the stream to be imported.
 * @type: [allow-none]: the candidate target GType.
 *
 * Returns: %TRUE if the @instance is willing to import @uri to @type.
 */
gboolean
ofa_iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type )
{
	static const gchar *thisfn = "ofa_iimporter_is_willing_to";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), FALSE );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->is_willing_to ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->is_willing_to( instance, uri, type ));
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

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), 0 );

	if( parms->progress ){
		my_iprogress_start_work( parms->progress, instance, NULL );
	}

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->import ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->import( instance, parms ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'import()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 0 );
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
		ofa_iimporter_progress_text( instance, parms, str );
		g_free( str );
	}
}

/**
 * ofa_iimporter_progress_text:
 * @instance: this #ofaIImporter instance.
 * @parms: the arguments of the methods.
 * @text: the text to be displayed.
 *
 * Acts as a proxy to #myIProgress::set_text() method.
 */
void
ofa_iimporter_progress_text( ofaIImporter *instance, ofsImporterParms *parms, const gchar *text )
{
	if( parms->progress ){
		my_iprogress_set_text( parms->progress, instance, text );
	}
}

#if 0
/**
 * ofa_iimporter_get_for_content:
 * @hub: the #ofaHub object of the application.
 * @content: the (guessed) mimetype of the imported file.
 *
 * Returns: a list of #ofaIImporter instances which are willing to deal
 * with the specified mimetype content.
 *
 * The returned list should be #g_list_free_full( L, ( GDestroyNotify ) g_object_unref )
 * by the caller.
 */
GList *
ofa_iimporter_get_for_content( ofaHub *hub, const gchar *content )
{
	GList *selected;
	GList *importers, *it;
	const GList *contents, *ic;
	ofaExtenderCollection *extenders;

	selected = NULL;
	extenders = ofa_hub_get_extender_collection( hub );
	importers = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IIMPORTER );

	for( it=importers ; it ; it=it->next ){
		contents = ofa_iimporter_get_accepted_contents( OFA_IIMPORTER( it->data ));
		for( ic=contents ; ic ; ic=ic->next ){
			if( !my_collate(( const gchar * ) ic->data, content )){
				selected = g_list_prepend( selected, g_object_ref( it->data ));
				break;
			}
		}
	}

	ofa_extender_collection_free_types( importers );

	return( selected );
}
#endif
