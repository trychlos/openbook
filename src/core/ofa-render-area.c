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

#include <cairo-pdf.h>

#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"

#include "core/ofa-render-area.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* UI
	 */
	GtkWidget  *drawing_area;
	GtkWidget  *event_box;

	/* runtime
	 */
	gdouble     page_width;
	gdouble     page_height;
	gdouble     margin_outside;
	gdouble     margin_between;
	gdouble     render_width;
	gdouble     render_height;
	GList      *pages;
}
	ofaRenderAreaPrivate;

#define COLOR_LIGHT_GRAY                0.90980, 0.90980, 0.90980	/* widget background: #e8e8e8 */
#define COLOR_WHITE                     1,       1,       1			/* page background: #ffffff */

static void       setup_bin( ofaRenderArea *self );
static gboolean   on_draw( GtkWidget *area, cairo_t *cr, ofaRenderArea *self );
static gint       do_draw( ofaRenderArea *self, cairo_t *cr, gdouble shift_x );
static void       draw_area_background( ofaRenderArea *self, cairo_t *cr, GtkWidget *area );
static void       draw_page_background( ofaRenderArea *self, cairo_t *cr, gdouble x, gdouble y );
static cairo_t   *create_context( ofaRenderArea *self, gdouble width, gdouble height );
static void       clear_rendered_pages( ofaRenderArea *self );
static void       icontext_iface_init( ofaIContextInterface *iface );
static guint      icontext_get_interface_version( void );
static GtkWidget *icontext_get_focused_widget( ofaIContext *instance );

G_DEFINE_TYPE_EXTENDED( ofaRenderArea, ofa_render_area, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaRenderArea )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONTEXT, icontext_iface_init ))

static void
render_area_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_render_area_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RENDER_AREA( instance ));

	/* free data members here */
	clear_rendered_pages( OFA_RENDER_AREA( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_area_parent_class )->finalize( instance );
}

static void
render_area_dispose( GObject *instance )
{
	ofaRenderAreaPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RENDER_AREA( instance ));

	priv = ofa_render_area_get_instance_private( OFA_RENDER_AREA( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_area_parent_class )->dispose( instance );
}

static void
ofa_render_area_init( ofaRenderArea *self )
{
	static const gchar *thisfn = "ofa_render_area_init";
	ofaRenderAreaPrivate *priv;

	g_return_if_fail( self && OFA_IS_RENDER_AREA( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_render_area_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->page_width = -1;
	priv->page_height = -1;
	priv->margin_between = -1;
	priv->margin_outside = -1;
	priv->render_width = -1;
	priv->render_height = -1;
	priv->pages = NULL;
}

static void
ofa_render_area_class_init( ofaRenderAreaClass *klass )
{
	static const gchar *thisfn = "ofa_render_area_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = render_area_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_area_finalize;
}

/**
 * ofa_render_area_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofaRenderArea instance.
 */
ofaRenderArea *
ofa_render_area_new( ofaIGetter *getter )
{
	ofaRenderArea *area;
	ofaRenderAreaPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	area = g_object_new( OFA_TYPE_RENDER_AREA, NULL );

	priv = ofa_render_area_get_instance_private( area );

	priv->getter = getter;

	setup_bin( area );

	return( area );
}

static void
setup_bin( ofaRenderArea *self )
{
	ofaRenderAreaPrivate *priv;
	GtkWidget *scrolled, *viewport, *drawing;

	priv = ofa_render_area_get_instance_private( self );

	/* have an event box to get the mouse/keyboard events */
	priv->event_box = gtk_event_box_new();
	gtk_container_add( GTK_CONTAINER( self ), priv->event_box );

	/* setup the drawing area */
	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( priv->event_box ), scrolled );

	viewport = gtk_viewport_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( scrolled ), viewport );

	drawing = gtk_drawing_area_new();
	gtk_widget_set_hexpand( drawing, TRUE );
	gtk_widget_set_vexpand( drawing, TRUE );
	gtk_container_add( GTK_CONTAINER( viewport ), drawing );

	g_signal_connect( drawing, "draw", G_CALLBACK( on_draw ), self );

	priv->drawing_area = drawing;
}

