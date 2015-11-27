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

#ifndef __OPENBOOK_API_OFA_IFILE_META_H__
#define __OPENBOOK_API_OFA_IFILE_META_H__

/**
 * SECTION: ifile_meta
 * @title: ofaIFileMeta
 * @short_description: An interface to manage dossiers meta properties.
 * @include: openbook/ofa-ifile-meta.h
 *
 * The #ofaIFileMeta interface manages the identification of the dossiers,
 * and other external properties. This interface is expected to be
 * implemented by objects instanciated by DBMS plugins.
 *
 * This is an Openbook software suite decision to have all these meta
 * properties stored in a single dedicated ini file, said dossiers
 * settings. This dossiers settings file is mainly managed through
 * the #ofaFileDir singleton.
 */

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-ifile-meta-def.h"
#include "api/ofa-ifile-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_IFILE_META                      ( ofa_ifile_meta_get_type())
#define OFA_IFILE_META( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IFILE_META, ofaIFileMeta ))
#define OFA_IS_IFILE_META( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IFILE_META ))
#define OFA_IFILE_META_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IFILE_META, ofaIFileMetaInterface ))

/**
 * ofaIFileMetaInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @update_period: [should]: update a period in the settings.
 *
 * This defines the interface that an #ofaIFileMeta should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIFileMeta *instance );

	/**
	 * update_period:
	 * @instance: the #ofaIFileMeta instance.
	 * @period: the #ofaIFilePeriod to be updated.
	 * @current: whether the financial period (exercice) is current.
	 * @begin: [allow-none]: the beginning date.
	 * @end: [allow-none]: the ending date.
	 *
	 * Update the dossier settings for this @period with the specified
	 * datas.
	 *
	 * Defaults to 1.
	 */
	void  ( *update_period )        ( ofaIFileMeta *instance,
											ofaIFilePeriod *period,
											gboolean current,
											const GDate *begin,
											const GDate *end );
}
	ofaIFileMetaInterface;

GType           ofa_ifile_meta_get_type                  ( void );

guint           ofa_ifile_meta_get_interface_last_version( void );

guint           ofa_ifile_meta_get_interface_version     ( const ofaIFileMeta *meta );

gchar          *ofa_ifile_meta_get_provider_name         ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_set_provider_name         ( ofaIFileMeta *meta,
																	const gchar *provider_name );

ofaIDBProvider *ofa_ifile_meta_get_provider_instance     ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_set_provider_instance     ( ofaIFileMeta *meta,
																	const ofaIDBProvider *instance );

mySettings     *ofa_ifile_meta_get_settings              ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_set_settings              ( ofaIFileMeta *meta,
																	mySettings *settings );

gchar          *ofa_ifile_meta_get_group_name            ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_set_group_name            ( ofaIFileMeta *meta,
																	const gchar *group_name );

gchar          *ofa_ifile_meta_get_dossier_name          ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_set_dossier_name          ( ofaIFileMeta *meta,
																	const gchar *dossier_name );

GList          *ofa_ifile_meta_get_periods               ( const ofaIFileMeta *meta );

#define         ofa_ifile_meta_free_periods(L)           g_list_free_full(( L ), \
																	( GDestroyNotify ) g_object_unref )

void            ofa_ifile_meta_set_periods               ( ofaIFileMeta *meta,
																	GList *periods );

ofaIFilePeriod *ofa_ifile_meta_get_current_period        ( const ofaIFileMeta *meta );

void            ofa_ifile_meta_update_period             ( ofaIFileMeta *meta,
																	ofaIFilePeriod *period,
																	gboolean current,
																	const GDate *begin,
																	const GDate *end );

ofaIDBConnect  *ofa_ifile_meta_get_connection            ( ofaIFileMeta *meta,
																	ofaIFilePeriod *period,
																	const gchar *account,
																	const gchar *password,
																	gchar **msg );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IFILE_META_H__ */
