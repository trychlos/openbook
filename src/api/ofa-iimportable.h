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

#ifndef __OPENBOOK_API_OFA_IIMPORTABLE_H__
#define __OPENBOOK_API_OFA_IIMPORTABLE_H__

/**
 * SECTION: iimportable
 * @title: ofaIImportable
 * @short_description: The Import Interface
 * @include: openbook/ofa-iimportable.h
 *
 * The #ofaIImportable interface imports items from the outside world.
 *
 * The #ofaIImportable interface is supposed to be implemented either
 * by object classes which want to be imported (so are "importable"),
 * or by the plugins which manage the import of files.
 *
 * The interface provides several function to let the implementation
 * communicate with the caller (see infra for what is a caller):
 * - increment the count of imported lines
 * - send a standard, warning or error message to the caller.
 * At the end, the implementation must return the count of found errors.
 *
 * A caller which would take advantage of this mechanism should implement
 * the #ofaIImporter interface. This later defines ad-hoc signals which
 * are used by the above functions.
 */

#include "api/ofa-box.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IIMPORTABLE                      ( ofa_iimportable_get_type())
#define OFA_IIMPORTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IIMPORTABLE, ofaIImportable ))
#define OFA_IS_IIMPORTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IIMPORTABLE ))
#define OFA_IIMPORTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IIMPORTABLE, ofaIImportableInterface ))

typedef struct _ofaIImportable                    ofaIImportable;

/**
 * ofaIImportableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @import:                [should] imports a dataset.
 * @is_willing_to:         [should] is this instance willing to import the uri ?
 * @import uri:            [should] tries to import a file.
 *
 * This defines the interface that an #ofaIImportable should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIImportable provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIImportable *instance );

	/**
	 * import:
	 * @instance: the #ofaIImportable provider.
	 * @lines: the lines of the imported file, as a #GSList list of
	 *  lines, where line->data is a #GSList of fields values.
	 * @hub: the current #ofaHub object.
	 *
	 * Import the dataset from the provided content.
	 *
	 * Return: the count of found errors.
	 *
	 * The recordset must be left unchanged if an error is found.
	 */
	gint     ( *import )               ( ofaIImportable *instance,
												GSList *lines,
												const ofaFileFormat *settings,
												ofaHub *hub );

	/**
	 * is_willing_to:
	 * @instance: the #ofaIImportable provider.
	 * @uri: the URI to be imported.
	 * @settings: the (supposed) input file format.
	 * @ref: [out]: the internal ref of the provider
	 * @count: [out]: the count of records to be imported.
	 *
	 * Return: %TRUE if the provider is willing to import this file.
	 */
	gboolean ( *is_willing_to )        ( ofaIImportable *instance,
												const gchar *uri,
												const ofaFileFormat *settings,
												void **ref,
												guint *count );

	/**
	 * import_uri:
	 * @instance: the #ofaIImportable provider.
	 * @ref: the internal ref of the provider as returned from #is_willing_to().
	 * @hub: the current #ofaHub object.
	 * @imported_id: [allow-none][out]: if non %NULL, then must point to an
	 *  #ofxCounter which will be set to the identifier of the newly
	 *  allocated #ofoBat object.
	 *
	 * Import the specified @uri.
	 *
	 * Return: the count of errors.
	 */
	guint    ( *import_uri )           ( ofaIImportable *instance,
												void *ref,
												const gchar *uri,
												const ofaFileFormat *settings,
												ofaHub *hub,
												ofxCounter *imported_id );
}
	ofaIImportableInterface;

/**
 * The import phase
 */
typedef enum {
	IMPORTABLE_PHASE_IMPORT = 1,
	IMPORTABLE_PHASE_INSERT,
}
	ofeImportablePhase;

/**
 * The nature of the message
 */
typedef enum {
	IMPORTABLE_MSG_STANDARD = 1,
	IMPORTABLE_MSG_WARNING,
	IMPORTABLE_MSG_ERROR,
}
	ofeImportableMsg;

GType           ofa_iimportable_get_type                  ( void );

guint           ofa_iimportable_get_interface_last_version( void );

/* an importer-oriented API
 */
ofaIImportable *ofa_iimportable_find_willing_to   ( const gchar *uri, const ofaFileFormat *settings );

gint            ofa_iimportable_import            ( ofaIImportable *importable,
															GSList *lines,
															const ofaFileFormat *settings,
															ofaHub *hub,
															void *caller );

guint           ofa_iimportable_import_uri        ( ofaIImportable *importable,
															ofaHub *hub,
															void *caller,
															ofxCounter *imported_id );

guint           ofa_iimportable_get_count         ( ofaIImportable *importable );

void            ofa_iimportable_set_count         ( ofaIImportable *importable,
															guint count );

/* an importable-oriented API
 */
gchar          *ofa_iimportable_get_string        ( GSList **it );

void            ofa_iimportable_pulse             ( ofaIImportable *importable,
															ofeImportablePhase phase );

void            ofa_iimportable_increment_progress( ofaIImportable *importable,
															ofeImportablePhase phase,
															guint count );

void            ofa_iimportable_set_message       ( ofaIImportable *importable,
															guint line_number,
															ofeImportableMsg status,
															const gchar *msg );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTABLE_H__ */
