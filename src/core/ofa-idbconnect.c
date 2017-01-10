/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include <stdlib.h>

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-json-header.h"
#include "api/ofo-dossier.h"

/* some data attached to each IDBConnect instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	gchar              *account;
	gchar              *password;
	ofaIDBDossierMeta  *dossier_meta;
	ofaIDBExerciceMeta *exercice_meta;
}
	sIDBConnect;

#define IDBCONNECT_LAST_VERSION           1
#define IDBCONNECT_DATA                  "idbconnect-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType          register_type( void );
static void           interface_base_init( ofaIDBConnectInterface *klass );
static void           interface_base_finalize( ofaIDBConnectInterface *klass );
static gboolean       idbconnect_query( const ofaIDBConnect *connect, const gchar *query, gboolean display_error );
static void           audit_query( const ofaIDBConnect *connect, const gchar *query );
static gchar         *quote_query( const gchar *query );
static void           error_query( const ofaIDBConnect *connect, const gchar *query );
static ofaJsonHeader *build_json_header( const ofaIDBConnect *self, const gchar *comment );
static gboolean       backup_json_header( const ofaIDBConnect *connect, GOutputStream *output_stream, const gchar *json );
static gboolean       set_admin_credentials( const ofaIDBConnect *connect, const gchar *adm_account, const gchar *adm_password, gchar **msgerr );
static sIDBConnect   *get_instance_data( const ofaIDBConnect *connect );
static void           on_instance_finalized( sIDBConnect *sdata, GObject *finalized_dbconnect );

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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idbconnect_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBCONNECT );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBConnectInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBConnectInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBConnect::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbconnect_get_account:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the account used to open the connection.
 */
const gchar *
ofa_idbconnect_get_account( const ofaIDBConnect *connect )
{
	sIDBConnect *sdata;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	sdata = get_instance_data( connect );

	return( sdata->account );
}

/**
 * ofa_idbconnect_set_account:
 * @connect: this #ofaIDBConnect instance.
 * @account: the account used to open the connection.
 *
 * Set the @account.
 */
void
ofa_idbconnect_set_account( ofaIDBConnect *connect, const gchar *account )
{
	sIDBConnect *sdata;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));
	g_return_if_fail( my_strlen( account ));

	sdata = get_instance_data( connect );

	g_free( sdata->account );
	sdata->account = g_strdup( account );
}

/**
 * ofa_idbconnect_get_password:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the password used to open the connection.
 */
const gchar *
ofa_idbconnect_get_password( const ofaIDBConnect *connect )
{
	sIDBConnect *sdata;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	sdata = get_instance_data( connect );

	return( sdata->password );
}

/**
 * ofa_idbconnect_set_password:
 * @connect: this #ofaIDBConnect instance.
 * @password: the password used to open the connection.
 */
void
ofa_idbconnect_set_password( ofaIDBConnect *connect, const gchar *password )
{
	sIDBConnect *sdata;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));
	g_return_if_fail( my_strlen( password ));

	sdata = get_instance_data( connect );

	g_free( sdata->password );
	sdata->password = g_strdup( password );
}

/**
 * ofa_idbconnect_get_dossier_meta:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the #ofaIDBDossierMeta dossier.
 *
 * The returned reference is owned by @connect instance, and should not
 * be release by the caller.
 */
ofaIDBDossierMeta *
ofa_idbconnect_get_dossier_meta( const ofaIDBConnect *connect )
{
	sIDBConnect *sdata;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	sdata = get_instance_data( connect );

	return( sdata->dossier_meta );
}

/**
 * ofa_idbconnect_set_dossier_meta:
 * @connect: this #ofaIDBConnect instance.
 * @dossier_meta: the #ofaIDBDossierMeta dossier.
 *
 * The interface takes a reference on the @meta object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization.
 */
void
ofa_idbconnect_set_dossier_meta( ofaIDBConnect *connect, ofaIDBDossierMeta *dossier_meta )
{
	sIDBConnect *sdata;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));
	g_return_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ));

	sdata = get_instance_data( connect );

	g_clear_object( &sdata->dossier_meta );
	sdata->dossier_meta = g_object_ref(( gpointer ) dossier_meta );
}

/**
 * ofa_idbconnect_get_exercice_meta:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the #ofaIDBExerciceMeta target.
 *
 * The returned reference is owned by @connect instance, and should not
 * be release by the caller.
 */
