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

#ifndef __OPENBOOK_API_OFA_IDBMS_H__
#define __OPENBOOK_API_OFA_IDBMS_H__

/**
 * SECTION: idbms
 * @title: ofaIDbms
 * @short_description: The DMBS Interface
 * @include: openbook/ofa-idbms.h
 *
 * The #ofaIDbms interface let the user choose and manage different
 * DBMS backends.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_IDBMS                      ( ofa_idbms_get_type())
#define OFA_IDBMS( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBMS, ofaIDbms ))
#define OFA_IS_IDBMS( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBMS ))
#define OFA_IDBMS_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBMS, ofaIDbmsInterface ))

typedef struct _ofaIDbms                    ofaIDbms;

/**
 * ofaIDbmsInterface:
 *
 * This defines the interface that an #ofaIDbms should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDbms provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDbms interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint         ( *get_interface_version )     ( const ofaIDbms *instance );

	/**
	 * connect:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the dossier in settings.
	 * @dbname: the database to be used as the default.
	 * @account: the connection account.
	 * @password: the password of the connection account.
	 *
	 * Connect to the DBMS.
	 *
	 * Return value: a non-NULL handle on the connection data provided
	 * by the DBMS provider, or %NULL.
	 *
	 * Since: version 1
	 */
	void        * ( *connect )                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *dbname,
															const gchar *account,
															const gchar *password );

	/**
	 * connect_ex:
	 * @instance: the #ofaIDbms provider.
	 * @infos: the connection informations as sent by the "changed"
	 *  signal in the #ofa_idbms_connect_enter_attach_to() function.
	 * @account: the DBMS root credentials.
	 * @password: the corresponding password.
	 *
	 * Check the DBMS connection.
	 *
	 * Return value: %TRUE if the credentials are valid on these
	 * connection informations.
	 *
	 * Since: version 1
	 */
	gboolean      ( *connect_ex )                ( const ofaIDbms *instance,
															void *infos,
															const gchar *account,
															const gchar *password );

	/**
	 * close:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 *
	 * Close the connection to the DBMS.
	 *
	 * Since: version 1
	 */
	void          ( *close )                     ( const ofaIDbms *instance,
															void *handle );

	/**
	 * get_provider_name:
	 * @instance: the #ofaIDbms provider.
	 *
	 * Returns the name of this DBMS provider.
	 *
	 * This name acts as an identifier for the DBMS provider, and is
	 * not localized. It is recorded in the user configuration file as
	 * an access key to the dossier external properties.
	 *
	 * Return value: the name of this DBMS provider.
	 *
	 * Since: version 1
	 */
	const gchar * ( *get_provider_name )         ( const ofaIDbms *instance );

	/**
	 * query:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @query: the SQL query to be executed.
	 *
	 * Execute a modification query (INSERT, UPDATE, DELETE, DROP,
	 * TRUNCATE)on the DBMS.
	 *
	 * Returns: %TRUE if the statement successfully executed, %FALSE
	 * else.
	 *
	 * Since: version 1
	 */
	gboolean      ( *query )                     ( const ofaIDbms *instance,
															void *handle, const gchar *query );

	/**
	 * query_ex:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @query: the SQL query to be executed.
	 * @result: a GSList * which will hold the result set;
	 *  each item of the returned GSList is itself a GSList of rows,
	 *  each item of a GSList row being a field.
	 *
	 * Execute a SELECT query on the DBMS.
	 *
	 * Returns: %TRUE if the statement successfully executed, %FALSE
	 * else.
	 *
	 * The returned list is free with ofo_sgbd_free_result().
	 *
	 * Since: version 1
	 */
	gboolean      ( *query_ex )                  ( const ofaIDbms *instance,
															void *handle, const gchar *query,
															GSList **result );

	/**
	 * last_error:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 *
	 * Returns: the last error message as a newly allocated string
	 * which should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar       * ( *last_error )                ( const ofaIDbms *instance,
															void *handle );

	/**
	 * connect_display_new:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the dname of the dossier.
	 *
	 * The DBMS provider is asked to return a widget to display the
	 * connection informations for the specified @dname dossier.
	 *
	 * The returned widget should be left undecorated, in order to
	 * give the client application full control on the global visual.
	 *
	 * Since: version 1
	 */
	GtkWidget   * ( *connect_display_new )       ( const ofaIDbms *instance,
															const gchar *dname );

	/**
	 * connect_enter_new:
	 * @instance: the #ofaIDbms provider.
	 * @group: [allow-none]: a #GtkSizegroup.
	 *
	 * The DBMS provider should returns a piece of dialog as a #GtkWidget.
	 * In following operations, this same #GtkWidget will be passed in,
	 * so the DBMS provider may set some data against it.
	 *
	 * The DBMS provider should send a "dbms-changed" signal when something
	 * is updated in the displayed piece of dialog, joining to the signal
	 * a pointer to connection informations.
	 *
	 * Since: version 1
	 */
	GtkWidget   * ( *connect_enter_new )         ( ofaIDbms *instance,
															GtkSizeGroup *group );

	/**
	 * connect_enter_is_valid:
	 * @instance: the #ofaIDbms provider.
	 * @piece: the #GtkContainer to which the dialog piece is attached.
	 * @message: [allow-none]: a message to be set.
	 *
	 * Returns: %TRUE if the entered connection informations are valid.
	 *
	 * Note that we only do here an intrinsic check as we do not have
	 * any credentials.
	 *
	 * Since: version 1
	 */
	gboolean      ( *connect_enter_is_valid )    ( const ofaIDbms *instance,
															GtkWidget *piece,
															gchar **message );

	/**
	 * connect_enter_get_database:
	 * @instance: the #ofaIDbms provider.
	 * @piece: the #GtkContainer to which the dialog piece is attached.
	 *
	 * Returns: the database name as a newly allocated string which
	 * should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *       ( *connect_enter_get_database )( const ofaIDbms *instance,
															GtkWidget *piece );

	/**
	 * connect_enter_apply:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the dossier.
	 * @infos: the connection informations as sent by the "dbms-changed"
	 *  signal in the #ofa_idbms_connect_enter_new() function.
	 *
	 * Record the newly defined dossier in settings.
	 *
	 * Since: version 1
	 */
	gboolean      ( *connect_enter_apply )       ( const ofaIDbms *instance,
															const gchar *dname,
															void *infos );

	/**
	 * new_dossier:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the new dossier
	 * @account: the administrative account for the dossier
	 * @password: the password of the administrative account
	 *
	 * Create and initialize a new dossier database.
	 * the database is dropped and recreated without any user
	 * confirmation.
	 *
	 * Returns: %TRUE is a definition is successful.
	 *
	 * Since: version 1
	 */
	gboolean      ( *new_dossier )               ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password );

	/**
	 * grant_user:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the dossier to be restored.
	 * @root_account: the root account of the DBMS server.
	 * @root_password: the corresponding password.
	 * @user_account: the account to be granted.
	 * @user_password: the corresponding password.
	 *
	 * Grant the user for access to the dossier.
	 *
	 * The #ofaIDbms interface code takes care of defining the account
	 * as an administrator of the current exercice for the dossier.
	 *
	 * The DBMS provider should take advantage of this method to define
	 * and grant the account at the DBMS level.
	 *
	 * Return value: %TRUE if the account has been successfully defined.
	 *
	 * Since: version 1
	 */
	gboolean      ( *grant_user )                ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password,
															const gchar *user_account,
															const gchar *user_password );

	/**
	 * backup:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @fname: the destination filename
	 * @verbose: run verbosely
	 *
	 * Backup the currently opened dossier.
	 *
	 * Return value: %TRUE if the dossier has been successfully saved.
	 *
	 * Since: version 1
	 */
	gboolean      ( *backup )                    ( const ofaIDbms *instance,
															void *handle,
															const gchar *fname,
															gboolean verbose );

	/**
	 * restore:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the dossier to be restored.
	 * @fname: the input filename.
	 * @root_account: the root account of the DBMS server.
	 * @root_password:
	 *
	 * Restore the given backup file to the named dossier.
	 *
	 * The destination dossier is supposed to be defined in the user's
	 * settings, and closed.
	 *
	 * The DBMS provider doesn't take any caution before restoring the
	 * database. This is up to the application to ask for a user
	 * confirmation, and to close the dossier before restoring the
	 * database.
	 *
	 * Return value: %TRUE if the dossier has been successfully restored.
	 *
	 * Since: version 1
	 */
	gboolean      ( *restore )                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *fname,
															const gchar *root_account,
															const gchar *root_password );

	/**
	 * archive:
	 * @instance: the #ofaIDbms provider.
	 * @dname: the name of the dossier to be archived.
	 * @root_account: the root account of the DBMS server.
	 * @root_password: the corresponding password.
	 * @user_account: the account whose privileges are to be duplicated.
	 * @begin_next: the beginning date of the next exercice.
	 * @end_next: the ending date of the next exercice.
	 *
	 * Archive the current exercice.
	 *
	 * It is up to the DBMS provider to choose whether to archive the
	 * current exercice, and to create a new database for the new
	 * exercice, or to archive the current exercice into a new database,
	 * keeping the current database for the new exercice, provided that
	 * user setting be updated accordingly.
	 *
	 * Return value: %TRUE if OK.
	 *
	 * Since: version 1
	 */
	gboolean      ( *archive )                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password,
															const gchar *user_account,
															const GDate *begin_next,
															const GDate *end_next );

	/* ... */

	/**
	 * get_dossier_host:
	 * @instance: the #ofaIDbms provider.
	 * @label: the name of the dossier.
	 *
	 * Returns the host name of the DBMS server.
	 *
	 * The hostname may be %NULL or empty if the DBMS host is localhost.
	 *
	 * Return value: a hostname as a newly allocated string which should
	 * be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *       ( *get_dossier_host )     ( const ofaIDbms *instance, const gchar *label );

	/**
	 * get_dossier_dbname:
	 * @instance: the #ofaIDbms provider.
	 * @label: the name of the dossier.
	 *
	 * Returns the database name of the dossier.
	 *
	 * Return value: a database name as a newly allocated string which
	 * should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *       ( *get_dossier_dbname )   ( const ofaIDbms *instance, const gchar *label );

	/**
	 * get_dbnames_list:
	 * @instance: the #ofaIDbms provider.
	 * @name: the name of the dossier.
	 *
	 * Returns the list of database names (one for each known exercice)
	 * recorded for the dossier is the settings.
	 *
	 * Return value: a #GList of semi-colon separated strings which are
	 * built as:
	 * - a displayable label (e.g. "Archived exercice from 01/01/1980 to 31/12/1980")
	 * - the corresponding database name
	 *
	 * The caller should #g_slist_free_full( list, ( GDestroyNotify ) g_free )
	 * the returned list.
	 *
	 * Since: version 1
	 */
	GSList *      ( *get_dbnames_list )     ( const ofaIDbms *instance,
														const gchar *name );

	/**
	 * delete_dossier:
	 * @instance: the #ofaIDbms provider.
	 * @name: the name of the dossier.
	 * @account: the connection account.
	 * @password: the password of the connection account.
	 * @drop_db: whether to drop the database
	 * @drop_accounts: whether to drop the accounts
	 *
	 * Delete the named dossier.
	 *
	 * The interface takes itself care of asking for user confirmation
	 * (if required), and, at last, deleting the dossier from the
	 * user configuration file. The DBMS provider is responsible for
	 * dropping the database and the accounts.
	 *
	 * Return value: %TRUE if the deletion has been confirmed (if
	 * required), and at least the connection was successful, %FALSE
	 * else.
	 *
	 * Since: version 1
	 */
	gboolean      ( *delete_dossier )       ( const ofaIDbms *instance, const gchar *name, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts );
}
	ofaIDbmsInterface;

