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

#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-connect-display-bin.h"

static const gchar *st_ui_xml           = PROVIDER_DATADIR "/ofa-mysql-connect-display-bin.ui";
static const gchar *st_ui_mysql         = "MySQLConnectDisplayBin";

GtkWidget *
ofa_mysql_connect_display_bin_new( const ofaIDbms *instance, const gchar *dname )
{
	GtkWidget *window;
	GtkWidget *top_widget, *new_parent;
	GtkWidget *label;
	gchar *text;
	gint port_num;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-decorated" );
	g_return_val_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ), NULL );

	new_parent = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_widget_reparent( top_widget, new_parent );
	gtk_widget_destroy( window );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "provider" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	gtk_label_set_text( GTK_LABEL( label ), ofa_mysql_idbms_get_provider_name( instance ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "host" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	text = ofa_settings_dossier_get_string( dname, SETTINGS_HOST );
	if( !my_strlen( text )){
		g_free( text );
		text = g_strdup( "localhost" );
	}
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "socket" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	text = ofa_settings_dossier_get_string( dname, SETTINGS_SOCKET );
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "database" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	text = ofa_dossier_misc_get_current_dbname( dname );
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "port" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	port_num = ofa_settings_dossier_get_int( dname, SETTINGS_PORT );
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	}

	return( new_parent );
}
