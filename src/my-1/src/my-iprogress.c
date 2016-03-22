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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-iprogress.h"

#define IPROGRESS_LAST_VERSION            1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myIProgressInterface *klass );
static void  interface_base_finalize( myIProgressInterface *klass );

/**
 * my_iprogress_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iprogress_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iprogress_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iprogress_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIProgressInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myIProgress", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIProgressInterface *klass )
{
	static const gchar *thisfn = "my_iprogress_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIProgressInterface *klass )
{
	static const gchar *thisfn = "my_iprogress_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iprogress_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_iprogress_get_interface_last_version( void )
{
	return( IPROGRESS_LAST_VERSION );
}

/**
 * my_iprogress_get_interface_version:
 * @instance: this #myIProgress instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
my_iprogress_get_interface_version( const myIProgress *instance )
{
	static const gchar *thisfn = "my_iprogress_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IPROGRESS( instance ), 0 );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->get_interface_version ){
		return( MY_IPROGRESS_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: myIProgress instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * my_iprogress_work_start:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed to mark the start
 *  of the work.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_work_start( myIProgress *instance, void *worker, GtkWidget *widget )
{
	static const gchar *thisfn = "my_iprogress_work_start";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->work_start ){
		MY_IPROGRESS_GET_INTERFACE( instance )->work_start( instance, worker, widget );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'work_start()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_work_end:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed to mark the end
 *  of the work.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_work_end( myIProgress *instance, void *worker, GtkWidget *widget )
{
	static const gchar *thisfn = "my_iprogress_work_end";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->work_end ){
		MY_IPROGRESS_GET_INTERFACE( instance )->work_end( instance, worker, widget );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'work_end()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_progress_start:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed to mark the start
 *  of the progress.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_progress_start( myIProgress *instance, void *worker, GtkWidget *widget )
{
	static const gchar *thisfn = "my_iprogress_progress_start";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->progress_start ){
		MY_IPROGRESS_GET_INTERFACE( instance )->progress_start( instance, worker, widget );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'progress_start()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_progress_pulse:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @progress: the progress, from 0 to 1.
 * @text: [allow-none]: a text to be set with the progress.
 *
 * Increments the progress bar.
 *
 * Since: version 1.
 */
void
my_iprogress_progress_pulse( myIProgress *instance, void *worker, gdouble progress, const gchar *text )
{
	static const gchar *thisfn = "my_iprogress_progress_pulse";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->progress_pulse ){
		MY_IPROGRESS_GET_INTERFACE( instance )->progress_pulse( instance, worker, progress, text );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'progress_pulse()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_progress_end:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed to mark the start
 *  of the work.
 * @errs_count: the count of errors.
 *
 * Display the @widget.
 * Display a 'OK'/'NOT OK' label.
 *
 * Since: version 1.
 */
void
my_iprogress_progress_end( myIProgress *instance, void *worker, GtkWidget *widget, gulong errs_count )
{
	static const gchar *thisfn = "my_iprogress_progress_end";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->progress_end ){
		MY_IPROGRESS_GET_INTERFACE( instance )->progress_end( instance, worker, widget, errs_count );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'progress_end()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_text:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @text: [allow-none]: a text to be displayed in a text view.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_text( myIProgress *instance, void *worker, const gchar *text )
{
	static const gchar *thisfn = "my_iprogress_text";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->text ){
		MY_IPROGRESS_GET_INTERFACE( instance )->text( instance, worker, text );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'text()' method",
			thisfn, ( void * ) instance );
}
