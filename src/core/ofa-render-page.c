/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>
#include <math.h>

#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-iprintable.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-render-page.h"

#include "core/ofa-render-area.h"

/* private instance data
 */
typedef struct {

	/* UI
	 */
	GtkWidget     *paned;
	ofaRenderArea *render_area;
	GtkWidget     *status_box;
	GtkWidget     *msg_label;
	myProgressBar *progress_bar;
	GtkWidget     *render_btn;
	GtkWidget     *print_btn;

	/* from the derived class
	 * this takes paper name and orientation into account
	 */
	gdouble        paper_width;				/* in points */
	gdouble        paper_height;
	gdouble        render_width;			/* in points */
	gdouble        render_height;
	GList         *dataset;
	GList         *pages;
}
	ofaRenderPagePrivate;

/*
 * A4 sheet size is 210 x 297 mm = 8.26772 x 11.69291 in
 *                               = 595.27559 x 841.88976 points
 *                               ~ 595 x 841
 *                               ~ 2.835 points/mm
 */
#define PAGE_SEPARATION_V_HEIGHT        4.0		/* separation between two pages */
#define PAGE_EXT_MARGIN_V_HEIGHT        2.0		/* a margin before the first and after the last page */

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
static void               render_page_free_dataset( ofaRenderPage *page );
static void               on_render_clicked( GtkButton *button, ofaRenderPage *page );
static void               render_pdf_pages( ofaRenderPage *page );
static void               on_print_clicked( GtkButton *button, ofaRenderPage *page );
static void               clear_rendered_pages( ofaRenderPage *self );
static void               set_message( ofaRenderPage *page, const gchar *message, const gchar *color_name );
static void               progress_begin( ofaRenderPage *self );
static void               progress_end( ofaRenderPage *self );
static void               on_irenderable_render_page( ofaIRenderable *instance, gboolean paginating, guint page_num, guint pages_count, ofaRenderPage *self );
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
	g_return_if_fail( instance && OFA_IS_RENDER_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
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
	static const gchar *thisfn = "ofa_render_page_paned_page_v_setup_view";
	ofaRenderPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_render_page_get_instance_private( OFA_RENDER_PAGE( page ));

	priv->paned = GTK_WIDGET( paned );

	ofa_irenderable_set_getter( OFA_IRENDERABLE( page ), ofa_page_get_getter( OFA_PAGE( page )));

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
	GtkWidget *grid;

	priv = ofa_render_page_get_instance_private( self );

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 2 );

	priv->render_area = ofa_render_area_new( ofa_page_get_getter( OFA_PAGE( self )));
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( priv->render_area ), 0, 0, 1, 1 );

	/* setup the box
	 * it defaults to contain a message zone (GtkLabel)
	 * but during rendering, the label is substituted withg a progress bar
	 */
	priv->status_box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_grid_attach( GTK_GRID( grid ), priv->status_box, 0, 1, 1, 1 );

	priv->msg_label = gtk_label_new( NULL );
	gtk_label_set_xalign( GTK_LABEL( priv->msg_label ), 0 );
	gtk_container_add( GTK_CONTAINER( priv->status_box ), priv->msg_label );

	g_signal_connect( self, "ofa-render-page", G_CALLBACK( on_irenderable_render_page ), self );

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

	g_object_unref( page_setup );
	gtk_paper_size_free( paper_size );

	ofa_render_area_set_page_size( priv->render_area, priv->paper_width, priv->paper_height );
	ofa_render_area_set_page_margins( priv->render_area, PAGE_EXT_MARGIN_V_HEIGHT, PAGE_SEPARATION_V_HEIGHT );
	ofa_render_area_set_render_size( priv->render_area, priv->render_width, priv->render_height );
}

/**
 * ofa_render_page_set_args_changed:
 * @page: this #ofaRenderPage page.
 * @is_valid: whether the args are valid.
 * @message: [allow-none]: the error message to be displayed.
 *
 * Called by the derived class, which handles its own argument dialog,
 * on each argument change, telling us if args are valid or not.
 *
 * Dataset and drawing area are cleared on each argument change.
 *
 * Render and print actions are always enabled as soon as arguments are
 * valid. This let the user re-render the pages as often as it wishes
 * in the case where some underlying data may have changed (and because
 * we don't care to connect to #ofaISignaler signaling system here).
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

	/* clear all datas as well as drawing area */
	render_page_free_dataset( page );
	clear_rendered_pages( page );
	ofa_render_area_clear( priv->render_area );

	gtk_widget_set_sensitive( priv->render_btn, is_valid );
	gtk_widget_set_sensitive( priv->print_btn, is_valid );
	set_message( page, message ? message : "", MSG_ERROR );
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

/*
 * render_page_free_dataset:
 * @page:
 *
 * Free the current dataset after an argument has changed.
 */
static void
render_page_free_dataset( ofaRenderPage *page )
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
 * Rendering is a two-phases action:
 * - render the pages, obtaining a #GList of rendered pages
 * - draw this list of pages to the drawing area
 */
static void
on_render_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	/* clear all datas as well as drawing area */
	render_page_free_dataset( page );
	clear_rendered_pages( page );
	ofa_render_area_clear( priv->render_area );

	/* render pages */
	render_pdf_pages( page );

	/* and draw pages to the drawing area */
	ofa_render_area_queue_draw( priv->render_area );
}

