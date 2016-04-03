/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IDBMETA_H__
#define __OPENBOOK_API_OFA_IDBMETA_H__

/**
 * SECTION: idbmeta
 * @title: ofaIDBMeta
 * @short_description: An interface to manage dossiers meta properties.
 * @include: openbook/ofa-idbmeta.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBMeta interface manages the identification of the dossiers,
 * and other external properties. This interface is expected to be
 * implemented by objects instanciated by DBMS plugins.
 *
 * This is an Openbook software suite decision to have all these meta
 * properties stored in a single dedicated ini file, said dossiers
 * settings. This dossiers settings file is mainly managed through
 * the #ofaPortfolioCollection singleton.
 */

#include "my/my-isettings.h"

#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta-def.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBMETA                      ( ofa_idbmeta_get_type())
#define OFA_IDBMETA( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBMETA, ofaIDBMeta ))
#define OFA_IS_IDBMETA( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBMETA ))
#define OFA_IDBMETA_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBMETA, ofaIDBMetaInterface ))

#if 0
typedef struct _ofaIDBMeta                     ofaIDBMeta;
typedef struct _ofaIDBMetaInterface            ofaIDBMetaInterface;
#endif

/**
 * ofaIDBMetaInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @set_from_settings: [should]: set datas from settings.
 * @set_from_editor: [should]: set datas from ofaIDBEditor.
 * @update_period: [should]: updates a period in the settings.
 * @remove_period: [should]: removes a period from the settings.
 * @dump: [should]: dump data.
 *
 * This defines the interface that an #ofaIDBMeta should/must
 * implement.
 */
struct _ofaIDBMetaInterface {
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
	guint            ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * set_from_settings:
	 * @instance: the #ofaIDBMeta instance.
	 * @settings: a #myISettings instance.
	 * @group: the group name in the settings.
	 *
	 * Set the @instance object with informations read from @settings.
	 * Reset the defined financial periods accordingly.
	 *
	 * Since: version 1
	 */
	void             ( *set_from_settings )    ( ofaIDBMeta *instance,
													myISettings *settings,
													const gchar *group );

	/**
	 * set_from_editor:
	 * @instance: the #ofaIDBMeta instance.
	 * @editor: the #ofaIDBEditor which handles the connection informations.
	 * @settings: a #myISettings instance.
	 * @group: the group name in the settings.
	 *
	 * Writes the connection informations to @settings file.
	 *
	 * Since: version 1
	 */
	void             ( *set_from_editor )      ( ofaIDBMeta *instance,
													const ofaIDBEditor *editor,
													myISettings *settings,
													const gchar *group );

	/**
	 * update_period:
	 * @instance: the #ofaIDBMeta instance.
	 * @period: the #ofaIDBPeriod to be updated.
	 * @current: whether the financial period (exercice) is current.
	 * @begin: [allow-none]: the beginning date.
	 * @end: [allow-none]: the ending date.
	 *
	 * Update the dossier settings for this @period with the specified
	 * datas.
	 *
	 * Since: version 1
	 */
	void             ( *update_period )        ( ofaIDBMeta *instance,
													ofaIDBPeriod *period,
													gboolean current,
													const GDate *begin,
													const GDate *end );

	/**
	 * remove_period:
	 * @instance: the #ofaIDBMeta instance.
	 * @period: the #ofaIDBPeriod to be removed.
	 *
	 * Removes the @period from the dossier settings file.
	 * The interface makes sure this method is only called when
	 * @period is not the last financial period of the @instance.
	 *
	 * Since: version 1
	 */
	void             ( *remove_period )        ( ofaIDBMeta *instance,
													ofaIDBPeriod *period );

	/**
	 * dump:
	 * @instance: the #ofaIDBMeta instance.
	 *
	 * Dumps the @instance.
	 *
	 * Since: version 1
	 */
	void             ( *dump )                 ( const ofaIDBMeta *instance );
};

/*
 * Interface-wide
 */
GType           ofa_idbmeta_get_type                  ( void );

guint           ofa_idbmeta_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint           ofa_idbmeta_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIDBProvider *ofa_idbmeta_get_provider              ( const ofaIDBMeta *meta );

void            ofa_idbmeta_set_provider              ( ofaIDBMeta *meta,
																const ofaIDBProvider *instance );

gchar          *ofa_idbmeta_get_dossier_name          ( const ofaIDBMeta *meta );

void            ofa_idbmeta_set_dossier_name          ( ofaIDBMeta *meta,
																const gchar *dossier_name );

myISettings    *ofa_idbmeta_get_settings              ( const ofaIDBMeta *meta );

gchar          *ofa_idbmeta_get_group_name            ( const ofaIDBMeta *meta );

void            ofa_idbmeta_set_from_settings         ( ofaIDBMeta *meta,
																myISettings *settings,
																const gchar *group_name );

void            ofa_idbmeta_set_from_editor           ( ofaIDBMeta *meta,
																const ofaIDBEditor *editor,
																myISettings *settings,
																const gchar *group_name );

void            ofa_idbmeta_remove_meta               ( ofaIDBMeta *meta );

GList          *ofa_idbmeta_get_periods               ( const ofaIDBMeta *meta );

#define         ofa_idbmeta_free_periods(L)           g_list_free_full(( L ), \
																( GDestroyNotify ) g_object_unref )

void            ofa_idbmeta_set_periods               ( ofaIDBMeta *meta,
																GList *periods );

void            ofa_idbmeta_add_period                ( ofaIDBMeta *meta,
																ofaIDBPeriod *period );

void            ofa_idbmeta_update_period             ( ofaIDBMeta *meta,
																ofaIDBPeriod *period,
																gboolean current,
																const GDate *begin,
																const GDate *end );

void            ofa_idbmeta_remove_period             ( ofaIDBMeta *meta,
																ofaIDBPeriod *period );

ofaIDBPeriod   *ofa_idbmeta_get_current_period        ( const ofaIDBMeta *meta );

ofaIDBPeriod   *ofa_idbmeta_get_period                ( const ofaIDBMeta *meta,
																const GDate *begin,
																const GDate *end );

void            ofa_idbmeta_dump                      ( const ofaIDBMeta *meta );

void            ofa_idbmeta_dump_rec                  ( const ofaIDBMeta *meta );

gboolean        ofa_idbmeta_are_equal                 ( const ofaIDBMeta *a,
																const ofaIDBMeta *b );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBMETA_H__ */
