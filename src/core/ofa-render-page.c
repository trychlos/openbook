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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <cairo-pdf.h>
#include <glib/gi18n.h>
#include <math.h>

#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-iprintable.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-render-page.h"

/* private instance data
 */
typedef struct {

	/* UI
	 */
	GtkWidget *paned;
	GtkWidget *drawing_area;
	GtkWidget *msg_label;
	GtkWidget *render_btn;
	GtkWidget *print_btn;

	/* from the derived class
	 * this takes paper name and orientation into account
	 */
	gdouble    paper_width;				/* in points */
	gdouble    paper_height;
	gdouble    render_width;			/* in points */
	gdouble    render_height;
	GList     *dataset;
	GList     *pdf_crs;					/* one pdf cairo context per printed page */
}
	ofaRenderPagePrivate;

/*
 * A4 sheet size is 21.0 x 29.7 mm = 8.26772 x 11.69291 in
 *                                 = 595.27559 x 841.88976 points
 *                                 ~ 595 x 841
 */
#define PAGE_SEPARATION_V_HEIGHT        4.0		/* separation between two pages */
#define PAGE_EXT_MARGIN_V_HEIGHT        2.0		/* a margin before the first and after the last page */

#define COLOR_LIGHT_GRAY                0.90980, 0.90980, 0.90980	/* widget background: #e8e8e8 */
#define COLOR_WHITE                     1,       1,       1			/* page background: #ffffff */
#define COLOR_BLACK                     0,       0,       0			/* #000000 */

#define MSG_ERROR                       "labelerror"	/* red */
#define MSG_INFO                        "labelinfo"		/* blue */

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-render-page.ui";
static const gchar *st_ui_name          = "RenderPageWindow";

static void               paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget         *setup_view1( ofaRenderPage *self );
static GtkWidget         *setup_view2( ofaRenderPage *self );
static void               setup_args_area( ofaRenderPage *self, GtkContainer *parent );
static void               setup_actions_area( ofaRenderPage *self, GtkContainer *parent );
static void               setup_page_size( ofaRenderPage *self );
static GList             *get_dataset( ofaRenderPage *page );
static gboolean           on_draw( GtkWidget *area, cairo_t *cr, ofaRenderPage *page );
static void               draw_widget_background( cairo_t *cr, GtkWidget *area );
static gint               do_drawing( ofaRenderPage *page, cairo_t *cr, gdouble shift_x );
static void               draw_page_background( ofaRenderPage *page, cairo_t *cr, gdouble x, gdouble y );
static void               on_render_clicked( GtkButton *button, ofaRenderPage *page );
static void               render_pdf( ofaRenderPage *page );
static void               on_print_clicked( GtkButton *button, ofaRenderPage *page );
static cairo_t           *create_context( ofaRenderPage *page, gdouble width, gdouble height );
static void               set_message( ofaRenderPage *page, const gchar *message, const gchar *color_name );
static void               pdf_crs_free( GList **pdf_crs );
static void               iprintable_iface_init( ofaIPrintableInterface *iface );
static guint              iprintable_get_interface_version( const ofaIPrintable *instance );
static const gchar       *iprintable_get_paper_name( ofaIPrintable *instance );
static GtkPageOrientation iprintable_get_page_orientation( ofaIPrintable *instance );
static void               iprintable_get_print_settings( ofaIPrintable *instance, GKeyFile **keyfile, gchar **group_name );
static void               iprintable_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void               iprintable_draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void               iprintable_end_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context );

G_DEFINE_TYPE_EXTENDED( ofaRenderPage, ofa_render_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaRenderPage )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IPRINTABLE, iprintable_iface_init ))

static void
render_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_render_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RENDER_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_page_parent_class )->finalize( instance );
}

static void
render_page_dispose( GObject *instance )
{
	ofaRenderPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RENDER_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ofa_render_page_get_instance_private( OFA_RENDER_PAGE( instance ));

		pdf_crs_free( &priv->pdf_crs );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_page_parent_class )->dispose( instance );
}

