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
 * This #ofaIDBProvider is dedicated to instance management.
 *
 * This is an Openbook software suite choice to store most of the
 * meta data a dossier may require in a dedicated settings file.
 */

#include "ofa-idbconnect.h"
#include "ofa-idbeditor.h"
#include "ofa-ifile-meta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBPROVIDER                      ( ofa_idbprovider_get_type())
#define OFA_IDBPROVIDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBPROVIDER, ofaIDBProvider ))
#define OFA_IS_IDBPROVIDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBPROVIDER ))
#define OFA_IDBPROVIDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBPROVIDER, ofaIDBProviderInterface ))

/**
 * ofaIDBProviderInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_provider_name: [must]: returns the identifier name of the DBMS provider.
 * @new_meta: [should]: returns a new ofaIFileMeta object.
 * @new_connect: [should]: returns a new ofaIDBConnect object.
 * @new_editor: [should]: returns a new ofaIDBEditor object.
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
	 * Returns: if implemented, this method must return the version
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
	 * Returns: the name of this DBMS provider.
	 *
	 * The returned value is owned by the DBMS provider, and should not
	 * be released by the caller.
	 *
	 * Since: version 1
	 */
	const gchar *   ( *get_provider_name )    ( const ofaIDBProvider *instance );

	/**
	 * new_meta:
	 *
	 * Returns: a newly defined #ofaIFileMeta object.
	 *
	 * Since: version 1
	 */
	ofaIFileMeta *  ( *new_meta )             ( void );

	/**
	 * new_connect:
	 *
	 * Returns: a newly defined #ofaIDBConnect object.
	 *
	 * Since: version 1
	 */
	ofaIDBConnect * ( *new_connect )          ( void );

	/**
	 * new_editor:
	 * @editable: whether the returned widget should handle informations
	 *  for edit or only display.
	 *
	 * Returns: a #GtkWidget which implements the #ofaIDBEditor
	 * interface, and handles the informations needed to qualify a
	 * DB server and the storage space required for a dossier.
	 *
	 * Since: version 1
	 */
	ofaIDBEditor *  ( *new_editor )           ( gboolean editable );
}
	ofaIDBProviderInterface;

GType           ofa_idbprovider_get_type                  ( void );

guint           ofa_idbprovider_get_interface_last_version( void );

guint           ofa_idbprovider_get_interface_version     ( const ofaIDBProvider *instance );

ofaIFileMeta   *ofa_idbprovider_new_meta                  ( const ofaIDBProvider *instance );

ofaIDBConnect  *ofa_idbprovider_new_connect               ( const ofaIDBProvider *instance );

ofaIDBEditor   *ofa_idbprovider_new_editor                ( const ofaIDBProvider *instance,
																	gboolean editable );

const gchar    *ofa_idbprovider_get_name                  ( const ofaIDBProvider *instance );

ofaIDBProvider *ofa_idbprovider_get_instance_by_name      ( const gchar *provider_name );

GList          *ofa_idbprovider_get_list                  ( void );

#define         ofa_idbprovider_free_list( L )            g_debug( "ofa_idbprovider_free_list: list=%p, count=%d", ( void * )( L ), g_list_length( L )); \
																	g_list_free_full(( L ), ( GDestroyNotify ) g_free )

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBPROVIDER_H__ */