/**
 * ofa_render_area_set_page_size:
 * @area: this #ofaRenderArea widget.
 * @width: the paper width in points.
 * @height: the paper height in points.
 *
 * Set the page size.
 */
void
ofa_render_area_set_page_size( ofaRenderArea *area, gdouble width, gdouble height )
{
	static const gchar *thisfn = "ofa_render_area_set_page_size";
	ofaRenderAreaPrivate *priv;

	g_debug( "%s: area=%p, width=%lf, height=%lf",
			thisfn, ( void * ) area, width, height );

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	priv->page_width = width;
	priv->page_height = height;
}

/**
 * ofa_render_area_set_page_margins:
 * @area: this #ofaRenderArea widget.
 * @outside: the height of the vertical margin before the first page
 *  and after the last page, in points.
 * @between: the height of the vertical space between pages, in points.
 *
 * Set the page margins.
 */
void
ofa_render_area_set_page_margins( ofaRenderArea *area, gdouble outside, gdouble between )
{
	static const gchar *thisfn = "ofa_render_area_set_page_margins";
	ofaRenderAreaPrivate *priv;

	g_debug( "%s: area=%p, outside=%lf, between=%lf",
			thisfn, ( void * ) area, outside, between );

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	priv->margin_outside = outside;
	priv->margin_between = between;
}

/**
 * ofa_render_area_set_render_size:
 * @area: this #ofaRenderArea widget.
 * @width: the rendering width in points.
 * @height: the rendering height in points.
 *
 * Set the rendering size.
 */
