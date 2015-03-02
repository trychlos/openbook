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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/my-window.h"
#include "core/my-window-prot.h"

#include "ui/ofa-iprintable.h"
#include "ui/ofa-main-window.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	const gchar       *paper_size;
	GtkPageOrientation paper_orientation;
	gint               default_font_size;
	gboolean           group_on_new_page;

	/* runtime data
	 */
	gchar             *filename;
	gint               current_font_size;
	PangoLayout       *layout;
	GList             *dataset;
	gint               pages_count;
	gdouble            max_y;
	gdouble            last_y;
	GList             *last_printed;
	gint               count;			/* count of printed lines */

	/* groups management
	 * @have_groups: is initialized to %FALSE on initialization,
	 *  then set to %TRUE during the pagination phase, when the first
	 *  line makes the implementation detect a new group (so tell us
	 *  that it actually manages groups)
	 * @group_footer_printed: is reset to %TRUE in #reset_runtime()
	 *  so that we do not try to draw it if the implementation does
	 *  not manage groups
	 */
	gboolean           have_groups;
	gboolean           group_footer_printed;
}
	sIPrintable;

#define IPRINTABLE_LAST_VERSION         1
#define IPRINTABLE_DATA                 "ofa-iprintable-data"

#define COLOR_BLACK                     0,      0,      0
#define COLOR_WHITE                     1,      1,      1
#define COLOR_DARK_RED                  0.5,    0,      0
#define COLOR_DARK_CYAN                 0,      0.5,    0.5
#define COLOR_GRAY                      0.6,    0.6,    0.6			/* #999999 */
#define COLOR_MIDDLE_GRAY               0.7688, 0.7688, 0.7688		/* #c4c4c4 */
#define COLOR_LIGHT_GRAY                0.9375, 0.9375, 0.9375		/* #f0f0f0 */

#define COLOR_HEADER_DOSSIER            COLOR_DARK_RED
#define COLOR_HEADER_TITLE              COLOR_DARK_CYAN
#define COLOR_FOOTER                    COLOR_GRAY
#define COLOR_NO_DATA                   COLOR_MIDDLE_GRAY

/* These are parms which describe the page layout
 *
 * Page setup: A4 portrait
 *   Unit=none
 *   context_width =559,275591 pixels -> 559.2 pix.
 *   context_height=783,569764 pixels -> 783.5 pix.
 *   DPI x,y = 72,72
 *
 *   Hard margins left=0,0, top=0,0, right=0,0, bottom=0,0
 *   thus these are outside of the print context.
 *
 * Font sizes
 * We make use of the following font size:
 * - standard body font size bfs (e.g. bfs=9)
 *   rather choose bfs=9 for landscape prints, bfs=8 for portrait
 *
 * In order to take into account ascending and descending letters, we
 * have to reserve about 1/4 of the font size above and below each line.
 * So the spacing between lines should be about 1/2 bfs.
 *
 * Printing a text on Cairo:
 * For a font size of 12, we get a height of 32 pix.
 * The text is drawn with about 5 pix free above, the descendant letters
 * go about 5 pix below the height - so the full height is about 32 pix,
 * though a bit shifted from 5 pix (about 1/6).
 * The minimal vspace between lines should of about 9 pix, so about 1/2
 * of body font size.
 *
 * Drawing a rubber:
 * Add about 1/3 above the text, 2/3 below, so that the string is
 * vertically centered.
 */

#define             st_default_paper_size                    GTK_PAPER_NAME_A4
#define             st_default_paper_orientation             GTK_PAGE_ORIENTATION_PORTRAIT
static const gint   st_default_body_font_size                = 8;

static const gchar *st_font_family                           = "Sans";
#define             st_line_vspace                           ( gdouble ) sdata->current_font_size/2
#define             st_line_height                           ( gdouble ) sdata->current_font_size*1.5

static const gint   st_page_header_dossier_name_font_size    = 14;
#define             st_page_header_dossier_vspace_after      st_line_vspace
static const gint   st_page_header_title_font_size           = 14;
#define             st_page_header_title_vspace_after        ( gdouble ) 3
static const gint   st_page_header_subtitle_font_size        = 10;
#define             st_page_header_subtitle_vspace_after     1.25*st_line_vspace
#define             st_page_header_columns_vspace_after      1.25*st_line_vspace
static const gint   st_footer_font_size                      = 7;
#define             st_footer_vspace_before                  ( gdouble ) 2
#define             st_footer_before_line_vspace             ( gdouble ) 2
#define             st_footer_after_line_vspace              ( gdouble ) 1
#define             st_page_margin                           ( gdouble ) 2
static const gint   st_no_data_font_size                     = 20;

static guint st_initializations = 0;	/* interface initialization count */

