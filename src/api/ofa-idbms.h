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

#include "api/ofo-sgbd-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBMS                      ( ofa_idbms_get_type())
#define OFA_IDBMS( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBMS, ofaIDbms ))
#define OFA_IS_IDBMS( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBMS ))
#define OFA_IDBMS_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBMS, ofaIDbmsInterface ))

typedef struct _ofaIDbms                    ofaIDbms;

/**
 * ofaIDbmsInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaIDbms should implement.
 *
 * The DBMS backend presents two sets of functions:
 * - a first one which addresses the DB server itself,
 * - the second one which manages the inside dossier through the opened
 *   DB server connexion.
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
	guint         ( *get_interface_version )( const ofaIDbms *instance );

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
	const gchar * ( *get_provider_name )    ( const ofaIDbms *instance );

	/**
	 * properties_new_init:
	 * @instance: the #ofaIDbms provider.
	 * @parent: the #GtkContainer which handles the dialog part
	 * @group: a #GtkSizeGroup to which the DBMS UI grid may be attached
	 *
	 * Initialize the GtkDialog part which let the user enter the
	 * properties for a new connection definition.
	 *
	 * Since: version 1
	 */
	void          ( *properties_new_init )  ( const ofaIDbms *instance, GtkContainer *parent, GtkSizeGroup *group );

	/**
	 * properties_new_check:
	 * @instance: the #ofaIDbms provider.
	 * @parent: the #GtkContainer which handles the dialog part
	 *
	 * Check that the definition is enough to be validable.
	 *
	 * Returns: %TRUE if enough information has been entered.
	 *
	 * Since: version 1
	 */
	gboolean      ( *properties_new_check ) ( const ofaIDbms *instance, GtkContainer *parent );

	/**
	 * properties_new_apply:
	 * @instance: the #ofaIDbms provider.
	 * @parent: the #GtkContainer which handles the dialog part
	 * @label: the label of the new dossier
	 * @account: the administrative account for the dossier
	 * @password: the password of the administrative account
	 *
	 * Try to apply for a new dossier definition.
	 *
	 * Returns: %TRUE is a definition is successful, thus letting the
	 * dialog box to be closed, %FALSE else. In this later case, the DBMS
	 * provider should make its best to clean up the database.
	 *
	 * Since: version 1
	 */
	gboolean      ( *properties_new_apply ) ( const ofaIDbms *instance, GtkContainer *parent, const gchar *label, const gchar *account, const gchar *password );

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
	 * connect:
	 * @instance: the #ofaIDbms provider.
	 * @label: the label of the dossier.
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
	void        * ( *connect )              ( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password );

	/**
	 * connect_ex:
	 * @instance: the #ofaIDbms provider.
	 * @label: the label of the dossier.
	 * @dbname: the database to be used as the default.
	 * @account: the connection account.
	 * @password: the password of the connection account.
	 *
	 * Connect to the DBMS, using the specified database as the default.
	 *
	 * Return value: a non-NULL handle on the connection data provided
	 * by the DBMS provider, or %NULL.
	 *
	 * Since: version 1
	 */
	void        * ( *connect_ex )           ( const ofaIDbms *instance, const gchar *label, const gchar *dbname, const gchar *account, const gchar *password );

	/**
	 * close:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 *
	 * Close the connection to the DBMS.
	 *
	 * Since: version 1
	 */
	void          ( *close )                ( const ofaIDbms *instance, void *handle );

	/**
	 * query:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @query: the SQL query to be executed.
	 *
	 * Execute a modification query (INSERT, UPDATE, DELETE, DROP,
	 * TRUNCATE)on the DBMS.
	 *
	 * Since: version 1
	 */
	gboolean      ( *query )                ( const ofaIDbms *instance, void *handle, const gchar *query );

	/**
	 * query_ex:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @query: the SQL query to be executed.
	 *
	 * Execute a SELECT query on the DBMS.
	 *
	 * Returns: a list of rows, or %NULL if an error has occured.
	 *
	 * The returned list is free with ofo_sgbd_free_result().
	 *
	 * Since: version 1
	 */
	GSList      * ( *query_ex )             ( const ofaIDbms *instance, void *handle, const gchar *query );

	/**
	 * error:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 *
	 * Returns: the last error message as a newly allocated string
	 * which should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar       * ( *error )                ( const ofaIDbms *instance, void *handle );

	/**
	 * delete_dossier:
	 * @instance: the #ofaIDbms provider.
	 * @label: the label of the dossier.
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
	gboolean      ( *delete_dossier )       ( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password, gboolean drop_db, gboolean drop_accounts );

	/**
	 * backup:
	 * @instance: the #ofaIDbms provider.
	 * @handle: the handle returned by the connection.
	 * @fname: the destination filename
	 *
	 * Backup the currently opened dossier.
	 *
	 * Return value: %TRUE if the dossier has been successfully saved.
	 *
	 * Since: version 1
	 */
	gboolean      ( *backup )               ( const ofaIDbms *instance, void *handle, const gchar *fname );
}
	ofaIDbmsInterface;

/**
 * ofnDBMode:
 *
 * What to do when the database already exists while defining a new
 * dossier.
 */
typedef enum {
	DBMODE_EMPTY = 0,
	DBMODE_REINIT,
	DBMODE_LEAVE_AS_IS
}
	ofnDBMode;

GType        ofa_idbms_get_type             ( void );

GSList      *ofa_idbms_get_providers_list   ( void );

void         ofa_idbms_free_providers_list  ( GSList *list );

ofaIDbms    *ofa_idbms_get_provider_by_name ( const gchar *name );

const gchar *ofa_idbms_get_provider_name    ( const ofaIDbms *instance );

void         ofa_idbms_properties_new_init  ( const ofaIDbms *instance, GtkContainer *parent,
												GtkSizeGroup *group );

gboolean     ofa_idbms_properties_new_check ( const ofaIDbms *instance, GtkContainer *parent );

gboolean     ofa_idbms_properties_new_apply ( const ofaIDbms *instance, GtkContainer *parent,
												const gchar *label, const gchar *account, const gchar *password );

gchar       *ofa_idbms_get_dossier_host     ( const ofaIDbms *instance, const gchar *label );

gchar       *ofa_idbms_get_dossier_dbname   ( const ofaIDbms *instance, const gchar *label );

void        *ofa_idbms_connect              ( const ofaIDbms *instance, const gchar *label,
												const gchar *account, const gchar *password );

void        *ofa_idbms_connect_ex           ( const ofaIDbms *instance, const gchar *label,
												const gchar *dbname, const gchar *account, const gchar *password );

void         ofa_idbms_close                ( const ofaIDbms *instance, void *handle );

gboolean     ofa_idbms_query                ( const ofaIDbms *instance, void *handle, const gchar *query );

GSList      *ofa_idbms_query_ex             ( const ofaIDbms *instance, void *handle, const gchar *query );

gchar       *ofa_idbms_error                ( const ofaIDbms *instance, void *handle );

gboolean     ofa_idbms_delete_dossier       ( const ofaIDbms *instance, const gchar *label, const gchar *account, const gchar *password,
												gboolean drop_db, gboolean drop_accounts, gboolean with_confirm );

gboolean     ofa_idbms_backup               ( const ofaIDbms *instance, void *handle, const gchar *fname );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBMS_H__ */
