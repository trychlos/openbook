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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-iprefs-provider.h"

#define IPREFS_PROVIDER_LAST_VERSION    1

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIPrefsProviderInterface *klass );
static void  interface_base_finalize( ofaIPrefsProviderInterface *klass );

/**
 * ofa_iprefs_provider_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iprefs_provider_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iprefs_provider_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iprefs_provider_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPrefsProviderInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPrefsProvider", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPrefsProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_provider_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/*klass->get_interface_version = ipreferences_get_interface_version;*/
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPrefsProviderInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_provider_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 *
 */
guint
ofa_iprefs_provider_get_interface_last_version( void )
{
	return( IPREFS_PROVIDER_LAST_VERSION );
}

/**
 * ofa_iprefs_provider_do_init:
 * @importer: this #ofaIPrefsProvider instance.
 * @label: the label to be set on the notebook page
 *
 * Initialize the page to let the user configure his preferences.
 */
GtkWidget *
ofa_iprefs_provider_do_init( const ofaIPrefsProvider *instance, gchar **label )
{
	static const gchar *thisfn = "ofa_iprefs_provider_do_init";
	GtkWidget *page;

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PROVIDER( instance ), NULL );

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	page = NULL;

	if( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_init ){
		page = OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_init( instance, label );
	}

	return( page );
}

/**
 * ofa_iprefs_provider_do_check:
 * @importer: this #ofaIPrefsProvider instance.
 * @page: the preferences page.
 * @message: [allow-none]: a message to be returned.
 *
 * Check that the page is valid.
 */
gboolean
ofa_iprefs_provider_do_check( const ofaIPrefsProvider *instance, GtkWidget *page, gchar **message )
{
	static const gchar *thisfn = "ofa_iprefs_provider_do_check";
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PROVIDER( instance ), FALSE );
	g_return_val_if_fail( page && GTK_IS_WIDGET( page ), FALSE );

	ok = FALSE;

	g_debug( "%s: instance=%p (%s), page=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) page );

	if( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_check ){
		ok = OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_check( instance, page, message );
	}

	return( ok );
}

/**
 * ofa_iprefs_provider_do_apply:
 * @importer: this #ofaIPrefsProvider instance.
 *
 * Saves the user preferences.
 */
void
ofa_iprefs_provider_do_apply( const ofaIPrefsProvider *instance, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_iprefs_provider_do_apply";

	g_return_if_fail( instance && OFA_IS_IPREFS_PROVIDER( instance ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	g_debug( "%s: instance=%p (%s), page=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) page );

	if( OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_apply ){
		OFA_IPREFS_PROVIDER_GET_INTERFACE( instance )->do_apply( instance, page );
	}
}