static void
ofa_render_page_init( ofaRenderPage *self )
{
	static const gchar *thisfn = "ofa_render_page_init";

	g_return_if_fail( OFA_IS_RENDER_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_render_page_class_init( ofaRenderPageClass *klass )
{
	static const gchar *thisfn = "ofa_render_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = render_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_page_finalize;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
}

static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_render_page_v_setup_view";
	ofaRenderPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_render_page_get_instance_private( OFA_RENDER_PAGE( page ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_RENDER_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_RENDER_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );

	setup_page_size( OFA_RENDER_PAGE( page ));
}

static GtkWidget *
setup_view1( ofaRenderPage *self )
{
	ofaRenderPagePrivate *priv;
	GtkWidget *grid, *scrolled, *viewport, *drawing, *label;

	priv = ofa_render_page_get_instance_private( self );

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 2 );

	/* setup the drawing area */
	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_grid_attach( GTK_GRID( grid ), scrolled, 0, 0, 1, 1 );

	viewport = gtk_viewport_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( scrolled ), viewport );

	drawing = gtk_drawing_area_new();
	gtk_container_add( GTK_CONTAINER( viewport ), drawing );
	gtk_widget_set_hexpand( drawing, TRUE );
	gtk_widget_set_vexpand( drawing, TRUE );
	g_signal_connect( drawing, "draw", G_CALLBACK( on_draw ), self );
	priv->drawing_area = drawing;

	/* setup the message zone */
	label = gtk_label_new( NULL );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, 1, 1, 1 );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	priv->msg_label = label;

	return( grid );
}

static GtkWidget *
setup_view2( ofaRenderPage *self )
{
	GtkWidget *parent;

	parent = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( parent ), st_resource_ui, st_ui_name, "top" );

	setup_args_area( self, GTK_CONTAINER( parent ));
	setup_actions_area( self, GTK_CONTAINER( parent ));

	return( parent );
}

static void
setup_args_area( ofaRenderPage *self, GtkContainer *parent )
{
	GtkWidget *area, *widget;

	area = my_utils_container_get_child_by_name( parent, "args-zone" );
	g_return_if_fail( area && GTK_IS_CONTAINER( area ));

	if( OFA_RENDER_PAGE_GET_CLASS( self )->get_args_widget ){
		widget = OFA_RENDER_PAGE_GET_CLASS( self )->get_args_widget( self );
		if( widget ){
			gtk_container_add( GTK_CONTAINER( area ), widget );
		}
	}
}

