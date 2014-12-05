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
#include <stdlib.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

/* private instance data
 */
struct _ofaDbmsPrivate {
	gboolean  dispose_has_run;

	/* runtime data
	 * these are set when the connection is successfully opened
	 */
	gchar    *dname;					/* name of the dossier */
	gchar    *dbname;					/* name of the database */
	gchar    *account;					/* account */
	gchar    *pname;
	ofaIDbms *pmodule;
	void     *phandle;					/* returned by the provider */
	gboolean  connected;
};

G_DEFINE_TYPE( ofaDbms, ofa_dbms, G_TYPE_OBJECT )

static gboolean  dbms_connect( ofaDbms *dbms, const gchar *name, const gchar *dbname, const gchar *account, const gchar *password, gboolean display_error );
static gchar    *get_provider_name_from_dossier_name( ofaDbms *dbms, const gchar *name, gboolean display_error );
static ofaIDbms *get_provider_module_from_provider_name( ofaDbms *dbms, const gchar *name, const gchar *provider_name, gboolean display_error );
static ofaIDbms *get_provider_module_from_dossier_name( ofaDbms *dbms, const gchar *dname, gboolean display_error );
static void      error_already_connected( const ofaDbms *dbms );
static void      error_dossier_not_exists( const ofaDbms *dbms, const gchar *name );
static void      error_provider_not_defined( const ofaDbms *dbms, const gchar *name );
static void      error_module_not_found( const ofaDbms *dbms, const gchar *name, const gchar *provider );
static void      error_connect( const ofaDbms *dbms, const gchar *name, const gchar *dbname, const gchar *account );
static void      error_with_infos( const ofaDbms *dbms, const gchar *name, const gchar *dbname, const gchar *account, const gchar *provider, const gchar *msg );
static void      error_query( const ofaDbms *dbms, const gchar *query );
static void      audit_query( const ofaDbms *dbms, const gchar *query );
static gchar    *quote_query( const gchar *query );

static void
dbms_finalize( GObject *instance )
{
	ofaDbmsPrivate *priv;

	static const gchar *thisfn = "ofa_dbms_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMS( instance ));

	/* free data members here */
	priv = OFA_DBMS( instance )->priv;

	g_free( priv->dname );
	g_free( priv->dbname );
	g_free( priv->account );
	g_free( priv->pname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_parent_class )->finalize( instance );
}

static void
dbms_dispose( GObject *instance )
{
	ofaDbmsPrivate *priv;

	priv = OFA_DBMS( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->pmodule ){
			if( priv->phandle ){
				ofa_idbms_close( priv->pmodule, priv->phandle );
				priv->phandle = NULL;
			}
			g_clear_object( &priv->pmodule );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_parent_class )->dispose( instance );
}

