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

#ifndef __OPENBOOK_API_OFA_IABOUT_H__
#define __OPENBOOK_API_OFA_IABOUT_H__

/**
 * SECTION: iabout
 * @title: ofaIAbout
 * @short_description: The IAbout Interface
 * @include: openbook/ofa-iabout.h
 *
 * The #ofaIAbout interface let a plugin advertize about its properties.
 */

#include <gtk/gtk.h>

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
 * The DBMS backend presents two sets of functions:
 * - a first one which addresses the DB server itself,
 * - the second one which manages the inside dossier through the opened
 *   DB server connexion.
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
	 * Since: version 1
	 */
	guint       ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * do_init:
	 * @instance: the #ofaIAbout provider.
	 *
	 * Initialize the dialog.
	 *
	 * The IAbout provider may use the ofa_settings_xxx_() API to
	 * get its value from the user's configuration file.
	 *
	 * Returns: a newly created page which will be displayed by the
	 * program.
	 *
	 * Since: version 1
	 */
	GtkWidget * ( *do_init )             ( const ofaIAbout *instance );
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
GtkWidget *ofa_iabout_do_init                   ( const ofaIAbout *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IABOUT_H__ */