static void
setup_actions_area( ofaRenderPage *self, GtkContainer *parent )
{
	ofaRenderPagePrivate *priv;
	GtkWidget *button;

	priv = ofa_render_page_get_instance_private( self );

	button = my_utils_container_get_child_by_name( parent, "render-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_render_clicked ), self );
	gtk_widget_set_sensitive( button, FALSE );
	priv->render_btn = button;

	button = my_utils_container_get_child_by_name( parent, "print-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_print_clicked ), self );
	gtk_widget_set_sensitive( button, FALSE );
	priv->print_btn = button;
}

static void
setup_page_size( ofaRenderPage *self )
{
	static const gchar *thisfn = "ofa_render_page_setup_page_size";
	ofaRenderPagePrivate *priv;
	const gchar *paper_name;
	GtkPaperSize *paper_size;
	GtkPageOrientation orientation;
	GtkPageSetup *page_setup;

	priv = ofa_render_page_get_instance_private( self );

	paper_name = OFA_RENDER_PAGE_GET_CLASS( self )->get_paper_name ?
			OFA_RENDER_PAGE_GET_CLASS( self )->get_paper_name( OFA_RENDER_PAGE( self )) :
			NULL;
	g_return_if_fail( my_strlen( paper_name ));

	orientation = OFA_RENDER_PAGE_GET_CLASS( self )->get_page_orientation ?
			OFA_RENDER_PAGE_GET_CLASS( self )->get_page_orientation( OFA_RENDER_PAGE( self )) :
			-1;
	g_return_if_fail( orientation != -1 );

	paper_size = gtk_paper_size_new( paper_name );
	page_setup = gtk_page_setup_new();
	gtk_page_setup_set_orientation( page_setup, orientation );
	gtk_page_setup_set_paper_size( page_setup, paper_size );

	priv->paper_width = gtk_page_setup_get_paper_width( page_setup, GTK_UNIT_POINTS );
	priv->paper_height = gtk_page_setup_get_paper_height( page_setup, GTK_UNIT_POINTS );
	priv->render_width = gtk_page_setup_get_page_width( page_setup, GTK_UNIT_POINTS );
	priv->render_height = gtk_page_setup_get_page_height( page_setup, GTK_UNIT_POINTS );

	g_debug( "%s: paper_width=%lf, paper_height=%lf, render_width=%lf, render_height=%lf",
			thisfn,
			priv->paper_width, priv->paper_height, priv->render_width, priv->render_height );

	g_object_unref( page_setup );
	gtk_paper_size_free( paper_size );
}

/**
 * ofa_render_page_clear_drawing_area:
 * @page:
 *
 * Clear the drawing area.
 */
void
ofa_render_page_clear_drawing_area( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	cairo_t *cr;

	g_return_if_fail( page && OFA_IS_RENDER_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_render_page_get_instance_private( page );

	/* free data */
	ofa_render_page_free_dataset( page );
	pdf_crs_free( &priv->pdf_crs );

	/* reset the drawing area */
	cr = create_context( page, priv->render_width, priv->render_height );
	draw_widget_background( cr, priv->drawing_area );
	cairo_destroy( cr );
}

/**
 * ofa_render_page_set_args_changed:
 * @page:
 * @is_valid:
 * @message:
 *
 * Called by the derived class, which handles its own argument dialog,
 * on each argument change, telling us if args are valid or not.
 */
void
ofa_render_page_set_args_changed( ofaRenderPage *page, gboolean is_valid, const gchar *message )
{
	static const gchar *thisfn = "ofa_render_page_set_args_changed";
	ofaRenderPagePrivate *priv;

	g_debug( "%s: page=%p (%s), is_valid=%s, message=%s",
			thisfn,
			( void * ) page, G_OBJECT_TYPE_NAME( page ),
			is_valid ? "True":"False", message );

	g_return_if_fail( page && OFA_IS_RENDER_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_render_page_get_instance_private( page );

	gtk_widget_set_sensitive( priv->render_btn, is_valid );
	gtk_widget_set_sensitive( priv->print_btn, is_valid );
	set_message( page, is_valid ? "" : message, MSG_ERROR );

	ofa_render_page_clear_drawing_area( page );
}

static GList *
get_dataset( ofaRenderPage *page )
{
	GList *dataset;

	dataset = OFA_RENDER_PAGE_GET_CLASS( page )->get_dataset ?
		OFA_RENDER_PAGE_GET_CLASS( page )->get_dataset( page ) :
		NULL;

	return( dataset );
}

/**
 * ofa_render_page_free_dataset:
 * @page:
 *
 * Free the current dataset after an argument has changed.
 */
void
ofa_render_page_free_dataset( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_RENDER_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	if( OFA_RENDER_PAGE_GET_CLASS( page )->free_dataset ){
		priv = ofa_render_page_get_instance_private( page );
		OFA_RENDER_PAGE_GET_CLASS( page )->free_dataset( page, priv->dataset );
		priv->dataset = NULL;
	}
}

/*
 * We are drawing pages:
 * - requested width is the width if the page
 *   if allocated width is greater than requested width, we center the
 *   drawn page into the widget, margins being light gray
 * - requested height is the total height of the pages, including the
 *   height of an horizontal separation between the pages
 */
static gboolean
on_draw( GtkWidget *area, cairo_t *cr, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	gint widget_width, req_width, req_height, nb_pages;
	gdouble x;

	priv = ofa_render_page_get_instance_private( page );

	draw_widget_background( cr, area );

	widget_width = gtk_widget_get_allocated_width( area );
	x = widget_width > priv->paper_width ?
			(( gdouble ) widget_width - priv->paper_width ) / 2.0 :
			0;

	nb_pages = do_drawing( page, cr, x );

	req_width = widget_width > priv->paper_width ? -1 : priv->paper_width;

	req_height = nb_pages > 0 ?
			nb_pages * priv->paper_height
				+ ( nb_pages-1 ) * PAGE_SEPARATION_V_HEIGHT
				+ 2 * PAGE_EXT_MARGIN_V_HEIGHT:
			-1;

	gtk_widget_set_size_request( area, req_width, req_height );

	/* TRUE stops other handlers from being invoked for the event */
	return( TRUE );
}

/*
 * draw the background of the widget on which we are going to draw the
 * pages
 */
static void
draw_widget_background( cairo_t *cr, GtkWidget *area )
{
	gint widget_width, widget_height;

	widget_width = gtk_widget_get_allocated_width( area );
	widget_height = gtk_widget_get_allocated_height( area );

	cairo_set_source_rgb( cr, COLOR_LIGHT_GRAY );
	cairo_rectangle( cr, 0, 0, widget_width, widget_height );
	cairo_fill( cr );
}

/*
 * @cr: the widget drawing area's cairo context
 * @x: the shift from be left border of the widget drawing area so that
 *  the page appears centered in the widget
 *
 * The passed-on cairo context is those of the drawing area widget.
 * Copy to it the prepared cairo pdf surface.
 *
 * Returns: the count of printed pages (needed to adjust the size
 * requirement of the widget drawing area)
 */
static gint
do_drawing( ofaRenderPage *page, cairo_t *cr, gdouble shift_x )
{
	ofaRenderPagePrivate *priv;
	GList *it;
	cairo_t *pdf_cr;
	gdouble y, dx, dy;

	priv = ofa_render_page_get_instance_private( page );

	y = PAGE_EXT_MARGIN_V_HEIGHT;
	dx = shift_x+(priv->paper_width-priv->render_width)/2.0;

	for( it=priv->pdf_crs ; it ; it=it->next ){
		draw_page_background( page, cr, shift_x, y );
		pdf_cr = ( cairo_t * ) it->data;
		dy = y+(priv->paper_height-priv->render_height)/2.0;
		cairo_set_source_surface( cr, cairo_get_target( pdf_cr ), dx, dy );
		cairo_paint( cr );
		y += priv->paper_height + PAGE_SEPARATION_V_HEIGHT;
	}

	return( g_list_length( priv->pdf_crs ));
}

/*
 * draw the background of the page
 */
static void
draw_page_background( ofaRenderPage *page, cairo_t *cr, gdouble x, gdouble y )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	cairo_set_source_rgb( cr, COLOR_WHITE );
	cairo_rectangle( cr, x, y, priv->paper_width, priv->paper_height );
	cairo_fill( cr );
}

static void
on_render_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	render_pdf( page );
	gtk_widget_queue_draw( priv->drawing_area );
}

static void
render_pdf( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	gchar *str;
	cairo_t *cr, *page_cr;
	gint pages_count, i;

	priv = ofa_render_page_get_instance_private( page );

	if( !priv->dataset ){
		priv->dataset = get_dataset( page );
		pdf_crs_free( &priv->pdf_crs );
	}
	if( !priv->pdf_crs ){
		cr = create_context( page, priv->render_width, priv->render_height );
		pages_count = ofa_irenderable_begin_render(
				OFA_IRENDERABLE( page ), cr, priv->render_width, priv->render_height, priv->dataset );

		for( i=0 ; i<pages_count ; ++i ){
			page_cr = create_context( page, priv->render_width, priv->render_height );
			ofa_irenderable_render_page( OFA_IRENDERABLE( page ), page_cr, i );
			priv->pdf_crs = g_list_append( priv->pdf_crs, page_cr );
		}

		ofa_irenderable_end_render( OFA_IRENDERABLE( page ), cr );
		cairo_destroy( cr );
	}

	str = g_strdup_printf( "%d printed page(s).", g_list_length( priv->pdf_crs ));
	set_message( page, str, MSG_INFO );
}

static void
on_print_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	if( !priv->dataset ){
		render_pdf( page );
	}

	ofa_iprintable_print( OFA_IPRINTABLE( page ));
}

static cairo_t *
create_context( ofaRenderPage *page, gdouble width, gdouble height )
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_pdf_surface_create( NULL, width, height );
	cr = cairo_create( surface );
	cairo_surface_destroy( surface );

	return( cr );
}