ofaIDBExerciceMeta *
ofa_idbconnect_get_exercice_meta( const ofaIDBConnect *connect )
{
	sIDBConnect *sdata;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	sdata = get_instance_data( connect );

	return( sdata->exercice_meta );
}

/**
 * ofa_idbconnect_set_exercice_meta:
 * @connect: this #ofaIDBConnect instance.
 * @exercice_meta: [allow-none]: the #ofaIDBExerciceMeta target.
 *
 * The interface takes a reference on the @period object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization.
 */
void
ofa_idbconnect_set_exercice_meta( ofaIDBConnect *connect, ofaIDBExerciceMeta *exercice_meta )
{
	sIDBConnect *sdata;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));
	g_return_if_fail( !exercice_meta || OFA_IS_IDBEXERCICE_META( exercice_meta ));

	sdata = get_instance_data( connect );

	g_clear_object( &sdata->exercice_meta );
	if( exercice_meta ){
		sdata->exercice_meta = g_object_ref(( gpointer ) exercice_meta );
	}
}

/**
 * ofa_idbconnect_open_with_editor:
 * @connect: this #ofaIDBConnect instance.
 * @editor: a #ofaIDBDossierEditor object which handles needed connection
 *  informations.
 *
 * Establishes a (root) connection to the DBMS at server level.
 *
 * Returns: %TRUE if the connection has been successfully established,
 * %FALSE else.
 */
gboolean
ofa_idbconnect_open_with_editor( ofaIDBConnect *connect, const ofaIDBDossierEditor *editor )
{
	static const gchar *thisfn = "ofa_idbconnect_open_with_editor";
	gboolean ok;

	g_debug( "%s: connect=%p, editor=%p",
			thisfn, ( void * ) connect, ( void * ) editor );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->open_with_editor ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->open_with_editor( connect, editor );
		return( ok );
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'open_with_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
}

/**
 * ofa_idbconnect_open_with_meta:
 * @connect: this #ofaIDBConnect instance.
 * @account: the user account.
 * @password: [allow-none]: the user password.
 * @dossier_meta: the #ofaIDBDossierMeta which identifies the dossier.
 * @period: [allow-none]: the #ofaIDBExerciceMeta which identifies the
 *  exercice, or %NULL to establish a connection at server-level
 *  to the host of @meta.
 *
 * Establish a connection to the specified dossier and exercice.
 *
 * Returns: %TRUE if the connection has been successfully established,
 * %FALSE else.
 */
gboolean
ofa_idbconnect_open_with_meta( ofaIDBConnect *connect, const gchar *account, const gchar *password, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbconnect_open_with_meta";
	gboolean ok;

	g_debug( "%s: connect=%p, account=%s, password=%s, dossier_meta=%p (%s), period=%p",
			thisfn, ( void * ) connect,
			account, password ? "******":password,
			( void * ) dossier_meta, G_OBJECT_TYPE_NAME( dossier_meta ), ( void * ) period );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), FALSE );
	g_return_val_if_fail( !period || OFA_IS_IDBEXERCICE_META( period ), FALSE );

	if( 1 ){
		ofa_idbdossier_meta_dump( dossier_meta );
		if( period ){
			ofa_idbexercice_meta_dump( period );
		}
	}

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->open_with_meta ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->open_with_meta( connect, account, password, dossier_meta, period );
#if 0
		if( ok ){
			ofa_idbconnect_set_account( connect, account );
			ofa_idbconnect_set_password( connect, password );
			ofa_idbconnect_set_dossier_meta( connect, dossier_meta );
			ofa_idbconnect_set_exercice_meta( connect, period );
		}
#endif
		return( ok );
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'open_with_meta()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
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
 * Execute a query whithout result set.
 * May display an error message
 */
