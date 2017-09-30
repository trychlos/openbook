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

#ifndef __OPENBOOK_API_OFA_IEXPORTER_H__
#define __OPENBOOK_API_OFA_IEXPORTER_H__

/**
 * SECTION: iexporter
 * @title: ofaIExporter
 * @short_description: The Export Interface
 * @include: openbook/ofa-iexporter.h
 *
 * The #ofaIExporter interface exports items to the outside world.
 */

#include <glib-object.h>

#include "api/ofa-iexportable-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXPORTER                      ( ofa_iexporter_get_type())
#define OFA_IEXPORTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXPORTER, ofaIExporter ))
#define OFA_IS_IEXPORTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXPORTER ))
#define OFA_IEXPORTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXPORTER, ofaIExporterInterface ))

typedef struct _ofaIExporter                    ofaIExporter;
typedef struct _ofsIExporterFormat              ofsIExporterFormat;

/**
 * ofaIExporterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_formats: [should] returns available formats.
 *
 * This defines the interface that an #ofaIExporter should implement.
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
	 * get_formats:
	 * @instance: the #ofaIExporter provider.
	 * @type: the #GType which is candidate to export;
	 *  the corresponding class must implement #ofaIExportable interface.
	 * @getter: the #ofaIGetter of the application.
	 *
	 * Returns: a null-terminated array of specific #ofsIExporterFormat
	 * structures managed by the target @instance class.
	 */
	ofsIExporterFormat * ( *get_formats )          ( ofaIExporter *instance,
														GType type,
														ofaIGetter *getter );

	/**
	 * export:
	 * @instance: the #ofaIExporter provider.
	 * @exportable: the #ofaIExportable to be exported.
	 * @format_id: the name of the export format.
	 *
	 * Returns: %TRUE if the export has been successful.
	 */
	gboolean             ( *export )               ( ofaIExporter *instance,
														ofaIExportable *exportable,
														const gchar *format_id );
}
	ofaIExporterInterface;

/**
 * ofsIExporterFormat:
 * @format_id: a string which identifies the format.
 * @format_label: a localized string to be displayed.
 *
 * A structure which defines a specific export format for a target
 * class.
 *
 * A null-terminated array of these structures has to be provided in
 * answer to the get_formats() method.
 */
struct _ofsIExporterFormat {
	gchar           *format_id;
	gchar           *format_label;
	ofaStreamFormat *stream_format;
};

#define OFA_IEXPORTER_DEFAULT_FORMAT_ID   "DEFAULT"

/*
 * Interface-wide
 */
GType               ofa_iexporter_get_type                  ( void );

guint               ofa_iexporter_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint               ofa_iexporter_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofsIExporterFormat *ofa_iexporter_get_formats               ( ofaIExporter *exporter,
																	GType type,
																	ofaIGetter *getter );

gboolean            ofa_iexporter_export                    ( ofaIExporter *exporter,
																	ofaIExportable *exportable,
																	const gchar *format_id );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXPORTER_H__ */
