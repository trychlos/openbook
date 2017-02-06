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

#ifndef __OPENBOOK_API_OFA_IDBDOSSIER_META_H__
#define __OPENBOOK_API_OFA_IDBDOSSIER_META_H__

/**
 * SECTION: idbdossier_meta
 * @title: ofaIDBDossierMeta
 * @short_description: An interface to manage dossiers meta properties.
 * @include: openbook/ofa-idbdossier-meta.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBDossierMeta interface manages the identification of the dossiers,
 * and other external properties. This interface is expected to be
 * implemented by objects instanciated by DBMS plugins.
 *
 * This is an Openbook software suite decision to have all these meta
 * properties stored in a single dedicated ini file, said dossiers
 * settings. This dossiers settings file is mainly managed through
 * the #ofaDossierCollection singleton.
 */

#include "my/my-isettings.h"

#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbdossier-editor-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-idbprovider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBDOSSIER_META                      ( ofa_idbdossier_meta_get_type())
#define OFA_IDBDOSSIER_META( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBDOSSIER_META, ofaIDBDossierMeta ))
#define OFA_IS_IDBDOSSIER_META( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBDOSSIER_META ))
#define OFA_IDBDOSSIER_META_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBDOSSIER_META, ofaIDBDossierMetaInterface ))

#if 0
typedef struct _ofaIDBDossierMeta                     ofaIDBDossierMeta;
typedef struct _ofaIDBDossierMetaInterface            ofaIDBDossierMetaInterface;
#endif

/**
 * ofaIDBDossierMetaInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @set_from_settings: [should]: set datas from settings.
 * @set_from_editor: [should]: set datas from ofaIDBEditor.
 * @new_connect: [should]: returns a new ofaIDBConnect object.
 * @new_period: [should]: instanciates a new #ofaIDBExerciceMeta instance.
 * @delete_period: [should]: deletes a period from dbms and settings.
 * @dump: [should]: dump data.
 *
 * This defines the interface that an #ofaIDBDossierMeta should/must
 * implement.
 */
struct _ofaIDBDossierMetaInterface {
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
	 * set_from_settings:
	 * @instance: this #ofaIDBDossierMeta instance.
	 *
	 * Set the @instance object with informations read from dossier settings.
	 * Reset the defined financial periods accordingly.
	 *
	 * Since: version 1
	 */
	void                   ( *set_from_settings )    ( ofaIDBDossierMeta *instance );

	/**
	 * set_from_editor:
	 * @instance: this #ofaIDBDossierMeta instance.
	 * @editor: the #ofaIDBDossierEditor which handles the connection
	 *  informations.
	 *
	 * Writes the connection informations to @settings file.
	 *
	 * Since: version 1
	 */
	void                   ( *set_from_editor )      ( ofaIDBDossierMeta *instance,
															ofaIDBDossierEditor *editor );

	/**
	 * new_connect:
	 * @dossier_meta: this #ofaIDBDossierMeta instance.
	 *
	 * Returns: a newly defined #ofaIDBConnect object.
	 *
	 * Due to the lack of a user account at this time, the connection
	 * to the DBMS is known to not be established.
	 *
	 * Since: version 1
	 */
	ofaIDBConnect *        ( *new_connect )          ( ofaIDBDossierMeta *instance );

	/**
	 * new_exercice_meta:
	 * @instance: this #ofaIDBDossierMeta instance.
	 *
	 * Returns: a newly defined #ofaIDBExerciceMeta object.
	 *
	 * Since: version 1
	 */
	ofaIDBExerciceMeta *   ( *new_period )           ( ofaIDBDossierMeta *instance );