static gboolean
idbconnect_query( const ofaIDBConnect *connect, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofa_idbconnect_query";
	gboolean ok;
	gchar *str;

	ok = FALSE;

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->query ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->query( connect, query );
		if( !ok && display_error ){
			error_query( connect, query );
		}

	} else {
		str = g_strdup_printf(
					_( "ofaIDBConnect's %s implementation does not provide 'query()' method" ),
					G_OBJECT_TYPE_NAME( connect ));

		if( display_error ){
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
		} else {
			g_info( "%s: %s", thisfn, str );
		}

		g_free( str );
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
	gchar *str;

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

	} else {
		str = g_strdup_printf(
					_( "ofaIDBConnect's %s implementation does not provide 'query_ex()' method" ),
					G_OBJECT_TYPE_NAME( connect ));

		if( display_error ){
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
		} else {
			g_info( "%s: %s", thisfn, str );
		}

		g_free( str );
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

/**
 * ofa_idbconnect_has_table:
 * @connect: this #ofaIDBConnect instance.
 * @table: the name of the searched table.
 *
 * Returns: %TRUE if the specified @table exists, %FALSE else.
 */
gboolean
ofa_idbconnect_has_table( const ofaIDBConnect *connect, const gchar *table )
{
	gboolean ok, found;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	query = g_strdup_printf( "SHOW TABLES LIKE '%s'", table );
	result = NULL;
	found = FALSE;

	ok = ofa_idbconnect_query_ex( connect, query, &result, FALSE );
	if( ok ){
		if( result ){
			icol = ( GSList * ) result->data;
			if( icol && icol->data ){
				found = TRUE;
			}
			ofa_idbconnect_free_results( result );
		}
	}
	g_free( query );

	return( found );
}

/**
 * ofa_idbconnect_table_backup:
 * @connect: this #ofaIDBConnect instance.
 * @table: the name of the table to be saved.
 *
 * Backup the @table table.
 *
 * Returns: the name of the backup table, as a newly allocated string
 * which should be g_free() by the caller.
 */
gchar *
ofa_idbconnect_table_backup( const ofaIDBConnect *connect, const gchar *table )
{
	gchar *output, *query;
	gboolean ok;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	output = g_strdup_printf( "BACKUP_%s", table );
	ok = TRUE;

	if( ok ){
		query = g_strdup_printf( "DROP TABLE IF EXISTS %s", output );
		ok = ofa_idbconnect_query( connect, query, FALSE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup_printf( "CREATE TABLE %s SELECT * FROM %s ", output, table );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( !ok ){
		g_free( output );
		output = NULL;
	}

	return( output );
}

/**
 * ofa_idbconnect_table_restore:
 * @connect: this #ofaIDBConnect instance.
 * @table_src: the source of the restoration.
 * @table_dest: the target of the restoration.
 *
 * Restore @table_src to @table_dest.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_table_restore( const ofaIDBConnect *connect, const gchar *table_src, const gchar *table_dest )
{
	gboolean ok;
	gchar *query;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( table_src ), FALSE );
	g_return_val_if_fail( my_strlen( table_dest ), FALSE );

	ok = TRUE;

	if( ok ){
		query = g_strdup_printf( "DROP TABLE IF EXISTS %s", table_dest );
		ok = ofa_idbconnect_query( connect, query, FALSE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup_printf( "CREATE TABLE %s SELECT * FROM %s ", table_dest, table_src );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
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

	quoted = my_utils_quote_single( new_str );
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
	static const gchar *thisfn = "ofa_idbconnect_get_last_error";

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->get_last_error ){
		return( OFA_IDBCONNECT_GET_INTERFACE( connect )->get_last_error( connect ));
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'get_last_error()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( NULL );
}

/**
 * ofa_idbconnect_backup:
 * @connect: a #ofaIDBConnect instance which handles a user
 *  connection on the dossier/exercice to be backuped.
 * @uri: the target file.
 *
 * Backup the current period to the @uri file.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_backup( const ofaIDBConnect *connect, const gchar *uri )
{
	static const gchar *thisfn = "ofa_idbconnect_backup";
	gboolean ok;

	g_debug( "%s: connect=%p, uri=%s", thisfn, ( void * ) connect, uri );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( uri ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->backup ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->backup( connect, uri );
		return( ok );
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'backup()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
}

/**
 * ofa_idbconnect_backup_db:
 * @connect: a #ofaIDBConnect instance which handles a user
 *  connection on the dossier/exercice to be backuped.
 * @comment: [allow-none]: a user comment.
 * @uri: the target file.
 * @parent: [allow-none]: in verbose mode, the parent #GtkWindow.
 *  If %NULL, then no message is outputed.
 *  If set, messages are displayed in a dedicated window.
 *
 * Backup the current period to the @uri file.
 *
 * The output @uri file is unconditionnally deleted before being
 * written with output datas.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_backup_db( const ofaIDBConnect *connect, const gchar *comment, const gchar *uri, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_idbconnect_backup_db";
	gboolean ok;
	GFile *file;
	GError *error;
	GFileOutputStream *output_stream;
	ofaJsonHeader *json_header;
	gchar *str;

	g_debug( "%s: connect=%p, comment=%s, uri=%s, parent=%p",
			thisfn, ( void * ) connect, comment, uri, ( void * ) parent );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( uri ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );

	ok = TRUE;

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->backup_db ){

		/* create an output stream for the output file */
		error = NULL;
		file = g_file_new_for_uri( uri );
		output_stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error );
		if( !output_stream ){
			my_utils_msg_dialog( parent, GTK_MESSAGE_ERROR, error->message );
			g_error_free( error );

		} else {
			/* write the JSON header to the output stream */
			json_header = build_json_header( connect, comment );
			str = ofa_json_header_get_string( json_header );
			ok = backup_json_header( connect, G_OUTPUT_STREAM( output_stream ), str );
			g_free( str );
			g_object_unref( json_header );

			/* write the DBMS backup to the output stream */

			g_output_stream_close( G_OUTPUT_STREAM( output_stream ), NULL, NULL );
			g_object_unref( output_stream );
		}

		g_object_unref( file );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'backup_db()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		ok = FALSE;
	}

	return( ok );
}

