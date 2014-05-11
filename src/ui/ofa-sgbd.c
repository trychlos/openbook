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
#include <mysql/mysql.h>

#include "ui/ofa-sgbd.h"

/* private class data
 */
struct _ofaSgbdClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaSgbdPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	gchar   *provider;
	MYSQL   *mysql;
};

static GObjectClass *st_parent_class  = NULL;

static GType register_type( void );
static void  class_init( ofaSgbdClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_dispose( GObject *instance );
static void  instance_finalize( GObject *instance );
static void  connect_error( ofaSgbd *sgbd, GtkWindow *parent, const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account );
static void  query_error( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query );

GType
ofa_sgbd_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_sgbd_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaSgbdClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaSgbd ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaSgbd", &info, 0 );

	return( type );
}

static void
class_init( ofaSgbdClass *klass )
{
	static const gchar *thisfn = "ofa_sgbd_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaSgbdClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_sgbd_instance_init";
	ofaSgbd *self;

	g_return_if_fail( OFA_IS_SGBD( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_SGBD( instance );

	self->private = g_new0( ofaSgbdPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_sgbd_instance_dispose";
	ofaSgbdPrivate *priv;

	g_return_if_fail( OFA_IS_SGBD( instance ));

	priv = ( OFA_SGBD( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		g_free( priv->provider );

		if( priv->mysql ){
			mysql_close( priv->mysql );
			g_free( priv->mysql );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_sgbd_instance_finalize";
	ofaSgbdPrivate *priv;

	g_return_if_fail( OFA_IS_SGBD( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFA_SGBD( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_sgbd_new:
 *
 * Allocates a new ofaSgbd object, and that's all
 */
ofaSgbd *
ofa_sgbd_new( const gchar *provider )
{
	static const gchar *thisfn = "ofa_sgbd_new";
	ofaSgbd *sgbd;

	g_debug( "%s: provider=%s", thisfn, provider );

	sgbd = g_object_new( OFA_TYPE_SGBD, NULL );

	sgbd->private->provider = g_strdup( provider );

	return( sgbd );
}

/**
 * ofa_sgbd_connect:
 *
 * The connection will be automatically closed when unreffing the object.
 */
gboolean
ofa_sgbd_connect( ofaSgbd *sgbd, GtkWindow *parent,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_sgbd_connect";
	MYSQL *mysql;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, parent=%p (%s), host=%s, port=%d, socket=%s, dbname=%s, account=%s, password=%s",
			thisfn,
			( void * ) sgbd,
			( void * ) parent, G_OBJECT_TYPE_NAME( parent ),
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

		connect_error( sgbd, parent, host, port, socket, dbname, account );
		g_free( mysql );
		return( FALSE );
	}

	sgbd->private->mysql = mysql;
	return( TRUE );
}

static void
connect_error( ofaSgbd *sgbd, GtkWindow *parent,
		const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account )
{
	GtkMessageDialog *dlg;
	GString *str;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				parent,
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
 * ofa_sgbd_exec_query:
 */
gboolean
ofa_sgbd_query( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query )
{
	static const gchar *thisfn = "ofa_sgbd_query";
	gboolean query_ok = FALSE;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, parent=%p, query='%s'",
			thisfn, ( void * ) sgbd, ( void * ) parent, query );

	if( sgbd->private->mysql ){
		if( mysql_query( sgbd->private->mysql, query )){
			query_error( sgbd, parent, query );
		} else {
			query_ok = TRUE;
		}
	} else {
		g_warning( "%s: trying to query a non-opened connection", thisfn );
	}

	return( query_ok );
}

/**
 * ofa_sgbd_exec_query_ex:
 *
 * Returns a GSList or ordered rows of the result set.
 * Each GSList->data is a pointer to a GSList of ordered columns
 * A field is so the GSList[column] data, is always allocated
 * (but maybe of a zero length), or NULL (SQL-NULL translation).
 *
 * Returns NULL is case of an error.
 *
 * The returned GSList should be freed with ofa_sgbd_free_result().
 */
GSList *
ofa_sgbd_query_ex( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query )
{
	static const gchar *thisfn = "ofa_sgbd_query_ex";
	GSList *result = NULL;
	MYSQL_RES *res;
	MYSQL_ROW row;
	gint fields_count, i;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	g_debug( "%s: sgbd=%p, parent=%p, query='%s'",
			thisfn, ( void * ) sgbd, ( void * ) parent, query );

	if( sgbd->private->mysql ){
		if( mysql_query( sgbd->private->mysql, query )){
			query_error( sgbd, parent, query );

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
query_error( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query )
{
	GtkMessageDialog *dlg;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				parent,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", query ));

	gtk_message_dialog_format_secondary_text( dlg, "%s", mysql_error( sgbd->private->mysql ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}
