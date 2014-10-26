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
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-sgbd.h"

/* private instance data
 */
struct _ofoSgbdPrivate {
	gboolean  dispose_has_run;

	/* properties
	 */
	gchar    *label;
	gchar    *provider;
	ofaIDbms *module;
	gboolean  connected;

	gchar    *account;
	gchar    *password;
	void     *handle;
};

G_DEFINE_TYPE( ofoSgbd, ofo_sgbd, G_TYPE_OBJECT )

static void     sgbd_connect_static( ofoSgbd *sgbd, gboolean with_dbname, const gchar *dbname, const gchar *account, const gchar *password, gboolean display_error );
static void     error_module_not_found( const gchar *provider );
static void     error_already_connected( const ofoSgbd *sgbd );
static void     error_provider_not_defined( const ofoSgbd *sgbd );
static void     error_connect( const ofoSgbd *sgbd, const gchar *account );
static void     error_query( const ofoSgbd *sgbd, const gchar *query );
static void     sgbd_audit_query( const ofoSgbd *sgbd, const gchar *query );
static gchar   *quote_query( const gchar *query );

static void
sgbd_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_sgbd_finalize";
	ofoSgbdPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_SGBD( instance ));

	/* free data members here */
	priv = OFO_SGBD( instance )->priv;

	g_free( priv->label );
	g_free( priv->provider );

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_sgbd_parent_class )->finalize( instance );
}

static void
sgbd_dispose( GObject *instance )
{
	ofoSgbdPrivate *priv;

	priv = OFO_SGBD( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->module ){
			if( priv->handle ){
				ofa_idbms_close( priv->module, priv->handle );
				priv->handle = NULL;
			}
			g_clear_object( &priv->module );
		}
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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_SGBD, ofoSgbdPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofo_sgbd_class_init( ofoSgbdClass *klass )
{
	static const gchar *thisfn = "ofo_sgbd_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoSgbdPrivate ));

	G_OBJECT_CLASS( klass )->dispose = sgbd_dispose;
	G_OBJECT_CLASS( klass )->finalize = sgbd_finalize;

	g_type_class_add_private( klass, sizeof( ofoSgbdPrivate ));
}

/**
 * ofo_sgbd_new:
 * @label: the label of the dossier.
 *
 * Allocates a new #ofoSgbd object intended to connect to the specified
 * dossier.
 *
 * Returns: a newly allocated #ofoSgbd object, which should be
 * g_object_unref() by the caller.
 */
ofoSgbd *
ofo_sgbd_new( const gchar *label )
{
	static const gchar *thisfn = "ofo_sgbd_new";
	ofoSgbd *sgbd;

	g_debug( "%s: label=%s", thisfn, label );

	sgbd = g_object_new( OFO_TYPE_SGBD, NULL );

	sgbd->priv->label = g_strdup( label );

	return( sgbd );
}

static void
error_module_not_found( const gchar *provider )
{
	GtkMessageDialog *dlg;
	gchar *str;

	str = g_strdup_printf(
				_( "Unable to find the required '%s' DBMS module" ), provider );

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str ));

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/**
 * ofo_sgbd_connect:
 * @sgbd: this #ofoSgbd object
 * @account: the account to be used, default to unix login name
 * @password: the password
 * @display_error: whether to display an error dialog in case of an
 *  unsuccessful connection.
 *
 * Open a connection either to the specified dossier.
 *
 * Returns: %TRUE if the connection is successful, %FALSE else.
 */
gboolean
ofo_sgbd_connect( ofoSgbd *sgbd, const gchar *account, const gchar *password,
				gboolean display_error )
{
	static const gchar *thisfn = "ofo_sgbd_connect";

	g_debug( "%s: sgbd=%p, account=%s, password=%s, display_error=%s",
			thisfn,
			( void * ) sgbd,
			account, password, display_error ? "True":"False" );

	sgbd_connect_static( sgbd, FALSE, NULL, account, password, display_error );

	return( sgbd->priv->connected );
}

/**
 * ofo_sgbd_connect_ex:
 * @sgbd: this #ofoSgbd object
 * @dbname: the name of the database to be used as default
 * @account: the account to be used, default to unix login name
 * @password: the password
 * @display_error: whether to display an error dialog in case of an
 *  unsuccessful connection.
 *
 * Open a connection either to the specified database using the
 * connection properties described for the specified dossier.
 *
 * Returns: %TRUE if the connection is successful, %FALSE else.
 */
