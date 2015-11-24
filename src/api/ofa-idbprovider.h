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
#include "ofa-ifile-meta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBPROVIDER                      ( ofa_idbprovider_get_type())
#define OFA_IDBPROVIDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBPROVIDER, ofaIDBProvider ))
#define OFA_IS_IDBPROVIDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBPROVIDER ))
#define OFA_IDBPROVIDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBPROVIDER, ofaIDBProviderInterface ))

typedef struct _ofaIDBProvider                    ofaIDBProvider;

/**
 * ofaIDBProviderInterface:
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
	guint          ( *get_interface_version )( const ofaIDBProvider *instance );

	/**
	 * get_provider_name:
	 * @instance: the #ofaIDBProvider provider.
	 *
	 * Returns the name of this DBPROVIDER provider.
	 *
	 * This name acts as an identifier for the DBPROVIDER provider, and is
	 * not localized. It is recorded in the user configuration file as
	 * an access key to the dossier external properties.
	 *
	 * Return value: the name of this DBPROVIDER provider.
	 *
	 * The returned value is owned by the DBPROVIDER provider, and should not
	 * be released by the caller.
	 *
	 * Since: version 1
	 */
	const gchar *  ( *get_provider_name )    ( const ofaIDBProvider *instance );

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
	ofaIFileMeta * ( *get_dossier_meta )     ( const ofaIDBProvider *instance,
														const gchar *dossier_name,
														mySettings *settings,
														const gchar *group );

	/**
	 * get_dossier_periods:
	 * @instance: the #ofaIDBProvider provider.
	 * @settings: the #mySettings instance.
	 * @group: the group name in the settings.
	 *
	 * Return value: the list of defined periods for the identified
	 * dossier, as a #GList of GObject -derived objects which implement
	 * the #ofaIFilePeriod interface.
	 *
	 * The returned list is expected to be free by the caller via
	 * #ofa_idbprovider_free_dossier_periods().
	 *
	 * Since: version 1
	 */
	GList *        ( *get_dossier_periods )  ( const ofaIDBProvider *instance,
														mySettings *settings,
														const gchar *group );
}
	ofaIDBProviderInterface;

GType           ofa_idbprovider_get_type                  ( void );

guint           ofa_idbprovider_get_interface_last_version( void );

guint           ofa_idbprovider_get_interface_version     ( const ofaIDBProvider *instance );

ofaIFileMeta   *ofa_idbprovider_get_dossier_meta          ( const ofaIDBProvider *instance,
																		const gchar *dossier_name,
																		mySettings *settings,
																		const gchar *group );

GList          *ofa_idbprovider_get_dossier_periods       ( const ofaIDBProvider *instance,
																		mySettings *settings,
																		const gchar *group );

#define         ofa_idbprovider_free_dossier_periods(L)   g_list_free_full(( L ), \
																		( GDestroyNotify ) g_object_unref )

ofaIDBProvider *ofa_idbprovider_get_instance_by_name      ( const gchar *provider_name );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBPROVIDER_H__ */