static void
ofa_dbms_init( ofaDbms *self )
{
	static const gchar *thisfn = "ofa_dbms_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DBMS, ofaDbmsPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_dbms_class_init( ofaDbmsClass *klass )
{
	static const gchar *thisfn = "ofa_dbms_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbms_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbms_finalize;

	g_type_class_add_private( klass, sizeof( ofaDbmsPrivate ));
}

/**
 * ofa_dbms_new:
 */
ofaDbms *
ofa_dbms_new( void )
{
	ofaDbms *dbms;

	dbms = g_object_new( OFA_TYPE_DBMS, NULL );

	return( dbms );
}

/**
 * ofa_dbms_connect:
 * @dbms: this #ofaDbms object
 * @dname: the name of the dossier (from the settings)
 * @dbname: [allow-none]: the name of the targeted database
 * @account: the account to be used, default to unix login name
 * @password: the password
 * @display_error: whether to display an error dialog in case of an
 *  unsuccessful connection.
 *
 * Open a connection to the dossier specified by its @dname name.
 *
 * Returns: %TRUE if the connection is successful, %FALSE else.
 *
 * Whether a connection has been successfully established is set by
 * positionning the 'connected' private variable to %TRUE.
 */
gboolean
ofa_dbms_connect( ofaDbms *dbms,
					const gchar *dname, const gchar *dbname,
					const gchar *account, const gchar *password, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_connect";
	ofaDbmsPrivate *priv;

	g_return_val_if_fail( dbms && OFA_IS_DBMS( dbms ), FALSE );
	g_return_val_if_fail( dname && g_utf8_strlen( dname, -1 ), FALSE );

	g_debug( "%s: dbms=%p, dname=%s, dbname=%s, account=%s, password=%s, display_error=%s",
			thisfn, ( void * ) dbms,
			dname, dbname,
			account, password, display_error ? "True":"False" );

	priv = dbms->priv;

	if( !priv->dispose_has_run ){

		return( dbms_connect( dbms, dname, dbname, account, password, display_error ));
	}

	return( FALSE );
}

static gboolean
dbms_connect( ofaDbms *dbms,
					const gchar *dname, const gchar *dbname,
					const gchar *account, const gchar *password, gboolean display_error )
{
	ofaDbmsPrivate *priv;
	ofaIDbms *module;
	void *handle;

	priv = dbms->priv;

	if( priv->connected ){
		if( display_error ){
			error_already_connected( dbms );
		}
		return( priv->connected );
	}

	module = get_provider_module_from_dossier_name( dbms, dname, display_error );
	if( !module ){
		return( FALSE );
	}
	g_return_val_if_fail( OFA_IS_IDBMS( module ), FALSE );

	handle = ofa_idbms_connect( module, dname, dbname, account, password );
	if( !handle ){
		if( display_error ){
			error_connect( dbms, dname, dbname, account );
		}
		return( FALSE );
	}

	priv->dname = g_strdup( dname );
	priv->dbname = g_strdup( dbname );
	priv->account = g_strdup( account );
	priv->pmodule = module;
	priv->phandle = handle;

	priv->connected = TRUE;

	return( priv->connected );
}

/*
 * get from the settings the provider name from the dossier name
 */
static gchar *
get_provider_name_from_dossier_name( ofaDbms *dbms, const gchar *dname, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_get_provider_name_from_dossier_name";
	gchar *provider_name;

	if( !ofa_settings_has_dossier( dname )){
		if( display_error ){
			error_dossier_not_exists( dbms, dname );
		} else {
			g_warning( "%s: dname=%s: dossier is not defined", thisfn, dname );
		}
		return( NULL );
	}

	provider_name = ofa_settings_get_dossier_provider( dname );
	if( !provider_name || !g_utf8_strlen( provider_name, -1 )){
		if( display_error ){
			error_provider_not_defined( dbms, dname );
		} else {
			g_warning( "%s: dname=%s: provider not defined", thisfn, dname );
		}
		g_free( provider_name );
		return( NULL );
	}

	return( provider_name );
}

/*
 * returns the IDbms provider module from its name
 */
static ofaIDbms *
get_provider_module_from_provider_name( ofaDbms *dbms, const gchar *dname, const gchar *provider_name, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_get_provider_module_from_provider_name";
	ofaIDbms *provider_module;

	provider_module = ofa_idbms_get_provider_by_name( provider_name );
	if( !provider_module ){
		if( display_error ){
			error_module_not_found( dbms, dname, provider_name );
		} else {
			g_warning( "%s: dname=%s, provider=%s: module not found",
					thisfn, dname, provider_name );
		}
		return( NULL );
	}

	return( provider_module );
}

/*
 * returns the IDbms provider module from the dossier name
 *
 * stores the provider name in the ofaDbms object so that it is
 * available in error messages
 */
static ofaIDbms *
get_provider_module_from_dossier_name( ofaDbms *dbms, const gchar *dname, gboolean display_error )
{
	ofaDbmsPrivate *priv;
	gchar *prov_name;
	ofaIDbms *prov_module;

	priv = dbms->priv;

	prov_name = get_provider_name_from_dossier_name( dbms, dname, display_error );
	if( !prov_name ){
		return( NULL );
	}
	g_return_val_if_fail( g_utf8_strlen( prov_name, -1 ), NULL );

	g_free( priv->pname );
	priv->pname = g_strdup( prov_name );

	prov_module = get_provider_module_from_provider_name( dbms, dname, prov_name, display_error );
	if( !prov_module ){
		return( NULL );
	}
	g_return_val_if_fail( OFA_IS_IDBMS( prov_module ), NULL );

	return( prov_module );
}

static void
error_already_connected( const ofaDbms *dbms )
{
	ofaDbmsPrivate *priv;

	priv = dbms->priv;

	error_with_infos( dbms,
			priv->dname, priv->dbname, priv->account, priv->pname,
			_( "Already connected" ));
}

static void
error_dossier_not_exists( const ofaDbms *dbms, const gchar *dname )
{
	GtkWidget *dlg;
	gchar *str;

	str = g_strdup_printf(
				_( "The '%s' dossier is not defined" ), dname );

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str );

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

static void
error_provider_not_defined( const ofaDbms *dbms, const gchar *dname )
{
	GtkWidget *dlg;
	gchar *str;

	str = g_strdup_printf(
				_( "No provider is defined for the '%s' dossier" ), dname );

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str );

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

static void
error_module_not_found( const ofaDbms *dbms, const gchar *dname, const gchar *provider )
{
	GtkWidget *dlg;
	gchar *str;

	str = g_strdup_printf(
				_( "The dossier '%s' is defined to use the '%s' DBMS provider, "
						"but this one is not found" ), dname, provider );

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str );

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

static void
error_connect( const ofaDbms *dbms,
				const gchar *dname, const gchar *dbname, const gchar *account )
{
	error_with_infos( dbms,
			dname, dbname, dbms->priv->pname, account,
			_( "Error while trying to connect to the database" ));
}

static void
error_with_infos( const ofaDbms *dbms,
		const gchar *dname, const gchar *dbname, const gchar *account, const gchar *provider,
		const gchar *msg )
{
	GtkWidget *dlg;
	GString *str;

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg );

	str = g_string_new( "" );
	g_string_append_printf( str, _( "Dossier name:\t%s\n" ), dname );
	g_string_append_printf( str, _( "Provider name:\t%s\n" ), provider );
	g_string_append_printf( str, _( "Database name:\t\t%s\n" ), dbname );
	g_string_append_printf( str, _( "Connection account:\t%s\n" ), account );

	gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), "%s", str->str );
	g_string_free( str, TRUE );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