gboolean
ofo_sgbd_connect_ex( ofoSgbd *sgbd, const gchar *dbname, const gchar *account, const gchar *password,
				gboolean display_error )
{
	static const gchar *thisfn = "ofo_sgbd_connect_ex";

	g_debug( "%s: sgbd=%p, account=%s, password=%s, display_error=%s",
			thisfn,
			( void * ) sgbd,
			account, password, display_error ? "True":"False" );

	sgbd_connect_static( sgbd, TRUE, dbname, account, password, display_error );

	return( sgbd->priv->connected );
}

/*
 * sgbd_connect_ex:
 * @sgbd: this #ofoSgbd object
 * @with_dbname: whether to use the specified database name
 * @dbname: the name of the database to be used as default
 * @account: the account to be used, default to unix login name
 * @password: the password
 * @display_error: whether to display an error dialog in case of an
 *  unsuccessful connection.
 *
 * Open a connection either to the specified database using the
 * connection properties described for the specified dossier.
 *
 * Returns: %TRUE if the connection is successful, %FALSE else.
 */
static void
sgbd_connect_static( ofoSgbd *sgbd,
					gboolean with_dbname, const gchar *dbname,
					const gchar *account, const gchar *password, gboolean display_error )
{
	static const gchar *thisfn = "ofo_sgbd_connect_static";
	gchar *provider_name;
	ofaIDbms *module;
	ofoSgbdPrivate *priv;

	g_return_if_fail( sgbd && OFO_IS_SGBD( sgbd ));

	priv = sgbd->priv;

	if( priv->dispose_has_run ){
		return;
	}

	if( priv->connected ){
		if( display_error ){
			error_already_connected( sgbd );
		}
		return;
	}

	g_return_if_fail( priv->label && g_utf8_strlen( priv->label, -1 ));

	provider_name = ofa_settings_get_dossier_provider( priv->label );
	if( !provider_name || !g_utf8_strlen( provider_name, -1 )){
		if( display_error ){
			error_provider_not_defined( sgbd );
		} else {
			g_warning( "%s: label=%s: provider not defined", thisfn, priv->label );
		}
		g_free( provider_name );
		return;
	}

	module = ofa_idbms_get_provider_by_name( provider_name );
	if( !module ){
		if( display_error ){
			error_module_not_found( provider_name );
		} else {
			g_warning( "%s: label=%s, provider=%s: module not found",
					thisfn, priv->label, provider_name );
		}
		g_free( provider_name );
		return;
	}

	priv->provider = provider_name;
	priv->module = module;
	priv->handle = ofa_idbms_connect(
							module, priv->label, dbname, with_dbname, account, password );

	if( !priv->handle ){
		if( display_error ){
			error_connect( sgbd, account );
		}
		return;
	}

	priv->connected = TRUE;
	priv->account = g_strdup( account );
	priv->password = g_strdup( password );
}

