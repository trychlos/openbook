/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OPENBOOK_API_OFA_IPREFERENCES_H__
#define __OPENBOOK_API_OFA_IPREFERENCES_H__

/**
 * SECTION: ipreferences
 * @title: ofaIPreferences
 * @short_description: The DMBS Interface
 * @include: openbook/ofa-ipreferences.h
 *
 * The #ofaIPreferences interface let the user choose and manage different
 * DBMS backends.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_IPREFERENCES                      ( ofa_ipreferences_get_type())
#define OFA_IPREFERENCES( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPREFERENCES, ofaIPreferences ))
#define OFA_IS_IPREFERENCES( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPREFERENCES ))
#define OFA_IPREFERENCES_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPREFERENCES, ofaIPreferencesInterface ))

typedef struct _ofaIPreferences                    ofaIPreferences;

/**
 * ofaIPreferencesInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaIPreferences should implement.
 *
 * The DBMS backend presents two sets of functions:
 * - a first one which addresses the DB server itself,
 * - the second one which manages the inside dossier through the opened
 *   DB server connexion.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIPreferences provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIPreferences interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint       ( *get_interface_version )( const ofaIPreferences *instance );

	/**
	 * run_init:
	 * @instance: the #ofaIPreferences provider.
	 *
	 * Initialize the Preferences dialog.
	 *
	 * The IPreferences provider may use the ofa_settings_xxx_() API to
	 * get its value to the user's configuration file.
	 *
	 * Returns: the page which has been added to the GtkNotebook to
	 * handle these preferences.
	 *
	 * Since: version 1
	 */
	GtkWidget * ( *run_init )             ( const ofaIPreferences *instance, GtkNotebook *book );

	/**
	 * run_check:
	 * @instance: the #ofaIPreferences provider.
	 * @page: the GtkNotebook page which handles these preferences, as
	 *  returned by #run_init().
	 *
	 * Checks for the Preferences dialog.
	 *
	 * Returns: %TRUE if the page doesn't contain any error, and is
	 * validable.
	 *
	 * Since: version 1
	 */
	gboolean    ( *run_check )            ( const ofaIPreferences *instance, GtkWidget *page );

	/**
	 * run_done:
	 * @instance: the #ofaIPreferences provider.
	 * @page: the GtkNotebook page which handles these preferences, as
	 *  returned by #run_init().
	 *
	 * Terminate the Preferences dialog.
	 *
	 * The IPreferences provider may use the ofa_settings_xxx_() API to
	 * write its value to the user's configuration file.
	 *
	 * Since: version 1
	 */
	void        ( *run_done )             ( const ofaIPreferences *instance, GtkWidget *page );
}
	ofaIPreferencesInterface;

GType      ofa_ipreferences_get_type ( void );

GtkWidget *ofa_ipreferences_run_init ( const ofaIPreferences *instance, GtkNotebook *book );

gboolean   ofa_ipreferences_run_check( const ofaIPreferences *instance, GtkWidget *page );

void       ofa_ipreferences_run_done ( const ofaIPreferences *instance, GtkWidget *page );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPREFERENCES_H__ */
