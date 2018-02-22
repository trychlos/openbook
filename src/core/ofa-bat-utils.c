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
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "core/ofa-bat-utils.h"

/**
 * ofa_bat_utils_import:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Open a GtkFileChooser dialog to let the user select the file to be
 * imported (and import it).
 *
 * Returns: the identifier of the newly imported BAT file, or zero if
 * an error has happened.
 */
ofxCounter
ofa_bat_utils_import( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_bat_utils_import";
	ofxCounter imported_id;
	GtkWidget *file_chooser;
	GList *importers;
	ofaIImporter *importer;
	gchar *uri, *str;
	ofsImporterParms parms;
	ofsImportedBat sbat;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), 0 );

	imported_id = 0;

	file_chooser = gtk_file_chooser_dialog_new(
			_( "Select a BAT file to be imported" ),
			parent,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_( "Cancel" ), GTK_RESPONSE_CANCEL,
			_( "Import" ), GTK_RESPONSE_OK,
			NULL );

	if( gtk_dialog_run( GTK_DIALOG( file_chooser )) == GTK_RESPONSE_OK ){

		/* take the uri before clearing bat lines */
		uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( file_chooser ));

		importers = ofa_iimporter_find_willing_to( getter, uri, OFO_TYPE_BAT );
		importer = importers ? g_object_ref( importers->data ) : NULL;
		g_list_free_full( importers, ( GDestroyNotify ) g_object_unref );

		if( importer ){
			memset( &parms, '\0', sizeof( parms ));
			parms.version = 1;
			parms.getter = getter;
			parms.empty = FALSE;
			parms.mode = OFA_IDUPLICATE_ABORT;
			parms.stop = TRUE;
			parms.uri = uri;
			parms.type = OFO_TYPE_BAT;
			parms.format = ofa_iimporter_get_default_format( importer, getter, NULL );
			parms.importable_data = &sbat;

			if( ofa_iimporter_import( importer, &parms ) == 0 ){
				imported_id = (( ofsImportedBat * ) parms.importable_data )->bat_id;

			} else {
				my_utils_msg_dialog( parent, GTK_MESSAGE_WARNING,
						_( "Errors have been detected.\n"
							"Try import assistant to get a detail of these errors." ));
			}

			g_debug( "%s: importer=%p (%s), parsed=%u, errs=%u, bat_id=%ld",
					thisfn, ( void * ) importer, G_OBJECT_TYPE_NAME( importer ),
					parms.parsed_count, parms.parse_errs+parms.insert_errs, imported_id );

			g_object_unref( importer );

		} else {
			str = g_strdup_printf(
					_( "Unable to find a module willing to import '%s' URI.\n\n"
						"The operation will be cancelled." ), uri );
			my_utils_msg_dialog( parent, GTK_MESSAGE_WARNING, str );
			g_free( str );
		}

		g_free( uri );
	}

	gtk_widget_destroy( file_chooser );

	return( imported_id );
}
