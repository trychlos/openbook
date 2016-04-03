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

#ifndef __OPENBOOK_API_OFA_IPREFS_PROVIDER_H__
#define __OPENBOOK_API_OFA_IPREFS_PROVIDER_H__

/**
 * SECTION: iprefs
 * @title: ofaIPrefsProvider
 * @short_description: The ofaIPrefs Interface
 * @include: openbook/ofa-iprefs-provider.h
 *
 * The #ofaIPrefsxxx interfaces serie let plugins (and any tierce code)
 * display and manage the user preferences.
 *
 * This #ofaIPrefsProvider is dedicated to instance management.
 */

#include <gtk/gtk.h>

#include "ofa-iprefs-page-def.h"
#include "ofa-iprefs-provider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IPREFS_PROVIDER                      ( ofa_iprefs_provider_get_type())
#define OFA_IPREFS_PROVIDER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPREFS_PROVIDER, ofaIPrefsProvider ))
#define OFA_IS_IPREFS_PROVIDER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPREFS_PROVIDER ))
#define OFA_IPREFS_PROVIDER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPREFS_PROVIDER, ofaIPrefsProviderInterface ))

#if 0
typedef struct _ofaIPrefsProvider                     ofaIPrefsProvider;
typedef struct _ofaIPrefsProviderInterface            ofaIPrefsProviderInterface;
#endif

/**
 * ofaIPrefsProviderInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @new_page: [should] returns a new user preferences page.
 *
 * This defines the interface that an #ofaIPrefsProvider should implement.
 */
struct _ofaIPrefsProviderInterface {
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
	 */
	guint           ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * new_page:
	 *
	 * Returns: a new page to be added to the #GtkNotebook user
	 * preferences dialog. The page must be a #GtkContainer, and
	 * be returned as a #GtkWidget. The object class must implement
	 * the #ofaIPrefsPage interface.
	 *
	 * Since: version 1
	 */
	ofaIPrefsPage * ( *new_page )             ( void );
};

/*
 * Interface-wide
 */
GType          ofa_iprefs_provider_get_type                  ( void );

guint          ofa_iprefs_provider_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint          ofa_iprefs_provider_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIPrefsPage *ofa_iprefs_provider_new_page                  ( ofaIPrefsProvider *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPREFS_PROVIDER_H__ */
