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

#ifndef __OPENBOOK_API_OFA_IPAGE_MANAGER_H__
#define __OPENBOOK_API_OFA_IPAGE_MANAGER_H__

/**
 * SECTION: ipage_manager
 * @title: ofaIPageManager
 * @short_description: The IPageManager Interface
 * @include: openbook/ofa-ipage-manager.h
 *
 * The #ofaIPageManager is the interface for theme management.
 */

#include <glib-object.h>

#include "api/ofa-ipage-manager-def.h"
#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IPAGE_MANAGER                      ( ofa_ipage_manager_get_type())
#define OFA_IPAGE_MANAGER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPAGE_MANAGER, ofaIPageManager ))
#define OFA_IS_IPAGE_MANAGER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPAGE_MANAGER ))
#define OFA_IPAGE_MANAGER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPAGE_MANAGER, ofaIPageManagerInterface ))

#if 0
typedef struct _ofaIPageManager                     ofaIPageManager;
#endif

/**
 * ofaIPageManagerInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 * @define: [should]: define a new theme.
 *
 * This defines the interface that an #ofaIPageManager must/should/may
 * implement.
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
	guint     ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * define:
	 * @instance: the #ofaIPageManager instance.
	 * @type: the GType of the page (and identifier of the theme).
	 * @label: the tab notebook label.
	 * @single: whether the page is unique, or may be opened several
	 *  times.
	 *
	 * Defines a new theme.
	 *
	 * Since: version 1.
	 */
	void      ( *define )               ( ofaIPageManager *instance,
												GType type,
												const gchar *label,
												gboolean single );

	/**
	 * activate:
	 * @instance: the #ofaIPageManager instance.
	 * @theme_id: the identifier of the theme to be activated.
	 *
	 * Returns: the theme's page.
	 *
	 * Since: version 1.
	 */
	ofaPage * ( *activate )             ( ofaIPageManager *instance,
												GType type );
}
	ofaIPageManagerInterface;

/*
 * Interface-wide
 */
GType    ofa_ipage_manager_get_type                  ( void );

guint    ofa_ipage_manager_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    ofa_ipage_manager_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void     ofa_ipage_manager_define                    ( ofaIPageManager *instance,
															GType type,
															const gchar *label,
															gboolean single );

ofaPage *ofa_ipage_manager_activate                  ( ofaIPageManager *instance,
															GType type );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPAGE_MANAGER_H__ */
