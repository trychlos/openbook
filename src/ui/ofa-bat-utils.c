/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-settings.h"

#include "ui/ofa-main-window.h"
#include "ui/ofa-bat-utils.h"

/**
 * ofa_bat_utils_import:
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * open GtkFileChooser dialog to let the user select the file to be
 * imported and import it
 *
 * Returns: the identifier of the newly imported BAT file, or zero if
 * an error has happened.
 */
ofxCounter
ofa_bat_utils_import( const ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_bat_utils_import";
	ofxCounter imported_id;
	GtkWidget *file_chooser;
	ofaFileFormat *settings;
	ofaIImportable *importable;
	ofoDossier *dossier;
	gchar *uri, *str;

	imported_id = 0;

	file_chooser = gtk_file_chooser_dialog_new(
			_( "Select a BAT file to be imported" ),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_( "Cancel" ), GTK_RESPONSE_CANCEL,
			_( "Import" ), GTK_RESPONSE_OK,
			NULL );

	if( gtk_dialog_run( GTK_DIALOG( file_chooser )) == GTK_RESPONSE_OK ){

		settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
		ofa_file_format_set( settings,
				NULL, OFA_FFTYPE_OTHER, OFA_FFMODE_IMPORT, "UTF-8", 0, ',', ' ', 0 );

		/* take the uri before clearing bat lines */
		uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( file_chooser ));

		importable = ofa_iimportable_find_willing_to( uri, settings );

		if( importable ){
			dossier = ofa_main_window_get_dossier( main_window );
			if( ofa_iimportable_import_uri( importable, dossier, NULL, &imported_id ) > 0 ){
				imported_id = 0;
			}

			g_debug( "%s: importable=%p (%s) ref_count=%d, imported_id=%ld",
					thisfn, ( void * ) importable,
					G_OBJECT_TYPE_NAME( importable ), G_OBJECT( importable )->ref_count, imported_id );

			g_object_unref( importable );

		} else {
			str = g_strdup_printf(
					_( "Unable to find a module willing to import '%s' URI.\n\n"
						"The operation will be cancelled." ), uri );
			my_utils_dialog_warning( str );
			g_free( str );
		}

		g_free( uri );
		g_object_unref( settings );
	}

	gtk_widget_destroy( file_chooser );

	return( imported_id );
}