static ofaJsonHeader *
build_json_header( const ofaIDBConnect *self, const gchar *comment )
{
	ofaJsonHeader *header;
	sIDBConnect *sdata;
	ofaIDBDossierMeta *dossier_meta;
	ofaIDBProvider *provider;
	ofaHub *hub;
	ofoDossier *dossier;
	ofaExtenderCollection *extenders;
	ofaExtenderModule *plugin;
	const GList *modules, *itm;
	GList *dbmodels, *itb;
	gchar *canon, *display, *id, *version;

	dossier_meta = ofa_idbconnect_get_dossier_meta( self );
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	hub = ofa_idbprovider_get_hub( provider );

	header = ofa_json_header_new();

	dossier = ofa_hub_get_dossier( hub );
	ofa_json_header_set_is_current( header, ofo_dossier_is_current( dossier ));
	ofa_json_header_set_begin_date( header, ofo_dossier_get_exe_begin( dossier ));
	ofa_json_header_set_end_date( header, ofo_dossier_get_exe_end( dossier ));

	extenders = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_modules( extenders );

	for( itm=modules ; itm ; itm=itm->next ){
		plugin = OFA_EXTENDER_MODULE( itm->data );
		canon = ofa_extender_module_get_canon_name( plugin );
		display = ofa_extender_module_get_display_name( plugin );
		version = ofa_extender_module_get_version( plugin );
		ofa_json_header_set_plugin( header, canon, display, version );
		g_free( version );
		g_free( display );
		g_free( canon );
	}

	dbmodels = ofa_hub_get_for_type( hub, OFA_TYPE_IDBMODEL );

	for( itb=dbmodels ; itb ; itb=itb->next ){
		if( MY_IS_IIDENT( itb->data )){
			id = my_iident_get_canon_name( MY_IIDENT( itb->data ), NULL );
			version = my_iident_get_version( MY_IIDENT( itb->data ), NULL );
			ofa_json_header_set_dbmodel( header, id, version );
			g_free( version );
			g_free( id );
		}
	}

	g_list_free_full( dbmodels, ( GDestroyNotify ) g_object_unref );

	ofa_json_header_set_comment( header, comment );

	sdata = get_instance_data( self );
	ofa_json_header_set_current_user( header, sdata->account );

	return( header );
}

