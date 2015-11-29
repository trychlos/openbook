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

#ifndef __OPENBOOK_API_OFA_IDBCONNECT_H__
#define __OPENBOOK_API_OFA_IDBCONNECT_H__

/**
 * SECTION: idbconnect
 * @title: ofaIDBConnect
 * @short_description: The DMBS Connection Interface
 * @include: openbook/ofa-idbconnect.h
 *
 * The ofaIDB interfaces serie let the user choose and manage different
 * DBMS backends.
 * This #ofaIDBConnect is the interface a connection object instanciated
 * by a DBMS backend should implement for the needs of the application.
 */

#include "ofa-idbprovider-def.h"
#include "ofa-ifile-meta-def.h"
#include "ofa-ifile-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBCONNECT                      ( ofa_idbconnect_get_type())
#define OFA_IDBCONNECT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBCONNECT, ofaIDBConnect ))
#define OFA_IS_IDBCONNECT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBCONNECT ))
#define OFA_IDBCONNECT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBCONNECT, ofaIDBConnectInterface ))

typedef struct _ofaIDBConnect                    ofaIDBConnect;

/**
 * ofaIDBConnectInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @query: [should]: executes an insert/update/delete query.
 * @query_ex: [should]: executes a select query.
 * @query_int: [should]: returns an integer from a select query.
 * @get_last_error: [should]: returns the last error.
 * @archive_and_new: [should]: archives the current and defines a new exercice.
 *
 * This defines the interface that an #ofaIDBConnect should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBConnect instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBConnect interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIDBConnect *instance );

	/**
	 * query:
	 * @instance: the #ofaIDBConnect instance.
	 * @query: the SQL query to be executed.
	 *
	 * Execute a modification query (INSERT, UPDATE, DELETE, DROP,
	 * TRUNCATE) on the DBMS.
	 *
	 * Return value: %TRUE if the statement successfully executed,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *query )                ( const ofaIDBConnect *instance,
											const gchar *query );

	/**
	 * query_ex:
	 * @instance: the #ofaIDBConnect instance.
	 * @query: the SQL query to be executed.
	 * @result: a GSList * which will hold the result set;
	 *  each item of the returned GSList is itself a GSList of rows,
	 *  each item of a GSList row being a field.
	 *
	 * Execute a SELECT query on the DBMS.
	 *
	 * Return value: %TRUE if the statement successfully executed,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *query_ex )             ( const ofaIDBConnect *instance,
											const gchar *query,
											GSList **result );

	/**
	 * get_last_error:
	 * @instance: the #ofaIDBConnect instance.
	 *
	 * Returns: the last error message as a newly allocated string
	 * which should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_last_error )       ( const ofaIDBConnect *instance );

	/**
	 * archive_and_new:
	 * @instance: the #ofaIDBConnect connection.
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
	 * Return value: %TRUE if OK.
	 *
	 * Since: version 1
	 */
	gboolean ( *archive_and_new )      ( const ofaIDBConnect *instance,
											const gchar *root_account,
											const gchar *root_password,
											const GDate *begin_next,
											const GDate *end_next );
}
	ofaIDBConnectInterface;

GType           ofa_idbconnect_get_type                  ( void );

guint           ofa_idbconnect_get_interface_last_version( void );

guint           ofa_idbconnect_get_interface_version     ( const ofaIDBConnect *connect );

ofaIFileMeta   *ofa_idbconnect_get_meta                  ( const ofaIDBConnect *connect );

void            ofa_idbconnect_set_meta                  ( ofaIDBConnect *connect,
																const ofaIFileMeta *meta );

ofaIFilePeriod *ofa_idbconnect_get_period                ( const ofaIDBConnect *connect );

void            ofa_idbconnect_set_period                ( ofaIDBConnect *connect,
																const ofaIFilePeriod *period );

gchar          *ofa_idbconnect_get_account               ( const ofaIDBConnect *connect );

void            ofa_idbconnect_set_account               ( ofaIDBConnect *connect,
																const gchar *account );

gchar          *ofa_idbconnect_get_password              ( const ofaIDBConnect *connect );

void            ofa_idbconnect_set_password              ( ofaIDBConnect *connect,
																const gchar *password );

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

#define         ofa_idbconnect_free_results( L )         g_debug( "ofa_idbconnect_free_results" ); \
																g_slist_foreach(( L ), ( GFunc ) g_slist_free_full, g_free ); \
																g_slist_free( L )

gchar          *ofa_idbconnect_get_last_error            ( const ofaIDBConnect *connect );

gboolean        ofa_idbconnect_archive_and_new           ( const ofaIDBConnect *connect,
															const gchar *root_account,
															const gchar *root_password,
															const GDate *begin_next,
															const GDate *end_next );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBCONNECT_H__ */