static void
set_message( ofaRenderPage *page, const gchar *message, const gchar *spec )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), message );
	my_style_add( priv->msg_label, spec );
}

static void
pdf_crs_free( GList **pdf_crs )
{
	g_list_free_full( *pdf_crs, ( GDestroyNotify ) cairo_destroy );
	*pdf_crs = NULL;
}

/**
 * ofa_render_page_get_top_paned:
 * @page:
 *
 * Returns: the top #GtkPaned widget.
 */
GtkWidget *
ofa_render_page_get_top_paned( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RENDER_PAGE( page ), NULL );
	g_return_val_if_fail( !OFA_PAGE( page )->prot->dispose_has_run, NULL );

	priv = ofa_render_page_get_instance_private( page );

	return( priv->paned );
}

/*
 * ofaIPrintable interface management
 */
static void
iprintable_iface_init( ofaIPrintableInterface *iface )
{
	static const gchar *thisfn = "ofa_render_page_iprintable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable_get_interface_version;
	iface->get_paper_name = iprintable_get_paper_name;
	iface->get_page_orientation = iprintable_get_page_orientation;
	iface->get_print_settings = iprintable_get_print_settings;
	iface->begin_print = iprintable_begin_print;
	iface->draw_page = iprintable_draw_page;
	iface->end_print = iprintable_end_print;
}

