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

#include <glib/gi18n.h>
#include <string.h>

#include "ui/my-utils.h"

static GTimeVal st_timeval;
static GDate    st_date;

/**
 * my_utils_quote:
 *
 * Replace "'" quote characters with "\\'" before executing SQL queries
 */
gchar *
my_utils_quote( const gchar *str )
{
	static const gchar *thisfn = "my_utils_quote";
	GRegex *regex;
	GError *error;
	gchar *new_str;

	error = NULL;
	regex = g_regex_new( "'", 0, 0, &error );
	if( error ){
		g_warning( "%s: g_regex_new=%s", thisfn, error->message );
		g_error_free( error );
		return( NULL );
	}

	new_str = g_regex_replace_literal( regex, str, -1, 0, "\\'", 0, &error );
	if( error ){
		g_warning( "%s: g_regex_replace_literal=%s", thisfn, error->message );
		g_error_free( error );
		return( NULL );
	}

	g_regex_unref( regex );

	return( new_str );
}

/**
 * my_utils_date_from_str:
 */
const GDate *
my_utils_date_from_str( const gchar *str )
{
	g_date_clear( &st_date, 1 );
	if( str ){
		g_date_set_parse( &st_date, str );
	}
	return( &st_date );
}

/**
 * my_utils_display_from_date:
 *
 * returns the date as 'dd mmm yyyy' suitable for display in a newly
 * allocated string that the user must g_free().
 * or a new empty string if the date is invalid
 */
gchar *
my_utils_display_from_date( const GDate *date, myUtilsDateFormat format )
{
	static const gchar *st_month[] = {
			N_( "janv." ),
			N_( "feb." ),
			N_( "mar." ),
			N_( "apr." ),
			N_( "may" ),
			N_( "jun." ),
			N_( "jul." ),
			N_( "aug." ),
			N_( "sept." ),
			N_( "oct." ),
			N_( "nov." ),
			N_( "dec." )
	};
	gchar *str;

	str = g_strdup( "" );

	if( date && g_date_valid( date )){
		switch( format ){

			case MY_UTILS_DATE_DMMM:
				g_free( str );
				str = g_strdup_printf( "%d %s %4.4d",
						g_date_get_day( date ),
						gettext( st_month[g_date_get_month( date )-1] ),
						g_date_get_year( date ));
				break;

			case MY_UTILS_DATE_DDMM:
				g_free( str );
				str = g_strdup_printf( "%2.2d/%2.2d/%4.4d",
						g_date_get_day( date ),
						g_date_get_month( date ),
						g_date_get_year( date ));
				break;
		}
	}

	return( str );
}

/**
 * my_utils_sql_from_date:
 *
 * returns the date as 'yyyy-mm-dd' suitable for SQL insertion in a newly
 * allocated string that the user must g_free().
 * or NULL if the date is invalid
 */
gchar *
my_utils_sql_from_date( const GDate *date )
{
	gchar *str;

	str = NULL ;

	if( g_date_valid( date )){
		str = g_strdup_printf( "%4.4d-%2.2d-%2.2d",
				g_date_get_year( date ),
				g_date_get_month( date ),
				g_date_get_day( date ));
	}

	return( str );
}

/**
 * my_utils_stamp_from_str:
 */
const GTimeVal *
my_utils_stamp_from_str( const gchar *str )
{
	g_time_val_from_iso8601( str, &st_timeval );
	return( &st_timeval );
}

/**
 * my_utils_timestamp:
 *
 * Returns a newly allocated string 'yyyy-mm-dd hh:mi:ss' suitable for
 * inserting as a timestamp into a sgbd
 */
gchar *
my_utils_timestamp( void )
{
	GDateTime *dt;
	gchar *str;

	dt = g_date_time_new_now_local();
	str = g_date_time_format( dt, "%F %T" );

	return( str );
}

/**
 * my_utils_entry_get_valid
 */
gboolean
my_utils_entry_get_valid( GtkEntry *entry )
{
	return( TRUE );
}

/**
 * my_utils_entry_set_valid
 */
void
my_utils_entry_set_valid( GtkEntry *entry, gboolean valid )
{
	static const gchar *thisfn = "my_utils_entry_set_valid";
	static GtkCssProvider *css_provider = NULL;
	GError *error;
	GtkStyleContext *style;

	if( !css_provider ){
		css_provider = gtk_css_provider_new();
		error = NULL;
		if( !gtk_css_provider_load_from_path( css_provider, PKGUIDIR "/ofa.css", &error )){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
			g_clear_object( &css_provider );
		}
	}

	if( css_provider ){
		style = gtk_widget_get_style_context( GTK_WIDGET( entry ));
		if( valid ){
			gtk_style_context_remove_class( style, "ofaInvalid" );
			gtk_style_context_add_class( style, "ofaValid" );
		} else {
			gtk_style_context_remove_class( style, "ofaValid" );
			gtk_style_context_add_class( style, "ofaInvalid" );
		}
		gtk_style_context_add_provider( style,
				GTK_STYLE_PROVIDER( css_provider ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
	}
}

/**
 * my_utils_container_get_child_by_name:
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
				found = my_utils_container_get_child_by_name( GTK_CONTAINER( child ), name );
			}
		}
	}

	g_list_free( children );
	return( found );
}

/**
 * my_utils_container_get_child_by_type:
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
				found = my_utils_container_get_child_by_type( GTK_CONTAINER( child ), type );
			}
		}
	}

	g_list_free( children );
	return( found );
}