/**
 * ofnDBMode:
 *
 * What to do when the database already exists while defining a new
 * dossier.
 */
typedef enum {
	DBMODE_REINIT = 1,
	DBMODE_LEAVE_AS_IS
}
	ofnDBMode;

/**
 * ofnDBMode:
 *
 * What to do about the database when deleting a dossier.
 */
typedef enum {
	DBMODE_DROP = 1,
	DBMODE_KEEP
}
	ofnDBDeleteMode;

GType        ofa_idbms_get_type                  ( void );

void        *ofa_idbms_connect                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *dbname,
															const gchar *account,
															const gchar *password );

gboolean     ofa_idbms_connect_ex                ( const ofaIDbms *instance,
															void *infos,
															const gchar *account,
															const gchar *password );

void         ofa_idbms_close                     ( const ofaIDbms *instance,
															void *handle );

ofaIDbms    *ofa_idbms_get_provider_by_name      ( const gchar *pname );

const gchar *ofa_idbms_get_provider_name         ( const ofaIDbms *instance );

GSList      *ofa_idbms_get_providers_list        ( void );

void         ofa_idbms_free_providers_list       ( GSList *list );

gboolean     ofa_idbms_query                     ( const ofaIDbms *instance,
															void *handle,
															const gchar *query );

gboolean     ofa_idbms_query_ex                  ( const ofaIDbms *instance,
															void *handle,
															const gchar *query,
															GSList **result );

