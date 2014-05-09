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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/my-dossier.h"

/**
 *
 */
GtkWidget *
my_utils_container_get_child_by_name( GtkContainer *container, const gchar *name )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;
	const gchar *child_name;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			child_name = gtk_buildable_get_name( GTK_BUILDABLE( child ));
			if( child_name && strlen( child_name ) && !g_ascii_strcasecmp( name, child_name )){
				found = child;
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = container_get_child_by_name( GTK_CONTAINER( child ), name );
			}
		}
	}

	g_list_free( children );
	return( found );
}

/**
 *
 */
GtkWidget *
my_utils_container_get_child_by_type( GtkContainer *container, GType type )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			if( G_OBJECT_TYPE( ic->data ) == type ){
				found = GTK_WIDGET( ic->data );
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = container_get_child_by_type( GTK_CONTAINER( child ), type );
			}
		}
	}

	g_list_free( children );
	return( found );
}