static GType              register_type( void );
static void               interface_base_init( ofaIPrintableInterface *klass );
static void               interface_base_finalize( ofaIPrintableInterface *klass );
static void               success_printing( const ofaIPrintable *instance, sIPrintable *sdata );
static gboolean           do_operate( ofaIPrintable *instance, sIPrintable *sdata );
static void               on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance );
static gboolean           on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance );
static void               reset_runtime( ofaIPrintable *instance );
static void               on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable *instance );
static gboolean           draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static void               v_draw_page_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void               iprintable_draw_page_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void               draw_page_header_dossier( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static void               draw_page_header_title( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static gchar             *v_get_page_header_title( const ofaIPrintable *instance );
static gchar             *iprintable_get_page_header_title( const ofaIPrintable *instance );
static void               draw_page_header_subtitle( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static gchar             *v_get_page_header_subtitle( const ofaIPrintable *instance );
static gchar             *iprintable_get_page_header_subtitle( const ofaIPrintable *instance );
static void               draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static gdouble            get_page_header_columns_height( const ofaIPrintable *instance, sIPrintable *sdata );
static void               draw_page_header_test_fonts( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata );
static void               draw_top_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata );
static gboolean           v_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev );
static void               draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint line_num, GList *line, sIPrintable *sdata );
static gdouble            get_group_header_height( const ofaIPrintable *instance, gint line_num, GList *line, sIPrintable *sdata );
static void               draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata );
static gboolean           draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, GList *line, GList *next, sIPrintable *sdata );
static void               draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata );
static gdouble            get_group_bottom_report_height( const ofaIPrintable *instance, sIPrintable *sdata );
static void               draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint line_num, sIPrintable *sdata );
static gdouble            get_group_footer_height( const ofaIPrintable *instance, sIPrintable *sdata );
static void               draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata );
static gdouble            get_bottom_summary_height( ofaIPrintable *instance, sIPrintable *sdata );
static void               v_draw_page_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void               iprintable_draw_page_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static gdouble            get_page_footer_height( const ofaIPrintable *instance );
static void               on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance );
static const gchar       *v_get_success_msg( const ofaIPrintable *instance );

static void
on_instance_weak_notify( sIPrintable *sdata, myDialog *finalized_dialog )
{
	static const gchar *thisfn = "ofa_iprintable_on_instance_weak_notify";

	g_debug( "%s: sdata=%p, finalized_dialog=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_dialog );

	g_free( sdata->filename );
	g_clear_object( &sdata->layout );
	g_free( sdata );
}

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

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;

	if( st_initializations == 1 ){

		klass->draw_page_header = iprintable_draw_page_header;
		klass->get_page_header_title = iprintable_get_page_header_title;
		klass->get_page_header_subtitle = iprintable_get_page_header_subtitle;
		klass->draw_page_footer = iprintable_draw_page_footer;
	}
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

/**
 * ofa_iprintable_init:
 * @instance: this #ofaIPrintable instance.
 *
 * Initialize a structure attached to the implementor #GObject.
 * This must be done as soon as possible in order to let the
 * implementation setup its own defaults before printing.
 */
void
ofa_iprintable_init( ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_if_fail( G_IS_OBJECT( instance ));
	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));

	sdata = g_new0( sIPrintable, 1 );

	g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_weak_notify, sdata );
	g_object_set_data( G_OBJECT( instance ), IPRINTABLE_DATA, sdata );

	sdata->group_on_new_page = FALSE;
	sdata->have_groups = FALSE;

	ofa_iprintable_set_paper_size( instance, st_default_paper_size );
	ofa_iprintable_set_paper_orientation( instance, st_default_paper_orientation );
	ofa_iprintable_set_default_font_size( instance, st_default_body_font_size );
}

/**
 * ofa_iprintable_set_paper_size:
 * @instance: this #ofaIPrintable instance.
 * @size: the size of the paper sheet
 *
 * Set the page size.
 *
 * Defaults to GTK_PAPER_NAME_A4.
 */
void
ofa_iprintable_set_paper_size( ofaIPrintable *instance, const gchar *size )
{
	sIPrintable *sdata;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->paper_size = size;

}

/**
 * ofa_iprintable_set_paper_orientation:
 * @instance: this #ofaIPrintable instance.
 * @orientation: the orientation of the paper sheet
 *
 * Set the page orientation.
 *
 * Defaults to GTK_PAGE_ORIENTATION_PORTRAIT.
 */
void
ofa_iprintable_set_paper_orientation( ofaIPrintable *instance, GtkPageOrientation orientation )
{
	sIPrintable *sdata;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->paper_orientation = orientation;

}

/**
 * ofa_iprintable_print_to_pdf:
 * @instance: this #ofaIPrintable instance.
 * @filename: the output PDF report.
 */
gboolean
ofa_iprintable_print_to_pdf( ofaIPrintable *instance, const gchar *filename )
{
	sIPrintable *sdata;
	gboolean ok;

	g_return_val_if_fail( OFA_IS_IPRINTABLE( instance ), FALSE );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, FALSE );

	sdata->filename = g_strdup( filename );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_dataset ){
		sdata->dataset = OFA_IPRINTABLE_GET_INTERFACE( instance )->get_dataset( instance );
	} else {
		my_utils_dialog_error( _( "get_dataset() virtual not implemented, but is mandatory" ));
		return( FALSE );
	}

	ok = do_operate( instance, sdata );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->free_dataset ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->free_dataset( sdata->dataset );
	}

	return( ok );
}

/**
 * ofa_iprintable_set_group_on_new_page:
 */
void
ofa_iprintable_set_group_on_new_page( const ofaIPrintable *instance, gboolean new_page )
{
	sIPrintable *sdata;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->group_on_new_page = new_page;
}

