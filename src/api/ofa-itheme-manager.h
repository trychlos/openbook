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

#ifndef __OPENBOOK_API_OFA_ITHEME_MANAGER_H__
#define __OPENBOOK_API_OFA_ITHEME_MANAGER_H__

/**
 * SECTION: itheme_manager
 * @title: ofaIThemeManager
 * @short_description: The IThemeManager Interface
 * @include: openbook/ofa-itheme-manager.h
 *
 * The #ofaIThemeManager is the interface for theme management.
 */

#include <glib-object.h>

#include "api/ofa-itheme-manager-def.h"
#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ITHEME_MANAGER                      ( ofa_itheme_manager_get_type())
#define OFA_ITHEME_MANAGER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITHEME_MANAGER, ofaIThemeManager ))
#define OFA_IS_ITHEME_MANAGER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITHEME_MANAGER ))
#define OFA_ITHEME_MANAGER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITHEME_MANAGER, ofaIThemeManagerInterface ))

#if 0
typedef struct _ofaIThemeManager                     ofaIThemeManager;
#endif

/**
 * ofaIThemeManagerInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 * @define: [should]: define a new theme.
 *
 * This defines the interface that an #ofaIThemeManager must/should/may
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIThemeManager instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint     ( *get_interface_version )( const ofaIThemeManager *instance );

	/**
	 * define:
	 * @instance: the #ofaIThemeManager instance.
	 * @label: the tab notebook label.
	 * @type: the GType of the page.
	 *
	 * Returns: the new theme identifier.
	 *
	 * Since: version 1.
	 */
	guint     ( *define )               ( ofaIThemeManager *instance,
												const gchar *label,
												GType type );

	/**
	 * activate:
	 * @instance: the #ofaIThemeManager instance.
	 * @theme_id: the identifier of the theme to be activated.
	 *
	 * Returns: the theme's page.
	 *
	 * Since: version 1.
	 */
	ofaPage * ( *activate )             ( ofaIThemeManager *instance,
												guint theme_id );
}
	ofaIThemeManagerInterface;

GType    ofa_itheme_manager_get_type                  ( void );

guint    ofa_itheme_manager_get_interface_last_version( void );

guint    ofa_itheme_manager_get_interface_version     ( const ofaIThemeManager *instance );

guint    ofa_itheme_manager_define                    ( ofaIThemeManager *instance,
															const gchar *label,
															GType type );

ofaPage *ofa_itheme_manager_activate                  ( ofaIThemeManager *instance,
															guint theme_id );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITHEME_MANAGER_H__ */