gchar       *ofa_idbms_last_error                ( const ofaIDbms *instance,
															void *handle );

GtkWidget   *ofa_idbms_connect_display_new       ( const gchar *dname );

GtkWidget   *ofa_idbms_connect_enter_new         ( ofaIDbms *instance,
															GtkSizeGroup *group );

gboolean     ofa_idbms_connect_enter_is_valid    ( const ofaIDbms *instance,
															GtkWidget *piece,
															gchar **message );

gchar       *ofa_idbms_connect_enter_get_database( const ofaIDbms *instance,
															GtkWidget *piece );

gboolean     ofa_idbms_connect_enter_apply       ( const ofaIDbms *instance,
															const gchar *dname,
															void *infos );

gboolean     ofa_idbms_new_dossier               ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password );

gboolean     ofa_idbms_set_admin_credentials     ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password,
															const gchar *adm_account,
															const gchar *adm_password );

gboolean     ofa_idbms_backup                    ( const ofaIDbms *instance,
															void *handle,
															const gchar *fname,
															gboolean verbose );

gboolean     ofa_idbms_restore                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *fname,
															const gchar *root_account,
															const gchar *root_password );

gboolean     ofa_idbms_archive                   ( const ofaIDbms *instance,
															const gchar *dname,
															const gchar *root_account,
															const gchar *root_password,
															const gchar *user_account,
															const GDate *begin_next,
															const GDate *end_next );

#if 0
gchar       *ofa_idbms_get_dossier_host     ( const ofaIDbms *instance, const gchar *label );

gchar       *ofa_idbms_get_dossier_dbname   ( const ofaIDbms *instance, const gchar *label );
#endif

gboolean     ofa_idbms_delete_dossier       ( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password,
												gboolean drop_db, gboolean drop_accounts, gboolean with_confirm );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBMS_H__ */
