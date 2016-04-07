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
 * The #ofaIImportable interface must be implemented by objects which
 * want to be imported in the application: the object class
 * implementation is provided with a list of lines, each line being
 * itself a list of fields.
 *
 * The #ofaIImporter is expected to take care of splitting the input
 * stream per line, and of splitting each line per field, according to
 * the provided stream format.
 *
 * It is the responsability of the #ofaIImportable implementation:
 * - to fill up the provided object with the provided fields contents,
 * - to advertize the importer with an eventual error,
 * - to advertize the importer with its progress.
 */

#include "api/ofa-box.h"
#include "api/ofa-hub-def.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-stream-format.h"
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
 * @get_label:             [should] returns the label asssociated to the class.
 * @import:                [should] imports an input stream.
 *
 * This defines the interface that an #ofaIImportable should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/**
	 * import:
	 * @importer: the #ofaIImporter instance.
	 * @parms: the #ofsImporterParms import arguments.
	 * @lines: the lines of the imported file, as a #GSList list of
	 *  lines, where line->data is a #GSList of fields values.
	 *
	 * Import the dataset from the provided content.
	 *
	 * Return: the total count of errors.
	 *
	 * Since: version 1.
	 */
	guint     ( *import )              ( ofaIImporter *importer,
												ofsImporterParms *parms,
												GSList *lines );

	/*** instance-wide ***/
	/**
	 * get_label:
	 * @instance: the #ofaIImportable provider.
	 *
	 * Returns: the label to be associated to the class.
	 */
	gchar *  ( *get_label )            ( const ofaIImportable *instance );
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

/*
 * Interface-wide
 */
GType           ofa_iimportable_get_type                  ( void );

guint           ofa_iimportable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint           ofa_iimportable_get_interface_version     ( GType type );

guint           ofa_iimportable_import                    ( GType type,
																ofaIImporter *importer,
																ofsImporterParms *parms,
																GSList *lines );

/*
 * Instance-wide
 */
gchar          *ofa_iimportable_get_label                 ( const ofaIImportable *importable );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTABLE_H__ */
