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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-connect-display-piece.h"

static const gchar *st_ui_xml           = PROVIDER_DATADIR "/ofa-mysql-connect-display-piece.ui";
static const gchar *st_ui_mysql         = "MySQLConnectDisplayPiece";

void
ofa_mysql_connect_display_piece_attach_to( const ofaIDbms *instance, const gchar *dname, GtkContainer *parent )
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *label;
	gchar *text;
	gint port_num;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "infos-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));
	gtk_widget_reparent( grid, GTK_WIDGET( parent ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "provider" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofa_mysql_idbms_get_provider_name( instance ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_settings_dossier_get_string( dname, SETTINGS_HOST );
	if( !text || !g_utf8_strlen( text, -1 )){
		g_free( text );
		text = g_strdup( "localhost" );
	}
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "socket" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_settings_dossier_get_string( dname, SETTINGS_SOCKET );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_dossier_misc_get_current_dbname( dname );
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	port_num = ofa_settings_dossier_get_int( dname, SETTINGS_PORT );
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	}
}
