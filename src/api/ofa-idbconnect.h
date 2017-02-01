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

#include <gtk/gtk.h>

#include "ofa-hub-def.h"
#include "ofa-idbconnect-def.h"
#include "ofa-idbdossier-meta-def.h"
#include "ofa-idbdossier-editor-def.h"
#include "ofa-idbexercice-meta-def.h"
#include "ofa-idbprovider-def.h"
#include "ofa-idbsuperuser-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBCONNECT                      ( ofa_idbconnect_get_type())
#define OFA_IDBCONNECT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBCONNECT, ofaIDBConnect ))
#define OFA_IS_IDBCONNECT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBCONNECT ))
#define OFA_IDBCONNECT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBCONNECT, ofaIDBConnectInterface ))

#if 0
typedef struct _ofaIDBConnect                    ofaIDBConnect;
typedef struct _ofaIDBConnectInterface           ofaIDBConnectInterface;
#endif

/**
 * ofaIDBConnectInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @open_with_account: [should]: open a connection with a user account.
 * @open_with_superuser: [should]: open a connection with super-user credentials.
 * @is_opened: [should]: tests if a connection is opened.
 * @get_display: [should]: returns a widget which displays connection infos.
 * @query: [should]: executes an insert/update/delete query.
 * @query_ex: [should]: executes a select query.
 * @get_last_error: [should]: returns the last error.
 * @backup_db: [should]: backups the currently opened dossier.
 * @restore_db: [should]: restore a file to a dossier.
 * @archive_and_new: [should]: archives the current and defines a new exercice.
 * @new_period: [should]: creates a new financial period.
 * @grant_user: [should]: grant permissions on a dossier to a user.
 * @transaction_start: [should]: start a transaction.
 * @transaction_cancel: [should]: cancel a transaction.
 * @transaction_commit: [should]: commit a transaction.
 *
 * This defines the interface that an #ofaIDBConnect should implement.
 */
