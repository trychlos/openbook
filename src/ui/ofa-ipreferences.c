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
 * $Id: ofa-ipreferences.c 3639 2014-06-05 22:27:39Z  $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <api/ofa-ipreferences.h>

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIPreferencesInterface *klass );
static void  interface_base_finalize( ofaIPreferencesInterface *klass );
static guint ipreferences_get_interface_version( const ofaIPreferences *instance );

/**
 * ofa_ipreferences_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ipreferences_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ipreferences_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ipreferences_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPreferencesInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPreferences", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPreferencesInterface *klass )
{
	static const gchar *thisfn = "ofa_ipreferences_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->get_interface_version = ipreferences_get_interface_version;
		klass->run_init = NULL;
		klass->run_check = NULL;
		klass->run_done = NULL;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPreferencesInterface *klass )
{
	static const gchar *thisfn = "ofa_ipreferences_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

static guint
ipreferences_get_interface_version( const ofaIPreferences *instance )
{
	return( 1 );
}

/**
 * ofa_ipreferences_run_init:
 * @importer: this #ofaIPreferences instance.
 *
 * Run the dialog to let the user configure his preferences.
 */
GtkWidget *
ofa_ipreferences_run_init( const ofaIPreferences *instance, GtkNotebook *book )
{
	static const gchar *thisfn = "ofa_ipreferences_run_init";
	GtkWidget *page;

	g_return_val_if_fail( instance && OFA_IS_IPREFERENCES( instance ), NULL );

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	page = NULL;

	if( OFA_IPREFERENCES_GET_INTERFACE( instance )->run_init ){
		page = OFA_IPREFERENCES_GET_INTERFACE( instance )->run_init( instance, book );
	}

	return( page );
}

/**
 * ofa_ipreferences_check:
 * @importer: this #ofaIPreferences instance.
 *
 * Run the dialog to let the user configure his preferences.
 */
gboolean
ofa_ipreferences_run_check( const ofaIPreferences *instance, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_ipreferences_run_check";
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IPREFERENCES( instance ), FALSE );

	ok = FALSE;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( OFA_IPREFERENCES_GET_INTERFACE( instance )->run_check ){
		ok = OFA_IPREFERENCES_GET_INTERFACE( instance )->run_check( instance, page );
	}

	return( ok );
}

/**
 * ofa_ipreferences_run_done:
 * @importer: this #ofaIPreferences instance.
 *
 * Run the dialog to let the user configure his preferences.
 */
void
ofa_ipreferences_run_done( const ofaIPreferences *instance, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_ipreferences_run_done";

	g_return_if_fail( instance && OFA_IS_IPREFERENCES( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( OFA_IPREFERENCES_GET_INTERFACE( instance )->run_done ){
		OFA_IPREFERENCES_GET_INTERFACE( instance )->run_done( instance, page );
	}
}
