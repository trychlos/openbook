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

#include <cairo.h>
#include <cairo-pdf.h>
#include <glib/gi18n.h>
#include <math.h>
#include <poppler.h>

#include "api/my-utils.h"

#include "ui/ofa-iprintable2.h"
#include "ui/ofa-irenderable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-render-page.h"

/* private instance data
 */
struct _ofaRenderPagePrivate {

	/* UI
	 */
	GtkWidget *drawing_area;
	GtkWidget *msg_label;
	GtkWidget *render_btn;
	GtkWidget *print_btn;

	/* from the derived class
	 * this takes paper name and orientation into account
	 */
	gdouble    paper_width;				/* in points */
	gdouble    paper_height;
	gboolean   paper_size_set;

	gdouble    render_width;			/* in points */
	gdouble    render_height;
	gboolean   render_size_set;

	/* pagination
	 */
	gint       pages_count;
	GList     *pdf_crs;					/* one pdf cairo context per printed page */
};

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

#define COLOR_ERROR                     "#ff0000"	/* red */

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-render-page.ui";
static const gchar *st_ui_name          = "RenderPageWindow";

static ofaPageClass *ofa_render_page_parent_class = NULL;

static GType              register_type( void );
static void               render_page_finalize( GObject *instance );
static void               render_page_dispose( GObject *instance );
static void               render_page_instance_init( ofaRenderPage *self );
static void               render_page_class_init( ofaRenderPageClass *klass );
static GtkWidget         *v_setup_view( ofaPage *page );
static void               setup_args_area( ofaRenderPage *page, GtkContainer *parent );
static void               setup_actions_area( ofaRenderPage *page, GtkContainer *parent );
static void               setup_drawing_area( ofaRenderPage *page, GtkContainer *parent );
static void               v_init_view( ofaPage *page );
static void               get_paper_size( ofaRenderPage *page, gdouble *paper_width, gdouble *paper_height );
static gboolean           setup_paper_size( ofaRenderPage *page );
static gboolean           compute_paper_size( GtkPaperSize *paper_size, GtkPageOrientation orientation, gdouble *paper_width, gdouble *paper_height );
static void               get_rendering_size( ofaRenderPage *page, gdouble *render_width, gdouble *render_height );
static gboolean           on_draw( GtkWidget *area, cairo_t *cr, ofaRenderPage *page );
static void               draw_widget_background( cairo_t *cr, GtkWidget *area );
static gint               do_drawing( ofaRenderPage *page, cairo_t *cr, gdouble shift_x, gdouble paper_width, gdouble paper_height );
static void               draw_page_background( ofaRenderPage *page, cairo_t *cr, gdouble x, gdouble y, gdouble paper_width, gdouble paper_height );
static void               on_render_clicked( GtkButton *button, ofaRenderPage *page );
static void               on_print_clicked( GtkButton *button, ofaRenderPage *page );
static void               push_context( ofaRenderPage *page, cairo_t *cr );
static void               set_message( ofaRenderPage *page, const gchar *message, const gchar *color_name );
static void               pdf_crs_free( GList **pdf_crs );
static void               iprintable2_iface_init( ofaIPrintable2Interface *iface );
static guint              iprintable2_get_interface_version( const ofaIPrintable2 *instance );
static const gchar       *iprintable2_get_paper_name( ofaIPrintable2 *instance );
static GtkPageOrientation iprintable2_get_page_orientation( ofaIPrintable2 *instance );
static void               iprintable2_begin_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context );
static void               iprintable2_draw_page( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void               iprintable2_end_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context );

GType
ofa_render_page_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_render_page_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaRenderPageClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) render_page_class_init,
		NULL,
		NULL,
		sizeof( ofaRenderPage ),
		0,
		( GInstanceInitFunc ) render_page_instance_init
	};

	static const GInterfaceInfo iprintable2_iface_info = {
		( GInterfaceInitFunc ) iprintable2_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_PAGE, "ofaRenderPage", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IPRINTABLE2, &iprintable2_iface_info );

	return( type );
}

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
		priv = OFA_RENDER_PAGE( instance )->priv;

		pdf_crs_free( &priv->pdf_crs );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_page_parent_class )->dispose( instance );
}

static void
render_page_instance_init( ofaRenderPage *self )
{
	static const gchar *thisfn = "ofa_render_page_instance_init";

	g_return_if_fail( OFA_IS_RENDER_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RENDER_PAGE, ofaRenderPagePrivate );
}