static gboolean
backup_json_header( const ofaIDBConnect *connect, GOutputStream *output_stream, const gchar *json )
{
	static const gchar *thisfn = "ofa_idbconnect_backup_json_header";
	GZlibCompressor *compressor;
	gboolean finished, abort;
	GConverterResult result;
	GError *error;
	gsize insize, outsize, bytes_read, bytes_written;
	void *outbuf;
	GInputStream *input_stream;
	GFileInfo *info;

	/* create a compressor */
	compressor = g_zlib_compressor_new( G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1 );
	info = g_file_info_new();
	g_file_info_set_name( info, "JSON_Header" );
	g_zlib_compressor_set_file_info( compressor, info );
	g_object_unref( info );

	finished = FALSE;
	abort = FALSE;
	error = NULL;
	insize = my_strlen( json );
	outsize = 2*insize;					// hope this is enough
	outbuf = g_new0( gchar, outsize );

	while( !finished && !abort ){
		result = g_converter_convert(
				G_CONVERTER( compressor ), json, insize, outbuf, outsize, G_CONVERTER_INPUT_AT_END, &bytes_read, &bytes_written, &error );
		/* when writing the json header
		 * result=G_CONVERTER_FINISHED, read=516, written=273 */
		switch( result ){
			case G_CONVERTER_ERROR:
				g_warning( "%s: result=G_CONVERTER_ERROR, error=%s", thisfn, error->message );
				g_error_free( error );
				abort = TRUE;
				break;
			case G_CONVERTER_CONVERTED:
				g_debug( "%s: result=G_CONVERTER_CONVERTED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				break;
			case G_CONVERTER_FINISHED:
				g_debug( "%s: result=G_CONVERTER_FINISHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				input_stream = g_memory_input_stream_new_from_data( outbuf, outsize, NULL );
				g_output_stream_splice( output_stream, input_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE, NULL, NULL );
				g_object_unref( input_stream );
				finished = TRUE;
				break;
			case G_CONVERTER_FLUSHED:
				g_debug( "%s: result=G_CONVERTER_FLUSHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				finished = TRUE;
				break;
		}
	}

	g_free( outbuf );
	g_object_unref( compressor );

	return( finished );
}

/**
 * ofa_idbconnect_restore:
 * @connect: a #ofaIDBConnect instance which handles a superuser
 *  connection on the DBMS at server-level. It is expected this
 *  @connect object holds a valid #ofaIDBDossierMeta object which describes
 *  the target dossier.
 * @period: [allow-none]: a #ofaIDBExerciceMeta object which describes the
 *  target exercice; if %NULL, the file is restored on the current
 *  exercice.
 * @uri: the source file to be restored.
 *
 * Restore the file on the specified period.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_restore( const ofaIDBConnect *connect,
							const ofaIDBExerciceMeta *period, const gchar *uri,
							const gchar *adm_account, const gchar *adm_password )
{
	static const gchar *thisfn = "ofa_idbconnect_restore";
	sIDBConnect *sdata;
	ofaIDBExerciceMeta *target_period;
	ofaIDBProvider *provider;
	ofaIDBConnect *target_connect;
	gboolean ok;

	g_debug( "%s: connect=%p, period=%p, uri=%s, adm_account=%s, adm_password=%s",
			thisfn, ( void * ) connect, ( void * ) period, uri,
			adm_account, adm_password ? "******" : adm_password );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( !period || OFA_IS_IDBEXERCICE_META( period ), FALSE );
	g_return_val_if_fail( my_strlen( uri ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->restore ){
		ok = FALSE;
		sdata = get_instance_data( connect );
		g_return_val_if_fail( sdata->dossier_meta && OFA_IS_IDBDOSSIER_META( sdata->dossier_meta ), FALSE );

		if( period ){
			target_period = ( ofaIDBExerciceMeta * ) period;
		} else {
			target_period = ofa_idbdossier_meta_get_current_period( sdata->dossier_meta );
		}
		g_return_val_if_fail( target_period && OFA_IS_IDBEXERCICE_META( target_period ), FALSE );

		if( OFA_IDBCONNECT_GET_INTERFACE( connect )->restore( connect, target_period, uri )){
			provider = ofa_idbdossier_meta_get_provider( sdata->dossier_meta );
			target_connect = ofa_idbprovider_new_connect(
					provider, sdata->account, sdata->password, sdata->dossier_meta, target_period );
			set_admin_credentials( target_connect, adm_account, adm_password, NULL );
			g_object_unref( target_connect );
			ok = TRUE;
		}

		return( ok );
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'restore()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
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
	static const gchar *thisfn = "ofa_idbconnect_archive_and_new";

	g_debug( "%s: connect=%p, root_account=%s, root_password=%s, begin_next=%p, end_next=%p",
			thisfn, ( void * ) connect, root_account, root_password ? "******":root_password,
			( void * ) begin_next, ( void * ) end_next );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->archive_and_new ){
		return( OFA_IDBCONNECT_GET_INTERFACE( connect )->archive_and_new( connect, root_account, root_password, begin_next, end_next ));
	}

	g_info( "%s: ofaIDBConnect's %s implementation does not provide 'archive_and_new()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
}

/**
 * ofa_idbconnect_period_new:
 * @connect: an #ofaIDBConnect object which handles a superuser
 *  connection on the DBMS at server-level. The connection may have
 *  been opened with editor informations, and period member is not
 *  expected to be valid.
 * @adm_account: the Openbook administrative user account.
 * @adm_password: the Openbook administrative user password.
 * @msgerr: [out][allow-none]: a placeholder for an error message.
 *
 * Creates the minimal storage space required to handle the dossier in
 * the DBMS provider.
 * Defines the administrative user, and grants permissions.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_period_new( const ofaIDBConnect *connect, const gchar *adm_account, const gchar *adm_password, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbconnect_period_new";
	sIDBConnect *sdata;
	gboolean ok;
	GString *query;
	ofaIDBProvider *provider;
	ofaIDBExerciceMeta *period;
	ofaIDBConnect *db_connection;

	g_debug( "%s: connect=%p, adm_account=%s, adm_password=%s, msgerr=%p",
			thisfn, ( void * ) connect, adm_account, adm_password ? "******":adm_password, ( void * ) msgerr );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	sdata = get_instance_data( connect );

	/* create the minimal database and grant the user */
	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->period_new ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->period_new( connect, msgerr );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'period_new()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		return( FALSE );
	}

	db_connection = NULL;
	period = NULL;
	provider = NULL;
	query = g_string_new( "" );

	/* define the dossier administrative account
	 * requires another superuser connection, on the exercice at this time */
	if( ok ){
		period = ofa_idbdossier_meta_get_current_period( sdata->dossier_meta );
		provider = ofa_idbdossier_meta_get_provider( sdata->dossier_meta );
		db_connection = ofa_idbprovider_new_connect( provider, sdata->account, sdata->password, sdata->dossier_meta, period );
		ok = ( db_connection != NULL );
	}
	if( ok ){
		/* initialize the newly created database */
		/* Resized in v28 */
		g_string_printf( query,
				"CREATE TABLE IF NOT EXISTS OFA_T_AUDIT ("
				"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
				"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query timestamp',"
				"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query content') " );
		ok = ofa_idbconnect_query( db_connection, query->str, FALSE );
		if( !ok && msgerr ){
			*msgerr = ofa_idbconnect_get_last_error( db_connection );
		}
	}
	if( ok ){
		g_string_printf( query,
				"CREATE TABLE IF NOT EXISTS OFA_T_ROLES ("
					"ROL_USER     VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'User account',"
					"ROL_IS_ADMIN INTEGER                            COMMENT 'Whether the user has administration role') " );
		ok = ofa_idbconnect_query( db_connection, query->str, FALSE );
		if( !ok && msgerr ){
			*msgerr = ofa_idbconnect_get_last_error( db_connection );
		}
	}
	/* set admin credentials */
	if( ok ){
		ok = set_admin_credentials( db_connection, adm_account, adm_password, msgerr );
	}
	g_string_free( query, TRUE );

	g_clear_object( &db_connection );

	return( ok );
}

/*
 * set_admin_credentials:
 * @connect: an #ofaIDBConnect object which handles a superuser
 *  connection on the DBMS. It is expected this @connect object holds
 *  valid #ofaIDBDossierMeta and #ofaIDBExerciceMeta objects which
 *  describe the target exercice.
 * @adm_account: the Openbook administrative user account.
 * @adm_password: the Openbook administrative user password.
 * @msgerr: [out][allow-none]: a placeholder for an error message.
 *
 * Defines the administrative user, and grants permissions to him on
 * the specified dossier/exercice.
 *
 * Returns: %TRUE if successful.
 */
static gboolean
set_admin_credentials( const ofaIDBConnect *connect, const gchar *adm_account, const gchar *adm_password, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbconnect_set_admin_credentials";
	sIDBConnect *sdata;
	gboolean ok;
	GString *query;

	g_debug( "%s: connect=%p, adm_account=%s, adm_password=%s, msgerr=%p",
			thisfn, ( void * ) connect, adm_account, adm_password ? "******" : adm_password, ( void * ) msgerr );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );
	g_return_val_if_fail( my_strlen( adm_account ), FALSE );

	ok = FALSE;
	sdata = get_instance_data( connect );
	query = g_string_new( "" );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->grant_user ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->grant_user( connect, sdata->exercice_meta, adm_account, adm_password, msgerr );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'grant_user()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		return( FALSE );
	}

	/* be sure the user has 'admin' role
	 * Insert works if row did not exist yet while Update works
	 * if row did exist */
	if( ok ){
		g_string_printf( query,
					"INSERT IGNORE INTO OFA_T_ROLES "
					"	(ROL_USER,ROL_IS_ADMIN) VALUES ('%s',1)", adm_account );
		ok = ofa_idbconnect_query( connect, query->str, FALSE );
		if( !ok && msgerr ){
			*msgerr = ofa_idbconnect_get_last_error( connect );
		}
	}
	if( ok ){
		g_string_printf( query,
					"UPDATE OFA_T_ROLES SET ROL_IS_ADMIN=1 WHERE ROL_USER='%s'", adm_account );
		ok = ofa_idbconnect_query( connect, query->str, FALSE );
		if( !ok && msgerr ){
			*msgerr = ofa_idbconnect_get_last_error( connect );
		}
	}
	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofa_idbconnect_transaction_start:
 * @connect: an #ofaIDBConnect object which handles a user
 *  connection on the DBMS.
 * @display_error: whether display an error in a dialog box.
 * @msgerr: [out][allow-none]: if set, and @display_error is %FALSE,
 *  then this is a placeholder for an error message.
 *
 * Start a transaction.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_transaction_start( const ofaIDBConnect *connect, gboolean display_error, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbconnect_transaction_start";
	gboolean ok;

	g_debug( "%s: connect=%p, display_error=%s, msgerr=%p",
			thisfn, ( void * ) connect, display_error ? "True":"False", ( void * ) msgerr );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_start ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_start( connect );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'transaction_start()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		return( FALSE );
	}

	return( ok );
}

