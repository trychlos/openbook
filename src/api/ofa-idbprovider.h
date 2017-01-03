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
 * The module which provides an #ofaIDBProvider instance should most
 * probably also provide an #ofaIDBModel implementation, as it is the
 * #ofaIDBProvider responsability to create the underlying DB model.
 *
 * As the two #ofaIDBProvider and #ofaIDBmodel must each provides their
 * own identification, and because this identification relies on the
 * myIIdent interface implementation, the #ofaIDBProvider and
 * #ofaIDBmodel must be provided by distinct classes.
 *
 * This is an Openbook software suite choice to store most of the
 * meta data a dossier may require in a dedicated settings file.
 *
 * The #ofaIDBProvider implementation *must* implement the #ofaISetter
 * interface. This is at least needed to get an access to the #ofaHub
 * object of the application, and though it to the #myISettings
 * interface used to managed the dedicated settings file.
 */

#include <glib-object.h>

#include "ofa-hub-def.h"
#include "ofa-idbconnect-def.h"
#include "ofa-idbdossier-meta-def.h"
#include "ofa-idbdossier-editor.h"
#include "ofa-idbeditor.h"
#include "ofa-idbexercice-editor.h"
#include "ofa-idbprovider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBPROVIDER                      ( ofa_idbprovider_get_type())
#define OFA_IDBPROVIDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBPROVIDER, ofaIDBProvider ))
#define OFA_IS_IDBPROVIDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBPROVIDER ))
#define OFA_IDBPROVIDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBPROVIDER, ofaIDBProviderInterface ))

#if 0
typedef struct _ofaIDBProvider                    ofaIDBProvider;
typedef struct _ofaIDBProviderInterface           ofaIDBProviderInterface;
#endif

/**
 * ofaIDBProviderInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @new_meta: [should]: returns a new ofaIDBDossierMeta object.
 * @new_connect: [should]: returns a new ofaIDBConnect object.
 * @new_editor: [should]: returns a new ofaIDBEditor object.
 * @new_dossier_editor: [should]: returns a new ofaIDBDossierEditor object.
 * @new_exercice_editor: [should]: returns a new ofaIDBExerciceEditor object.
 *
 * This defines the interface that an #ofaIDBProvider should implement.
 */
struct _ofaIDBProviderInterface {
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
	guint                  ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * new_dossier_meta:
	 * @instance: this #ofaIDBProvider instance.
	 *
	 * Returns: a newly defined #ofaIDBDossierMeta object.
	 *
	 * Since: version 1
	 */
	ofaIDBDossierMeta *    ( *new_dossier_meta )     ( ofaIDBProvider *instance );

	/**
	 * new_dossier_editor:
	 * @instance: this #ofaIDBProvider instance.
	 * @settings_prefix: the prefix of a user preference key.
	 * @rule: the usage of the editor.
	 *
	 * Returns: a #GtkWidget which implements the #ofaIDBDossierEditor
	 * interface, and handles the informations needed to qualify a
	 * DB server and the storage space required for a dossier.
	 *
	 * Since: version 1
	 */
	ofaIDBDossierEditor *  ( *new_dossier_editor )   ( ofaIDBProvider *instance,
															const gchar *settings_prefix,
															guint rule );

	/**
	 * new_connect:
	 * @instance: this #ofaIDBProvider instance.
	 * @account: the account of the connection.
	 * @password: the password of the connection.
	 * @dossier_meta: the #ofaIDBDossierMeta object.
	 * @exercice_meta: [allow-none]: the #ofaIDBEXerciceMeta object;
	 *  if %NULL, the connection is established at server level.
	 *
	 * Returns: a newly defined #ofaIDBConnect object, or %NULL if the
	 * connection could not be established.
	 *
	 * Since: version 1
	 */
	ofaIDBConnect *        ( *new_connect )          ( ofaIDBProvider *instance,
															const gchar *account,
															const gchar *password,
															ofaIDBDossierMeta *dossier_meta,
															ofaIDBExerciceMeta *exercice_meta );

	/**
	 * new_editor:
	 * @instance: this #ofaIDBProvider instance.
	 * @editable: whether the returned widget should handle informations
	 *  for edit or only display.
	 *
	 * Returns: a #GtkWidget which implements the #ofaIDBEditor
	 * interface, and handles the informations needed to qualify a
	 * DB server and the storage space required for a dossier.
	 *
	 * Since: version 1
	 */
	ofaIDBEditor *         ( *new_editor )           ( ofaIDBProvider *instance,
															gboolean editable );

	/**
	 * new_exercice_editor:
	 * @instance: this #ofaIDBProvider instance.
	 * @settings_prefix: the prefix of a user preference key.
	 * @rule: the usage of the editor.
	 *
	 * Returns: a #GtkWidget which implements the #ofaIDBExerciceEditor
	 * interface, and handles the informations needed to qualify a
	 * DB server and the storage space required for a dossier.
	 *
	 * Since: version 1
	 */
	ofaIDBExerciceEditor * ( *new_exercice_editor )  ( ofaIDBProvider *instance,
															const gchar *settings_prefix,
															guint rule );
};

/*
 * Interface-wide
 */
GType                 ofa_idbprovider_get_type                  ( void );

guint                 ofa_idbprovider_get_interface_last_version( void );

GList                *ofa_idbprovider_get_all                   ( ofaHub *hub );

ofaIDBProvider       *ofa_idbprovider_get_by_name               ( ofaHub *hub,
																		const gchar *provider_name );

/*
 * Implementation-wide
 */
guint                 ofa_idbprovider_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gchar                *ofa_idbprovider_get_canon_name            ( const ofaIDBProvider *provider );

gchar                *ofa_idbprovider_get_display_name          ( const ofaIDBProvider *provider );

ofaHub               *ofa_idbprovider_get_hub                   ( ofaIDBProvider *provider );

ofaIDBDossierMeta    *ofa_idbprovider_new_dossier_meta          ( ofaIDBProvider *provider,
																		const gchar *dossier_name );

ofaIDBDossierEditor  *ofa_idbprovider_new_dossier_editor        ( ofaIDBProvider *provider,
																		const gchar *settings_prefix,
																		guint rule );

ofaIDBConnect        *ofa_idbprovider_new_connect               ( ofaIDBProvider *provider,
																		const gchar *account,
																		const gchar *password,
																		ofaIDBDossierMeta *dossier_meta,
																		ofaIDBExerciceMeta *exercice_meta );

ofaIDBEditor         *ofa_idbprovider_new_editor                ( ofaIDBProvider *provider,
																		gboolean editable );

ofaIDBExerciceEditor *ofa_idbprovider_new_exercice_editor       ( ofaIDBProvider *provider,
																		const gchar *settings_prefix,
																		guint rule,
																		ofaIDBDossierEditor *dossier_editor );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBPROVIDER_H__ */