static void
success_printing( const ofaIPrintable *instance, sIPrintable *sdata )
{
	GtkWidget *dialog;
	gchar *str;

	if( sdata->pages_count <= 1 ){
		str = g_strdup_printf(
				_( "%s\n(%u printed page)" ),
				v_get_success_msg( instance ), sdata->pages_count );
	} else {
		str = g_strdup_printf(
				_( "%s\n(%u printed pages)" ),
				v_get_success_msg( instance ), sdata->pages_count );
	}

	dialog = gtk_message_dialog_new(
						my_window_get_toplevel( MY_WINDOW( instance )),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_INFO,
						GTK_BUTTONS_CLOSE,
						"%s", str );
	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static gboolean
do_operate( ofaIPrintable *instance, sIPrintable *sdata )
{
	static const gchar *thisfn = "ofa_iprintable_do_operate";
	gboolean printed;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GError *error;
	GtkPageSetup *psetup;
	GtkPaperSize *psize;
	gchar *str;

	printed = FALSE;
	error = NULL;
	print = gtk_print_operation_new ();

	/* Sets up the transformation for the cairo context obtained from
	 * GtkPrintContext in such a way that distances are measured in mm */
	/*gtk_print_operation_set_unit( print, GTK_UNIT_MM );
	 * setting the unit in mm makes the GtkPrintContext gives its width
	 * and height in mm (althouh the doc says in pix)
	 * so: width=197,3, height=276,4 mm for a A4
	 * but 197,3 mm / 25,4 mm per inch * 72 dots per inch = 547 dots, not 559 pix
	 *     276,4                                          = 782  */

	/* unit_none gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( print, GTK_UNIT_NONE );

	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), ( gpointer ) instance );
	g_signal_connect( print, "paginate", G_CALLBACK( on_paginate ), ( gpointer ) instance );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), ( gpointer ) instance );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), ( gpointer ) instance );

	psize = gtk_paper_size_new( sdata->paper_size );
	psetup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( psetup, psize );
	gtk_paper_size_free( psize );
	gtk_page_setup_set_orientation( psetup, sdata->paper_orientation );
	gtk_print_operation_set_default_page_setup( print, psetup );
	g_object_unref( psetup );

	gtk_print_operation_set_export_filename( print, sdata->filename );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->on_print_operation_new ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->on_print_operation_new( instance, print );
	}

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_EXPORT,
	                my_window_get_toplevel( MY_WINDOW( instance )),
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		my_utils_dialog_error( str );
		g_free( str );
		g_error_free( error );

	} else {
		success_printing( instance, sdata );
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
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance )
{
	static const gchar *thisfn = "ofa_iprintable_on_begin_print";
	sIPrintable *sdata;

	g_debug( "%s: operation=%p, context=%p, instance=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) instance );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->max_y =
			gtk_print_context_get_height( context )
			- get_page_footer_height( instance );
	g_debug( "%s: max_y=%lf", thisfn, sdata->max_y );

	sdata->layout = gtk_print_context_create_pango_layout( context );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->on_begin_print ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->on_begin_print( instance, operation, context );
	}

	/* go on now to paginate and drawing phases */
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
on_paginate( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance )
{
	static const gchar *thisfn = "ofa_iprintable_on_paginate";
	sIPrintable *sdata;
	gint page_num;

	g_debug( "%s: operation=%p, context=%p, instance=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) instance );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, FALSE );

	reset_runtime( instance );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->on_begin_paginate ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->on_begin_paginate( instance, operation, context );
	}

	page_num = 0;
	while( draw_page( instance, NULL, NULL, page_num, sdata )){
		page_num += 1;
	}

	/* page_num is counted from zero, so add 1 for count */
	sdata->pages_count = page_num+1;
	g_debug( "%s: end of pagination: about to draw %d page(s)", thisfn, sdata->pages_count );
	gtk_print_operation_set_n_pages( operation, sdata->pages_count );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->on_end_paginate ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->on_end_paginate( instance, operation, context );
	}

	reset_runtime( instance );

	return( TRUE );
}

/*
 * Note that our own reset runtime is not a default that the
 * application would supersede. Instead, it is actually part of the
 * whole algorithm, and the application may only add its own code.
 */
static void
reset_runtime( ofaIPrintable *instance )
{
	sIPrintable *sdata;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->last_printed = NULL;
	sdata->group_footer_printed = TRUE;
	sdata->count = 0;

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->reset_runtime ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->reset_runtime( instance );
	}
}

/*
 * GtkPrintOperation signal handler
 * call once per page, with a page_num counted from zero
 */
static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaIPrintable *instance )
{
	static const gchar *thisfn = "ofa_printable_on_draw_page";
	sIPrintable *sdata;

	g_debug( "%s: operation=%p, context=%p, page_num=%d, instance=%p",
			thisfn, ( void * ) operation, ( void * ) context, page_num, ( void * ) instance );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	draw_page( instance, operation, context, page_num, sdata );
}

/*
 * Used when paginating first, then for actually drawing
 * @operation: %NULL during pagination phase
 * @context: %NULL during pagination phase
 *
 * Returns: %TRUE while there is still page(s) to be printed,
 * %FALSE at the end.
 *
 * The returned value is only used while paginating.
 */
static gboolean
draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	GList *line, *next;
	gint count;
	gboolean is_last;
	gdouble req_height;

	sdata->last_y = 0;

	v_draw_page_header( instance, operation, context, page_num );

	if( page_num == 0 ){
		draw_top_summary( instance, operation, context, sdata );
	}

	line = sdata->last_printed ?
				g_list_next( sdata->last_printed ) : sdata->dataset;

	for( count=0 ; line ; count+=1 ){
		next = g_list_next( line );		/* may be %NULL */
		if( !draw_line( instance, operation, context, page_num, count, line, next, sdata )){
			break;
		}
		sdata->last_printed = line;
		line = next;
	}

	/* end of the last page ? */
	is_last = FALSE;

	if( !line ){
		if( !sdata->group_footer_printed ){
			draw_group_footer( instance, operation, context, count, sdata );
		}
		req_height = get_bottom_summary_height( instance, sdata );
		if( sdata->last_y + req_height <= sdata->max_y ){
			is_last = TRUE;
		}
		if( is_last ){
			draw_bottom_summary( instance, operation, context, sdata );
			/*g_debug( "draw_page: page_num=%d, is_last=%s", page_num, is_last ? "True":"False" );*/
		}
	}

	v_draw_page_footer( instance, operation, context, page_num );

	return( !is_last );
}

