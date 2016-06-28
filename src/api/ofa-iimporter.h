/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IIMPORTER_H__
#define __OPENBOOK_API_OFA_IIMPORTER_H__

/**
 * SECTION: iimporter
 * @title: ofaIImporter
 * @short_description: The Import Interface
 * @include: openbook/ofa-iimporter.h
 *
 * The #ofaIImporter interface is called by the application in order to
 * try to import objects from an external stream.
 *
 * The provider (the implementation) should try to transform the
 * specified uri file content to a list of ofoBase-derived objects.
 *
 * The #ofaIImporter lets the importable object communicate with the
 * importer code. Two signals are defined:
 * - "progress" to visually render the progress of the import (resp.
 *   the insertion in the DBMS)
 * - "message" to display a standard, warning or error message during
 *   the import (resp. the DBMS insertion).
 *
 * The #ofaIImporter implementation should also implement #myIIdent
 * interface.
 */

#include <glib-object.h>

#include "my/my-iprogress.h"

#include "api/ofa-hub-def.h"
#include "api/ofa-import-duplicate.h"
#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_IIMPORTER                      ( ofa_iimporter_get_type())
#define OFA_IIMPORTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IIMPORTER, ofaIImporter ))
#define OFA_IS_IIMPORTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IIMPORTER ))
#define OFA_IIMPORTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IIMPORTER, ofaIImporterInterface ))

typedef struct _ofaIImporter                    ofaIImporter;
typedef struct _ofsImporterParms                ofsImporterParms;

/**
 * ofaIImporterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_accepted_contents: [should] returns accepted contents.
 * @is_willing_to: [should] whether the importer is willing to import this uri.
 * @import: [should]: import the data.
 *
 * This defines the interface that an #ofaIImporter should implement.
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
	guint             ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_accepted_contents:
	 * @instance: the #ofaIImporter provider.
	 *
	 * Returns: a list of accepted mimetypes content.
	 *
	 * Since: version 1.
	 */
	const GList *     ( *get_accepted_contents )( const ofaIImporter *instance );

	/**
	 * is_willing_to:
	 * @instance: the #ofaIImporter provider.
	 * @uri: [allow-none]: the imported uri.
	 * @type: [allow-none]: the candidate GType.
	 *
	 * Returns: %TRUE if the @instance is willing to import @uri into @type.
	 *
	 * Since: version 1.
	 */
	gboolean          ( *is_willing_to )        ( const ofaIImporter *instance,
														const gchar *uri,
														GType type );

	/**
	 * get_default_format:
	 * @instance: the #ofaIImporter provider.
	 * @is_user_modifiable: whether the returned format is modifiable
	 *  by the user.
	 *
	 * Returns: a new #ofaStreamFormat instance, or %NULL.
	 *
	 * Since: version 1.
	 */
	ofaStreamFormat * ( *get_default_format )   ( const ofaIImporter *instance,
														gboolean *is_user_modifiable );

	/**
	 * import:
	 * @instance: the #ofaIImporter provider.
	 * @parms: the arguments of the method.
	 *
	 * Returns: a GSList of lines, each being a GSList of fields,
	 * or %NULL if an error has occured.
	 *
	 * Since: version 1.
	 */
	GSList *          ( *parse )                ( ofaIImporter *instance,
														ofsImporterParms *parms,
														gchar **msgerr );
}
	ofaIImporterInterface;

/**
 * ofsImporterParms:
 * @version: the version number of this structure.
 * @hub: the #ofaHub object of the application.
 * @empty: whether to empty the target table before insertion.
 * @mode: the behavior regarding duplicates.
 * @stop: whether to stop on first error.
 * @uri: the imported uri.
 * @type: the candidate GType.
 * @format: the #ofaStreamFormat description of the input stream format.
 * @lines_count: [out]: the total count of lines read from input stream.
 * @parsed_count: [out]: the count of successfullly parsed records.
 * @duplicate_count: [out]: the count of duplicate records.
 * @inserted_count: [out]: the count of successfully inserted records.
 * @inserted_count: [out]: the count of successfully inserted records.
 * @import_errs: [out]: the count of import errors.
 * @insert_errs: [out]: the count of insert errors.
 * @progress: [allow-none]: a #myIProgress instance.
 * @importable_data: [allow-none][out]: the #ofaIImportable
 *  implementation may returns here some data for the caller.
 *
 * The data structure which hosts #ofa_iimporter_import() arguments.
 */
struct _ofsImporterParms {
	guint              version;
										/* v 1 */
	ofaHub            *hub;
	gboolean           empty;
	ofeImportDuplicate mode;
	gboolean           stop;
	gchar             *uri;
	GType              type;
	ofaStreamFormat   *format;
	guint              lines_count;
	guint              parsed_count;
	guint              duplicate_count;
	guint              inserted_count;
	guint              parse_errs;
	guint              insert_errs;
	myIProgress       *progress;
	void              *importable_data;
};

/*
 * Interface-wide
 */
GType            ofa_iimporter_get_type                  ( void );

guint            ofa_iimporter_get_interface_last_version( void );

GList           *ofa_iimporter_find_willing_to           ( ofaHub *hub,
																const gchar *uri,
																GType type );

/*
 * Implementation-wide
 */
guint            ofa_iimporter_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gchar           *ofa_iimporter_get_canon_name            ( const ofaIImporter *instance );

gchar           *ofa_iimporter_get_display_name          ( const ofaIImporter *instance );

gchar           *ofa_iimporter_get_version               ( const ofaIImporter *instance );

const GList     *ofa_iimporter_get_accepted_contents     ( const ofaIImporter *instance );

gboolean         ofa_iimporter_get_accept_content        ( const ofaIImporter *instance,
															const gchar *content );

gboolean         ofa_iimporter_is_willing_to             ( const ofaIImporter *instance,
																const gchar *uri,
																GType type );

ofaStreamFormat *ofa_iimporter_get_default_format        ( const ofaIImporter *instance,
																gboolean *is_user_modifiable );

guint            ofa_iimporter_import                    ( ofaIImporter *instance,
																ofsImporterParms *parms );

void             ofa_iimporter_progress_start            ( ofaIImporter *instance,
																ofsImporterParms *parms );

void             ofa_iimporter_progress_pulse            ( ofaIImporter *instance,
																ofsImporterParms *parms,
																gulong count,
																gulong total );

void             ofa_iimporter_progress_num_text         ( ofaIImporter *instance,
																ofsImporterParms *parms,
																guint numline,
																const gchar *text );

void             ofa_iimporter_progress_text             ( ofaIImporter *instance,
																ofsImporterParms *parms,
																const gchar *text );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTER_H__ */
