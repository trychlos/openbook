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

#ifndef __OPENBOOK_API_OFA_IGETTER_H__
#define __OPENBOOK_API_OFA_IGETTER_H__

/**
 * SECTION: igetter
 * @title: ofaIGetter
 * @short_description: The IGetter Interface
 * @include: openbook/ofa-igetter.h
 *
 * The #ofaIGetter interface lets plugins, external modules and more
 * generally all parts of application access to some global interest
 * variables.
 *
 * All Openbook-related applications, whether they are GUI-oriented or
 * only command-line tools, should at least have an #ofaHub global
 * object, as it is supposed to hold all non-gui related variables.
 *
 * The Openbook GUI also provides a theme manager, implemented by the
 * main window, which displays pages as tabs of a main notebook.
 */

#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-itheme-manager-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IGETTER                      ( ofa_igetter_get_type())
#define OFA_IGETTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IGETTER, ofaIGetter ))
#define OFA_IS_IGETTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IGETTER ))
#define OFA_IGETTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IGETTER, ofaIGetterInterface ))

#if 0
typedef struct _ofaIGetter                    ofaIGetter;
#endif

/**
 * ofaIGetterInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_application: [should]: return the #GApplication instance.
 * @get_hub: [should]: return the #ofaHub instance.
 * @get_theme_manager: [should]: return the #ofaIThemeManager instance.
 *
 * This defines the interface that an #ofaIGetter must/should/may implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: an #ofaIGetter instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIGetter interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint                  ( *get_interface_version )( const ofaIGetter *instance );

	/**
	 * get_application:
	 * @instance: this #ofaIGetter instance.
	 *
	 * Returns: the GApplication.
	 *
	 * Since: version 1
	 */
	GApplication *         ( *get_application )      ( const ofaIGetter *instance );

	/**
	 * get_hub:
	 * @instance: this #ofaIGetter instance.
	 *
	 * Returns: the main hub object of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	ofaHub *               ( *get_hub )              ( const ofaIGetter *instance );

	/**
	 * get_main_window:
	 * @instance: this #ofaIGetter instance.
	 *
	 * Returns: the main window of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	GtkApplicationWindow * ( *get_main_window )      ( const ofaIGetter *instance );

	/**
	 * get_theme_manager:
	 * @instance: this #ofaIGetter instance.
	 *
	 * Returns: the theme manager of the application, or %NULL.
	 *
	 * Since: version 1
	 */
	ofaIThemeManager *     ( *get_theme_manager )    ( const ofaIGetter *instance );
}
	ofaIGetterInterface;

GType                 ofa_igetter_get_type                  ( void );

guint                 ofa_igetter_get_interface_last_version( void );

guint                 ofa_igetter_get_interface_version     ( const ofaIGetter *instance );

GApplication         *ofa_igetter_get_application           ( const ofaIGetter *instance );

ofaHub               *ofa_igetter_get_hub                   ( const ofaIGetter *instance );

GtkApplicationWindow *ofa_igetter_get_main_window           ( const ofaIGetter *instance );

ofaIThemeManager     *ofa_igetter_get_theme_manager         ( const ofaIGetter *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IGETTER_H__ */