/**
 * ofa_idbconnect_transaction_cancel:
 * @connect: an #ofaIDBConnect object which handles a user
 *  connection on the DBMS.
 * @display_error: whether display an error in a dialog box.
 * @msgerr: [out][allow-none]: if set, and @display_error is %FALSE,
 *  then this is a placeholder for an error message.
 *
 * Cancel a transaction.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_transaction_cancel( const ofaIDBConnect *connect, gboolean display_error, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbconnect_transaction_cancel";
	gboolean ok;

	g_debug( "%s: connect=%p, display_error=%s, msgerr=%p",
			thisfn, ( void * ) connect, display_error ? "True":"False", ( void * ) msgerr );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_cancel ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_cancel( connect );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'transaction_cancel()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		return( FALSE );
	}

	return( ok );
}

/**
 * ofa_idbconnect_transaction_commit:
 * @connect: an #ofaIDBConnect object which handles a user
 *  connection on the DBMS.
 * @display_error: whether display an error in a dialog box.
 * @msgerr: [out][allow-none]: if set, and @display_error is %FALSE,
 *  then this is a placeholder for an error message.
 *
 * Commit a transaction.
 *
 * Returns: %TRUE if successful.
 */
gboolean
ofa_idbconnect_transaction_commit( const ofaIDBConnect *connect, gboolean display_error, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbconnect_transaction_commit";
	gboolean ok;

	g_debug( "%s: connect=%p, display_error=%s, msgerr=%p",
			thisfn, ( void * ) connect, display_error ? "True":"False", ( void * ) msgerr );

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_commit ){
		ok = OFA_IDBCONNECT_GET_INTERFACE( connect )->transaction_commit( connect );

	} else {
		g_info( "%s: ofaIDBConnect's %s implementation does not provide 'transaction_commit()' method",
				thisfn, G_OBJECT_TYPE_NAME( connect ));
		return( FALSE );
	}

	return( ok );
}

static sIDBConnect *
get_instance_data( const ofaIDBConnect *connect )
{
	sIDBConnect *sdata;

	sdata = ( sIDBConnect * ) g_object_get_data( G_OBJECT( connect ), IDBCONNECT_DATA );

	if( !sdata ){
		sdata = g_new0( sIDBConnect, 1 );
		g_object_set_data( G_OBJECT( connect ), IDBCONNECT_DATA, sdata );
		g_object_weak_ref( G_OBJECT( connect ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIDBConnect *sdata, GObject *finalized_connect )
{
	static const gchar *thisfn = "ofa_idbconnect_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_connect=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_connect );

	g_clear_object( &sdata->dossier_meta );
	g_clear_object( &sdata->exercice_meta );
	g_free( sdata->account );
	g_free( sdata->password );
	g_free( sdata );
}