static void
v_draw_page_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_header ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_header( instance, operation, context, page_num );
	}
}

/*
 * default implementation of #draw_page_header() virtual
 */
static void
iprintable_draw_page_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	sIPrintable *sdata;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	draw_page_header_dossier( instance, operation, context, page_num, sdata );
	draw_page_header_title( instance, operation, context, page_num, sdata );
	draw_page_header_subtitle( instance, operation, context, page_num, sdata );
	draw_page_header_columns( instance, operation, context, page_num, sdata );

	/* test font size */
	if( 0 ){
		draw_page_header_test_fonts( instance, operation, context, page_num, sdata );
	}

	/*g_debug( "iprintable_draw_page_header: last_y=%lf", sdata->last_y );*/
}

/*
 * default implementation of #draw_page_header() virtual
 * - dossier name
 */
static void
draw_page_header_dossier( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	gdouble y;
	ofoDossier *dossier;

	y = sdata->last_y;

	ofa_iprintable_set_color( instance, context, COLOR_HEADER_DOSSIER );
	ofa_iprintable_set_font( instance, "Bold Italic", st_page_header_dossier_name_font_size );

	/* dossier name on line 1 */
	dossier = ofa_main_window_get_dossier( MY_WINDOW( instance )->prot->main_window );
	ofa_iprintable_set_text( instance, context,
			0, y, ofo_dossier_get_name( dossier ), PANGO_ALIGN_LEFT );

	y += sdata->current_font_size + st_page_header_dossier_vspace_after;
	sdata->last_y = y;
}

/*
 * default implementation of #draw_page_header() virtual
 * - report title
 */
static void
draw_page_header_title( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	gdouble y, width;
	gchar *str;

	y = sdata->last_y;

	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", st_page_header_title_font_size );

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));
		width = gtk_print_context_get_width( context );
		str = v_get_page_header_title( instance );
		ofa_iprintable_set_text( instance, context, width/2, y, str, PANGO_ALIGN_CENTER );
		g_free( str );
	}

	y += sdata->current_font_size + st_page_header_title_vspace_after;
	sdata->last_y = y;
}

static gchar *
v_get_page_header_title( const ofaIPrintable *instance )
{
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_page_header_title ){
		return( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_page_header_title( instance ));
	}

	return( NULL );
}

static gchar *
iprintable_get_page_header_title( const ofaIPrintable *instance )
{
	return( _( "Report Title" ));
}

/*
 * default implementation of #draw_page_header() virtual
 * - report subtitle
 */
static void
draw_page_header_subtitle( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	gdouble y, width;
	gchar *str;

	y = sdata->last_y;

	ofa_iprintable_set_color( instance, context,COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", st_page_header_subtitle_font_size );

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));
		width = gtk_print_context_get_width( context );
		str = v_get_page_header_subtitle( instance );
		ofa_iprintable_set_text( instance, context, width/2, y,str, PANGO_ALIGN_CENTER );
		g_free( str );
	}

	y += sdata->current_font_size + st_page_header_subtitle_vspace_after;
	sdata->last_y = y;
}

static gchar *
v_get_page_header_subtitle( const ofaIPrintable *instance )
{
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_page_header_subtitle ){
		return( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_page_header_subtitle( instance ));
	}

	return( NULL );
}

static gchar *
iprintable_get_page_header_subtitle( const ofaIPrintable *instance )
{
	return( _( "Report subtitle" ));
}

/*
 * default implementation of #draw_page_header() virtual
 * - columns header
 */
static void
draw_page_header_columns( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	gdouble width, height;
	cairo_t *cr;

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));
		width = gtk_print_context_get_width( context );
		cr = gtk_print_context_get_cairo_context( context );

		/* draw and paint a rectangle
		 * this must be done before writing the columns headers */
		ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
		height = get_page_header_columns_height( instance, sdata );
		cairo_rectangle( cr, 0, sdata->last_y, width, height );
		cairo_fill( cr );
	}

	ofa_iprintable_set_color( instance, context, COLOR_WHITE );
	ofa_iprintable_set_font( instance, "Bold", sdata->default_font_size-1 );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_header_columns ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_header_columns( instance, operation, context );
	}

	sdata->last_y += st_page_header_columns_vspace_after;
}

static gdouble
get_page_header_columns_height( const ofaIPrintable *instance, sIPrintable *sdata )
{
	gdouble prev_y, height;

	prev_y = sdata->last_y;
	draw_page_header_columns( OFA_IPRINTABLE( instance ), NULL, NULL, 0, sdata );
	height = sdata->last_y - prev_y - st_page_header_columns_vspace_after;
	sdata->last_y = prev_y;

	return( height );
}

/**
 * ofa_iprintable_get_page_header_columns_height
 *
 * Returns: the height of the surrounding rectangle of the columns
 * header.
 *
 * This height is computed by just drawing on a NULL context, and then
 * measuring the 'last_y' difference.
 */
gdouble
ofa_iprintable_get_page_header_columns_height( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( get_page_header_columns_height( instance, sdata ));
}

