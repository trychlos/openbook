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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-idbconnect.h"

/* some data attached to each IDBConnect instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIFileMeta   *meta;
	ofaIFilePeriod *period;
	gchar          *account;
	gchar          *password;
}
	sIDBConnect;

#define IDBCONNECT_LAST_VERSION         1
#define IDBCONNECT_DATA                 "idbconnect-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIDBConnectInterface *klass );
static void         interface_base_finalize( ofaIDBConnectInterface *klass );
static gboolean     idbconnect_query( const ofaIDBConnect *connect, const gchar *query, gboolean display_error );
static void         audit_query( const ofaIDBConnect *connect, const gchar *query );
static gchar       *quote_query( const gchar *query );
static void         error_query( const ofaIDBConnect *connect, const gchar *query );
static sIDBConnect *get_idbconnect_data( const ofaIDBConnect *connect );
static void         on_dbconnect_finalized( sIDBConnect *data, GObject *finalized_dbconnect );

/**
 * ofa_idbconnect_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbconnect_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbconnect_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbconnect_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBConnectInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBConnect", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBConnectInterface *klass )
{
	static const gchar *thisfn = "ofa_idbconnect_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBConnectInterface *klass )
{
	static const gchar *thisfn = "ofa_idbconnect_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbconnect_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbconnect_get_interface_last_version( void )
{
	return( IDBCONNECT_LAST_VERSION );
}

/**
 * ofa_idbconnect_get_interface_version:
 * @instance: this #ofaIDBConnect instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_idbconnect_get_interface_version( const ofaIDBConnect *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IDBCONNECT( instance ), 0 );

	if( OFA_IDBCONNECT_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBCONNECT_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_idbconnect_get_meta:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: a new reference to the target #ofaIFileMeta dossier, which
 * should be g_object_unref() by the caller.
 */
ofaIFileMeta *
ofa_idbconnect_get_meta( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_object_ref( data->meta ));
}

/**
 * ofa_idbconnect_set_meta:
 * @connect: this #ofaIDBConnect instance.
 * @meta: the #ofaIFileMeta object which manages the dossier.
 *
 * The interface takes a reference on the @meta object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization.
 */
void
ofa_idbconnect_set_meta( ofaIDBConnect *connect, const ofaIFileMeta *meta )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_clear_object( &data->meta );
	data->meta = g_object_ref(( gpointer ) meta );
}

/**
 * ofa_idbconnect_get_period:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: a new reference to the target #ofaIFilePeriod dossier, which
 * should be g_object_unref() by the caller.
 */
ofaIFilePeriod *
ofa_idbconnect_get_period( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_object_ref( data->period ));
}

/**
 * ofa_idbconnect_set_period:
 * @connect: this #ofaIDBConnect instance.
 * @period: the #ofaIFilePeriod object which manages the exercice.
 *
 * The interface takes a reference on the @period object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization.
 */
void
ofa_idbconnect_set_period( ofaIDBConnect *connect, const ofaIFilePeriod *period )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_clear_object( &data->period );
	data->period = g_object_ref(( gpointer ) period );
}

/**
 * ofa_idbconnect_get_account:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the account used to open the connection, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_idbconnect_get_account( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_strdup( data->account ));
}

/**
 * ofa_idbconnect_set_account:
 * @connect: this #ofaIDBConnect instance.
 * @account: the account which holds the connection.
 */
void
ofa_idbconnect_set_account( ofaIDBConnect *connect, const gchar *account )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_free( data->account );
	data->account = g_strdup( account );
}

/**
 * ofa_idbconnect_get_password:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the password used to open the connection, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_idbconnect_get_password( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_strdup( data->password ));
}

/**
 * ofa_idbconnect_set_password:
 * @connect: this #ofaIDBConnect instance.
 * @password: the password which holds the connection.
 */
void
ofa_idbconnect_set_password( ofaIDBConnect *connect, const gchar *password )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_free( data->password );
	data->password = g_strdup( password );
}

/**
 * ofa_idbconnect_query:
 * @connect: this #ofaIDBConnect instance.
 * @query: the query to be executed.
 * @display_error: whether the error should be published in a dialog box.
 *
 * Returns: %TRUE if the sentence has been successfully executed,
 * %FALSE else.
 */
gboolean
ofa_idbconnect_query( const ofaIDBConnect *connect, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofa_idbconnect_query";
	gboolean ok;

	g_debug( "%s: connect=%p, query='%s', display_error=%s",
			thisfn, ( void * ) connect, query, display_error ? "True":"False" );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( query ), FALSE );

	ok = idbconnect_query( connect, query, display_error );
	if( ok ){
		audit_query( connect, query );
	}

	return( ok );
}

/*
 * execute a query without audit
 */
static gboolean
idbconnect_query( const ofaIDBConnect *connect, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofa_idbconnect_query";
	gboolean ok;

	g_debug( "%s: connect=%p, query='%s', display_error=%s",
			thisfn, ( void * ) connect, query, display_error ? "True":"False" );

	ok = FALSE;

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->query ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->query( connect, query );
		if( !ok && display_error ){
			error_query( connect, query );
		}

	} else if( display_error ){
		my_utils_dialog_warning(
				_( "The IDBConnect instance does not implement the query() method." ));
	}

	return( ok );
}

/**
 * ofa_idbconnect_query_ex:
 * @connect: this #ofaIDBConnect instance.
 * @query: the query to be executed.
 * @result: [out]: the result set as a GSList of ordered rows.
 * @display_error: whether the error should be published in a dialog box.
 *
 * Returns: %TRUE if the sentence has been successfully executed,
 * %FALSE else.
 *
 * Each GSList->data of the result set is a pointer to a GSList of
 * ordered columns. A field is so the GSList[column] data ; it is
 * always allocated (though maybe of a zero length), or NULL (SQL-NULL
 * translation).
 *
 * The result set should be freed with #ofa_idbconnect_free_results().
 */
