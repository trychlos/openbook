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

#ifndef __OPENBOOK_API_OFA_IPROPERTIES_H__
#define __OPENBOOK_API_OFA_IPROPERTIES_H__

/**
 * SECTION: iprefs
 * @title: ofaIProperties
 * @short_description: The ofaIProperties Interface
 * @include: openbook/ofa-iproperties.h
 *
 * The #ofaIProperties interface let plugins (and any tierce code)
 * display and manage the user preferences.
 *
 * This is a #GtkWidget built to be added as a new page of the
 * #GtkNotebok user preferences dialog. This object class must
 * implement this #ofaIProperties interface.
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IPROPERTIES                      ( ofa_iproperties_get_type())
#define OFA_IPROPERTIES( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPROPERTIES, ofaIProperties ))
#define OFA_IS_IPROPERTIES( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPROPERTIES ))
#define OFA_IPROPERTIES_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPROPERTIES, ofaIPropertiesInterface ))

typedef struct _ofaIProperties                     ofaIProperties;
typedef struct _ofaIPropertiesInterface            ofaIPropertiesInterface;

/**
 * ofaIPropertiesInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @init: [should] initialize the page.
 * @get_valid: [should] returns if the page is valid.
 * @apply: [should] write to the settings.
 *
 * This defines the interface that an #ofaIProperties should implement.
 */
struct _ofaIPropertiesInterface {
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
	 * init:
	 * @instance: the #ofaIProperties provider.
	 * @hub: the #ofaHub object of the application.
	 *
	 * Returns: the newly created GtkNotebook page.
	 *
	 * Since: version 1
	 */
	GtkWidget * ( *init )                 ( ofaIProperties *instance,
												ofaHub *hub );

	/**
	 * get_valid:
	 * @instance: the #ofaIProperties provider.
	 * @widget: the page as returned by init().
	 * @msgerr: [allow-none][out]: an error message placeholder.
	 *
	 * Checks for the Preferences dialog.
	 *
	 * Returns: %TRUE if the page doesn't contain any error, and is
	 * validable.
	 *
	 * Since: version 1
	 */
	gboolean    ( *get_valid )            ( const ofaIProperties *instance,
												GtkWidget *page,
												gchar **msgerr );

	/**
	 * apply:
	 * @instance: the #ofaIProperties provider.
	 *
	 * Terminate the Preferences dialog, writing the user preferences
	 * to the same settings file used at #init() time.
	 *
	 * Since: version 1
	 */
	void        ( *apply )                ( const ofaIProperties *instance,
												GtkWidget *page );
};

/*
 * Interface-wide
 */
GType      ofa_iproperties_get_type                  ( void );

guint      ofa_iproperties_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      ofa_iproperties_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkWidget *ofa_iproperties_init                      ( ofaIProperties *instance,
															ofaHub *hub );

gboolean   ofa_iproperties_get_valid                 ( GtkWidget *widget,
															gchar **msgerr );

void       ofa_iproperties_apply                     ( GtkWidget *widget );

gchar     *ofa_iproperties_get_title                 ( const ofaIProperties *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPROPERTIES_H__ */