static void
draw_page_header_test_fonts( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, sIPrintable *sdata )
{
	gint font_size = 12;
	gdouble y, width, bottom;
	cairo_t *cr;
	gchar *text;

	y = sdata->last_y;

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));
		ofa_iprintable_set_color( instance, context, COLOR_HEADER_DOSSIER );
		ofa_iprintable_set_font( instance, "Bold Italic", font_size );

		text = g_strdup_printf(
				"%s %d ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", "Bold Italic", font_size );

		cr = gtk_print_context_get_cairo_context( context );
		width = gtk_print_context_get_width( context );

		ofa_iprintable_set_text( instance, context, 0, y, text, PANGO_ALIGN_LEFT );

		bottom = y+font_size;
		cairo_set_line_width( cr, 0.25 );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, width, y );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, bottom );
		cairo_line_to( cr, width, bottom );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, 0, bottom );
		cairo_stroke( cr );

		cairo_move_to( cr, width, y );
		cairo_line_to( cr, width, bottom );
		cairo_stroke( cr );

		y += font_size + font_size;

		ofa_iprintable_set_text( instance, context, 0, y, text, PANGO_ALIGN_LEFT );

		bottom = y+font_size;
		cairo_set_line_width( cr, 0.25 );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, width, y );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, bottom );
		cairo_line_to( cr, width, bottom );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, 0, bottom );
		cairo_stroke( cr );

		cairo_move_to( cr, width, y );
		cairo_line_to( cr, width, bottom );
		cairo_stroke( cr );

		y += font_size + font_size;
		g_free( text );
	}

	sdata->last_y = y;
}

static void
draw_top_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata )
{
	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", sdata->default_font_size+1 );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_top_summary ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_top_summary( instance, operation, context );
	}
}

/*
 * Do not put here any other default value than FALSE as it is used to
 * draw group header and footer (so do not even test here for prev=NULL).
 * It is up to the implementor to decide if it will make use of groups
 * and to return TRUE on a new group.
 */
static gboolean
v_is_new_group( const ofaIPrintable *instance, GList *current, GList *prev )
{
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->is_new_group ){
		return( OFA_IPRINTABLE_GET_INTERFACE( instance )->is_new_group( instance, current, prev ));
	}

	return( FALSE );
}

/*
 * only called when there is actually a group header in this report
 * ( is_new_group() has returned %TRUE)
 *
 * we take care of not having just the group header on the bottom of
 * the page, but at least:
 * - the group header
 * - a line
 * - a bottom page report or the group footer if the group only
 *   contains one single line
 */
static void
draw_group_header( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context,
						gint line_num, GList *line, sIPrintable *sdata )
{
	gdouble y;
	cairo_t *cr;

	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", sdata->default_font_size );

	/* separation line */
	if( !sdata->group_on_new_page && line_num>0 ){
		cr = context ? gtk_print_context_get_cairo_context( context ) : NULL;
		y = sdata->last_y;
		y += 0.5*st_line_vspace;
		if( context ){
			cairo_set_line_width( cr, 0.5 );
			cairo_move_to( cr, 0, y );
			cairo_line_to( cr, gtk_print_context_get_width( context ), y );
			cairo_stroke( cr );
		}
		y += 1.5;
		if( context ){
			cairo_move_to( cr, 0, y );
			cairo_line_to( cr, gtk_print_context_get_width( context ), y );
			cairo_stroke( cr );
		}
		y += 1.5*st_line_vspace;
		sdata->last_y = y;
	}

	/* display the group header */
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_header ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_header( instance, operation, context, line );
	}

	/* setup the group properties */
	sdata->group_footer_printed = FALSE;
}

/*
 * the value cannot be cached as the height may at least depend of the
 * position of the group header in the sheet
 */
static gdouble
get_group_header_height( const ofaIPrintable *instance, gint line_num, GList *line, sIPrintable *sdata )
{
	gdouble prev_y, height;
	gboolean prev_printed;

	prev_y = sdata->last_y;
	prev_printed = sdata->group_footer_printed;

	draw_group_header(( ofaIPrintable * ) instance, NULL, NULL, line_num, line, sdata );
	height = sdata->last_y - prev_y;

	sdata->group_footer_printed = prev_printed;
	sdata->last_y = prev_y;

	return( height );
}

/*
 * draw the report for the group at the top of the next body page
 */
static void
draw_group_top_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata )
{
	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "", sdata->default_font_size );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_top_report ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_top_report( instance, operation, context );
	}
}

/*
 * @line_num: line number in the page, counted from 0
 * @line: the line candidate to be printed
 *  called from draw_page(), which makes sure @line is non null
 * @next: the next line after this one, may be %NULL if we are at the
 *  end of the list
 *
 * Returns: %TRUE if this line has been printed, %FALSE else.
 */