static guint
iprintable_get_interface_version( const ofaIPrintable *instance )
{
	return( 1 );
}

static const gchar *
iprintable_get_paper_name( ofaIPrintable *instance )
{
	const gchar *paper_name;

	paper_name = OFA_RENDER_PAGE_GET_CLASS( instance )->get_paper_name ?
			OFA_RENDER_PAGE_GET_CLASS( instance )->get_paper_name( OFA_RENDER_PAGE( instance )) :
			NULL;
	g_return_val_if_fail( my_strlen( paper_name ), NULL );

	return( paper_name );
}

static GtkPageOrientation
iprintable_get_page_orientation( ofaIPrintable *instance )
{
	GtkPageOrientation orientation;

	orientation = OFA_RENDER_PAGE_GET_CLASS( instance )->get_page_orientation ?
			OFA_RENDER_PAGE_GET_CLASS( instance )->get_page_orientation( OFA_RENDER_PAGE( instance )) :
			-1;
	g_return_val_if_fail( orientation != -1, -1 );

	return( orientation );
}

static void
iprintable_get_print_settings( ofaIPrintable *instance, GKeyFile **keyfile, gchar **group_name )
{
	if( OFA_RENDER_PAGE_GET_CLASS( instance )->get_print_settings ){
		OFA_RENDER_PAGE_GET_CLASS( instance )->get_print_settings( OFA_RENDER_PAGE( instance ), keyfile, group_name );
	}
}


static void
iprintable_begin_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_render_page_iprintable_begin_print";
	ofaRenderPagePrivate *priv;
	gint pages_count;

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	priv = ofa_render_page_get_instance_private( OFA_RENDER_PAGE( instance ));

	pages_count = ofa_irenderable_begin_render(
			OFA_IRENDERABLE( instance ),
			gtk_print_context_get_cairo_context( context ),
			gtk_print_context_get_width( context ),
			gtk_print_context_get_height( context ),
			priv->dataset );

	gtk_print_operation_set_n_pages( operation, pages_count );
}

/*
 * GtkPrintOperation signal handler
 * call once per page, with a page_num counted from zero
 */
static void
iprintable_draw_page( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	static const gchar *thisfn = "ofa_render_page_iprintable_draw_page";
	cairo_t *cr;

	g_debug( "%s: instance=%p, operation=%p, context=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context, page_num );

	cr = gtk_print_context_get_cairo_context( context );
	ofa_irenderable_render_page( OFA_IRENDERABLE( instance ), cr, page_num );
}

static void
iprintable_end_print( ofaIPrintable *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_render_page_iprintable_end_print";
	cairo_t *cr;

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	cr = gtk_print_context_get_cairo_context( context );
	ofa_irenderable_end_render( OFA_IRENDERABLE( instance ), cr );
}
