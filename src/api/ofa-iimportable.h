/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OPENBOOK_API_OFA_IIMPORTABLE_H__
#define __OPENBOOK_API_OFA_IIMPORTABLE_H__

/**
 * SECTION: iimportable
 * @title: ofaIImportable
 * @short_description: The Import Interface
 * @include: api/ofa-iimportable.h
 *
 * The #ofaIImportable interface imports items from the outside world.
 */

#include "api/ofo-dossier-def.h"

#include "core/ofa-file-format.h"

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
	 * @dossier: the opened dossier.
	 *
	 * Import the dataset from the provided content.
	 *
	 * Return: the count of found errors.
	 *
	 * The recordset must be left unchanged if an error is found.
	 */
	gint     ( *import )               ( ofaIImportable *instance,
												GSList *lines,
												ofoDossier *dossier );
}
	ofaIImportableInterface;

GType    ofa_iimportable_get_type                  ( void );

guint    ofa_iimportable_get_interface_last_version( void );

/* an importer-oriented API
 */
gint     ofa_iimportable_import                    ( ofaIImportable *importable,
															GSList *lines,
															const ofaFileFormat *settings,
															ofoDossier *dossier,
															void *caller );

/* an importable-oriented API
 */
void     ofa_iimportable_set_import_ok             ( ofaIImportable *importable );

void     ofa_iimportable_set_import_error          ( ofaIImportable *importable,
															guint line_number,
															const gchar *msg );

void     ofa_iimportable_set_insert_ok             ( ofaIImportable *importable );

gchar   *ofa_iimportable_convert_date              ( ofaIImportable *importable,
															const gchar *imported_date );

gdouble  ofa_iimportable_convert_amount            ( ofaIImportable *importable,
															const gchar *imported_amount );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTABLE_H__ */