static gboolean
draw_line( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context,
					gint page_num, gint line_num, GList *line, GList *next, sIPrintable *sdata )
{
	/*static const gchar *thisfn = "ofa_iprintable_draw_line";*/
	gdouble y, req_height, end_height;

	/*g_debug( "%s: instance=%p, operation=%p, context=%p, page_num=%d, line_num=%d, line=%p, next=%p, sdata=%p",
			thisfn,
			( void * ) instance, ( void * ) operation, ( void * ) context,
			page_num, line_num, ( void * ) line, ( void * ) next, ( void * ) sdata );*/

	/* must be set before any height computing as this is the main
	 * parameter  */
	ofa_iprintable_set_color( instance, context, COLOR_BLACK );
	ofa_iprintable_set_font( instance, NULL, sdata->default_font_size );

	/* this line + a bottom report or a group footer */
	end_height =
			st_line_height
			+ (( !next || v_is_new_group( instance, next, line )) ?
					get_group_footer_height( instance, sdata ) :
					get_group_bottom_report_height( instance, sdata ));
	/*g_debug( "line_height=%lf, group_footer=%lf, bottom_report=%lf",
			st_line_height, get_group_footer_height( instance, sdata ), get_group_bottom_report_height( instance, sdata ));*/

	/* does the group change ? */
	if( v_is_new_group( instance, line, sdata->last_printed )){
		sdata->have_groups = TRUE;

		/* do we have a previous group footer not yet printed ? */
		if( sdata->last_printed && !sdata->group_footer_printed ){
			draw_group_footer( instance, operation, context, line_num, sdata );
		}
		/* is the group header requested on a new page ? */
		if( sdata->group_on_new_page && line_num > 0 ){
			return( FALSE );
		}
		/* do we have enough vertical space for the group header,
		 * at least one line, and a group bottom report or a group
		 * footer ? */
		req_height =
				get_group_header_height( instance, line_num, line, sdata )
				+ end_height;
		if( sdata->last_y + req_height > sdata->max_y ){
			return( FALSE );
		}
		/* so draw the group header */
		draw_group_header( instance, operation, context, line_num, line, sdata );

	} else if( line_num == 0 && sdata->have_groups ){
			draw_group_top_report( instance, operation, context, sdata );

	} else {
		/* do we have enough vertical space for this line, and a group
		 * bottom report or a group footer ? */
		req_height = end_height;
		/*g_debug( "draw_line: last_y=%lf, req=%lf", sdata->last_y, req_height );*/
		if( sdata->last_y + req_height > sdata->max_y ){
			draw_group_bottom_report( instance, operation, context, sdata );
			/*if( !next || v_is_new_group( instance, line, next )){
				g_debug( "draw_line: count=%d, line_debit=%lf, line_credit=%lf, next=%p", sdata->count, ofo_entry_get_debit( OFO_ENTRY( line->data )), ofo_entry_get_credit( OFO_ENTRY( line->data )), ( void * ) next );
				draw_group_footer( instance, operation, context, line_num, sdata );
			} else {
				draw_group_bottom_report( instance, operation, context, sdata );
			}*/
			return( FALSE );
		}
	}

	/* so, we are OK to draw the line ! */
	/* we are using a unique font to draw the lines */
	sdata->count += 1;
	y = sdata->last_y;

	if( context ){
		/* have a rubber every other line */
		if( line_num % 2 ){
			ofa_iprintable_draw_rubber( instance, context,
					y - (0.5*st_line_vspace - sdata->current_font_size/6),
					sdata->current_font_size+st_line_vspace );
		}
	}

	ofa_iprintable_set_color( instance, context, COLOR_BLACK );
	ofa_iprintable_set_font( instance, NULL, sdata->default_font_size );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_line ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_line( instance, operation, context, line );
	}

	y += st_line_height;
	sdata->last_y = y;

	return( TRUE );
}

/*
 * draw the report for the group at the bottom of the current body page
 */
static void
draw_group_bottom_report( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata )
{
	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "", sdata->default_font_size );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_bottom_report ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_bottom_report( instance, operation, context );
	}
}

/*
 * Only called during pagination phase
 */
static gdouble
get_group_bottom_report_height( const ofaIPrintable *instance, sIPrintable *sdata )
{
	gdouble prev_y, height;

	prev_y = sdata->last_y;

	draw_group_bottom_report(( ofaIPrintable * ) instance, NULL, NULL, sdata );
	height = sdata->last_y - prev_y;

	sdata->last_y = prev_y;

	return( height );
}

/*
 * only called if there is actually a group footer in this report
 */
static void
draw_group_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint line_num, sIPrintable *sdata )
{
	g_return_if_fail( line_num );

	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", sdata->default_font_size );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_footer ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_group_footer( instance, operation, context );
	}

	sdata->group_footer_printed = TRUE;
}

static gdouble
get_group_footer_height( const ofaIPrintable *instance, sIPrintable *sdata )
{
	gboolean prev_printed;
	gdouble prev_y, height;

	prev_y = sdata->last_y;
	prev_printed = sdata->group_footer_printed;

	draw_group_footer(( ofaIPrintable * ) instance, NULL, NULL, -1, sdata );
	height = sdata->last_y - prev_y;

	sdata->last_y = prev_y;
	sdata->group_footer_printed = prev_printed;

	return( height );
}

/*
 * Returns %TRUE if we have been able to print the bottom summary,
 * %FALSE else (and we need another page)
 */
static void
draw_bottom_summary( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, sIPrintable *sdata )
{
	ofa_iprintable_set_color( instance, context, COLOR_HEADER_TITLE );
	ofa_iprintable_set_font( instance, "Bold", sdata->default_font_size+1 );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_bottom_summary ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_bottom_summary( instance, operation, context );
	}
}

static gdouble
get_bottom_summary_height( ofaIPrintable *instance, sIPrintable *sdata )
{
	gdouble prev_y, height;

	prev_y = sdata->last_y;
	draw_bottom_summary( instance, NULL, NULL, sdata );
	height = sdata->last_y - prev_y;
	sdata->last_y = prev_y;

	return( height );
}

static void
v_draw_page_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_footer ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->draw_page_footer( instance, operation, context, page_num );
	}
}

static void
iprintable_draw_page_footer( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	sIPrintable *sdata;
	gchar *str, *stamp_str;
	GTimeVal stamp;
	gdouble width, y;
	cairo_t *cr;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	ofa_iprintable_set_color( instance, context, COLOR_FOOTER );
	ofa_iprintable_set_font( instance, "Italic", st_footer_font_size );

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));
		width = gtk_print_context_get_width( context );
		y = gtk_print_context_get_height( context );


		str = g_strdup_printf( "%s v %s", PACKAGE_NAME, PACKAGE_VERSION );
		ofa_iprintable_set_text( instance, context, st_page_margin, y, str, PANGO_ALIGN_LEFT );
		g_free( str );

		my_utils_stamp_set_now( &stamp );
		stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_DMYYHM );
		str = g_strdup_printf(
					_( "Printed on %s - Page %d/%d" ), stamp_str, 1+page_num, sdata->pages_count );
		g_free( stamp_str );
		ofa_iprintable_set_text( instance, context, width-st_page_margin, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );

		y -= st_footer_after_line_vspace;

		cr = gtk_print_context_get_cairo_context( context );
		cairo_set_line_width( cr, 0.5 );
		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, width, y );
		cairo_stroke( cr );
	}
}