void
ofa_render_area_set_render_size( ofaRenderArea *area, gdouble width, gdouble height )
{
	static const gchar *thisfn = "ofa_render_area_set_render_size";
	ofaRenderAreaPrivate *priv;

	g_debug( "%s: area=%p, width=%lf, height=%lf",
			thisfn, ( void * ) area, width, height );

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	priv->render_width = width;
	priv->render_height = height;
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
on_draw( GtkWidget *area, cairo_t *cr, ofaRenderArea *self )
{
	ofaRenderAreaPrivate *priv;
	gint widget_width, req_width, req_height, nb_pages;
	gdouble x;

	priv = ofa_render_area_get_instance_private( self );

	draw_area_background( self, cr, area );

	widget_width = gtk_widget_get_allocated_width( area );
	x = widget_width > priv->page_width ?
			(( gdouble ) widget_width - priv->page_width ) / 2.0 :
			0;

	nb_pages = do_draw( self, cr, x );

	req_width = widget_width > priv->page_width ? -1 : priv->page_width;

	req_height = nb_pages > 0 ?
			nb_pages * priv->page_height
				+ ( nb_pages-1 ) * priv->margin_between
				+ 2 * priv->margin_outside:
			-1;

	gtk_widget_set_size_request( area, req_width, req_height );

	/* TRUE stops other handlers from being invoked for the event */
	return( TRUE );
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
do_draw( ofaRenderArea *self, cairo_t *cr, gdouble shift_x )
{
	ofaRenderAreaPrivate *priv;
	GList *it;
	cairo_t *page;
	gdouble y, dx, dy;

	priv = ofa_render_area_get_instance_private( self );

	y = priv->margin_outside;
	dx = shift_x+(priv->page_width-priv->render_width)/2.0;

	for( it=priv->pages ; it ; it=it->next ){
		draw_page_background( self, cr, shift_x, y );
		page = ( cairo_t * ) it->data;
		dy = y+(priv->page_height-priv->render_height)/2.0;
		cairo_set_source_surface( cr, cairo_get_target( page ), dx, dy );
		cairo_paint( cr );
		y += priv->page_height + priv->margin_between;
	}

	return( g_list_length( priv->pages ));
}

/*
 * draw the background of the widget on which we are going to draw the
 * pages
 */
static void
draw_area_background( ofaRenderArea *self, cairo_t *cr, GtkWidget *area )
{
	gint widget_width, widget_height;

	widget_width = gtk_widget_get_allocated_width( area );
	widget_height = gtk_widget_get_allocated_height( area );

	cairo_set_source_rgb( cr, COLOR_LIGHT_GRAY );
	cairo_rectangle( cr, 0, 0, widget_width, widget_height );
	cairo_fill( cr );
}

/*
 * draw the background of the page
 */
static void
draw_page_background( ofaRenderArea *self, cairo_t *cr, gdouble x, gdouble y )
{
	ofaRenderAreaPrivate *priv;

	priv = ofa_render_area_get_instance_private( self );

	cairo_set_source_rgb( cr, COLOR_WHITE );
	cairo_rectangle( cr, x, y, priv->page_width, priv->page_height );
	cairo_fill( cr );
}

/**
 * ofa_render_area_clear:
 * @area: this #ofaRenderArea widget.
 *
 * Clear the rendering area, internally also clearing the previously
 * rendered pages.
 */
void
ofa_render_area_clear( ofaRenderArea *area )
{
	static const gchar *thisfn = "ofa_render_area_clear";
	ofaRenderAreaPrivate *priv;
	cairo_t *cr;

	g_debug( "%s: area=%p", thisfn, ( void * ) area );

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	/* clear the drawing area */
	cr = create_context( area, priv->render_width, priv->render_height );
	draw_area_background( area, cr, priv->drawing_area );
	cairo_destroy( cr );
	gtk_widget_queue_draw( priv->drawing_area );

	/* clear previously rendered pages */
	clear_rendered_pages( area );
}

/**
 * ofa_render_area_new_context:
 * @area: this #ofaRenderArea widget.
 *
 * Returns: a new #cairo_t context, suitable to render a PDF area on
 * the known page and rendering sizes.
 */
cairo_t *
ofa_render_area_new_context( ofaRenderArea *area )
{
	static const gchar *thisfn = "ofa_render_area_new_context";
	ofaRenderAreaPrivate *priv;

	g_debug( "%s: area=%p", thisfn, ( void * ) area );

	g_return_val_if_fail( area && OFA_IS_RENDER_AREA( area ), NULL );

	priv = ofa_render_area_get_instance_private( area );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( create_context( area, priv->page_width, priv->page_height ));
}

/*
 * Creates a cairo context suitable to render a PDF page of given size
 */
static cairo_t *
create_context( ofaRenderArea *self, gdouble width, gdouble height )
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_pdf_surface_create( NULL, width, height );
	cr = cairo_create( surface );
	cairo_surface_destroy( surface );

	return( cr );
}

/**
 * ofa_render_area_append_page:
 * @area: this #ofaRenderArea widget.
 * @page: a page rendered on #cairo_t pdf context.
 *
 * Appends the @page to our list of pages to be drawn.
 *
 * Note that we only record here a pointer to the structure, and do not
 * copy it. The caller keeps its (pseudo) reference and keeps to have
 * to manager the page.
 */
void
ofa_render_area_append_page( ofaRenderArea *area, cairo_t *page )
{
	ofaRenderAreaPrivate *priv;

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	priv->pages = g_list_append( priv->pages, page );
}

/**
 * ofa_render_area_queue_draw:
 * @area: this #ofaRenderArea widget.
 *
 * Queue a drawing request for the entire drawing area.
 */
void
ofa_render_area_queue_draw( ofaRenderArea *area )
{
	static const gchar *thisfn = "ofa_render_area_queue_draw";
	ofaRenderAreaPrivate *priv;

	g_debug( "%s: area=%p", thisfn, ( void * ) area );

	g_return_if_fail( area && OFA_IS_RENDER_AREA( area ));

	priv = ofa_render_area_get_instance_private( area );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_widget_queue_draw( priv->drawing_area );
}

/*
 * Clear the list of rendered pages.
 */
static void
clear_rendered_pages( ofaRenderArea *self )
{
	ofaRenderAreaPrivate *priv;

	priv = ofa_render_area_get_instance_private( self );

	g_list_free( priv->pages );
	priv->pages = NULL;
}

/*
 * ofaIContext interface management
 */
static void
icontext_iface_init( ofaIContextInterface *iface )
{
	static const gchar *thisfn = "ofa_render_area_icontext_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icontext_get_interface_version;
	iface->get_focused_widget = icontext_get_focused_widget;
}

static guint
icontext_get_interface_version( void )
{
	return( 1 );
}

static GtkWidget *
icontext_get_focused_widget( ofaIContext *instance )
{
	ofaRenderAreaPrivate *priv;

	priv = ofa_render_area_get_instance_private( OFA_RENDER_AREA( instance ));

	return( priv->event_box );
}
