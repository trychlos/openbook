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

#ifndef __OPENBOOK_API_OFA_IEXPORTABLE_H__
#define __OPENBOOK_API_OFA_IEXPORTABLE_H__

/**
 * SECTION: iexportable
 * @title: ofaIExportable
 * @short_description: The Export Interface
 * @include: openbook/ofa-iexportable.h
 *
 * The #ofaIExportable interface exports items to the outside world.
 *
 * This interface addresses the requested class by the mean of a
 * particular object, most often just allocated for this need.
 *
 * The implementation should begin by counting and advertizing the
 * interface about the total count of lines it expects to output.
 * Then each call to #ofa_iexportable_append_line() method will increment
 * the progress.
 */

#include <glib-object.h>

#include "my/my-iprogress.h"

#include "api/ofa-box.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXPORTABLE                      ( ofa_iexportable_get_type())
#define OFA_IEXPORTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXPORTABLE, ofaIExportable ))
#define OFA_IS_IEXPORTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXPORTABLE ))
#define OFA_IEXPORTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXPORTABLE, ofaIExportableInterface ))

typedef struct _ofaIExportable                    ofaIExportable;

/**
 * ofaIExportableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_basename           [may] returns the default basename of the export file.
 * @get_label              [should] returns the label.
 * @get_published          [should] returns whether the label should be published.
 * @export:                [should] exports a dataset to an URI.
 *
 * This defines the interface that an #ofaIExportable should implement.
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
	guint                  ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_basename:
	 * @instance: the #ofaIExportable provider.
	 *
	 * Returns: the default basename to be used for the export file,
	 * as a newly allocated string which will be g_free() by the caller.
	 *
	 * If not implemented, the interface defaults to the class name of
	 * the @instance.
	 */
	gchar *                ( *get_basename )         ( ofaIExportable *instance );

	/**
	 * get_label:
	 * @instance: the #ofaIExportable provider.
	 *
	 * Returns: the label to be associated with this @instance, as a
	 * newly allocated string which should be g_free() by the caller.
	 *
	 * If the implementation does not return a label (or does not
	 * provide this method), then the @instance will not be advertized
	 * in the export assistant relevant page.
	 *
	 * In order to be advertized, the implementation must also return
	 * %TRUE to the get_published() method below.
	 */
	gchar *                ( *get_label )            ( const ofaIExportable *instance );

	/**
	 * get_published:
	 * @instance: the #ofaIExportable provider.
	 *
	 * Returns: %TRUE if the label returned by get_label() method above
	 * has to be advertized on the relevant page of the export assistant.
	 */
	gboolean               ( *get_published )        ( const ofaIExportable *instance );

	/**
	 * export:
	 * @instance: the #ofaIExportable provider.
	 * @format_id: the name of the export format,
	 *  which defaults to OFA_IEXPORTER_DEFAULT_FORMAT_ID.
	 *
	 * Export the dataset to the named file.
	 *
	 * Returns: %TRUE if the dataset has been successfully exported.
	 */
	gboolean               ( *export )               ( ofaIExportable *instance,
															const gchar *format_id );
}
	ofaIExportableInterface;

/*
 * Interface-wide
 */
GType                 ofa_iexportable_get_type                  ( void );

guint                 ofa_iexportable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint                 ofa_iexportable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gchar                *ofa_iexportable_get_basename              ( ofaIExportable *exportable );

gchar                *ofa_iexportable_get_label                 ( const ofaIExportable *exportable );

gboolean              ofa_iexportable_get_published             ( const ofaIExportable *exportable );

gboolean              ofa_iexportable_export_to_uri             ( ofaIExportable *exportable,
																		const gchar *uri,
																		ofaIExporter *exporter,
																		const gchar *format_id,
																		ofaStreamFormat *stformat,
																		ofaIGetter *getter,
																		myIProgress *progress );

ofaIGetter           *ofa_iexportable_get_getter                ( const ofaIExportable *exportable );

ofaStreamFormat      *ofa_iexportable_get_stream_format         ( const ofaIExportable *exportable );

gulong                ofa_iexportable_get_count                 ( ofaIExportable *exportable );

void                  ofa_iexportable_set_count                 ( ofaIExportable *exportable,
																		gulong count );

gboolean              ofa_iexportable_append_headers            ( ofaIExportable *exportable,
																		guint tables_count,
																		... );

gboolean              ofa_iexportable_append_line               ( ofaIExportable *exportable,
																		const gchar *line );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXPORTABLE_H__ */