static void
error_already_connected( const ofoSgbd *sgbd )
{
	ofoSgbdPrivate *priv;
	GtkMessageDialog *dlg;
	GString *str;

	priv = sgbd->priv;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", _( "Already connected to the database" )));

	str = g_string_new( "" );

	if( priv->label ){
		g_string_append_printf( str, "Label: %s\n", priv->label );
	}
	g_string_append_printf( str, "Provider: %s\n", priv->provider );
	g_string_append_printf( str, "Account: %s\n", priv->account );

	gtk_message_dialog_format_secondary_text( dlg, "%s", str->str );
	g_string_free( str, TRUE );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

static void
error_provider_not_defined( const ofoSgbd *sgbd )
{
	GtkMessageDialog *dlg;
	gchar *str;

	str = g_strdup_printf(
				_( "No provider defined for '%s' dossier" ), sgbd->priv->label );

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", str ));
	g_free( str );
	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

static void
error_connect( const ofoSgbd *sgbd, const gchar *account )
{
	ofoSgbdPrivate *priv;
	GtkMessageDialog *dlg;
	GString *str;

	priv = sgbd->priv;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", _( "Unable to connect to the database" )));

	str = g_string_new( "" );

	g_string_append_printf( str, "Label: %s\n", priv->label );
	g_string_append_printf( str, "Account: %s\n", account );

	gtk_message_dialog_format_secondary_text( dlg, "%s", str->str );
	g_string_free( str, TRUE );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/**
 * ofo_sgbd_query:
 */
gboolean
ofo_sgbd_query( const ofoSgbd *sgbd, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofo_sgbd_query";
	ofoSgbdPrivate *priv;
	gboolean query_ok;

	g_debug( "%s: sgbd=%p, query='%s', display_error=%s",
			thisfn, ( void * ) sgbd, query, display_error ? "True":"False" );

	g_return_val_if_fail( sgbd && OFO_IS_SGBD( sgbd ), FALSE );

	priv = sgbd->priv;
	query_ok = FALSE;

	g_return_val_if_fail( priv->module && OFA_IS_IDBMS( priv->module ), FALSE );
	g_return_val_if_fail( priv->handle, FALSE );

	if( !priv->dispose_has_run ){

		query_ok = ofa_idbms_query( priv->module, priv->handle, query );

		if( !query_ok ){
			if( display_error ){
				error_query( sgbd, query );
			}
		} else {
			sgbd_audit_query( sgbd, query );
		}
	}

	return( query_ok );
}

/**
 * ofo_sgbd_query_ex:
 * @sgbd: this #ofoSgbd object
 * @query: the query to be executed
 * @display_error: if NULL, do not display error message
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
ofo_sgbd_query_ex( const ofoSgbd *sgbd, const gchar *query, gboolean display_error )
{
	static const gchar *thisfn = "ofo_sgbd_query_ex";
	ofoSgbdPrivate *priv;
	GSList *result;

	g_debug( "%s: sgbd=%p, query='%s', display_error=%s",
			thisfn, ( void * ) sgbd, query, display_error ? "True":"False" );

	g_return_val_if_fail( sgbd && OFO_IS_SGBD( sgbd ), NULL );

	priv = sgbd->priv;
	result = NULL;

	g_return_val_if_fail( priv->module && OFA_IS_IDBMS( priv->module ), NULL );
	g_return_val_if_fail( priv->handle, NULL );

	if( !priv->dispose_has_run ){

		result = ofa_idbms_query_ex( priv->module, priv->handle, query );

		if( !result ){
			if( display_error ){
				error_query( sgbd, query );
			}
		}
	}

	return( result );
}

static void
error_query( const ofoSgbd *sgbd, const gchar *query )
{
	GtkMessageDialog *dlg;
	gchar *str;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", query ));

	str = ofa_idbms_error( sgbd->priv->module, sgbd->priv->handle );

	/* query_ex returns NULL if the result is empty: this is not an error */
	if( str && g_utf8_strlen( str, -1 )){
		gtk_message_dialog_format_secondary_text( dlg, "%s", str );
		gtk_dialog_run( GTK_DIALOG( dlg ));
	}

	g_free( str );
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

static void
sgbd_audit_query( const ofoSgbd *sgbd, const gchar *query )
{
	gchar *quoted;
	gchar *audit;

	quoted = quote_query( query );
	audit = g_strdup_printf( "INSERT INTO OFA_T_AUDIT (AUD_QUERY) VALUES ('%s')", quoted );
	ofa_idbms_query( sgbd->priv->module, sgbd->priv->handle, audit );

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

/**
 * ofo_sgbd_free_result:
 */
void
ofo_sgbd_free_result( GSList *result )
{
	g_slist_foreach( result, ( GFunc ) g_slist_free_full, g_free );
}

/**
 * ofo_sgbd_get_dbname:
 */
gchar *
ofo_sgbd_get_dbname( const ofoSgbd *sgbd )
{
	ofoSgbdPrivate *priv;

	g_return_val_if_fail( sgbd && OFO_IS_SGBD( sgbd ), NULL );

	priv = sgbd->priv;

	if( !priv->dispose_has_run &&
		priv->module &&
		OFA_IS_IDBMS( priv->module )){

		return( ofa_idbms_get_dossier_dbname( priv->module, priv->label ));
	}

	return( NULL );
}

/**
 * ofo_sgbd_backup:
 *
 * Backup the database behind the dossier.
 */
gboolean
ofo_sgbd_backup( const ofoSgbd *sgbd, const gchar *fname )
{
	gboolean ok;
	ofoSgbdPrivate *priv;

	g_return_val_if_fail( sgbd && OFO_IS_SGBD( sgbd ), FALSE );
	g_return_val_if_fail( fname && g_utf8_strlen( fname, -1 ), FALSE );

	ok = FALSE;
	priv = sgbd->priv;

	if( !priv->dispose_has_run ){

		ok = ofa_idbms_backup( priv->module, priv->handle, fname );
	}

	return( ok );
}