static gdouble
get_page_footer_height( const ofaIPrintable *instance )
{
	gdouble y;

	y = 0;
	y += st_footer_before_line_vspace;
	y += st_footer_after_line_vspace;

	return( y );
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaIPrintable *instance )
{
	static const gchar *thisfn = "ofa_printable_on_end_print";

	g_debug( "%s: operation=%p, context=%p, instance=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) instance );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->on_end_print ){
		OFA_IPRINTABLE_GET_INTERFACE( instance )->on_end_print( instance, operation, context );
	}
}

static const gchar *
v_get_success_msg( const ofaIPrintable *instance )
{
	g_return_val_if_fail( OFA_IS_IPRINTABLE( instance ), NULL );

	if( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_success_msg ){
		return( OFA_IPRINTABLE_GET_INTERFACE( instance )->get_success_msg( instance ));
	}

	return( _( "The report has been successfully printed" ));
}

/**
 * ofa_iprintable_get_page_margin:
 * @instance: this #ofaIPrintable instance.
 */
gdouble
ofa_iprintable_get_page_margin( const ofaIPrintable *instance )
{
	return( st_page_margin );
}

/**
 * ofa_iprintable_set_font:
 */
void
ofa_iprintable_set_font( ofaIPrintable *instance, const gchar *font_desc, gint size )
{
	sIPrintable *sdata;
	PangoFontDescription *desc;
	gchar *str;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	str = g_strdup_printf( "%s %s %d", st_font_family, font_desc ? font_desc : "", size );
	desc = pango_font_description_from_string( str );
	g_free( str );
	pango_layout_set_font_description( sdata->layout, desc );
	pango_font_description_free( desc );

	sdata->current_font_size = size;
}

/**
 * ofa_iprintable_get_default_font_size:
 * @instance: this #ofaIPrintable instance.
 *
 * Returns: the default font size, which serves as a computing base for
 * all fonts of the printing.
 */
gint
ofa_iprintable_get_default_font_size( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( G_IS_OBJECT( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->default_font_size );
}

/**
 * ofa_iprintable_set_default_font_size:
 * @instance: this #ofaIPrintable instance.
 * @bfs: the body font size to be set.
 *
 * #ofaIPrintable class defaults to a font size of 8.
 */
void
ofa_iprintable_set_default_font_size( ofaIPrintable *instance, gint bfs )
{
	sIPrintable *sdata;

	g_return_if_fail( G_IS_OBJECT( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->default_font_size = bfs;
}

/**
 * ofa_iprintable_get_current_font_size:
 * @instance: this #ofaIPrintable instance.
 *
 * Returns: the current font size, which serves as a computing base
 * for the current vertical space between lines.
 */
gint
ofa_iprintable_get_current_font_size( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->current_font_size );
}

/**
 * ofa_iprintable_get_default_line_vspace:
 * @instance: this #ofaIPrintable instance.
 */
gdouble
ofa_iprintable_get_default_line_vspace( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return(( gdouble ) sdata->default_font_size/2 );
}

/**
 * ofa_iprintable_get_current_line_vspace:
 * @instance: this #ofaIPrintable instance.
 */
gdouble
ofa_iprintable_get_current_line_vspace( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return(( gdouble ) sdata->current_font_size/2 );
}

/**
 * ofa_iprintable_get_current_line_height:
 * @instance: this #ofaIPrintable instance.
 *
 * Returns: the current line height, i.e. the sum of the current body
 * font size + the corresponding vertical space interline.
 */
gdouble
ofa_iprintable_get_current_line_height( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( st_line_height );
}

/**
 * ofa_iprintable_get_last_y:
 * @instance: this #ofaIPrintable instance.
 */
gdouble
ofa_iprintable_get_last_y( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->last_y );
}

/**
 * ofa_iprintable_set_last_y:
 * @instance: this #ofaIPrintable instance.
 * @y: the new ordinate to be set
 */
void
ofa_iprintable_set_last_y( ofaIPrintable *instance, gdouble y )
{
	sIPrintable *sdata;

	g_return_if_fail( MY_IS_DIALOG( instance ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	sdata->last_y = y;
}

/**
 * ofa_iprintable_get_max_y:
 * @instance: this #ofaIPrintable instance.
 */
gdouble
ofa_iprintable_get_max_y( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( MY_IS_DIALOG( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->max_y );
}

/**
 * ofa_iprintable_get_pages_count:
 * @instance: this #ofaIPrintable instance.
 */
gint
ofa_iprintable_get_pages_count( const ofaIPrintable *instance )
{
	sIPrintable *sdata;

	g_return_val_if_fail( G_IS_OBJECT( instance ), 0 );

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->pages_count );
}

/**
 * ofa_iprintable_draw_rubber:
 */
void
ofa_iprintable_draw_rubber( ofaIPrintable *instance, GtkPrintContext *context, gdouble top, gdouble height )
{
	cairo_t *cr;
	gdouble width;

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));

		cr = gtk_print_context_get_cairo_context( context );
		cairo_set_source_rgb( cr, COLOR_LIGHT_GRAY );
		width = gtk_print_context_get_width( context );
		cairo_rectangle( cr, 0, top, width, height );
		cairo_fill( cr );
	}
}

