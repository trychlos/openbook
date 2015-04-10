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

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "api/my-utils.h"

#include "ui/ofa-iprintable2.h"

/* data associated to each implementor object
 */
typedef struct {
	GtkPaperSize      *paper_size;
	GtkPageOrientation page_orientation;
}
	sIPrintable;

#define IPRINTABLE2_LAST_VERSION         1
#define IPRINTABLE2_DATA                 "ofa-iprintable2-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIPrintable2Interface *klass );
static void         interface_base_finalize( ofaIPrintable2Interface *klass );
static sIPrintable *get_iprintable_data( ofaIPrintable2 *instance );
static void         on_instance_finalized( sIPrintable *sdata, void *instance );
static gboolean     do_preview( ofaIPrintable2 *instance, const gchar *filename, sIPrintable *sdata );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance );
static void         iprintable2_begin_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gboolean     on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance );
static gboolean     iprintable2_paginate( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable2 *instance );
static void         iprintable2_draw_page( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance );
static void         iprintable2_end_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gboolean     do_print( ofaIPrintable2 *instance, sIPrintable *sdata );

/**
 * ofa_iprintable2_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iprintable2_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iprintable2_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iprintable2_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPrintable2Interface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPrintable2", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPrintable2Interface *klass )
{
	static const gchar *thisfn = "ofa_iprintable2_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->begin_print = iprintable2_begin_print;
		klass->paginate = iprintable2_paginate;
		klass->draw_page = iprintable2_draw_page;
		klass->end_print = iprintable2_end_print;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPrintable2Interface *klass )
{
	static const gchar *thisfn = "ofa_iprintable2_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iprintable2_get_interface_last_version:
 * @instance: this #ofaIPrintable2 instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iprintable2_get_interface_last_version( const ofaIPrintable2 *instance )
{
	return( IPRINTABLE2_LAST_VERSION );
}

/**
 * ofa_iprintable2_print:
 * @instance: this #ofaIPrintable2 instance.
 *
 * Preview.
 *
 * in Openbook, ofa_iprintable2_preview() is always called before
 * ofa_iprintable2_print(). Preparation which is made here (e.g. page
 * setup) is not re-made when actually printing.
 */
gboolean
ofa_iprintable2_preview( ofaIPrintable2 *instance )
{
	static const gchar *thisfn = "ofa_iprintable2_preview";
	static const gchar *sufix = ".pdf";
	sIPrintable *sdata;
	gboolean ok;
	gchar *str, *template;
	gint fd;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IPRINTABLE2( instance ), FALSE );

	sdata = get_iprintable_data( instance );
	fd = -1;

	/* on preview mode, create a temporary pdf file
	 * do not know why, but its cairo context is much more clean than
	 * those we are wreating ourselves */
	str = g_strdup_printf( "openbook_XXXXXX%s", sufix );
	template = g_build_filename( g_get_tmp_dir(), str, NULL );
	g_free( str );
	fd = mkstemps( template, my_strlen( sufix ));
	if( fd == -1 ){
		str = g_strdup_printf( "%s", strerror( errno ));
		my_utils_dialog_warning( str );
		g_free( str );
		return( FALSE );
	}
	close( fd );

	ok = do_preview( instance, template, sdata );

	unlink( template );
	g_free( template );

	return( ok );
}

/*
 * Returns: the data associated with this IPrintable instance,
 * allocating a new structure if needed.
 *
 * On allocation, the paper characteristics (name and properties) are
 * set.
 */
static sIPrintable *
get_iprintable_data( ofaIPrintable2 *instance )
{
	sIPrintable *sdata;
	const gchar *paper_name;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE2_DATA );

	if( !sdata ){
		sdata = g_new0( sIPrintable, 1 );
		g_object_set_data( G_OBJECT( instance ), IPRINTABLE2_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		/* as we provide default values, we are sure that these two methods
		 * are implemented */
		paper_name = OFA_IPRINTABLE2_GET_INTERFACE( instance )->get_paper_name( instance );
		sdata->paper_size = gtk_paper_size_new( paper_name );
		sdata->page_orientation = OFA_IPRINTABLE2_GET_INTERFACE( instance )->get_page_orientation( instance );
	}

	return( sdata );
}