struct _ofaIDBConnectInterface {
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
	guint       ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * open_with_account:
	 * @instance: this #ofaIDBConnect connection.
	 * @account: the user account.
	 * @password: the user password.
	 *
	 * Returns: %TRUE if the connection has been successfully
	 * established, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *open_with_account )    ( ofaIDBConnect *instance,
												const gchar *account,
												const gchar *password );

	/**
	 * open_with_superuser:
	 * @instance: this #ofaIDBConnect connection.
	 * @su: a #ofaIDBSuperuser object.
	 *
	 * Returns: %TRUE if the connection has been successfully
	 * established, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *open_with_superuser )  ( ofaIDBConnect *instance,
												ofaIDBSuperuser *su );

	/**
	 * set_exercice_meta:
	 * @instance: this #ofaIDBConnect connection.
	 * @meta: an #ofaIDBExerciceMeta object.
	 *
	 * Set the exercice meta.
	 *
	 * Since: version 1
	 */
	void        ( *set_exercice_meta )    ( ofaIDBConnect *instance,
												ofaIDBExerciceMeta *meta );

	/**
	 * is_opened:
	 * @instance: this #ofaIDBConnect connection.
	 *
	 * Returns: %TRUE if the connection is opened, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *is_opened )            ( const ofaIDBConnect *instance );

	/**
	 * get_display:
	 * @instance: the #ofaIDBConnect user connection.
	 * @style: [allow-none]: the display style name.
	 *
	 * Returns: a widget which displays connection informations.
	 *
	 * The returned widget may implement the #myISizegroup interface.
	 *
	 * Since: version 1
	 */
	GtkWidget * ( *get_display )          ( ofaIDBConnect *instance,
												const gchar *style );

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
	gboolean    ( *query )                ( const ofaIDBConnect *instance,
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
	gboolean    ( *query_ex )             ( const ofaIDBConnect *instance,
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
	gchar *     ( *get_last_error )       ( const ofaIDBConnect *instance );

	/**
	 * backup_db:
	 * @instance: a #ofaIDBConnect user connection on the period.
	 * @uri: the target uri.
	 * @msg_cb: [allow-none]: a #ofaMsgCb callback function;
	 *  the data passed to the callback is expected to be a null-
	 *  terminated string;
	 *  may be %NULL if the caller does not wish this sort of display.
	 * @data_cb: a #ofaDataMsg callback function;
	 *  the callee is expected to provide its datas to this callback.
	 * @user_data: will be passed to both @msg_cb and @data_cb.
	 *
	 * Backup the currently opened period.
	 *
	 * The method is expected to return only when the backup is finished.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *backup_db )            ( const ofaIDBConnect *instance,
												const gchar *uri,
												ofaMsgCb msg_cb,
												ofaDataCb data_cb,
												void *user_data );

	/**
	 * restore_db:
	 * @instance: a #ofaIDBConnect superuser connection on the DBMS
	 *  at server-level. The embedded #ofaIDBDossierMeta object describes
	 *  the target dossier.
	 * @period: the target financial period.
	 * @uri: the source URI.
	 * @format: the format of the archive (from #ofaBackupHeader header).
	 * @msg_cb: [allow-none]: a #ofaMsgCb callback function;
	 *  the data passed to the callback is expected to be a null-
	 *  terminated string;
	 *  may be %NULL if the caller does not wish this sort of display.
	 * @data_cb: a #ofaDataMsg callback function;
	 *  the callee is expected to get its datas from this callback;
	 *  its has to provide its own buffer.
	 * @user_data: will be passed to both @msg_cb and @data_cb.
	 *
	 * Restore the specified @uri file to the target @period.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *restore_db )           ( const ofaIDBConnect *instance,
												const ofaIDBExerciceMeta *period,
												const gchar *uri,
												guint format,
												ofaMsgCb msg_cb,
												ofaDataCb data_cb,
												void *user_data );

	/**
	 * archive_and_new:
	 * @instance: the #ofaIDBConnect user connection.
	 * @su: the super-user credentials.
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
	gboolean    ( *archive_and_new )      ( const ofaIDBConnect *instance,
												ofaIDBSuperuser *su,
												const GDate *begin_next,
												const GDate *end_next );

	/**
	 * new_period:
	 * @instance: an #ofaIDBConnect superuser connection on the DBMS server.
	 * @period: the #ofaIDBExerciceMeta to be created.
	 * @msgerr: [out][allow-none]: a placeholder for an error message.
	 *
	 * Create and initialize a new minimal dossier database.
	 * It is expected that the DBMS provider drops its database and
	 * recreates it without any user confirmation.
	 *
	 * Returns: %TRUE if successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean    ( *new_period )           ( ofaIDBConnect *instance,
												ofaIDBExerciceMeta *period,
												gchar **msgerr );

	/**
	 * grant_user:
	 * @instance: an #ofaIDBConnect superuser connection on the DBMS at
	 * 	server-level. The #ofaIDBDossierMeta embedded object is expected to
	 * 	define the target dossier.
	 * @period: the target financial period.
	 * @user_account: the account to be granted.
	 * @user_password: the corresponding password.
	 * @msgerr: [out][allow-none]: a placeholder for an error message.
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
	gboolean    ( *grant_user )           ( const ofaIDBConnect *instance,
												const ofaIDBExerciceMeta *period,
												const gchar *user_account,
												const gchar *user_password,
												gchar **msgerr );

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
	gboolean    ( *transaction_start )    ( const ofaIDBConnect *instance );

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
	gboolean    ( *transaction_cancel )   ( const ofaIDBConnect *instance );

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
	gboolean    ( *transaction_commit )   ( const ofaIDBConnect *instance );
};

/*
 * Interface-wide
 */
GType               ofa_idbconnect_get_type                  ( void );

guint               ofa_idbconnect_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint               ofa_idbconnect_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
const gchar        *ofa_idbconnect_get_account              ( const ofaIDBConnect *connect );

const gchar        *ofa_idbconnect_get_password             ( const ofaIDBConnect *connect );

void                ofa_idbconnect_set_account              ( ofaIDBConnect *connect,
																	const gchar *account,
																	const gchar *password );

ofaIDBDossierMeta  *ofa_idbconnect_get_dossier_meta         ( const ofaIDBConnect *connect );

void                ofa_idbconnect_set_dossier_meta         ( ofaIDBConnect *connect,
																	ofaIDBDossierMeta *dossier_meta );

ofaIDBExerciceMeta *ofa_idbconnect_get_exercice_meta        ( const ofaIDBConnect *connect );

void                ofa_idbconnect_set_exercice_meta        ( ofaIDBConnect *connect,
																	ofaIDBExerciceMeta *exercice_meta );

gboolean            ofa_idbconnect_open_with_account        ( ofaIDBConnect *connect,
																	const gchar *account,
																	const gchar *password );

gboolean            ofa_idbconnect_open_with_superuser      ( ofaIDBConnect *connect,
																	ofaIDBSuperuser *su );

gboolean            ofa_idbconnect_is_opened                ( const ofaIDBConnect *connect );

GtkWidget          *ofa_idbconnect_get_display              ( ofaIDBConnect *connect,
																	const gchar *style );

gboolean            ofa_idbconnect_query                    ( const ofaIDBConnect *connect,
																	const gchar *query,
																	gboolean display_error );

gboolean            ofa_idbconnect_query_ex                 ( const ofaIDBConnect *connect,
																	const gchar *query,
																	GSList **result,
																	gboolean display_error );

gboolean            ofa_idbconnect_query_int                ( const ofaIDBConnect *connect,
																	const gchar *query,
																	gint *result,
																	gboolean display_error );

gboolean            ofa_idbconnect_has_table                ( const ofaIDBConnect *connect,
																	const gchar *table );

gchar              *ofa_idbconnect_table_backup             ( const ofaIDBConnect *connect,
																	const gchar *table );

gboolean            ofa_idbconnect_table_restore            ( const ofaIDBConnect *connect,
																	const gchar *table_src,
																	const gchar *table_dest );

#define             ofa_idbconnect_free_results( L )        g_debug( "ofa_idbconnect_free_results" ); \
																	g_slist_foreach(( L ),( GFunc ) g_slist_free_full, g_free ); \
																	g_slist_free( L )

gchar              *ofa_idbconnect_get_last_error           ( const ofaIDBConnect *connect );

gboolean            ofa_idbconnect_backup_db                ( const ofaIDBConnect *connect,
																	const gchar *comment,
																	const gchar *uri,
																	ofaMsgCb msg_cb,
																	void *user_data );

gboolean            ofa_idbconnect_restore_db               ( const ofaIDBConnect *connect,
																	const ofaIDBExerciceMeta *period,
																	const gchar *uri,
																	guint format,
																	const gchar *adm_account,
																	const gchar *adm_password,
																	ofaMsgCb msg_cb,
																	void *user_data );

gboolean            ofa_idbconnect_archive_and_new          ( const ofaIDBConnect *connect,
																	ofaIDBSuperuser *su,
																	const GDate *begin_next,
																	const GDate *end_next );

gboolean            ofa_idbconnect_new_period               ( ofaIDBConnect *connect,
																	ofaIDBExerciceMeta *period,
																	const gchar *adm_account,
																	const gchar *adm_password,
																	gchar **msgerr );

gboolean            ofa_idbconnect_transaction_start        ( const ofaIDBConnect *connect,
																	gboolean display_error,
																	gchar **msgerr );

gboolean            ofa_idbconnect_transaction_cancel       ( const ofaIDBConnect *connect,
																	gboolean display_error,
																	gchar **msgerr );

gboolean            ofa_idbconnect_transaction_commit       ( const ofaIDBConnect *connect,
																	gboolean display_error,
																	gchar **msgerr );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBCONNECT_H__ */
