/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IDBCONNECT_H__
#define __OPENBOOK_API_OFA_IDBCONNECT_H__

/**
 * SECTION: idbconnect
 * @title: ofaIDBConnect
 * @short_description: The DMBS Connection Interface
 * @include: openbook/ofa-idbconnect.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * This #ofaIDBConnect is the interface a connection object instanciated
 * by a DBMS backend should implement for the needs of the application.
 */

#include "ofa-idbeditor.h"
#include "ofa-idbprovider-def.h"
#include "ofa-idbmeta-def.h"
#include "ofa-idbperiod.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBCONNECT                      ( ofa_idbconnect_get_type())
#define OFA_IDBCONNECT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBCONNECT, ofaIDBConnect ))
#define OFA_IS_IDBCONNECT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBCONNECT ))
#define OFA_IDBCONNECT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBCONNECT, ofaIDBConnectInterface ))

typedef struct _ofaIDBConnect                    ofaIDBConnect;

/**
 * ofaIDBConnectInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @open_with_editor: [should]: open a connection with ofaIDBEditor informations.
 * @open_with_meta: [should]: open a connection with ofaIDBMeta informations.
 * @query: [should]: executes an insert/update/delete query.
 * @query_ex: [should]: executes a select query.
 * @get_last_error: [should]: returns the last error.
 * @backup: [should]: backups the currently opened dossier.
 * @restore: [should]: restore a file to a dossier.
 * @archive_and_new: [should]: archives the current and defines a new exercice.
 * @create_dossier: [should]: creates a new dossier.
 * @grant_user: [should]: grant permissions on a dossier to a user.
 *
 * This defines the interface that an #ofaIDBConnect should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * open_with_editor:
	 * @instance: this #ofaIDBConnect connection.
	 * @account: the user account.
	 * @password: [allow-none]: the user password.
	 * @editor: a #ofaIDBEditor object which handles connection
	 *  informations.
	 * @server_only: whether to use full connection informations.
	 *
	 * Returns: %TRUE if the connection has been successfully
	 * established, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *open_with_editor )     ( ofaIDBConnect *instance,
											const gchar *account,
											const gchar *password,
											const ofaIDBEditor *editor,
											gboolean server_only );

	/**
	 * open_with_meta:
	 * @instance: this #ofaIDBConnect connection.
	 * @account: the user account.
	 * @password: [allow-none]: the user password.
	 * @meta: the #ofaIDBMeta which identifies the dossier.
	 * @period: [allow-none]: the #ofaIDBPeriod which identifies the
	 *  exercice, or %NULL to establish a connection to the server
	 *  which holds the @meta dossier.
	 *
	 * Returns: %TRUE if the connection has been successfully
	 * established, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *open_with_meta )       ( ofaIDBConnect *instance,
											const gchar *account,
											const gchar *password,
											const ofaIDBMeta *meta,
											const ofaIDBPeriod *period );

	/**
	 * query:
	 * @instance: the #ofaIDBConnect user connection.
	 * @query: the SQL query to be executed.
	 *
	 * Execute a modification query (INSERT, UPDATE, DELETE, DROP,
	 * TRUNCATE) on the DBMS.
	 *
	 * Returns: %TRUE if the statement successfully executed,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *query )                ( const ofaIDBConnect *instance,
											const gchar *query );

	/**
	 * query_ex:
	 * @instance: the #ofaIDBConnect user connection.
	 * @query: the SQL query to be executed.
	 * @result: a GSList * which will hold the result set;
	 *  each item of the returned GSList is itself a GSList of rows,
	 *  each item of a GSList row being a field.
	 *
	 * Execute a SELECT query on the DBMS.
	 *
	 * Returns: %TRUE if the statement successfully executed,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *query_ex )             ( const ofaIDBConnect *instance,
											const gchar *query,
											GSList **result );

	/**
	 * get_last_error:
	 * @instance: the #ofaIDBConnect user connection.
	 *
	 * Returns: the last error message as a newly allocated string
	 * which should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_last_error )       ( const ofaIDBConnect *instance );

	/**
	 * backup:
	 * @instance: a #ofaIDBConnect user connection on the period.
	 * @uri: the target file.
	 *
	 * Backup the currently opened period to the @uri file.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *backup )               ( const ofaIDBConnect *instance,
											const gchar *uri );

	/**
	 * restore:
	 * @instance: a #ofaIDBConnect superuser connection on the DBMS
	 *  at server-level. The embedded #ofaIDBMeta object describes
	 *  the target dossier.
	 * @period: the target financial period.
	 * @uri: the file to be restored.
	 *
	 * Restore the specified @uri file to the target @period.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *restore )              ( const ofaIDBConnect *instance,
											const ofaIDBPeriod *period,
											const gchar *uri );

	/**
	 * archive_and_new:
	 * @instance: the #ofaIDBConnect user connection.
	 * @root_account: the root account of the DBMS server.
	 * @root_password: the corresponding password.
	 * @begin_next: the beginning date of the next exercice.
	 * @end_next: the ending date of the next exercice.
	 *
	 * Duplicate the current exercice into a new one.
	 *
	 * It is up to the DBMS provider to choose whether to archive the
	 * current exercice, and to create a new database for the new
	 * exercice, or to archive the current exercice into a new database,
	 * keeping the current database for the new exercice, provided that
	 * dossier settings be updated accordingly.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *archive_and_new )      ( const ofaIDBConnect *instance,
											const gchar *root_account,
											const gchar *root_password,
											const GDate *begin_next,
											const GDate *end_next );

	/**
	 * create_dossier:
	 * @instance: an #ofaIDBConnect superuser connection on the DBMS server.
	 * @meta: the #ofaIDBMeta object which describes the new dossier.
	 *
	 * Create and initialize a new minimal dossier database.
	 * It is expected that the DBMS provider drops its database and
	 * recreates it without any user confirmation.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *create_dossier )       ( const ofaIDBConnect *instance,
											const ofaIDBMeta *meta );

	/**
	 * grant_user:
	 * @instance: an #ofaIDBConnect superuser connection on the DBMS at
	 * 	server-level. The #ofaIDBMeta embedded object is expected to
	 * 	define the target dossier.
	 * @period: the target financial period.
	 * @user_account: the account to be granted.
	 * @user_password: the corresponding password.
	 *
	 * Grant the user for access to the dossier/exercice.
	 *
	 * The #ofaIDBConnect interface code takes care of defining the
	 * account as an administrator of the current exercice for the
	 * dossier.
	 *
	 * The DBMS provider should take advantage of this method to define
	 * and grant the account at the DBMS level.
	 *
	 * Returns: %TRUE if the account has been successfully defined,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *grant_user )           ( const ofaIDBConnect *instance,
											const ofaIDBPeriod *period,
											const gchar *user_account,
											const gchar *user_password );

	/**
	 * transaction_start:
	 * @instance: an #ofaIDBConnect user connection on the DBMS server.
	 *
	 * Start a transaction.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *transaction_start )    ( const ofaIDBConnect *instance );

	/**
	 * transaction_commit:
	 * @instance: an #ofaIDBConnect user connection on the DBMS server.
	 *
	 * Commit a transaction.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *transaction_commit )   ( const ofaIDBConnect *instance );

	/**
	 * transaction_cancel:
	 * @instance: an #ofaIDBConnect user connection on the DBMS server.
	 *
	 * Cancel a transaction.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *transaction_cancel )   ( const ofaIDBConnect *instance );
}
	ofaIDBConnectInterface;

/*
 * Interface-wide
 */