static void
on_instance_finalized( sIPrintable *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_iprintable2_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_object_set_data( G_OBJECT( instance ), IPRINTABLE2_DATA, NULL );
	gtk_paper_size_free( sdata->paper_size );
	g_free( sdata );
}

static gboolean
do_preview( ofaIPrintable2 *instance, const gchar *filename, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable2_do_preview";
	gboolean printed;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GError *error;
	GtkPageSetup *page_setup;
	gchar *str;

	printed = FALSE;
	error = NULL;
	print = gtk_print_operation_new ();

	/* unit_points gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( print, GTK_UNIT_POINTS );

	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), instance );
	g_signal_connect( print, "paginate", G_CALLBACK( on_paginate ), instance );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), instance );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), instance );

	page_setup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( page_setup, sdata->paper_size );
	gtk_page_setup_set_orientation( page_setup, sdata->page_orientation );
	gtk_print_operation_set_default_page_setup( print, page_setup );
	g_object_unref( page_setup );

	gtk_print_operation_set_export_filename( print, filename );

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_EXPORT,
	                NULL,
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		my_utils_dialog_warning( str );
		g_free( str );
		g_error_free( error );

	} else {
		printed = TRUE;
	}

	g_object_unref( print );
	g_debug( "%s: printed=%s", thisfn, printed ? "True":"False" );

	return( printed );
}

/*
 * signal handler
 * - compute the max ordonate in the page
 * - create a new dedicated layout which will be used during drawing operation
 */
static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance )
{
	OFA_IPRINTABLE2_GET_INTERFACE( instance )->begin_print( instance, operation, context );
}

/*
 * default interface handler
 */
static void
iprintable2_begin_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_iprintable2_begin_print";

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );
}

/*
 * GtkPrintOperation signal handler
 * repeatdly called while not %TRUE
 *
 * Returns: %TRUE at the end of the pagination
 *
 * We implement it so that it is called only once.
 */
static gboolean
on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance )
{
	return( OFA_IPRINTABLE2_GET_INTERFACE( instance )->paginate( instance, operation, context ));
}

/*
 * default interface handler.
 */
static gboolean
iprintable2_paginate( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_iprintable2_paginate";

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	return( TRUE );
}

/*
 * GtkPrintOperation signal handler
 * call once per page, with a page_num counted from zero
 */
static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable2 *instance )
{
	OFA_IPRINTABLE2_GET_INTERFACE( instance )->draw_page( instance, operation, context, page_num );
}

/*
 * default interface handler
 */
static void
iprintable2_draw_page( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	static const gchar *thisfn = "ofa_iprintable2_draw_page";

	g_debug( "%s: instance=%p, operation=%p, context=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context, page_num );
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable2 *instance )
{
	OFA_IPRINTABLE2_GET_INTERFACE( instance )->end_print( instance, operation, context );
}

static void
iprintable2_end_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_iprintable2_end_print";

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );
}

/**
 * ofa_iprintable2_print:
 * @instance:
 *
 * Print.
 *
 * Heavily relies on preparations which were made while preview.
 */
gboolean
ofa_iprintable2_print( ofaIPrintable2 *instance )
{
	sIPrintable *sdata;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IPRINTABLE2( instance ), FALSE );

	sdata = get_iprintable_data( instance );

	ok = do_print( instance, sdata );

	return( ok );
}

static gboolean
do_print( ofaIPrintable2 *instance, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable2_do_print";
	gboolean printed;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GError *error;
	GtkPageSetup *page_setup;
	gchar *str;

	printed = FALSE;
	error = NULL;
	print = gtk_print_operation_new ();

	/* unit_points gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( print, GTK_UNIT_POINTS );

	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), instance );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), instance );

	page_setup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( page_setup, sdata->paper_size );
	gtk_page_setup_set_orientation( page_setup, sdata->page_orientation );
	gtk_print_operation_set_default_page_setup( print, page_setup );
	g_object_unref( page_setup );

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                NULL,
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		my_utils_dialog_warning( str );
		g_free( str );
		g_error_free( error );

	} else {
		printed = TRUE;
	}

	g_object_unref( print );
	g_debug( "%s: printed=%s", thisfn, printed ? "True":"False" );

	return( printed );
}