	/**
	 * delete_period:
	 * @instance: this #ofaIDBDossierMeta instance.
	 * @connect: a superuser connect on the DBMS;
	 *  the #ofaIDBDossierMeta is expected to be set to this @instance;
	 *  the #ofaIDBExerciceMeta is ignored.
	 * @exercice_meta: the #ofaIDBExerciceMeta to be deleted.
	 * @msgerr: [out][allow-none]: a placeholder for an error message.
	 *
	 * Deletes from the DBMS the datas attached to the @period at
	 * @instance level.
	 *
	 * Returns: %TRUE if the deletion has been successful.
	 *
	 * Since: version 1
	 */
	gboolean               ( *delete_period )        ( ofaIDBDossierMeta *instance,
															ofaIDBConnect *connect,
															ofaIDBExerciceMeta *exercice_meta,
															gchar **msgerr );

	/**
	 * dump:
	 * @instance: this #ofaIDBDossierMeta instance.
	 *
	 * Dumps the @instance.
	 *
	 * Since: version 1
	 */
	void                   ( *dump )                 ( const ofaIDBDossierMeta *instance );
};

/*
 * Interface-wide
 */
GType                 ofa_idbdossier_meta_get_type                  ( void );

guint                 ofa_idbdossier_meta_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint                 ofa_idbdossier_meta_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIDBProvider       *ofa_idbdossier_meta_get_provider              ( const ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_set_provider              ( ofaIDBDossierMeta *meta,
																			ofaIDBProvider *provider );

const gchar          *ofa_idbdossier_meta_get_dossier_name          ( const ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_set_dossier_name          ( ofaIDBDossierMeta *meta,
																			const gchar *dossier_name );

myISettings          *ofa_idbdossier_meta_get_settings_iface        ( const ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_set_settings_iface        ( ofaIDBDossierMeta *meta,
																			myISettings *settings );

const gchar          *ofa_idbdossier_meta_get_settings_group        ( const ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_set_settings_group        ( ofaIDBDossierMeta *meta,
																			const gchar *group_name );

void                  ofa_idbdossier_meta_set_from_settings         ( ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_set_from_editor           ( ofaIDBDossierMeta *meta,
																			ofaIDBDossierEditor *editor );

ofaIDBConnect        *ofa_idbdossier_meta_new_connect               ( ofaIDBDossierMeta *meta,
																			ofaIDBExerciceMeta *period );

ofaIDBExerciceMeta   *ofa_idbdossier_meta_new_period                ( ofaIDBDossierMeta *meta,
																			gboolean attach );

const GList          *ofa_idbdossier_meta_get_periods               ( const ofaIDBDossierMeta *meta );

guint                 ofa_idbdossier_meta_get_periods_count         ( const ofaIDBDossierMeta *meta );

ofaIDBExerciceMeta   *ofa_idbdossier_meta_get_period                ( const ofaIDBDossierMeta *meta,
																			const GDate *date,
																			gboolean accept_empty );

ofaIDBExerciceMeta   *ofa_idbdossier_meta_get_current_period        ( const ofaIDBDossierMeta *meta );

ofaIDBExerciceMeta   *ofa_idbdossier_meta_get_archived_period       ( const ofaIDBDossierMeta *meta,
																			const GDate *date );

ofaIDBExerciceMeta   *ofa_idbdossier_meta_get_suitable_period       ( const ofaIDBDossierMeta *meta,
																			const GDate *begin,
																			const GDate *end );

gint                  ofa_idbdossier_meta_compare                   ( const ofaIDBDossierMeta *a,
																			const ofaIDBDossierMeta *b );

gint                  ofa_idbdossier_meta_compare_by_name           ( const ofaIDBDossierMeta *a,
																			const gchar *name );

gboolean              ofa_idbdossier_meta_delete_period             ( ofaIDBDossierMeta *meta,
																			ofaIDBConnect *connect,
																			ofaIDBExerciceMeta *period,
																			gboolean delete_dossier_on_last,
																			gchar **msgerr );

void                  ofa_idbdossier_meta_dump                      ( const ofaIDBDossierMeta *meta );

void                  ofa_idbdossier_meta_dump_full                 ( const ofaIDBDossierMeta *meta );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBDOSSIER_META_H__ */