static void
render_pdf_pages( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	gchar *str;
	cairo_t *cr, *page_cr;
	gint pages_count, i;

	priv = ofa_render_page_get_instance_private( page );

	progress_begin( page );

	if( !priv->dataset ){
		priv->dataset = get_dataset( page );
		clear_rendered_pages( page );
		ofa_render_area_clear( priv->render_area );
	}

	if( !priv->pages ){
		cr = ofa_render_area_new_context( priv->render_area );
		pages_count = ofa_irenderable_begin_render(
				OFA_IRENDERABLE( page ), cr, priv->render_width, priv->render_height, priv->dataset );

		for( i=0 ; i<pages_count ; ++i ){
			page_cr = ofa_render_area_new_context( priv->render_area );
			ofa_irenderable_render_page( OFA_IRENDERABLE( page ), page_cr, i );
			priv->pages = g_list_append( priv->pages, page_cr );
			ofa_render_area_append_page( priv->render_area, page_cr );
		}

		ofa_irenderable_end_render( OFA_IRENDERABLE( page ), cr );
		cairo_destroy( cr );

	} else {
		pages_count = g_list_length( priv->pages );
	}

	progress_end( page );

	if( pages_count == 1 ){
		str = g_strdup_printf( _( "%d rendered page." ), pages_count );
	} else {
		str = g_strdup_printf( _( "%d rendered pages." ), pages_count );
	}

	set_message( page, str, MSG_INFO );
}

/*
 * Printing is a two-phases action:
 * - render the pages, obtaining a #GList of rendered pages
 * - print this list of pages, which happens to need rendering another
 *   time each page to print context
 */
static void
on_print_clicked( GtkButton *button, ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;
	gchar *str;
	guint pages_count;

	priv = ofa_render_page_get_instance_private( page );

	if( !priv->pages ){
		render_pdf_pages( page );
	}

	progress_begin( page );
	ofa_iprintable_print( OFA_IPRINTABLE( page ));
	progress_end( page );

	pages_count = g_list_length( priv->pages );

	if( pages_count == 1 ){
		str = g_strdup_printf( _( "%d printed page." ), pages_count );
	} else {
		str = g_strdup_printf( _( "%d printed pages." ), pages_count );
	}

	set_message( page, str, MSG_INFO );
}

/*
 * Clear the list of rendered pages.
 */
static void
clear_rendered_pages( ofaRenderPage *self )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( self );

	g_list_free_full( priv->pages, ( GDestroyNotify ) cairo_destroy );
	priv->pages = NULL;
}

static void
set_message( ofaRenderPage *page, const gchar *message, const gchar *spec )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( page );

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), message );
	my_style_add( priv->msg_label, spec );
}

/**
 * ofa_render_page_get_icontext:
 * @page: this #ofaRenderPage page.
 *
 * Returns: the widget as an #ofaIContext where the user can right-clicks
 * to open a contextual submenu, here the drawing area.
 */
ofaIContext *
ofa_render_page_get_icontext( ofaRenderPage *page )
{
	ofaRenderPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RENDER_PAGE( page ), NULL );
	g_return_val_if_fail( !OFA_PAGE( page )->prot->dispose_has_run, NULL );

	priv = ofa_render_page_get_instance_private( page );

	return( OFA_ICONTEXT( priv->render_area ));
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
 * During pagination and rendering, replace the message label from the
 * status box by a progress bar
 */
static void
progress_begin( ofaRenderPage *self )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( self );

	g_object_ref( priv->msg_label );
	gtk_container_remove( GTK_CONTAINER( priv->status_box ), priv->msg_label );

	priv->progress_bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( priv->status_box ), GTK_WIDGET( priv->progress_bar ));

	gtk_widget_show_all( priv->status_box );
}

static void
progress_end( ofaRenderPage *self )
{
	ofaRenderPagePrivate *priv;

	priv = ofa_render_page_get_instance_private( self );

	gtk_container_remove( GTK_CONTAINER( priv->status_box ), GTK_WIDGET( priv->progress_bar ));
	priv->progress_bar = NULL;

	gtk_container_add( GTK_CONTAINER( priv->status_box ), priv->msg_label );
	g_object_unref( priv->msg_label );

	gtk_widget_show_all( priv->status_box );
}

static void
on_irenderable_render_page( ofaIRenderable *instance, gboolean paginating, guint page_num, guint pages_count, ofaRenderPage *self )
{
	ofaRenderPagePrivate *priv;
	gdouble progress;
	gchar *text;

	priv = ofa_render_page_get_instance_private( self );

	if( priv->progress_bar ){
		g_return_if_fail( MY_IS_PROGRESS_BAR( priv->progress_bar ));

		if( paginating ){
			text = g_strdup_printf( _( "Paginating %u" ), page_num );
			g_signal_emit_by_name( priv->progress_bar, "my-text", text );
			g_free( text );

		} else {
			gtk_progress_bar_set_show_text( GTK_PROGRESS_BAR( priv->progress_bar ), FALSE );
			progress = ( gdouble ) page_num / ( gdouble ) pages_count;
			g_signal_emit_by_name( priv->progress_bar, "my-double", progress );
		}
	}
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
