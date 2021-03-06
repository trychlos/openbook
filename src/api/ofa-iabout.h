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

#ifndef __OPENBOOK_API_OFA_IABOUT_H__
#define __OPENBOOK_API_OFA_IABOUT_H__

/**
 * SECTION: iabout
 * @title: ofaIAbout
 * @short_description: The IAbout Interface
 * @include: openbook/ofa-iabout.h
 *
 * The #ofaIAbout interface let a plugin advertize about its properties
 * through a #GtkWidget which will be embedded in an application-provided
 * container.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IABOUT                      ( ofa_iabout_get_type())
#define OFA_IABOUT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IABOUT, ofaIAbout ))
#define OFA_IS_IABOUT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IABOUT ))
#define OFA_IABOUT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IABOUT, ofaIAboutInterface ))

typedef struct _ofaIAbout                    ofaIAbout;

/**
 * ofaIAboutInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaIAbout should implement.
 *
 * When implemented by a dynamically loadable module, the plugin should
 * take into account the fact #ofaPluginManager only takes care of the
 * first #ofaIAbout interface found.
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
	guint       ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * do_init:
	 * @instance: the #ofaIAbout provider.
	 * @getter: a #ofaIGetter of the application.
	 *
	 * Initialize the dialog.
	 *
	 * The IAbout provider may use the #myISettings user settings instance
	 * to get its value from the user's configuration file.
	 *
	 * Returns: a newly created page which will be displayed by the
	 * program.
	 *
	 * Since: version 1
	 */
	GtkWidget * ( *do_init )             ( ofaIAbout *instance,
												ofaIGetter *getter );
}
	ofaIAboutInterface;

/*
 * Interface-wide
 */
GType      ofa_iabout_get_type                  ( void );

guint      ofa_iabout_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      ofa_iabout_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkWidget *ofa_iabout_do_init                   ( ofaIAbout *instance,
														ofaIGetter *getter );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IABOUT_H__ */
