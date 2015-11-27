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

#ifndef __OPENBOOK_API_OFA_IDBPROVIDER_H__
#define __OPENBOOK_API_OFA_IDBPROVIDER_H__

/**
 * SECTION: idbprovider
 * @title: ofaIDBProvider
 * @short_description: The DMBS Provider Interface
 * @include: openbook/ofa-idbprovider.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * This #ofaIDBProvider is dedicated to meta data management.
 *
 * This is an Openbook software suite choice to store most of these
 * meta data in a dossier settings file.
 */

#include "my-settings.h"
#include "ofa-idbconnect.h"
#include "ofa-ifile-meta-def.h"
#include "ofa-ifile-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBPROVIDER                      ( ofa_idbprovider_get_type())
#define OFA_IDBPROVIDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBPROVIDER, ofaIDBProvider ))
#define OFA_IS_IDBPROVIDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBPROVIDER ))
#define OFA_IDBPROVIDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBPROVIDER, ofaIDBProviderInterface ))

/**
 * ofaIDBProviderInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_provider_name: [must]: returns the identifier name of the DBMS provider.
 * @get_dossier_meta: [should]: returns the dossier meta datas.
 * @get_dossier_periods: [should]: returns the defined periods of the dossier.
 * @connect_dossier: [should]: connect to the specified dossier.
 *
 * This defines the interface that an #ofaIDBProvider should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBProvider provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBProvider interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint           ( *get_interface_version )( const ofaIDBProvider *instance );

	/**
	 * get_provider_name:
	 * @instance: the #ofaIDBProvider provider.
	 *
	 * Returns the name of this DBMS provider.
	 *
	 * This name acts as an identifier for the DBMS provider, and is
	 * not localized. It is recorded in the user configuration file as
	 * an access key to the dossier external properties.
	 *
	 * Return value: the name of this DBMS provider.
	 *
	 * The returned value is owned by the DBMS provider, and should not
	 * be released by the caller.
	 *
	 * Since: version 1
	 */
	const gchar *   ( *get_provider_name )    ( const ofaIDBProvider *instance );

	/**
	 * get_dossier_meta:
	 * @instance: the #ofaIDBProvider provider.
	 * @dssier_name: the name of the dossier.
	 * @settings: the #mySettings instance.
	 * @group: the group name in the settings.
	 *
	 * Return value: an #ofaIFileMeta object which holds dossier name
	 * and other external properties. The returned reference should be
	 * g_object_unref() by the caller.
	 *
	 * Since: version 1
	 */
	ofaIFileMeta *  ( *get_dossier_meta )     ( const ofaIDBProvider *instance,
														const gchar *dossier_name,
														mySettings *settings,
														const gchar *group );

	/**
	 * connect_dossier:
	 * @instance: the #ofaIDBProvider provider.
	 * @meta: the #ofaIFileMeta instance which manages the dossier.
	 * @period: the #ofaIFilePeriod which identifies the exercice.
	 * @account: the user account.
	 * @password: the user password.
	 * @msg: an error message placeholder.
	 *
	 * Return value: an object which implements the #ofaIDBConnect
	 * interface, and handles the dossier connection to its database,
	 * or %NULL if unable to open a valid connection.
	 *
	 * Since: version 1
	 */
	ofaIDBConnect * ( *connect_dossier )      ( const ofaIDBProvider *instance,
														ofaIFileMeta *meta,
														ofaIFilePeriod *period,
														const gchar *account,
														const gchar *password,
														gchar **msg );

	/**
	 * archive_and_new:
	 * @instance: the #ofaIDBProvider DBMS provider.
	 * @meta: the #ofaIFileMeta instance of the dossier.
	 * @period: the #ofaIFilePeriod instance of the source exercice.
	 * @root_account: the root account of the DBMS server.
	 * @root_password: the corresponding password.
	 * @user_account: the account whose privileges are to be duplicated.
	 * @begin_next: the beginning date of the next exercice.
	 * @end_next: the ending date of the next exercice.
	 *
	 * Duplicate the current @period exercice into a new one.
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
	gboolean        ( *archive_and_new )      ( const ofaIDBProvider *instance,
														ofaIFileMeta *meta,
														ofaIFilePeriod *period,
														const gchar *root_account,
														const gchar *root_password,
														const gchar *user_account,
														const GDate *begin_next,
														const GDate *end_next );
}
	ofaIDBProviderInterface;

GType           ofa_idbprovider_get_type                  ( void );

guint           ofa_idbprovider_get_interface_last_version( void );

guint           ofa_idbprovider_get_interface_version     ( const ofaIDBProvider *instance );

ofaIFileMeta   *ofa_idbprovider_get_dossier_meta          ( const ofaIDBProvider *instance,
																		const gchar *dossier_name,
																		mySettings *settings,
																		const gchar *group );

ofaIDBConnect  *ofa_idbprovider_connect_dossier           ( const ofaIDBProvider *instance,
																		ofaIFileMeta *meta,
																		ofaIFilePeriod *period,
																		const gchar *account,
																		const gchar *password,
																		gchar **msg );

gboolean        ofa_idbprovider_archive_and_new           ( const ofaIDBProvider *instance,
																	ofaIFileMeta *meta,
																	ofaIFilePeriod *period,
																	const gchar *root_account,
																	const gchar *root_password,
																	const gchar *user_account,
																	const GDate *begin_next,
																	const GDate *end_next );

ofaIDBProvider *ofa_idbprovider_get_instance_by_name      ( const gchar *provider_name );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBPROVIDER_H__ */
