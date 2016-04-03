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

#ifndef __OPENBOOK_API_OFA_IPREFS_PAGE_H__
#define __OPENBOOK_API_OFA_IPREFS_PAGE_H__

/**
 * SECTION: iprefs
 * @title: ofaIPrefsPage
 * @short_description: The ofaIPrefs Interface
 * @include: openbook/ofa-iprefs-page.h
 *
 * The #ofaIPrefsxxx interfaces serie let plugins (and any tierce code)
 * display and manage the user preferences.
 *
 * This #ofaIPrefsPage manages the user preferences page.
 *
 * This is a #GtkWidget built to be added as a new page of the
 * #GtkNotebok user preferences dialog. This object class must
 * implement this #ofaIPrefsPage interface.
 */

#include <gtk/gtk.h>

#include "my/my-isettings.h"

#include "api/ofa-iprefs-page-def.h"
#include "api/ofa-iprefs-provider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IPREFS_PAGE                      ( ofa_iprefs_page_get_type())
#define OFA_IPREFS_PAGE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPREFS_PAGE, ofaIPrefsPage ))
#define OFA_IS_IPREFS_PAGE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPREFS_PAGE ))
#define OFA_IPREFS_PAGE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPREFS_PAGE, ofaIPrefsPageInterface ))

#if 0
typedef struct _ofaIPrefsPage                     ofaIPrefsPage;
typedef struct _ofaIPrefsPageInterface            ofaIPrefsPageInterface;
#endif

/**
 * ofaIPrefsPageInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @init: [should] initialize the page.
 * @get_valid: [should] returns if the page is valid.
 * @apply: [should] write to the settings.
 *
 * This defines the interface that an #ofaIPrefsPage should implement.
 */
struct _ofaIPrefsPageInterface {
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
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * init:
	 * @instance: the #ofaIPrefsPage provider.
	 * @settings: the #myISettings instance which manages the settings
	 *  file.
	 * @label: [allow-none][out]: the label to be set as the notebook
	 *  page tab title.
	 * @msgerr: [allow-none][out]: an error message to be returned.
	 *
	 * Initialize the user preferences dialog.
	 *
	 * Returns: %TRUE if the page has been successfully initialized,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *init )                 ( const ofaIPrefsPage *instance,
												myISettings *settings,
												gchar **label,
												gchar **msgerr );

	/**
	 * get_valid:
	 * @instance: the #ofaIPrefsPage provider.
	 * @msgerr: [allow-none][out]: an error message to be returned.
	 *
	 * Checks for the Preferences dialog.
	 *
	 * Returns: %TRUE if the page doesn't contain any error, and is
	 * validable.
	 *
	 * Since: version 1
	 */
	gboolean ( *get_valid )            ( const ofaIPrefsPage *instance,
													gchar **msgerr );

	/**
	 * apply:
	 * @instance: the #ofaIPrefsPage provider.
	 * @msgerr: [allow-none][out]: an error message to be returned.
	 *
	 * Terminate the Preferences dialog, writing the user preferences
	 * to the same settings file used at #init() time.
	 *
	 * Returns: %TRUE if the updates have been successfully applied,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *apply )                ( const ofaIPrefsPage *instance,
													gchar **msgerr );
};

/*
 * Interface-wide
 */
GType              ofa_iprefs_page_get_type                  ( void );

guint              ofa_iprefs_page_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint              ofa_iprefs_page_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIPrefsProvider *ofa_iprefs_page_get_provider              ( const ofaIPrefsPage *instance );

void               ofa_iprefs_page_set_provider              ( ofaIPrefsPage *instance,
																	ofaIPrefsProvider *provider );

gboolean           ofa_iprefs_page_init                      ( const ofaIPrefsPage *instance,
																	myISettings *settings,
																	gchar **label,
																	gchar **msgerr );

gboolean           ofa_iprefs_page_get_valid                 ( const ofaIPrefsPage *instance,
																	gchar **msgerr );

gboolean           ofa_iprefs_page_apply                     ( const ofaIPrefsPage *instance,
																	gchar **msgerr);

gchar             *ofa_iprefs_page_get_display_name          ( const ofaIPrefsPage *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPREFS_PAGE_H__ */