/**
 * ofa_dbms_get_exercices:
 * @dbms: this #ofaDbms object
 * @dname: the name of the dossier from the settings.
 *
 * Returns: the list of exercices as a #GSList of semi-colon separated
 * strings:
 * - a displayable label
 * - the corresponding database name.
 *
 * The returned list should be #ofa_dbms_free_exercices() by the caller.
 */
GSList *
ofa_dbms_get_exercices( ofaDbms *dbms, const gchar *dname )
{
	ofaIDbms *pmodule;
	GSList *list;

	list = NULL;
	pmodule = get_provider_module_from_dossier_name( dbms, dname, FALSE );
	if( pmodule ){
		g_return_val_if_fail( OFA_IS_IDBMS( pmodule ), NULL );

		list = ofa_idbms_get_exercices( pmodule, dname );
	}

	return( list );
}

/**
 * ofa_dbms_query:
 * @dbms: this #ofaDbms object
 * @query: the query
 * @display_error: whether the error should be published in a dialog box
 *
 * Executes the specified @query query.
 *
 * As this query doesn't return results, it is most probably an update.
 * It is so written in the audit table.
 */
gboolean
ofa_dbms_query( const ofaDbms *dbms, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_query";
	ofaDbmsPrivate *priv;
	gboolean query_ok;

	g_debug( "%s: dbms=%p, query='%s', display_error=%s",
			thisfn, ( void * ) dbms, query, display_error ? "True":"False" );

	g_return_val_if_fail( dbms && OFA_IS_DBMS( dbms ), FALSE );

	priv = dbms->priv;
	query_ok = FALSE;

	g_return_val_if_fail( priv->pmodule && OFA_IS_IDBMS( priv->pmodule ), FALSE );
	g_return_val_if_fail( priv->phandle, FALSE );

	if( !priv->dispose_has_run ){

		query_ok = ofa_idbms_query( priv->pmodule, priv->phandle, query );

		if( !query_ok ){
			if( display_error ){
				error_query( dbms, query );
			}
		} else {
			audit_query( dbms, query );
		}
	}

	return( query_ok );
}

