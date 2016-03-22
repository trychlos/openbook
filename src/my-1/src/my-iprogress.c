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
 * my_iprogress_start_work:
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
my_iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	static const gchar *thisfn = "my_iprogress_start_work";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->start_work ){
		MY_IPROGRESS_GET_INTERFACE( instance )->start_work( instance, worker, widget );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'start_work()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_start_progress:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed to mark the start
 *  of the progress.
 * @with_bar: whether to create a progress bar on the right of the
 *  @widget.
 *
 * Display the @widget.
 * Maybe create a progress bar.
 *
 * Since: version 1.
 */
void
my_iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar )
{
	static const gchar *thisfn = "my_iprogress_start_progress";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->start_progress ){
		MY_IPROGRESS_GET_INTERFACE( instance )->start_progress( instance, worker, widget, with_bar );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'start_progress()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_pulse:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @count: the current counter.
 * @total: the expected total.
 *
 * Increments the progress bar.
 *
 * Since: version 1.
 */
void
my_iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	static const gchar *thisfn = "my_iprogress_pulse";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->pulse ){
		MY_IPROGRESS_GET_INTERFACE( instance )->pulse( instance, worker, count, total );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'pulse()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_set_row:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @widget: [allow-none]: a widget to be displayed on the latest row.
 *  of the work.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_set_row( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	static const gchar *thisfn = "my_iprogress_set_row";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->set_row ){
		MY_IPROGRESS_GET_INTERFACE( instance )->set_row( instance, worker, widget );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'set_row()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_set_ok:
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
my_iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count )
{
	static const gchar *thisfn = "my_iprogress_set_ok";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->set_ok ){
		MY_IPROGRESS_GET_INTERFACE( instance )->set_ok( instance, worker, widget, errs_count );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'set_ok()' method",
			thisfn, ( void * ) instance );
}

/**
 * my_iprogress_set_text:
 * @instance: the #myIProgress instance.
 * @worker: any worker.
 * @text: [allow-none]: a text to be displayed in a text view.
 *
 * Display the @widget.
 *
 * Since: version 1.
 */
void
my_iprogress_set_text( myIProgress *instance, const void *worker, const gchar *text )
{
	static const gchar *thisfn = "my_iprogress_set_text";

	g_return_if_fail( instance && MY_IS_IPROGRESS( instance ));
	g_return_if_fail( worker );

	if( MY_IPROGRESS_GET_INTERFACE( instance )->set_text ){
		MY_IPROGRESS_GET_INTERFACE( instance )->set_text( instance, worker, text );
		return;
	}

	g_info( "%s: myIProgress instance %p does not provide 'set_text()' method",
			thisfn, ( void * ) instance );
}