/**
 * ofa_iprintable_draw_rect:
 */
void
ofa_iprintable_draw_rect( ofaIPrintable *instance, GtkPrintContext *context, gdouble x, gdouble y, gdouble width, gdouble height )
{
	cairo_t *cr;
	gdouble cx;

	if( context ){
		g_return_if_fail( GTK_IS_PRINT_CONTEXT( context ));

		cx = width < 0 ? gtk_print_context_get_width( context ) : width;

		cr = gtk_print_context_get_cairo_context( context );
		cairo_set_line_width( cr, 0.5 );

		cairo_move_to( cr, x, y );				/* ---- */
		cairo_line_to( cr, x+cx, y );
		cairo_stroke( cr );

		cairo_move_to( cr, x, y );				/* +---- */
		cairo_line_to( cr, x, y+height );		/* |     */
		cairo_stroke( cr );

		cairo_move_to( cr, x+cx, y );			/* +---+ */
		cairo_line_to( cr, x+cx, y+height );	/* |   | */
		cairo_stroke( cr );

		cairo_move_to( cr, x, y+height );		/* +---+ */
		cairo_line_to( cr, x+cx, y+height );	/* +---+ */
		cairo_stroke( cr );
	}
}

/**
 * ofa_iprintable_draw_no_data:
 */
void
ofa_iprintable_draw_no_data( ofaIPrintable *instance, GtkPrintContext *context )
{
	sIPrintable *sdata;
	gdouble y, width;

	g_return_if_fail( G_IS_OBJECT( instance ));
	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	ofa_iprintable_set_color( instance, context, COLOR_NO_DATA );
	ofa_iprintable_set_font( instance, "Bold", st_no_data_font_size );

	/* vertically centered */
	y = sdata->last_y + (sdata->max_y-sdata->last_y-st_no_data_font_size)/2;
	width = context ? gtk_print_context_get_width( context ) : 0;
	ofa_iprintable_set_text( instance, context, width/2, y, _( "Empty dataset" ), PANGO_ALIGN_CENTER );
	sdata->last_y = y+st_line_height;
}

/**
 * ofa_iprintable_set_color:
 */
void
ofa_iprintable_set_color( const ofaIPrintable *instance, GtkPrintContext *context,
									double r, double g, double b )
{
	cairo_t *cr;

	g_return_if_fail( instance && OFA_IS_IPRINTABLE( instance ));
	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	if( context ){
		cr = gtk_print_context_get_cairo_context( context );
		cairo_set_source_rgb( cr, r, g, b );
	}
}

/**
 * ofa_iprintable_set_text:
 *
 * the 'x' abcisse must point to the tab reference:
 * - when aligned on left, to the left
 * - when aligned on right, to the right
 * - when centered, to the middle point
 */
void
ofa_iprintable_set_text( const ofaIPrintable *instance, GtkPrintContext *context,
								gdouble x, gdouble y, const gchar *text, PangoAlignment align )
{
	static const gchar *thisfn = "ofa_iprintable_set_text";
	sIPrintable *sdata;
	cairo_t *cr;
	PangoRectangle rc;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));
	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	if( context ){
		pango_layout_set_text( sdata->layout, text, -1 );
		cr = gtk_print_context_get_cairo_context( context );

		if( align == PANGO_ALIGN_LEFT ){
			cairo_move_to( cr, x, y );

		} else if( align == PANGO_ALIGN_RIGHT ){
			pango_layout_get_pixel_extents( sdata->layout, NULL, &rc );
			cairo_move_to( cr, x-rc.width, y );

		} else if( align == PANGO_ALIGN_CENTER ){
			pango_layout_get_pixel_extents( sdata->layout, NULL, &rc );
			cairo_move_to( cr, x-rc.width/2, y );

		} else {
			g_warning( "%s: %d: unknown print alignment indicator", thisfn, align );
		}

		pango_cairo_update_layout( cr, sdata->layout );
		pango_cairo_show_layout( cr, sdata->layout );
	}
}

/**
 * ofa_iprintable_ellipsize_text:
 */
void
ofa_iprintable_ellipsize_text( const ofaIPrintable *instance, GtkPrintContext *context,
									gdouble x, gdouble y, const gchar *text,
									gdouble max_size )
{
	sIPrintable *sdata;
	cairo_t *cr;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));
	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	if( context ){
		cr = gtk_print_context_get_cairo_context( context );
		pango_layout_set_text( sdata->layout, text, -1 );
		my_utils_pango_layout_ellipsize( sdata->layout, max_size );
		cairo_move_to( cr, x, y );
		pango_cairo_update_layout( cr, sdata->layout );
		pango_cairo_show_layout( cr, sdata->layout );
	}
}

/**
 * ofa_iprintable_set_wrapped_text:
 * @instance:
 * @context:
 * @x:
 * @y:
 * @width: the maximum with, in Pango units
 * @text:
 * @align:
 *
 * Returns: the height of the printed text.
 */
void
ofa_iprintable_set_wrapped_text( const ofaIPrintable *instance, GtkPrintContext *context,
									gdouble x, gdouble y, gdouble width, const gchar *text, PangoAlignment align )
{
	sIPrintable *sdata;

	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));
	g_return_if_fail( !context || GTK_IS_PRINT_CONTEXT( context ));

	sdata = ( sIPrintable * ) g_object_get_data( G_OBJECT( instance ), IPRINTABLE_DATA );
	g_return_if_fail( sdata );

	pango_layout_set_width( sdata->layout, width );
	pango_layout_set_wrap( sdata->layout, PANGO_WRAP_WORD );
	ofa_iprintable_set_text( instance, context, x, y, text, align );
}
