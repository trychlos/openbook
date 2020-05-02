/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IDBSUPERUSER_H__
#define __OPENBOOK_API_OFA_IDBSUPERUSER_H__

/**
 * SECTION: idbsuperuser
 * @title: ofaIDBSuperuser
 * @short_description: The DMBS New Interface
 * @include: openbook/ofa-idbsuperuser.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBSuperuser is the interface a #GtkWidget instanciated by a
 * DBMS provider should implement to let the user enter super-user
 * credentials.
 *
 * in its most simple form (see e.g. MySQL implementation), the super-
 * user priviles are just an account and its password.
 *
 * The implementation should provide a 'ofa-changed' signal in order
 * the application is able to detect the modifications brought up by
 * the user.
 *
 * If this interface is not implemented by the DBMS provider, then the
 * application considers that this provider does not have any sort of
 * super-user privileges.
 */

#include <gtk/gtk.h>

#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbprovider-def.h"
#include "api/ofa-idbsuperuser-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBSUPERUSER                      ( ofa_idbsuperuser_get_type())
#define OFA_IDBSUPERUSER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBSUPERUSER, ofaIDBSuperuser ))
#define OFA_IS_IDBSUPERUSER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBSUPERUSER ))
#define OFA_IDBSUPERUSER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBSUPERUSER, ofaIDBSuperuserInterface ))

#if 0
typedef struct _ofaIDBSuperuser                    ofaIDBSuperuser;
#endif

/**
 * ofaIDBSuperuserInterface:
 * @get_interface_version: [should]: returns the interface version number.
 * @set_dossier_meta: [may]: set the #ofaIDBDossierMeta.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 * @set_with_remember: [may]: set the sensitivity of the 'Remember' button.
 * @is_valid: [may]: returns %TRUE if the entered informations are valid.
 * @set_valid: [may]: set the validity status.
 *
 * This defines the interface that an #ofaIDBSuperuser should implement.
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
	guint           ( *get_interface_version )       ( void );

	/*** instance-wide ***/
	/**
	 * set_dossier_meta:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @dossier_meta: a #ofaIDBDossierMeta.
	 *
	 * Set the @dossier_meta.
	 *
	 * The interface code maintains itself this data, and it is thus
	 * not really needed for the implementation to also maintain it.
	 * This method is more thought to advertize the implementation of
	 * the availability of the data as soon as it is set.
	 *
	 * Since: version 1
	 */
	void            ( *set_dossier_meta )            ( ofaIDBSuperuser *instance,
															ofaIDBDossierMeta *dossier_meta );

	/**
	 * get_size_group:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @column: the desired column.
	 *
	 * Returns: the #GtkSizeGroup for the desired @column.
	 *
	 * Since: version 1
	 */
	GtkSizeGroup *  ( *get_size_group )              ( const ofaIDBSuperuser *instance,
															guint column );

	/**
	 * set_with_remember:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @with_remember: whether the 'Remember' button should be active.
	 *
	 * Set the sensitivity of the 'Remember' button.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1
	 */
	void             ( *set_with_remember )          ( ofaIDBSuperuser *instance,
															gboolean with_remember );

	/**
	 * is_valid:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @message: [allow-none][out]: a message to be set.
	 *
	 * Returns: %TRUE if the entered connection informations are valid.
	 *
	 * Note that we only do here an intrinsic check as we do not have
	 * any credentials to test for a real server connection.
	 *
	 * Since: version 1
	 */
	gboolean        ( *is_valid )                    ( const ofaIDBSuperuser *instance,
															gchar **message );

	/**
	 * set_valid:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @valid: the validity status.
	 *
	 * Set the validity status.
	 *
	 * Since: version 1
	 */
	void             ( *set_valid )                  ( ofaIDBSuperuser *instance,
															gboolean valid );

	/**
	 * set_credentials_from_connect:
	 * @instance: the #ofaIDBSuperuser instance.
	 * @connect: an #ofaIDBConnect.
	 *
	 * Set credentials from @connect.
	 *
	 * Since: version 1
	 */
	void             ( *set_credentials_from_connect)( ofaIDBSuperuser *instance,
															ofaIDBConnect *connect );
}
	ofaIDBSuperuserInterface;

/*
 * Interface-wide
 */
GType              ofa_idbsuperuser_get_type                    ( void );

guint              ofa_idbsuperuser_get_interface_last_version  ( void );

/*
 * Implementation-wide
 */
guint              ofa_idbsuperuser_get_interface_version       ( GType type );

/*
 * Instance-wide
 */
ofaIDBProvider    *ofa_idbsuperuser_get_provider                ( const ofaIDBSuperuser *instance );

void               ofa_idbsuperuser_set_provider                ( ofaIDBSuperuser *instance,
																		ofaIDBProvider *provider );

ofaIDBDossierMeta *ofa_idbsuperuser_get_dossier_meta            ( const ofaIDBSuperuser *instance );

void               ofa_idbsuperuser_set_dossier_meta            ( ofaIDBSuperuser *instance,
																		ofaIDBDossierMeta *dossier_meta );

GtkSizeGroup      *ofa_idbsuperuser_get_size_group              ( const ofaIDBSuperuser *instance,
																		guint column );

void               ofa_idbsuperuser_set_with_remember           ( ofaIDBSuperuser *instance,
																		gboolean with_remember );

gboolean           ofa_idbsuperuser_is_valid                    ( const ofaIDBSuperuser *instance,
																		gchar **message );

void               ofa_idbsuperuser_set_valid                   ( ofaIDBSuperuser *instance,
																		gboolean valid );

void               ofa_idbsuperuser_set_credentials_from_connect( ofaIDBSuperuser *instance,
																		ofaIDBConnect *connect );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBSUPERUSER_H__ */
