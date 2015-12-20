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

#ifndef __OPENBOOK_API_OFA_ITHEME_H__
#define __OPENBOOK_API_OFA_ITHEME_H__

/**
 * SECTION: itheme
 * @title: ofaITheme
 * @short_description: The ofaITheme Interface
 * @include: openbook/ofa-itheme.h
 *
 * The #ofaITheme interface manages the themes displayed by the main
 * window. This interface is mainly implemented by the main window.
 * It lets the plugins add and activate themes in the user interface.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_ITHEME                      ( ofa_itheme_get_type())
#define OFA_ITHEME( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITHEME, ofaITheme ))
#define OFA_IS_ITHEME( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITHEME ))
#define OFA_ITHEME_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITHEME, ofaIThemeInterface ))

typedef struct _ofaITheme                    ofaITheme;

/**
 * ofaIThemeInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @add_theme:
 * @activate_theme.
 *
 * This defines the interface that an #ofaITheme should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaITheme instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaITheme interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaITheme *instance );

	/**
	 * add_theme:
	 * @instance: the #ofaITheme instance.
	 * @name: the name of the theme; the main window implementation
	 *  displays this @name as the label of the page tab of the main
	 *  notebook.
	 * @fntype: a pointer to the get_type() function of the page.
	 * @with_entries: whether the page will allow a 'View entries'
	 *  button.
	 *
	 * Defines and initialise a new theme.
	 *
	 * Returns: the theme identifier provided by the implementation.
	 *
	 * Since: version 1
	 */
	guint    ( *add_theme )            ( ofaITheme *instance,
												const gchar *name,
												gpointer fntype,
												gboolean with_entries );

	/**
	 * activate_theme:
	 * @instance: the #ofaITheme instance.
	 * @theme: the theme identifier as returned by #add_theme().
	 *
	 * Display the page corresponding to the @theme identifier.
	 * Creates it if it did not exist yet.
	 *
	 * Since: version 1
	 */
	void     ( *activate_theme )       ( ofaITheme *instance,
												guint theme );
}
	ofaIThemeInterface;

GType ofa_itheme_get_type                  ( void );

guint ofa_itheme_get_interface_last_version( void );

guint ofa_itheme_get_interface_version     ( const ofaITheme *instance );

guint ofa_itheme_add_theme                 ( ofaITheme *instance,
													const gchar *name,
													gpointer fntype,
													gboolean with_entries );

void  ofa_itheme_activate_theme            ( ofaITheme *instance,
													guint theme );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITHEME_H__ */
