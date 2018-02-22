/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "my/my-utils.h"

#include "api/ofa-iprintable.h"

/* data associated to each implementor object
 */
typedef struct {
	GtkPaperSize      *paper_size;
	GtkPageOrientation page_orientation;
	GtkPrintOperation *print;
	GKeyFile          *keyfile;
	gchar             *group_name;
}
	sIPrintable;

#define IPRINTABLE_LAST_VERSION         1
#define IPRINTABLE_DATA                 "ofa-iprintable-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIPrintableInterface *klass );
static void         interface_base_finalize( ofaIPrintableInterface *klass );
static sIPrintable *get_iprintable_data( ofaIPrintable *instance );
static void         on_instance_finalized( sIPrintable *sdata, void *instance );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance );
static void         iprintable_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable *instance );
static void         iprintable_draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance );
static void         iprintable_end_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static gboolean     do_print( ofaIPrintable *instance, sIPrintable *sdata );
static gboolean     load_settings( ofaIPrintable *instance, sIPrintable *sdata );
static void         save_settings( ofaIPrintable *instance, sIPrintable *sdata );

/**
 * ofa_iprintable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iprintable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iprintable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iprintable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPrintableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPrintable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPrintableInterface *klass )
{
	static const gchar *thisfn = "ofa_iprintable_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->begin_print = iprintable_begin_print;
		klass->draw_page = iprintable_draw_page;
		klass->end_print = iprintable_end_print;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPrintableInterface *klass )
{
	static const gchar *thisfn = "ofa_iprintable_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iprintable_get_interface_last_version:
 * @instance: this #ofaIPrintable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iprintable_get_interface_last_version( const ofaIPrintable *instance )
{
	return( IPRINTABLE_LAST_VERSION );
}

/*
 * Returns: the data associated with this IPrintable instance,
 * allocating a new structure if needed.
 *
 * On allocation, the paper characteristics (name and properties) are
 * set.
 */
static sIPrintable *
get_iprintable_data( ofaIPrintable *instance )
{
	sIPrintable *sdata;
	const gchar *paper_name;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIPrintable, 1 );
		g_object_set_data( G_OBJECT( instance ), IPRINTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		/* as we provide default values, we are sure that these two methods
		 * are implemented */
		paper_name = OFA_IPRINTABLE_GET_INTERFACE( instance )->get_paper_name( instance );
		sdata->paper_size = gtk_paper_size_new( paper_name );
		sdata->page_orientation = OFA_IPRINTABLE_GET_INTERFACE( instance )->get_page_orientation( instance );
	}

	return( sdata );
}

static void
on_instance_finalized( sIPrintable *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_iprintable_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_object_set_data( G_OBJECT( instance ), IPRINTABLE_DATA, NULL );
	gtk_paper_size_free( sdata->paper_size );
	g_clear_object( &sdata->print );
	g_free( sdata->group_name );
	g_free( sdata );
}

/*
 * signal handler
 * - compute the max ordonate in the page
 * - create a new dedicated layout which will be used during drawing operation
 */
static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance )
{
	OFA_IPRINTABLE_GET_INTERFACE( instance )->begin_print( instance, operation, context );
}

/*
 * default interface handler
 */
static void
iprintable_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_iprintable_begin_print";

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );
}

/*
 * GtkPrintOperation signal handler
 * call once per page, with a page_num counted from zero
 */
static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable *instance )
{
	OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page( instance, operation, context, page_num );
}

/*
 * default interface handler
 */
static void
iprintable_draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	static const gchar *thisfn = "ofa_iprintable_draw_page";

	g_debug( "%s: instance=%p, operation=%p, context=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context, page_num );
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance )
{
	OFA_IPRINTABLE_GET_INTERFACE( instance )->end_print( instance, operation, context );
}

static void
iprintable_end_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_iprintable_end_print";

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );
}

/**
 * ofa_iprintable_print:
 * @instance:
 *
 * Print.
 *
 * Heavily relies on preparations which were made while preview.
 */
gboolean
ofa_iprintable_print( ofaIPrintable *instance )
{
	sIPrintable *sdata;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IPRINTABLE( instance ), FALSE );

	sdata = get_iprintable_data( instance );

	ok = do_print( instance, sdata );

	return( ok );
}

static gboolean
do_print( ofaIPrintable *instance, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable_do_print";
	gboolean printed;
	GtkPrintOperationResult res;
	GError *error;
	GtkPageSetup *page_setup;
	gchar *str;
	GtkMessageType type;

	printed = FALSE;
	error = NULL;
	sdata->print = gtk_print_operation_new ();

	/* unit_points gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( sdata->print, GTK_UNIT_POINTS );

	g_signal_connect( sdata->print, "begin-print", G_CALLBACK( on_begin_print ), instance );
	g_signal_connect( sdata->print, "draw-page", G_CALLBACK( on_draw_page ), instance );
	g_signal_connect( sdata->print, "end-print", G_CALLBACK( on_end_print ), instance );

	load_settings( instance, sdata );

	page_setup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( page_setup, sdata->paper_size );
	gtk_page_setup_set_orientation( page_setup, sdata->page_orientation );
	gtk_print_operation_set_default_page_setup( sdata->print, page_setup );
	g_object_unref( page_setup );

	res = gtk_print_operation_run(
				sdata->print,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	            NULL,
	            &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing the document:\n%s" ), error->message );
		type = GTK_MESSAGE_WARNING;
		g_error_free( error );

	} else if( res != GTK_PRINT_OPERATION_RESULT_CANCEL ){
		printed = TRUE;
		str = g_strdup( _( "The document has been successfully printed" ));
		type = GTK_MESSAGE_INFO;
		save_settings( instance, sdata );

	} else {
		str = NULL;
	}

	if( str ){
		my_utils_msg_dialog( NULL, type, str );
		g_free( str );
	}

	g_debug( "%s: printed=%s", thisfn, printed ? "True":"False" );

	return( printed );
}

/*
 * load_settings:
 * @instance:
 * @sdata:
 *
 * Note that print settings do not include page setup
 */
static gboolean
load_settings( ofaIPrintable *instance, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable_load_settings";
	GtkPrintSettings *settings;
	GError *error;
	gboolean ok;

	ok = FALSE;

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_print_settings ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->get_print_settings( instance, &sdata->keyfile, &sdata->group_name );
	}
	g_debug( "%s: group_name=%s", thisfn, sdata->group_name );

	if( sdata->keyfile && my_strlen( sdata->group_name )){
		settings = gtk_print_settings_new();
		error = NULL;
		if( !gtk_print_settings_load_key_file( settings, sdata->keyfile, sdata->group_name, &error )){
			if( error->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND ){
				my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, error->message );
			}
			g_error_free( error );
		} else {
			gtk_print_operation_set_print_settings( sdata->print, settings );
			ok = TRUE;
		}
		g_object_unref( settings );
	}

	return( ok );
}

/*
 * save_settings:
 * @instance:
 * @sdata:
 */
static void
save_settings( ofaIPrintable *instance, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable_save_settings";
	GtkPrintSettings *settings;

	settings = gtk_print_operation_get_print_settings( sdata->print );
	gtk_print_settings_to_key_file( settings, sdata->keyfile, sdata->group_name );

	g_debug( "%s: group_name=%s", thisfn, sdata->group_name );
}
