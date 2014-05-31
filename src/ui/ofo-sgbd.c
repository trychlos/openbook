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
#include <gtk/gtk.h>
#include <mysql/mysql.h>
#include <string.h>

#include "ui/ofo-sgbd.h"

/* private instance data
 */
struct _ofoSgbdPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar   *provider;
	MYSQL   *mysql;
};

G_DEFINE_TYPE( ofoSgbd, ofo_sgbd, G_TYPE_OBJECT )

static void  error_connect( const ofoSgbd *sgbd, const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account );
static void  error_query( const ofoSgbd *sgbd, const gchar *query );

static void
ofo_sgbd_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_sgbd_finalize";
	ofoSgbdPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_SGBD( instance )->private;

	/* free data members here */
	g_free( priv->provider );

	if( priv->mysql ){
		mysql_close( priv->mysql );
		g_free( priv->mysql );
	}
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_sgbd_parent_class )->finalize( instance );
}

static void
ofo_sgbd_dispose( GObject *instance )
{
	ofoSgbd *self;

	self = OFO_SGBD( instance );

	if( !self->private->dispose_has_run ){

		self->private->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_sgbd_parent_class )->dispose( instance );
}

static void
ofo_sgbd_init( ofoSgbd *self )
{
	static const gchar *thisfn = "ofo_sgbd_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoSgbdPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofo_sgbd_class_init( ofoSgbdClass *klass )
{
	static const gchar *thisfn = "ofo_sgbd_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoSgbdPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_sgbd_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_sgbd_finalize;
}

/**
 * ofo_sgbd_new:
 *
 * Allocates a new ofoSgbd object, and that's all
 */
ofoSgbd *
ofo_sgbd_new( const gchar *provider )
{
	static const gchar *thisfn = "ofo_sgbd_new";
	ofoSgbd *sgbd;

	g_debug( "%s: provider=%s", thisfn, provider );

	sgbd = g_object_new( OFO_TYPE_SGBD, NULL );

	sgbd->private->provider = g_strdup( provider );

	return( sgbd );
}

/**
 * ofo_sgbd_connect:
 *
 * The connection will be automatically closed when unreffing the object.
 */
gboolean
ofo_sgbd_connect( ofoSgbd *sgbd,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofo_sgbd_connect";
	MYSQL *mysql;

	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, host=%s, port=%d, socket=%s, dbname=%s, account=%s, password=%s",
			thisfn,
			( void * ) sgbd,
			host, port, socket, dbname, account, password );

	mysql = g_new0( MYSQL, 1 );
	mysql_init( mysql );

	if( !mysql_real_connect( mysql,
			host,
			account,
			password,
			dbname,
			port,
			socket,
			CLIENT_MULTI_RESULTS )){

		error_connect( sgbd, host, port, socket, dbname, account );
		g_free( mysql );
		return( FALSE );
	}

	sgbd->private->mysql = mysql;
	return( TRUE );
}

static void
error_connect( const ofoSgbd *sgbd,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account )
{
	GtkMessageDialog *dlg;
	GString *str;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", _( "Unable to connect to the database" )));

	str = g_string_new( "" );
	if( host ){
		g_string_append_printf( str, "Host: %s\n", host );
	}
	if( port > 0 ){
		g_string_append_printf( str, "Port: %d\n", port );
	}
	if( socket ){
		g_string_append_printf( str, "Socket: %s\n", socket );
	}
	if( dbname ){
		g_string_append_printf( str, "Database: %s\n", dbname );
	}
	if( account ){
		g_string_append_printf( str, "Account: %s\n", account );
	}
	gtk_message_dialog_format_secondary_text( dlg, "%s", str->str );
	g_string_free( str, TRUE );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/**
 * ofo_sgbd_exec_query:
 */
gboolean
ofo_sgbd_query( const ofoSgbd *sgbd, const gchar *query )
{
	static const gchar *thisfn = "ofo_sgbd_query";
	gboolean query_ok = FALSE;
	/*gchar *to_str;
	glong length;*/

	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, query='%s'", thisfn, ( void * ) sgbd, query );

	if( sgbd->private->mysql ){
		/*length = g_utf8_strlen( query, -1 );
		to_str = g_new0( char, 2*length+1 );
		mysql_real_escape_string( sgbd->private->mysql, to_str, query, length );*/
		if( mysql_query( sgbd->private->mysql, query )){
			error_query( sgbd, query );
		} else {
			query_ok = TRUE;
		}
		/*g_free( to_str );*/
	} else {
		g_warning( "%s: trying to query a non-opened connection", thisfn );
	}

	return( query_ok );
}

/**
 * ofo_sgbd_exec_query_ex:
 *
 * @parent: if NULL, do not display error message
 *
 * Returns a GSList or ordered rows of the result set.
 * Each GSList->data is a pointer to a GSList of ordered columns
 * A field is so the GSList[column] data, is always allocated
 * (but maybe of a zero length), or NULL (SQL-NULL translation).
 *
 * Returns NULL is case of an error.
 *
 * The returned GSList should be freed with ofo_sgbd_free_result().
 */
GSList *
ofo_sgbd_query_ex( const ofoSgbd *sgbd, const gchar *query )
{
	static const gchar *thisfn = "ofo_sgbd_query_ex";
	GSList *result = NULL;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, query='%s'", thisfn, ( void * ) sgbd, query );

	if( sgbd->private->mysql ){
		if( mysql_query( sgbd->private->mysql, query )){
			error_query( sgbd, query );

		} else {
			res = mysql_store_result( sgbd->private->mysql );
			if( res ){
				fields_count = mysql_num_fields( res );
				while(( row = mysql_fetch_row( res ))){
					GSList *col = NULL;
					for( i=0 ; i<fields_count ; ++i ){
						col = g_slist_prepend( col, row[i] ? g_strdup( row[i] ) : NULL );
					}
					col = g_slist_reverse( col );
					result = g_slist_prepend( result, col );
				}
				result = g_slist_reverse( result );
			}
		}

	} else {
		g_warning( "%s: trying to query a non-opened connection", thisfn );
	}

	return( result );
}

static void
error_query( const ofoSgbd *sgbd, const gchar *query )
{
	GtkMessageDialog *dlg;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", query ));

	gtk_message_dialog_format_secondary_text( dlg, "%s", mysql_error( sgbd->private->mysql ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/**
 * ofo_sgbd_free_result:
 */
void
ofo_sgbd_free_result( GSList *result )
{
	g_slist_foreach( result, ( GFunc ) g_slist_free_full, g_free );
}