/**
 * ofa_dbms_query_ex:
 * @dbms: this #ofaDbms object
 * @query: the query to be executed
 * @result: [out]: the result set as a GSList of ordered rows
 * @display_error: whether the error should be published in a dialog box
 *
 * Returns: %TRUE if the sentence has been successfully executed,
 * %FALSE else.
 *
 * Each GSList->data of the result set is a pointer to a GSList of
 * ordered columns. A field is so the GSList[column] data ; it is
 * always allocated (though maybe of a zero length), or NULL (SQL-NULL
 * translation).
 *
 * The result set should be freed with #ofa_dbms_free_results().
 */
gboolean
ofa_dbms_query_ex( const ofaDbms *dbms, const gchar *query, GSList **result, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_query_ex";
	ofaDbmsPrivate *priv;
	gboolean ok;

	g_debug( "%s: dbms=%p, query='%s', result=%p, display_error=%s",
			thisfn, ( void * ) dbms, query, ( void * ) result, display_error ? "True":"False" );

	g_return_val_if_fail( dbms && OFA_IS_DBMS( dbms ), FALSE );
	g_return_val_if_fail( result, FALSE );

	ok = FALSE;
	priv = dbms->priv;

	g_return_val_if_fail( priv->pmodule && OFA_IS_IDBMS( priv->pmodule ), FALSE );
	g_return_val_if_fail( priv->phandle, FALSE );

	if( !priv->dispose_has_run ){

		if( !ofa_idbms_query_ex( priv->pmodule, priv->phandle, query, result )){
			if( display_error ){
				error_query( dbms, query );
			}
		} else {
			ok = TRUE;
		}
	}

	return( ok );
}

/**
 * ofa_dbms_query_int:
 * @dbms: this #ofaDbms object
 * @query: the query to be executed
 * @ivalue: [out]: the returned integer value
 * @display_error: whether the error should be published in a dialog box
 *
 * A simple query for getting a single int.
 *
 * Returns: %TRUE if the sentence has been successfully executed,
 * %FALSE else.
 */
gboolean
ofa_dbms_query_int( const ofaDbms *dbms, const gchar *query, gint *ivalue, gboolean display_error )
{
	static const gchar *thisfn = "ofa_dbms_query_int";
	gboolean ok;
	GSList *result, *icol;

	g_debug( "%s: dbms=%p, query='%s', ivalue=%p, display_error=%s",
			thisfn, ( void * ) dbms, query, ( void * ) ivalue, display_error ? "True":"False" );

	*ivalue = 0;
	ok = ofa_dbms_query_ex( dbms, query, &result, display_error );

	if( ok ){
		icol = ( GSList * ) result->data;
		if( icol && icol->data ){
			*ivalue = atoi(( gchar * ) icol->data );
		}
		ofa_dbms_free_results( result );
	}

	return( ok );
}

static void
error_query( const ofaDbms *dbms, const gchar *query )
{
	ofaDbmsPrivate *priv;
	GtkWidget *dlg;
	gchar *str;

	priv = dbms->priv;

	dlg = gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", query );

	str = ofa_idbms_last_error( priv->pmodule, priv->phandle );

	/* query_ex returns NULL if the result is empty: this is not an error */
	if( str && g_utf8_strlen( str, -1 )){
		gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), "%s", str );
	}
	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );
}

static void
audit_query( const ofaDbms *dbms, const gchar *query )
{
	ofaDbmsPrivate *priv;
	gchar *quoted;
	gchar *audit;

	priv = dbms->priv;

	quoted = quote_query( query );
	audit = g_strdup_printf( "INSERT INTO OFA_T_AUDIT (AUD_QUERY) VALUES ('%s')", quoted );
	ofa_idbms_query( priv->pmodule, priv->phandle, audit );

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