GType           ofa_idbconnect_get_type                  ( void );

guint           ofa_idbconnect_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint           ofa_idbconnect_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIDBProvider *ofa_idbconnect_get_provider              ( const ofaIDBConnect *connect );

void            ofa_idbconnect_set_provider              ( ofaIDBConnect *connect,
																const ofaIDBProvider *provider );

gboolean        ofa_idbconnect_open_with_editor          ( ofaIDBConnect *connect,
																const gchar *account,
																const gchar *password,
																const ofaIDBEditor *editor,
																gboolean server_only );

gboolean        ofa_idbconnect_open_with_meta            ( ofaIDBConnect *connect,
																const gchar *account,
																const gchar *password,
																const ofaIDBMeta *meta,
																const ofaIDBPeriod *period );

gchar          *ofa_idbconnect_get_account               ( const ofaIDBConnect *connect );

gchar          *ofa_idbconnect_get_password              ( const ofaIDBConnect *connect );

ofaIDBMeta     *ofa_idbconnect_get_meta                  ( const ofaIDBConnect *connect );

ofaIDBPeriod   *ofa_idbconnect_get_period                ( const ofaIDBConnect *connect );

gboolean        ofa_idbconnect_query                     ( const ofaIDBConnect *connect,
																const gchar *query,
																gboolean display_error );

gboolean        ofa_idbconnect_query_ex                  ( const ofaIDBConnect *connect,
																const gchar *query,
																GSList **result,
																gboolean display_error );

gboolean        ofa_idbconnect_query_int                 ( const ofaIDBConnect *connect,
																const gchar *query,
																gint *result,
																gboolean display_error );

gboolean        ofa_idbconnect_has_table                 ( const ofaIDBConnect *connect,
																const gchar *table );

gchar          *ofa_idbconnect_table_backup              ( const ofaIDBConnect *connect,
																const gchar *table );

gboolean        ofa_idbconnect_table_restore             ( const ofaIDBConnect *connect,
																const gchar *table_src,
																const gchar *table_dest );

#define         ofa_idbconnect_free_results( L )         g_debug( "ofa_idbconnect_free_results" ); \
																g_slist_foreach(( L ), ( GFunc ) g_slist_free_full, g_free ); \
																g_slist_free( L )

gchar          *ofa_idbconnect_get_last_error            ( const ofaIDBConnect *connect );

gboolean        ofa_idbconnect_backup                    ( const ofaIDBConnect *connect,
																const gchar *uri );

gboolean        ofa_idbconnect_restore                   ( const ofaIDBConnect *connect,
																const ofaIDBPeriod *period,
																const gchar *uri,
																const gchar *adm_account,
																const gchar *adm_password );

gboolean        ofa_idbconnect_archive_and_new           ( const ofaIDBConnect *connect,
																const gchar *root_account,
																const gchar *root_password,
																const GDate *begin_next,
																const GDate *end_next );

gboolean        ofa_idbconnect_create_dossier            ( const ofaIDBConnect *connect,
																const ofaIDBMeta *meta,
																const gchar *adm_account,
																const gchar *adm_password );

gboolean        ofa_idbconnect_transaction_start         ( const ofaIDBConnect *connect,
																gboolean display_error,
																gchar **msgerr );

gboolean        ofa_idbconnect_transaction_commit        ( const ofaIDBConnect *connect,
																gboolean display_error,
																gchar **msgerr );

gboolean        ofa_idbconnect_transaction_cancel        ( const ofaIDBConnect *connect,
																gboolean display_error,
																gchar **msgerr );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBCONNECT_H__ */