static void
render_page_class_init( ofaRenderPageClass *klass )
{
	static const gchar *thisfn = "ofa_render_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofa_render_page_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = render_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;

	g_type_class_add_private( klass, sizeof( ofaRenderPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	GtkWidget *window, *widget, *page_widget;

	page_widget = gtk_alignment_new( 0.5, 0.5, 1, 1 );

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_name );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top" );
	g_return_val_if_fail( widget && GTK_IS_CONTAINER( widget ), NULL );

	gtk_widget_reparent( widget, page_widget );

	setup_args_area( OFA_RENDER_PAGE( page ), GTK_CONTAINER( widget ));
	setup_actions_area( OFA_RENDER_PAGE( page ), GTK_CONTAINER( widget ));
	setup_drawing_area( OFA_RENDER_PAGE( page ), GTK_CONTAINER( widget ));

	return( page_widget );
}

static void
setup_args_area( ofaRenderPage *page, GtkContainer *parent )
{
	GtkWidget *area, *widget;

	area = my_utils_container_get_child_by_name( parent, "args-zone" );
	g_return_if_fail( area && GTK_IS_CONTAINER( area ));

	if( OFA_RENDER_PAGE_GET_CLASS( page )->get_args_widget ){
		widget = OFA_RENDER_PAGE_GET_CLASS( page )->get_args_widget( page );
		if( widget ){
			gtk_container_add( GTK_CONTAINER( area ), widget );
		}
	}
}

static void
setup_actions_area( ofaRenderPage *page, GtkContainer *parent )
{
	ofaRenderPagePrivate *priv;
	GtkWidget *button;

	priv = page->priv;

	button = my_utils_container_get_child_by_name( parent, "render-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_render_clicked ), page );
	gtk_widget_set_sensitive( button, FALSE );
	priv->render_btn = button;

	button = my_utils_container_get_child_by_name( parent, "print-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_print_clicked ), page );
	gtk_widget_set_sensitive( button, FALSE );
	priv->print_btn = button;
}

static void
setup_drawing_area( ofaRenderPage *page, GtkContainer *parent )
{
	ofaRenderPagePrivate *priv;
	GtkWidget *drawing, *label;

	priv = page->priv;

	drawing = my_utils_container_get_child_by_name( parent, "drawing-zone" );
	g_return_if_fail( drawing && GTK_IS_DRAWING_AREA( drawing ));
	g_signal_connect( G_OBJECT( drawing ), "draw", G_CALLBACK( on_draw ), page );
	priv->drawing_area = drawing;

	label = my_utils_container_get_child_by_name( parent, "message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->msg_label = label;
}

static void
v_init_view( ofaPage *page )
{
}

/**
 * ofa_render_page_set_args_valid:
 * @page:
 * @is_valid:
 * @message:
 *
 * Called by the derived class, which handles its own argument dialog,
 * on each argument change, telling us if args are valid or not.
 */
void
ofa_render_page_set_args_valid( ofaRenderPage *page, gboolean is_valid, const gchar *message )
{
	ofaRenderPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_RENDER_PAGE( page ));

	if( !OFA_PAGE( page )->prot->dispose_has_run ){

		priv = page->priv;
		gtk_widget_set_sensitive( priv->render_btn, is_valid );
		set_message( page, is_valid ? "" : message, COLOR_ERROR );
	}
}

static void
get_paper_size( ofaRenderPage *page, gdouble *paper_width, gdouble *paper_height )
{
	ofaRenderPagePrivate *priv;

	priv = page->priv;

	if( !priv->paper_size_set ){
		priv->paper_size_set = setup_paper_size( page );
	}
	if( priv->paper_size_set ){
		*paper_width = priv->paper_width;
		*paper_height = priv->paper_height;
	}
}

/*
 * this is called after the end of the rendering, before allowing the draw
 */
static gboolean
setup_paper_size( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	const gchar *paper_name;
	GtkPageOrientation orientation;
	GtkPaperSize *paper_size;
	gboolean ok;
	gdouble width, height;

	priv = page->priv;

	paper_name = OFA_RENDER_PAGE_GET_CLASS( page )->get_paper_name ?
			OFA_RENDER_PAGE_GET_CLASS( page )->get_paper_name( page ) :
			NULL;
	g_return_val_if_fail( my_strlen( paper_name ), FALSE );

	orientation = OFA_RENDER_PAGE_GET_CLASS( page )->get_page_orientation ?
			OFA_RENDER_PAGE_GET_CLASS( page )->get_page_orientation( page ) :
			-1;
	g_return_val_if_fail( orientation != -1, FALSE );

	paper_size = gtk_paper_size_new( paper_name );
	ok = compute_paper_size( paper_size, orientation, &width, &height );
	g_return_val_if_fail( ok, FALSE );

	priv->paper_width = width;
	priv->paper_height = height;

	return( TRUE );
}

/*
 * compute the paper width and height in points, taking into account
 * the orientation of the page
 */
static gboolean
compute_paper_size( GtkPaperSize *paper_size, GtkPageOrientation orientation, gdouble *paper_width, gdouble *paper_height )
{
	static const gchar *thisfn = "ofa_render_page_compute_paper_size";
	gdouble cx, cy, temp;

	cx = gtk_paper_size_get_width( paper_size, GTK_UNIT_POINTS );
	cy = gtk_paper_size_get_height( paper_size, GTK_UNIT_POINTS );
	g_return_val_if_fail( cx > 0 && cy > 0, FALSE );

	switch( orientation ){
		case GTK_PAGE_ORIENTATION_PORTRAIT:
		case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
			if( cx > cy ){
				temp = cy;
				cy = cx;
				cx = temp;
			}
			break;
		case GTK_PAGE_ORIENTATION_LANDSCAPE:
		case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
			if( cy > cx ){
				temp = cy;
				cy = cx;
				cx = temp;
			}
			break;
	}

	*paper_width = cx;
	*paper_height = cy;
	g_debug( "%s: paper_width=%lf, paper_height=%lf", thisfn, cx, cy );

	return( TRUE );
}

static void
get_rendering_size( ofaRenderPage *page, gdouble *render_width, gdouble *render_height )
{
	ofaRenderPagePrivate *priv;

	priv = page->priv;

	if( !priv->render_size_set ){
		if( OFA_RENDER_PAGE_GET_CLASS( page )->get_rendering_size ){
			OFA_RENDER_PAGE_GET_CLASS( page )->get_rendering_size( page, &priv->render_width, &priv->render_height );
			if( priv->render_width > 0 && priv->render_height > 0 ){
				priv->render_size_set = TRUE;
			}
		}
	}
	if( priv->render_size_set ){
		*render_width = priv->render_width;
		*render_height = priv->render_height;
	} else {
		get_paper_size( page, render_width, render_height );
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
	gint widget_width, req_width, req_height, nb_pages;
	gdouble x;
	gdouble paper_width, paper_height;

	draw_widget_background( cr, area );
	paper_width = 0;
	paper_height = 0;
	get_paper_size( page, &paper_width, &paper_height );

	widget_width = gtk_widget_get_allocated_width( area );
	x = widget_width > paper_width ?
			(( gdouble ) widget_width - paper_width ) / 2.0 :
			0;

	nb_pages = do_drawing( page, cr, x, paper_width, paper_height );

	req_width = widget_width > paper_width ? -1 : paper_width;

	req_height = nb_pages > 0 ?
			nb_pages * paper_height
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
do_drawing( ofaRenderPage *page, cairo_t *cr, gdouble shift_x, gdouble paper_width, gdouble paper_height )
{
	static const gchar *thisfn = "ofa_render_page_do_drawing";
	ofaRenderPagePrivate *priv;
	GList *it;
	cairo_t *pdf_cr;
	gdouble y, dx/*, dy*/;
	gdouble render_width, render_height;

	priv = page->priv;
	y = PAGE_EXT_MARGIN_V_HEIGHT;
	get_rendering_size( page, &render_width, &render_height );
	dx = shift_x+(paper_width-render_width)/2.0;

	if( 0 ){
		g_debug( "%s: shift_x=%lf, paper_width=%lf, paper_height=%lf, "
				"render_width=%lf, render_height=%lf, dx=%lf",
				thisfn, shift_x, paper_width, paper_height,
				render_width, render_height, dx );
	}

	for( it=priv->pdf_crs ; it ; it=it->next ){
		draw_page_background( page, cr, shift_x, y, paper_width, paper_height );
		pdf_cr = ( cairo_t * ) it->data;
		/*dy = y+(paper_height-render_height)/2.0;*/
		cairo_set_source_surface( cr, cairo_get_target( pdf_cr ), shift_x, y );
		cairo_paint( cr );
		y += paper_height + PAGE_SEPARATION_V_HEIGHT;
	}

	return( g_list_length( priv->pdf_crs ));
}

/*
 * draw the background of the page
 */
static void
draw_page_background( ofaRenderPage *page, cairo_t *cr, gdouble x, gdouble y, gdouble paper_width, gdouble paper_height )
{
	cairo_set_source_rgb( cr, COLOR_WHITE );
	cairo_rectangle( cr, x, y, paper_width, paper_height );
	cairo_fill( cr );
}

static void
on_render_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	gboolean ok;

	priv = page->priv;

	pdf_crs_free( &priv->pdf_crs );

	ok = ofa_iprintable2_preview( OFA_IPRINTABLE2( page ));

	if( ok && ( !priv->paper_width || !priv->paper_height )){
		ok &= setup_paper_size( page );
	}
	if( ok ){
		gtk_widget_queue_draw( priv->drawing_area );
	}

	gtk_widget_set_sensitive( priv->print_btn, ok );
}

static void
on_print_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofa_iprintable2_print( OFA_IPRINTABLE2( page ));
}

/*
 * ofa_render_page_push_context:
 * @page:
 * @context:
 * @width:
 * @height:
 */
static void
push_context( ofaRenderPage *page, cairo_t *context )
{
	static const gchar *thisfn = "ofa_render_page_push_context";
	ofaRenderPagePrivate *priv;
	cairo_surface_t *context_surface, *cr_surface;
	cairo_t *cr;
	gdouble width, height;

	priv = page->priv;

	get_rendering_size( page, &width, &height );

	if( 0 ){
		g_debug( "%s: page=%p, context=%p, render_width=%lf, render_height=%lf",
				thisfn, ( void * ) page, ( void * ) context, width, height );
	}

	context_surface = cairo_get_target( context );
	cr_surface = cairo_pdf_surface_create( NULL, width, height );
	cr = cairo_create( cr_surface );
	cairo_surface_destroy( cr_surface );

	cairo_set_source_surface( cr, context_surface, 0, 0 );
	cairo_paint( cr );
	priv->pdf_crs = g_list_append( priv->pdf_crs, cr );
}

static void
set_message( ofaRenderPage *page, const gchar *message, const gchar *color_name )
{
	ofaRenderPagePrivate *priv;
	GdkRGBA color;

	priv = page->priv;

	gdk_rgba_parse( &color, color_name );
	gtk_label_set_text( GTK_LABEL( priv->msg_label ), message );
	gtk_widget_override_color( priv->msg_label, GTK_STATE_FLAG_NORMAL, &color );
}

static void
pdf_crs_free( GList **pdf_crs )
{
	g_list_free_full( *pdf_crs, ( GDestroyNotify ) cairo_destroy );
	*pdf_crs = NULL;
}

static void
iprintable2_iface_init( ofaIPrintable2Interface *iface )
{
	static const gchar *thisfn = "ofa_pdf_balances_iprintable2_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprintable2_get_interface_version;
	iface->get_paper_name = iprintable2_get_paper_name;
	iface->get_page_orientation = iprintable2_get_page_orientation;
	iface->begin_print = iprintable2_begin_print;
	iface->draw_page = iprintable2_draw_page;
	iface->end_print = iprintable2_end_print;
}

static guint
iprintable2_get_interface_version( const ofaIPrintable2 *instance )
{
	return( 1 );
}

static const gchar *
iprintable2_get_paper_name( ofaIPrintable2 *instance )
{
	const gchar *paper_name;

	paper_name = OFA_RENDER_PAGE_GET_CLASS( instance )->get_paper_name ?
			OFA_RENDER_PAGE_GET_CLASS( instance )->get_paper_name( OFA_RENDER_PAGE( instance )) :
			NULL;
	g_return_val_if_fail( my_strlen( paper_name ), NULL );

	return( paper_name );
}

static GtkPageOrientation
iprintable2_get_page_orientation( ofaIPrintable2 *instance )
{
	GtkPageOrientation orientation;

	orientation = OFA_RENDER_PAGE_GET_CLASS( instance )->get_page_orientation ?
			OFA_RENDER_PAGE_GET_CLASS( instance )->get_page_orientation( OFA_RENDER_PAGE( instance )) :
			-1;
	g_return_val_if_fail( orientation != -1, -1 );

	return( orientation );
}

static void
iprintable2_begin_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_render_page_iprintable2_begin_print";
	gint pages_count;

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	pages_count = ofa_irenderable_begin_render(
			OFA_IRENDERABLE( instance ),
			gtk_print_context_get_cairo_context( context ),
			gtk_print_context_get_width( context ),
			gtk_print_context_get_height( context ));

	gtk_print_operation_set_n_pages( operation, pages_count );
}

/*
 * GtkPrintOperation signal handler
 * call once per page, with a page_num counted from zero
 */
static void
iprintable2_draw_page( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	static const gchar *thisfn = "ofa_render_page_iprintable2_draw_page";
	cairo_t *cr;

	g_debug( "%s: instance=%p, operation=%p, context=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context, page_num );

	cr = gtk_print_context_get_cairo_context( context );
	ofa_irenderable_render_page( OFA_IRENDERABLE( instance ), cr, page_num );
	push_context( OFA_RENDER_PAGE( instance ), cr );
}

static void
iprintable2_end_print( ofaIPrintable2 *instance, GtkPrintOperation *operation, GtkPrintContext *context )
{
	static const gchar *thisfn = "ofa_render_page_iprintable2_end_print";
	cairo_t *cr;

	g_debug( "%s: instance=%p, operation=%p, context=%p",
			thisfn, ( void * ) instance, ( void * ) operation, ( void * ) context );

	cr = gtk_print_context_get_cairo_context( context );
	ofa_irenderable_end_render( OFA_IRENDERABLE( instance ), cr );
}
