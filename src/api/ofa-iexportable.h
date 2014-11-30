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

#ifndef __OPENBOOK_API_OFA_IEXPORTABLE_H__
#define __OPENBOOK_API_OFA_IEXPORTABLE_H__

/**
 * SECTION: iexportable
 * @title: ofaIExportable
 * @short_description: The Export Interface
 * @include: api/ofa-iexportable.h
 *
 * The #ofaIExportable interface exports items to the outside world.
 */

#include "api/ofo-dossier-def.h"

#include "core/ofa-export-settings.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXPORTABLE                      ( ofa_iexportable_get_type())
#define OFA_IEXPORTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXPORTABLE, ofaIExportable ))
#define OFA_IS_IEXPORTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXPORTABLE ))
#define OFA_IEXPORTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXPORTABLE, ofaIExportableInterface ))

typedef struct _ofaIExportable                    ofaIExportable;

/**
 * Two callbacks which let the ofaIEXportable instance announce the
 * progression of the export. It is advised to use them on each exported
 * line.
 *
 * The two callbacks are to be called with the provided @instance data.
 */
typedef void ( *ofaIExportableFnDouble )( const void *instance, gdouble progress );
typedef void ( *ofaIExportableFnText )  ( const void *instance, const gchar *text );

/**
 * ofaIExportableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @export:                [should] exports a dataset.
 *
 * This defines the interface that an #ofaIExportable should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIExportable provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIExportable *instance );

	/**
	 * export_as_csv:
	 * @instance: the #ofaIExportable provider.
	 * @settings: the current export settings for the operation.
	 * @dossier: the opened dossier.
	 *
	 * Export the dataset to the named file.
	 *
	 * Return: %TRUE if the dataset has been successfully exported.
	 */
	gboolean ( *export )               ( ofaIExportable *instance,
												const ofaExportSettings *settings,
												ofoDossier *dossier );
}
	ofaIExportableInterface;

GType    ofa_iexportable_get_type                  ( void );

guint    ofa_iexportable_get_interface_last_version( void );

gboolean ofa_iexportable_export_to_path            ( ofaIExportable *exportable,
															const gchar *fname,
															const ofaExportSettings *settings,
															ofoDossier *dossier,
															const ofaIExportableFnDouble fn_double,
															const ofaIExportableFnText fn_text,
															const void *instance );

gulong   ofa_iexportable_get_count                 ( ofaIExportable *exportable );

void     ofa_iexportable_set_count                 ( ofaIExportable *exportable,
															gulong count );

gboolean ofa_iexportable_export_lines              ( ofaIExportable *exportable,
															GSList *lines );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXPORTABLE_H__ */