gboolean
ofa_idbconnect_query_ex( const ofaIDBConnect *connect, const gchar *query, GSList **result, gboolean display_error )
{
	static const gchar *thisfn = "ofa_idbconnect_query_ex";
	gboolean ok;

	g_debug( "%s: connect=%p, query='%s', result=%p, display_error=%s",
			thisfn, ( void * ) connect, query, ( void * ) result, display_error ? "True":"False" );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( query ), FALSE );
	g_return_val_if_fail( result, FALSE );

	ok = FALSE;

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->query_ex ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->query_ex( connect, query, result );
		if( !ok && display_error ){
			error_query( connect, query );
		}

	} else if( display_error ){
		my_utils_dialog_warning(
				_( "The IDBConnect instance does not implement the query_ex() method." ));
	}

	return( ok );
}

/**
 * ofa_idbconnect_query_int:
 * @connect: this #ofaIDBConnect instance.
 * @query: the query to be executed
 * @result: [out]: the returned integer value
 * @display_error: whether the error should be published in a dialog box
 *
 * A simple query for getting a single int.
 *
 * Returns: %TRUE if the sentence has been successfully executed,
 * %FALSE else.
 */
gboolean
ofa_idbconnect_query_int( const ofaIDBConnect *connect, const gchar *query, gint *result, gboolean display_error )
{
	gboolean ok;
	GSList *reslist, *icol;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( query ), FALSE );
	g_return_val_if_fail( result, FALSE );

	*result = 0;
	ok = ofa_idbconnect_query_ex( connect, query, &reslist, display_error );

	if( ok ){
		icol = ( GSList * ) reslist->data;
		if( icol && icol->data ){
			*result = atoi(( gchar * ) icol->data );
		}
		ofa_idbconnect_free_results( reslist );
	}

	return( ok );
}

static void
audit_query( const ofaIDBConnect *connect, const gchar *query )
{
	gchar *quoted;
	gchar *audit;

	quoted = quote_query( query );
	audit = g_strdup_printf( "INSERT INTO OFA_T_AUDIT (AUD_QUERY) VALUES ('%s')", quoted );

	idbconnect_query( connect, audit, FALSE );

	g_free( quoted );
	g_free( audit );
}

static gchar *
quote_query( const gchar *query )
{
	gchar *quoted;
	GRegex *regex;
	gchar *new_str;

	new_str = g_strdup( query );

	regex = g_regex_new( "\\\\", 0, 0, NULL );
	if( regex ){
		g_free( new_str );
		new_str = g_regex_replace_literal( regex, query, -1, 0, "", 0, NULL );
		g_regex_unref( regex );
	}

	quoted = my_utils_quote( new_str );
	g_free( new_str );

	return( quoted );
}

static void
error_query( const ofaIDBConnect *connect, const gchar *query )
{
	GtkWidget *dlg;
	gchar *str;

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", query );

	str = ofa_idbconnect_get_last_error( connect );

	/* query_ex returns NULL if the result is empty: this is not an error */
	if( my_strlen( str )){
		gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), "%s", str );
	}
	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

/**
 * ofa_idbconnect_get_last_error:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: The last found error, as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofa_idbconnect_get_last_error( const ofaIDBConnect *connect )
{
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->get_last_error ){
		return( OFA_IDBCONNECT_GET_INTERFACE( connect )->get_last_error( connect ));
	}

	g_warning( _( "The IDBConnect instance does not implement the get_last_error() method." ));

	return( NULL );
}

/**
 * ofa_idbconnect_archive_and_new:
 * @connect: this #ofaIDBConnect instance which handles an active user
 *  connection on the closed exercice.
 * @root_account: the DBMS root account.
 * @root_password: the DBMS root password.
 * @begin_next: the beginning date of the new exercice.
 * @end_next: the ending date of the new exercice.
 *
 * Duplicate the storage space (the database) of the period
 * to a new one, and records the new properties as a new financial
 * period in the dossier settings.
 * Initialize curren user account with required permissions for the
 * new database.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_archive_and_new( const ofaIDBConnect *connect,
		const gchar *root_account, const gchar *root_password,
		const GDate *begin_next, const GDate *end_next )
{
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->archive_and_new ){
		return( OFA_IDBCONNECT_GET_INTERFACE( connect )->archive_and_new( connect, root_account, root_password, begin_next, end_next ));
	}

	return( FALSE );
}

static sIDBConnect *
get_idbconnect_data( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	data = ( sIDBConnect * ) g_object_get_data( G_OBJECT( connect ), IDBCONNECT_DATA );

	if( !data ){
		data = g_new0( sIDBConnect, 1 );
		g_object_set_data( G_OBJECT( connect ), IDBCONNECT_DATA, data );
		g_object_weak_ref( G_OBJECT( connect ), ( GWeakNotify ) on_dbconnect_finalized, data );
	}

	return( data );
}

static void
on_dbconnect_finalized( sIDBConnect *data, GObject *finalized_connect )
{
	static const gchar *thisfn = "ofa_idbconnect_on_dbconnect_finalized";

	g_debug( "%s: data=%p, finalized_connect=%p",
			thisfn, ( void * ) data, ( void * ) finalized_connect );

	g_clear_object( &data->meta );
	g_clear_object( &data->period );
	g_free( data->account );
	g_free( data );
}
