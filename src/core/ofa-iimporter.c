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

#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"

/* signals defined here
 */
enum {
	PROGRESS = 0,
	PULSE,
	MESSAGE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

#define IIMPORTER_LAST_VERSION            1

#define IIMPORTER_DATA                   "ofa-iimporter-data"

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
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	if( !st_initializations ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIImporter::progress:
		 *
		 * This signal is to be sent to the importer by an importable
		 * in order to let the former visually render the import
		 * progression of the later.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIImporter        *importer,
		 *						ofeImportablePhase phase
		 * 						gdouble            progress,
		 * 						const gchar       *text,
		 * 						gpointer           user_data );
		 */
		st_signals[ PROGRESS ] = g_signal_new_class_handler(
					"progress",
					interface_type,
					G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_UINT, G_TYPE_DOUBLE, G_TYPE_STRING );

		/**
		 * ofaIImporter::pulse:
		 *
		 * This signal is to be sent to the importer by an importable
		 * in order to let the former visually render the import
		 * progression of the later.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIImporter        *importer,
		 *						ofeImportablePhase phase,
		 * 						gpointer           user_data );
		 */
		st_signals[ PULSE ] = g_signal_new_class_handler(
					"pulse",
					interface_type,
					G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_UINT );

		/**
		 * ofaIImporter::message:
		 *
		 * This signal is to be sent to the importer by an importable
		 * in order to let the former receive a message during
		 * the import or insert operations.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIImporter      *importer,
		 * 						guint            line_number,
		 * 						ofeImportableMsg status,
		 * 						const gchar     *message,
		 * 						gpointer         user_data );
		 */
		st_signals[ MESSAGE ] = g_signal_new_class_handler(
					"message",
					interface_type,
					G_SIGNAL_ACTION,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIImporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iimporter_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

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
 * ofa_iimporter_get_label:
 * @instance: this #ofaIImporter instance.
 *
 * Returns: the label to be associated with the @instance importer,
 * as a newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_iimporter_get_label( const ofaIImporter *instance )
{
	static const gchar *thisfn = "ofa_iimporter_get_label";

	g_return_val_if_fail( instance && OFA_IS_IIMPORTER( instance ), NULL );

	if( OFA_IIMPORTER_GET_INTERFACE( instance )->get_label ){
		return( OFA_IIMPORTER_GET_INTERFACE( instance )->get_label( instance ));
	}

	g_info( "%s: ofaIImporter's %s implementation does not provide 'get_label()' method",
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

/**
 * ofa_iimporter_import_from_uri:
 * @importer: this #ofaIImporter instance.
 * @parms: a #ofaIImporterParms structure.
 *
 * Tries to import data from the URI specified in @parms, returning
 * the result in <structfield>@parms->imported</structfield>.
 *
 * On input of this function, only parms->uri is set.
 * parms->messages may be set or not, but should not be reinitialized
 * by the importer plugin.
 *
 * It is the responsability of the importer to set the other datas.
 *
 * Returns: the return code of the operation.
 */

guint
ofa_iimporter_import_from_uri( const ofaIImporter *importer, ofaIImporterParms *parms )
{
	static const gchar *thisfn = "ofa_iimporter_import_from_uri";
	guint code;

	g_return_val_if_fail( OFA_IS_IIMPORTER( importer ), IMPORTER_CODE_PROGRAM_ERROR );
	g_return_val_if_fail( parms, IMPORTER_CODE_PROGRAM_ERROR );

	code = IMPORTER_CODE_NOT_WILLING_TO;

	g_debug( "%s: importer=%p (%s), parms=%p", thisfn,
			( void * ) importer, G_OBJECT_TYPE_NAME( importer), ( void * ) parms );

	if( OFA_IIMPORTER_GET_INTERFACE( importer )->import_from_uri ){
		code = OFA_IIMPORTER_GET_INTERFACE( importer )->import_from_uri( importer, parms );
	}

	return( code );
}

static void
free_output_sbatv1( ofaIImporterSBatv1 *bat )
{
	g_free( bat->currency );
	g_free( bat->label );
	g_free( bat->ref );
	g_free( bat );
}

/**
 * ofa_iimporter_free_output:
 *
 * Free the structure which handles a bank account transaction
 */
void
ofa_iimporter_free_output( ofaIImporterParms *parms )
{
	static const gchar *thisfn = "ofa_iimporter_free_output";

	g_free( parms->format );

	switch( parms->type ){

		case IMPORTER_TYPE_BAT:
			if( parms->version == 1 ){
				g_free( parms->batv1.rib );
				g_free( parms->batv1.currency );
				g_list_free_full( parms->batv1.results, ( GDestroyNotify ) free_output_sbatv1 );
				memset( &parms->batv1, '\0', sizeof( ofaIImporterSBatv1 ));
			} else {
				g_warning( "%s: IMPORTER_TYPE_BAT: version=%d", thisfn, parms->version );
			}
			break;

		default:
			g_debug( "%s: TO BE WRITTEN", thisfn );
	}
}
#endif
