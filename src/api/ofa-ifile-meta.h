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

#include "api/ofa-idbprovider.h"
#include "api/ofa-ifile-meta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IFILE_META                      ( ofa_ifile_meta_get_type())
#define OFA_IFILE_META( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IFILE_META, ofaIFileMeta ))
#define OFA_IS_IFILE_META( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IFILE_META ))
#define OFA_IFILE_META_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IFILE_META, ofaIFileMetaInterface ))

/**
 * ofaIFileMetaInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_dossier_name: [must]: returns the identifier name of the dossier.
 * @get_provider_name: [should]: returns the IDbms provider name.
 * @get_provider_instance: [should]: returns the IDbms provider instance.
 * @get_settings: [should]: returns the mySettings instance.
 * @get_group_name: [should]: returns the settings group name.
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
	guint            ( *get_interface_version )( const ofaIFileMeta *instance );

	/**
	 * get_dossier_name:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * Returns: the identifier name of the dossier as a newly allocated
	 * string which should be g_free() by the caller.
	 */
	gchar *          ( *get_dossier_name )     ( const ofaIFileMeta *instance );

	/**
	 * get_provider_name:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * Returns: the provider name as a newly allocated
	 * string which should be g_free() by the caller.
	 */
	gchar *          ( *get_provider_name )    ( const ofaIFileMeta *instance );

	/**
	 * get_provider_instance:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * Returns: a new reference to the provider instance which should
	 * be g_object_unref() by the caller.
	 */
	ofaIDBProvider * ( *get_provider_instance )( const ofaIFileMeta *instance );

	/**
	 * get_settings:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * Returns: the #mySettings object which manages the dossier meta
	 * datas.
	 */
	mySettings *     ( *get_settings )( const ofaIFileMeta *instance );

	/**
	 * get_group_name:
	 * @instance: the #ofaIFileMeta instance.
	 *
	 * Returns: the settings group name as a newly allocated string
	 * which should be g_free() by the caller.
	 */
	gchar *          ( *get_group_name )( const ofaIFileMeta *instance );
}
	ofaIFileMetaInterface;

GType           ofa_ifile_meta_get_type                  ( void );

guint           ofa_ifile_meta_get_interface_last_version( void );

guint           ofa_ifile_meta_get_interface_version     ( const ofaIFileMeta *meta );

gchar          *ofa_ifile_meta_get_dossier_name          ( const ofaIFileMeta *meta );

gchar          *ofa_ifile_meta_get_provider_name         ( const ofaIFileMeta *meta );

ofaIDBProvider *ofa_ifile_meta_get_provider_instance     ( const ofaIFileMeta *meta );

GList          *ofa_ifile_meta_get_periods               ( const ofaIFileMeta *meta );

#define         ofa_ifile_meta_free_periods(L)           g_list_free_full(( L ), \
																	( GDestroyNotify ) g_object_unref )

mySettings     *ofa_ifile_meta_get_settings              ( const ofaIFileMeta *meta );

gchar          *ofa_ifile_meta_get_group_name            ( const ofaIFileMeta *meta );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IFILE_META_H__ */
