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
#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-irenderer.h"

/* data associated to each implementor object
 */
typedef struct {

	/* interface initialization
	 */
	ofaIGetter        *getter;
	GList             *renderer_plugins;

	/* begin_render() initialization
	 */
	gdouble            render_width;
	gdouble            render_height;
	GList             *dataset;

	/* begin_render() computings
	 */
	gdouble            header_columns_height;
	gdouble            footer_height;
	gdouble            group_sep_line_height;
	gdouble            max_y;
	guint              pages_count;

	/* data reset from each api entry point:
	 * begin_render(), render_page() and end_render()
	 * cf. create_temp_context()
	 */
	cairo_t           *in_context;			/* the provided cairo context with its associated pango layout */
	PangoLayout       *in_layout;
	cairo_t           *temp_context;		/* a temp context and its layout to be used for pagination */
	PangoLayout       *temp_layout;

	/* runtime data
	 */
	guint              page_num;
	GList             *line;
	gboolean           paginating;
	cairo_t           *current_context;
	PangoLayout       *current_layout;
	gdouble            last_y;
	GList             *prev_rendered;
	guint              count_rendered;
	gboolean           have_groups;
	ofeIRenderableMode line_mode;
}
	sIRenderable;

#define IRENDERABLE_LAST_VERSION         1
#define IRENDERABLE_DATA                 "ofa-irenderable-data"

/* these are default values for rendering
 * each of these is expected to be overridable by the implementation
 *
 * Usage                   Default font                    Default foreground color  Default background color
 * ----------------------  ------------------------------  ------------------------  ------------------------
 * Dossier identification  st_default_header_dossier_font  COLOR_HEADER_DOSSIER      (none)
 * Title                   st_default_header_title_font    COLOR_HEADER_TITLE        (none)
 * Columns header          st_default_header_columns_font  COLOR_HEADER_COLUMNS_FG   COLOR_HEADER_COLUMNS_BG
 * Body                    st_default_body_font            COLOR_BLACK               (none)
 */
#define COLOR_BLACK                     0,      0,      0
#define COLOR_DARK_CYAN                 0,      0.3765, 0.5
#define COLOR_DARK_RED                  0.5,    0,      0
#define COLOR_GRAY                      0.6,    0.6,    0.6			/* #999999 */
#define COLOR_LIGHT_GRAY                0.9375, 0.9375, 0.9375		/* #f0f0f0 */
#define COLOR_MIDDLE_GRAY               0.7688, 0.7688, 0.7688		/* #c4c4c4 */
#define COLOR_WHITE                     1,      1,      1

#define COLOR_HEADER_DOSSIER            COLOR_DARK_RED
#define COLOR_HEADER_TITLE              COLOR_DARK_CYAN
#define COLOR_HEADER_NOTES              COLOR_BLACK
#define COLOR_HEADER_COLUMNS_BG         COLOR_DARK_CYAN
#define COLOR_HEADER_COLUMNS_FG         COLOR_WHITE
#define COLOR_BODY                      COLOR_BLACK
#define COLOR_FOOTER                    COLOR_GRAY
#define COLOR_NO_DATA                   COLOR_MIDDLE_GRAY

static const gchar  *st_default_body_font                  = "Sans 6";
static const gchar  *st_default_header_dossier_font        = "Sans Bold Italic 11";
static const gchar  *st_default_header_title_font          = "Sans Bold 10";
static const gchar  *st_default_header_columns_font        = "Sans Bold 5";
static const gchar  *st_default_summary_font               = "Sans Bold 7";
static const gchar  *st_default_group_font                 = "Sans Bold 6";
static const gchar  *st_default_report_font                = "Sans 6";
static const gchar  *st_default_footer_font                = "Sans Italic 5";
static const gchar  *st_default_no_data_font               = "Sans 18";

static const gdouble st_page_margin                        = 2.0;
static const gdouble st_body_vspace_rate                   = 0.25;
static const gdouble st_column_hspacing                    = 4.0;

/* signals defined here
 */
enum {
	RENDER = 0,
	N_SIGNALS
};

static gint  st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations         =   0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIRenderableInterface *klass );
static void          interface_base_finalize( ofaIRenderableInterface *klass );
static void          setup_renderer_plugins( ofaIRenderable *instance, sIRenderable *sdata );
static void          create_temp_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata );
static void          clear_runtime_data( ofaIRenderable *instance, sIRenderable *sdata );
static gboolean      draw_page( ofaIRenderable *instance, sIRenderable *sdata );
static gboolean      draw_line( ofaIRenderable *instance, guint line_num, sIRenderable *sdata );
static void          irenderable_draw_page_header_dossier( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_page_header_title( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_page_header_notes( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_page_header_columns( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_top_summary( ofaIRenderable *instance, sIRenderable *sdata );
static gboolean      irenderable_is_new_group( ofaIRenderable *instance, GList *line, GList *next, ofeIRenderableBreak *sep, sIRenderable *sdata );
static void          irenderable_draw_group_header( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       draw_group_separation( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_group_header_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_top_report( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_bottom_report( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_bottom_report_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_group_footer( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_group_footer_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_last_summary( ofaIRenderable *instance, sIRenderable *sdata );
static void          irenderable_draw_page_footer( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_page_footer_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          set_rgb( gdouble *r, gdouble *g, gdouble *b, gdouble sr, gdouble sg, gdouble sb );
static void          set_font( PangoLayout *layout, const gchar *font_str, gdouble *size );
static gdouble       set_text( PangoLayout *layout, cairo_t *context, gdouble x, gdouble y, const gchar *text, PangoAlignment align );
static sIRenderable *get_instance_data( const ofaIRenderable *instance );
static void          on_instance_finalized( sIRenderable *sdata, void *instance );

/**
 * ofa_irenderable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_irenderable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_irenderable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_irenderable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIRenderableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIRenderable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIRenderableInterface *klass )
{
	static const gchar *thisfn = "ofa_irenderable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIRenderable::ofa-render-page:
		 * @page_num: page number, counted from 1.
		 * @pages_count:
		 *  while pagination, is equal to current @page_num;
		 *  equal to total pages count during rendering.
		 *
		 * The signal is emitted each time a page is about to drawned,
		 * first when paginating, then when rendering.
		 * If print is requested, then all pages are re-drawn another
		 * time.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaIRenderable *instance,
		 * 		                    gboolean       paginating,
		 * 							guint          page_num,
		 * 							guint          pages_count,
		 * 							gpointer       user_data );
		 */
		st_signals[ RENDER ] = g_signal_new_class_handler(
					"ofa-render-page",
					OFA_TYPE_IRENDERABLE,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_BOOLEAN, G_TYPE_UINT, G_TYPE_UINT );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIRenderableInterface *klass )
{
	static const gchar *thisfn = "ofa_irenderable_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_irenderable_get_interface_last_version:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_irenderable_get_interface_last_version( const ofaIRenderable *instance )
{
	return( IRENDERABLE_LAST_VERSION );
}

/**
 * ofa_irenderable_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_irenderable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IRENDERABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIRenderableInterface * ) iface )->get_interface_version ){
		version = (( ofaIRenderableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIRenderable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_irenderable_set_getter:
 * @instance: this #ofaIRenderable instance.
 * @getter: the #ofaIGetter of the application.
 *
 * Set the @getter.
 *
 * This function is called at the very beginning of the #ofaRenderPage
 * initialization. This is a good time to do once time initializations.
 */
void
ofa_irenderable_set_getter( ofaIRenderable *instance, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_irenderable_set_getter";
	sIRenderable *sdata;

	g_debug( "%s: instance=%p, getter=%p", thisfn, ( void * ) instance, ( void * ) getter );

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_instance_data( instance );

	sdata->getter = getter;

	setup_renderer_plugins( instance, sdata );
}

static void
setup_renderer_plugins( ofaIRenderable *instance, sIRenderable *sdata )
{
	sdata->renderer_plugins = ofa_igetter_get_for_type( sdata->getter, OFA_TYPE_IRENDERER );
}

/**
 * ofa_irenderable_begin_render:
 * @instance: this #ofaIRenderable instance.
 * @cr: the target cairo context.
 * @render_width: the max renderable width in points.
 * @render_height: the max renderable height in points.
 *  the max renderable size is the page size, less the print margins.
 *
 * The first entry point of the interface.
 * This initialize all main variables, and paginates the rendering.
 * Must be called before any rendering.
 *
 * Returns: the pages count.
 */
gint
ofa_irenderable_begin_render( ofaIRenderable *instance, cairo_t *cr, gdouble render_width, gdouble render_height, GList *dataset )
{
	static const gchar *thisfn = "ofa_irenderable_begin_render";
	sIRenderable *sdata;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );
	sdata->paginating = TRUE;

	sdata->render_width = render_width;
	sdata->render_height = render_height;
	sdata->dataset = dataset;
	create_temp_context( instance, cr, sdata );

	sdata->current_context = sdata->temp_context;
	sdata->current_layout = sdata->temp_layout;

	sdata->have_groups = ( OFA_IRENDERABLE_GET_INTERFACE( instance )->is_new_group != NULL );

	sdata->footer_height = get_page_footer_height( instance, sdata );
	sdata->max_y = render_height - sdata->footer_height;
	sdata->group_sep_line_height = draw_group_separation( instance, sdata );

	g_debug( "%s: instance=%p, cr=%p, render_width=%lf, render_height=%lf, max_y=%lf, footer_height=%lf, dataset_count=%d",
			thisfn, ( void * ) instance, ( void * ) cr, render_width, render_height, sdata->max_y,
			sdata->footer_height, g_list_length( sdata->dataset ));

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->begin_render ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->begin_render( instance );
	}
	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		ofa_irenderer_begin_render( OFA_IRENDERER( it->data ), instance );
	}

	/* run the pagination
	 */
	clear_runtime_data( instance, sdata );
	sdata->pages_count = 1;

	while( draw_page( instance, sdata )){
		sdata->page_num += 1;
		sdata->pages_count += 1;
	}

	sdata->paginating = FALSE;

	g_debug( "%s: about to render %d page(s)", thisfn, sdata->pages_count );

	clear_runtime_data( instance, sdata );

	return( sdata->pages_count );
}

/**
 * ofa_irenderable_render_page:
 * @instance: this #ofaIRenderable instance.
 * @cr: the target cairo context.
 * @page_number: the page number, counted from zero.
 *
 * The second main entry point of the interface.
 * Must be called once for each page in order each page be rendered.
 */
void
ofa_irenderable_render_page( ofaIRenderable *instance, cairo_t *cr, guint page_number )
{
	static const gchar *thisfn = "ofa_irenderable_render_page";
	sIRenderable *sdata;
	gboolean done;
	GList *it;

	g_debug( "%s: instance=%p, cr=%p, page_number=%d",
			thisfn, ( void * ) instance, ( void * ) cr, page_number );

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	done = FALSE;
	sdata = get_instance_data( instance );

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_render_page( OFA_IRENDERER( it->data ), instance );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->render_page ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->render_page( instance );
			done = TRUE;
		}
	}

	if( !done ){
		create_temp_context( instance, cr, sdata );

		sdata->current_context = sdata->in_context;
		sdata->current_layout = sdata->in_layout;

		sdata->page_num = page_number;
		draw_page( instance, sdata );
	}
}

/**
 * ofa_irenderable_end_render:
 * @instance: this #ofaIRenderable instance.
 * @cr: the target cairo context.
 *
 * The last entry point of the interface.
 * Must be called after all pages have been rendered.
 */
void
ofa_irenderable_end_render( ofaIRenderable *instance, cairo_t *cr )
{
	static const gchar *thisfn = "ofa_irenderable_end_render";
	sIRenderable *sdata;
	GList *it;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		ofa_irenderer_end_render( OFA_IRENDERER( it->data ), instance );
	}

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render( instance );
	}

	g_debug( "%s: dataset_count=%u, rendered_count=%u",
			thisfn, g_list_length( sdata->dataset ), sdata->count_rendered );
}

/*
 * Save the provided context, and create an associated layout
 * this is called on each entry point of the interface:
 * - begin_render(), render_page() and end_render().
 *
 * Simultaneously create a temp context which is used:
 * - first during pagination phase
 * - then each time we need to compute some dimension without actually
 *   drawing anything
 */
static void
create_temp_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata )
{
	static const gchar *thisfn = "ofa_irenderable_create_temp_context";
	cairo_surface_t *surface;

	/* save the provided context */
	sdata->in_context = context;

	/* create an associated pango layout */
	g_clear_object( &sdata->in_layout );
	sdata->in_layout = pango_cairo_create_layout( sdata->in_context );

	/* create a temp context */
	if( sdata->temp_context ){
		cairo_destroy( sdata->temp_context );
	}
	surface = cairo_pdf_surface_create( NULL, sdata->render_width, sdata->render_height );
	sdata->temp_context = cairo_create( surface );
	cairo_surface_destroy( surface );

	/* create an associated temp pango layout */
	g_clear_object( &sdata->temp_layout );
	sdata->temp_layout = pango_cairo_create_layout( sdata->temp_context );

	/* draw a text in input context/layout and get its dimensions
	   => check that dimensions are same in both input and temp contexts */
	if( 0 ){
		set_font( sdata->in_layout, "Sans 8", NULL );
		gdouble height = set_text( sdata->in_layout, sdata->in_context, 0, 0, "This is a text", PANGO_ALIGN_LEFT );
		g_debug( "%s: input context/layout: height=%lf", thisfn, height );
		set_font( sdata->temp_layout, "Sans 8", NULL );
		height = set_text( sdata->temp_layout, sdata->temp_context, 0, 0, "This is a text", PANGO_ALIGN_LEFT );
		g_debug( "%s: temp context/layout: height=%lf", thisfn, height );
	}
}

static void
clear_runtime_data( ofaIRenderable *instance, sIRenderable *sdata )
{
	sdata->prev_rendered = NULL;
	sdata->count_rendered = 0;
	sdata->page_num = 0;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->clear_runtime_data ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->clear_runtime_data( instance );
	}
}

/*
 * Used when paginating first, then for actually drawing
 *
 * Returns: %TRUE while there is still page(s) to be printed,
 * %FALSE at the end.
 *
 * The returned value is only used while paginating.
 */
static gboolean
draw_page( ofaIRenderable *instance, sIRenderable *sdata )
{
	static const gchar *thisfn = "ofa_irenderable_draw_page";
	GList *next;
	guint count;
	gboolean is_last;
	gdouble req_height;

	g_signal_emit_by_name( instance, "ofa-render-page",
			sdata->paginating, 1+sdata->page_num, sdata->pages_count ? sdata->pages_count : 1+sdata->page_num );

	g_debug( "%s: instance=%p, paginating=%s, page_num=%u, dataset_count=%u",
			thisfn, ( void * ) instance, sdata->paginating ? "True":"False",
			sdata->page_num, g_list_length( sdata->dataset ));

	sdata->last_y = 0;

	irenderable_draw_page_header_dossier( instance, sdata );
	irenderable_draw_page_header_title( instance, sdata );
	irenderable_draw_page_header_notes( instance, sdata );
	irenderable_draw_page_header_columns( instance, sdata );
	irenderable_draw_top_summary( instance, sdata );

	sdata->line = sdata->prev_rendered ? g_list_next( sdata->prev_rendered ) : sdata->dataset;

	if( sdata->line ){
		irenderable_draw_top_report( instance, sdata );
	}

	for( count=0 ; sdata->line ; count+=1 ){
		next = g_list_next( sdata->line );		/* may be %NULL */
		if( !draw_line( instance, count, sdata )){
			if( 0 ){
				g_debug( "%s: draw_line returned False, line_num=%u, line=%p, next=%p",
						thisfn, count, ( void * ) sdata->line, ( void * ) next );
			}
			break;
		}
		sdata->line = next;
	}
	if( 0 ){
		g_debug( "%s: rendered_count=%u, line=%p", thisfn, sdata->count_rendered, sdata->line );
	}

	/* end of the last page ? */
	is_last = FALSE;

	if( !sdata->line ){
		req_height = ofa_irenderable_get_last_summary_height( instance );
		if( sdata->last_y + req_height <= sdata->max_y ){
			is_last = TRUE;
			irenderable_draw_last_summary( instance, sdata );
		}
	}

	irenderable_draw_page_footer( instance, sdata );

	if( 0 ){
		g_debug( "%s: is_last=%s", thisfn, is_last ? "True":"False" );
	}

	return( !is_last );
}

/*
 * @page_num: counted from 0
 * @line_num: line number in the page, counted from 0
 * @line: the line candidate to be printed
 *  called from draw_page(), which makes sure @line is non null
 * @next: the next line after this one, may be %NULL if we are at the
 *  end of the list
 *
 * Returns: %TRUE if this line has been printed, %FALSE else.
 *
 * Rationale: ofaIRenderable interface cannot and must not be stucked
 * to only one or two group breaks, and it would be very complex to try
 * to handle a variable count of group breaks here.
 *
 * So we just ask the implementation how much height the line will take,
 * given the next line which may lead to a group break, knowing that we
 * do not want any orphan line on the bottom of the page.
 *
 * When about to draw the @line:
 *
 * - must have enough place to draw it + the bottom report (if no footer)
 *
 * - must have enough place to draw the prev group footer if any
 *   + the line group header
 *   + the @line itself
 */
static gboolean
draw_line( ofaIRenderable *instance, guint line_num, sIRenderable *sdata )
{
	static const gchar *thisfn = "ofa_irenderable_draw_line";
	gdouble y, req_height, line_height, font_height, bottom_report_height;
	gdouble r, g, b;
	gboolean new_group, draw_group_header, draw_group_footer;
	ofeIRenderableBreak group_sep, next_sep;

	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));
	line_height = ofa_irenderable_get_line_height( instance );
	req_height = line_height;
	bottom_report_height = get_bottom_report_height( instance, sdata );

	/* First take a glance at which may must come before the line
	 * do we need a group header before @line ?
	 */
	new_group = FALSE;
	draw_group_header = FALSE;
	if( sdata->have_groups ){
		new_group = irenderable_is_new_group( instance, sdata->prev_rendered, sdata->line, &group_sep, sdata );
		if( new_group ){
			switch( group_sep ){
				case IRENDERABLE_BREAK_NEW_PAGE:
					if( line_num > 0 ){
						/* new page on group break: no bottom report */
						return( FALSE );
					}
					break;
				case IRENDERABLE_BREAK_BLANK_LINE:
					if( line_num > 0 ){
						req_height += line_height;
					}
					break;
				case IRENDERABLE_BREAK_SEP_LINE:
					if( line_num > 0 ){
						req_height += sdata->group_sep_line_height;
					}
					break;
				default:
					break;
			}
			req_height += get_group_header_height( instance, sdata );
			draw_group_header = TRUE;
		}
	}

	/* Next take a glance at which will come after the line
	 * will we need a group footer or a bottom report ?
	 */
	new_group = FALSE;
	draw_group_footer = FALSE;
	if( sdata->have_groups ){
		new_group = irenderable_is_new_group( instance, sdata->line, sdata->line->next, &next_sep, sdata );
		if( new_group ){
			req_height += get_group_footer_height( instance, sdata );
			draw_group_footer = TRUE;
		}
	}
	if( !new_group ){
		req_height += bottom_report_height;
	}

	/* not enough space to draw at least our unbreakable lines
	 * so have a new page (maybe with a bottom report)
	 */
	if( sdata->last_y + req_height > sdata->max_y ){
		if( !draw_group_header ){
			irenderable_draw_bottom_report( instance, sdata );
		}
		return( FALSE );
	}

	/* so, we are OK to draw the line(s) !
	 */
	if( draw_group_header ){
		if( line_num > 0 ){
			if( group_sep == IRENDERABLE_BREAK_BLANK_LINE ){
				sdata->last_y += line_height;

			} else if( group_sep == IRENDERABLE_BREAK_SEP_LINE ){
				draw_group_separation( instance, sdata );
			}
		}
		irenderable_draw_group_header( instance, sdata );
	}

	/* we are using a unique font to draw the lines */
	y = sdata->last_y;
	ofa_irenderable_set_font( instance, ofa_irenderable_get_body_font( instance ));

	/* have a rubber every other line */
	if( sdata->line_mode != IRENDERABLE_MODE_NOPRINT ){
		if( line_num % 2 ){
			font_height = ofa_irenderable_get_text_height( instance );
			ofa_irenderable_draw_rubber( instance, y-(line_height-font_height)*0.5, line_height );
		}
	}

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_line ){
		ofa_irenderable_get_body_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_line( instance );
	}

	if( sdata->line_mode != IRENDERABLE_MODE_NOPRINT ){
		y += line_height;
		sdata->last_y = y;
	}

	sdata->prev_rendered = sdata->line;
	sdata->count_rendered += 1;

	if( draw_group_footer ){
		if( 0 ){
			g_debug( "%s: draw group footer for page_num=%u, line_num=%u", thisfn, sdata->page_num, line_num );
		}
		irenderable_draw_group_footer( instance, sdata );

	} else if( sdata->last_y + bottom_report_height + line_height > sdata->max_y ){
		irenderable_draw_bottom_report( instance, sdata );
		return( FALSE );
	}

	return( TRUE );
}

static void
irenderable_draw_page_header_dossier( ofaIRenderable *instance, sIRenderable *sdata )
{
	gchar *label;
	gdouble y, height, r, g, b;
	GList *it;
	gboolean done;

	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_draw_page_header_dossier( OFA_IRENDERER( it->data ), instance );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_dossier ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_dossier( instance );
			done = TRUE;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_label ){

			ofa_irenderable_get_dossier_color( instance, &r, &g, &b );
			ofa_irenderable_set_color( instance, r, g, b );
			ofa_irenderable_set_font( instance, ofa_irenderable_get_dossier_font( instance, sdata->page_num ));

			y = sdata->last_y;
			label = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_label( instance );
			height = ofa_irenderable_set_text( instance, 0, y, label, PANGO_ALIGN_LEFT );
			g_free( label );

			y += height;
			sdata->last_y = y;
		}
	}
}

static void
irenderable_draw_page_header_title( ofaIRenderable *instance, sIRenderable *sdata )
{
	gchar *label;
	gdouble y, height, r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_title ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_title( instance );

	} else if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_label ){

		ofa_irenderable_get_title_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_title_font( instance, sdata->page_num ));

		y = sdata->last_y;
		label = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_label( instance );
		height = ofa_irenderable_set_text( instance, sdata->render_width/2, y, label, PANGO_ALIGN_CENTER );
		g_free( label );

		y += height;
		sdata->last_y = y;
	}
}

/*
 * insert notes between the page title and the columns headers
 */
static void
irenderable_draw_page_header_notes( ofaIRenderable *instance, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes( instance );
	}
}

static void
irenderable_draw_page_header_columns( ofaIRenderable *instance, sIRenderable *sdata )
{
	static const gdouble st_vspace_rate_before = 0.5;
	static const gdouble st_vspace_rate_after = 0.5;
	gdouble cy_before, cy_after, prev_y;
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns( instance );

	} else if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_header_column_names ){

		ofa_irenderable_set_font( instance, ofa_irenderable_get_columns_font( instance, sdata->page_num ));
		cy_before = ofa_irenderable_get_text_height( instance ) * st_vspace_rate_before;
		cy_after = ofa_irenderable_get_text_height( instance ) * st_vspace_rate_after;
		sdata->last_y += cy_before;

		/* draw and paint a rectangle
		 * this must be done before writing the columns headers */
		if( sdata->header_columns_height > 0 ){
			ofa_irenderable_set_color( instance, COLOR_HEADER_COLUMNS_BG );
			cairo_rectangle( sdata->current_context,
					0, sdata->last_y, sdata->render_width, sdata->header_columns_height );
			cairo_fill( sdata->current_context );
		}

		ofa_irenderable_get_columns_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		prev_y = sdata->last_y;
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_header_column_names( instance );
		sdata->header_columns_height = sdata->last_y - prev_y;
		sdata->last_y += cy_after;
	}
}

/*
 * on top on each page, after the column headers
 */
static void
irenderable_draw_top_summary( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_summary ){

		ofa_irenderable_get_summary_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_summary( instance );
	}
}

/*
 * Does @line begins a new group for the implementation ?
 *
 * If %TRUE, then we will take care of drawing the group header and
 * footer.
 */
static gboolean
irenderable_is_new_group( ofaIRenderable *instance, GList *line, GList *next, ofeIRenderableBreak *sep, sIRenderable *sdata )
{
	gboolean new_group;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->is_new_group ){
		new_group = OFA_IRENDERABLE_GET_INTERFACE( instance )->is_new_group( instance, line, next, sep );

	} else {
		new_group = FALSE;
	}

	return( new_group );
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
irenderable_draw_group_header( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_header ){

		ofa_irenderable_get_group_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_header( instance );
	}
}

/*
 * draws a separation line between two groups on the same page
 */
static gdouble
draw_group_separation( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble y, text_height, rate, r, g, b, height;

	ofa_irenderable_get_group_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );
	ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, 0 ));
	text_height = ofa_irenderable_get_text_height( instance );
	rate = ofa_irenderable_get_body_vspace_rate( instance );

	/* separation line */
	y = sdata->last_y;
	y += rate * text_height;
	cairo_set_line_width( sdata->current_context, 0.5 );
	cairo_move_to( sdata->current_context, 0, y );
	cairo_line_to( sdata->current_context, sdata->render_width, y );
	cairo_stroke( sdata->current_context );
	y += 1.5;
	cairo_move_to( sdata->current_context, 0, y );
	cairo_line_to( sdata->current_context, sdata->render_width, y );
	cairo_stroke( sdata->current_context );
	y += rate * text_height;

	height = y - sdata->last_y;
	sdata->last_y = y;

	return( height );
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_group_header_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble height, prev_y;
	gboolean prev_paginating;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	prev_y = sdata->last_y;
	sdata->last_y = 0;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	irenderable_draw_group_header( instance, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

/*
 * draw a top report for the current page
 */
static void
irenderable_draw_top_report( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_report ){

		ofa_irenderable_get_report_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_report_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_report( instance );
	}
}

/*
 * draw a bottom report for the current page
 */
static void
irenderable_draw_bottom_report( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_bottom_report ){

		ofa_irenderable_get_report_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_report_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_bottom_report( instance );
	}
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_bottom_report_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble height, prev_y;
	cairo_t *prev_context;
	PangoLayout *prev_layout;
	gboolean prev_paginating;

	prev_y = sdata->last_y;
	sdata->last_y = 0;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	irenderable_draw_bottom_report( instance, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

static void
irenderable_draw_group_footer( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_footer ){

		ofa_irenderable_get_group_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_group_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_footer( instance );
	}
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_group_footer_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gboolean prev_paginating;
	gdouble height, prev_y;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	prev_y = sdata->last_y;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	irenderable_draw_group_footer( instance, sdata );

	height = sdata->last_y - prev_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

/*
 * Let the implementation have a final summary on the last page
 */
static void
irenderable_draw_last_summary( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble r, g, b;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_last_summary ){

		ofa_irenderable_get_summary_color( instance, &r, &g, &b );
		ofa_irenderable_set_color( instance, r, g, b );
		ofa_irenderable_set_font( instance, ofa_irenderable_get_summary_font( instance, sdata->page_num ));

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_last_summary( instance );
	}
}

/**
 * ofa_irenderable_get_last_summary_height:
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
gdouble
ofa_irenderable_get_last_summary_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble height, prev_y;
	cairo_t *prev_context;
	PangoLayout *prev_layout;
	gboolean prev_paginating;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	prev_y = sdata->last_y;
	sdata->last_y = 0;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	irenderable_draw_last_summary( instance, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

static void
irenderable_draw_page_footer( ofaIRenderable *instance, sIRenderable *sdata )
{
	GList *it;
	gboolean done;

	sdata->last_y = sdata->max_y;

	done = FALSE;
	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		if( ofa_irenderer_draw_page_footer( OFA_IRENDERER( it->data ), instance )){
			done = TRUE;
			break;
		}
	}
	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_footer ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_footer( instance );

		} else {
			ofa_irenderable_draw_default_page_footer( instance );
		}
	}
}

/*
 * The height of the default page footer is computed by just drawing it
 * at y=0, and then requiring Pango the layout height
 *
 * Page footer is expected to have a fixed height
 * so it is worth to caching it
 * so it is called once from begin_render() on start of the pagination phase.
 *
 * Note that pre-computing the page footer height is needed in order to
 * be able to compute the max_y.
 */
static gdouble
get_page_footer_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble prev_y, height;

	if(( height = sdata->footer_height ) == 0 ){

		prev_y = sdata->last_y;

		irenderable_draw_page_footer( instance, sdata );

		height = sdata->last_y - prev_y;
		sdata->last_y = prev_y;
	}

	return( height );
}

/*
 * a convenience function to convert a COLOR_XXX macro to three gdouble
 * pointers
 */
static void
set_rgb( gdouble *r, gdouble *g, gdouble *b, gdouble sr, gdouble sg, gdouble sb )
{
	*r = sr;
	*g = sg;
	*b = sb;
}

/**
 * ofa_irenderable_is_paginating:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: %TRUE if while pagination.
 */
gboolean
ofa_irenderable_is_paginating( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_instance_data( instance );

	return( sdata->paginating );
}

/**
 * ofa_irenderable_get_line_mode:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the line rendering mode.
 */
ofeIRenderableMode
ofa_irenderable_get_line_mode( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->line_mode );
}

/**
 * ofa_irenderable_set_line_mode:
 * @instance: this #ofaIRenderable instance.
 * @mode: the requested line mode.
 *
 * Set the requested line rendering mode.
 *
 * Must be set once, before rendering the pages.
 */
void
ofa_irenderable_set_line_mode( ofaIRenderable *instance, ofeIRenderableMode mode )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );

	sdata->line_mode = mode;
}

/**
 * ofa_irenderable_get_render_width:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the rendering width in points.
 */
gdouble
ofa_irenderable_get_render_width( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->render_width );
}

/**
 * ofa_irenderable_get_render_height:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the rendering height in points.
 */
gdouble
ofa_irenderable_get_render_height( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->render_height );
}

/**
 * ofa_irenderable_get_dataset:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the current dataset.
 */
GList *
ofa_irenderable_get_dataset( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->dataset );
}

/**
 * ofa_irenderable_get_current_page_num:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the current page number, counted from zero.
 */
guint
ofa_irenderable_get_current_page_num( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->page_num );
}

/**
 * ofa_irenderable_get_current_line:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the current to-be-rendered line.
 */
GList *
ofa_irenderable_get_current_line( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->line );
}

/**
 * ofa_irenderable_get_last_y:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the last ordonate position.
 */
gdouble
ofa_irenderable_get_last_y( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->last_y );
}

/**
 * ofa_irenderable_set_last_y:
 * @instance: this #ofaIRenderable instance.
 * @y: the new ordinate to be set
 *
 * Set the new ordinate position.
 */
void
ofa_irenderable_set_last_y( ofaIRenderable *instance, gdouble y )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );

	sdata->last_y = y;
}

/**
 * ofa_irenderable_get_max_y:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the maximum ordinate position, which is where is drawn the
 * footer separation line (after a small vertical space).
 */
gdouble
ofa_irenderable_get_max_y( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( sdata->max_y );
}

/**
 * ofa_irenderable_set_max_y:
 * @instance: this #ofaIRenderable instance.
 * @max_y: the new maximum rendering ordonate.
 *
 * set @max_y.
 */
void
ofa_irenderable_set_max_y( ofaIRenderable *instance, gdouble max_y )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );

	sdata->max_y = max_y;
}

/**
 * ofa_irenderable_get_text_height:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the height in cairo units used by a text drawn in the
 * current font.
 */
gdouble
ofa_irenderable_get_text_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble height;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	prev_context = sdata->current_context;
	prev_layout = sdata->current_layout;
	sdata->current_context = sdata->temp_context;
	sdata->current_layout = sdata->temp_layout;

	height = ofa_irenderable_set_text( instance, 0, 0, "My testing Text", PANGO_ALIGN_LEFT );

	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;

	return( height );
}

/**
 * ofa_irenderable_get_text_width:
 * @instance: this #ofaIRenderable instance.
 * @text: the text to be measured.
 *
 * Returns: the width in cairo units used by a text drawn in the current font.
 */
gdouble
ofa_irenderable_get_text_width( ofaIRenderable *instance, const gchar *text )
{
	sIRenderable *sdata;
	gdouble cairo_width;
	cairo_t *prev_context;
	PangoLayout *prev_layout;
	gint pango_width, pango_height;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	prev_context = sdata->current_context;
	prev_layout = sdata->current_layout;
	sdata->current_context = sdata->temp_context;
	sdata->current_layout = sdata->temp_layout;

	ofa_irenderable_set_text( instance, 0, 0, text, PANGO_ALIGN_LEFT );
	pango_layout_get_size( sdata->current_layout, &pango_width, &pango_height );
	cairo_width = ( gdouble ) pango_width / PANGO_SCALE;

	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;

	return( cairo_width );
}

/**
 * ofa_irenderable_get_line_height:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the height in cairo units used by a line, which corresponds
 * to the text height to which is added the body vertical space rate.
 */
gdouble
ofa_irenderable_get_line_height( ofaIRenderable *instance )
{
	gdouble line_height;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	line_height = ofa_irenderable_get_text_height( instance );
	line_height *= ( 1 + ofa_irenderable_get_body_vspace_rate( instance ));

	return( line_height );
}

/**
 * ofa_irenderable_set_font:
 */
void
ofa_irenderable_set_font( ofaIRenderable *instance, const gchar *font_str )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );
	set_font( sdata->in_layout, font_str, NULL );
	set_font( sdata->temp_layout, font_str, NULL );
}

/*
 * set_font:
 */
static void
set_font( PangoLayout *layout, const gchar *font_str, gdouble *size )
{
	PangoFontDescription *desc;
	gchar *str;

	desc = pango_font_description_from_string( font_str );
	pango_layout_set_font_description( layout, desc );
	pango_font_description_free( desc );

	if( size && g_utf8_validate( font_str, -1, NULL )){
		str = g_utf8_strreverse( font_str, -1 );
		*size = g_strtod( str, NULL );
		g_free( str );
	}
	/*g_debug( "set_font %s: size=%lf", font_str, *size );*/
}

/**
 * ofa_irenderable_set_color:
 */
void
ofa_irenderable_set_color( ofaIRenderable *instance, double r, double g, double b )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );
	cairo_set_source_rgb( sdata->current_context, r, g, b );
}

/**
 * ofa_irenderable_set_text:
 *
 * the 'x' abcisse must point to the tab reference:
 * - when aligned on left, to the left
 * - when aligned on right, to the right
 * - when centered, to the middle point
 *
 * Returns: the height of text, in cairo units (points)
 */
gdouble
ofa_irenderable_set_text( ofaIRenderable *instance,
								gdouble x, gdouble y, const gchar *text, PangoAlignment align )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( text ? set_text( sdata->current_layout, sdata->current_context, x, y, text, align ) : 0 );
}

static gdouble
set_text( PangoLayout *layout, cairo_t *context,
								gdouble x, gdouble y, const gchar *text, PangoAlignment align )
{
	static const gchar *thisfn = "ofa_irenderable_set_text";
	PangoRectangle rc;
	gint pango_width, pango_height;
	gdouble cairo_height;

	pango_layout_set_text( layout, text, -1 );

	if( align == PANGO_ALIGN_LEFT ){
		cairo_move_to( context, x, y );

	} else if( align == PANGO_ALIGN_RIGHT ){
		pango_layout_get_pixel_extents( layout, NULL, &rc );
		cairo_move_to( context, x-rc.width, y );

	} else if( align == PANGO_ALIGN_CENTER ){
		pango_layout_get_pixel_extents( layout, NULL, &rc );
		cairo_move_to( context, x-rc.width/2, y );

	} else {
		g_warning( "%s: %d: unknown print alignment indicator", thisfn, align );
	}

	pango_cairo_update_layout( context, layout );
	pango_cairo_show_layout( context, layout );

	pango_layout_get_size( layout, &pango_width, &pango_height );
	cairo_height = ( gdouble ) pango_height / PANGO_SCALE;

	return( cairo_height );
}

/**
 * ofa_irenderable_ellipsize_text:
 */
gdouble
ofa_irenderable_ellipsize_text( ofaIRenderable *instance,
									gdouble x, gdouble y, const gchar *text,
									gdouble max_size )
{
	sIRenderable *sdata;
	gint pango_width, pango_height;
	gdouble cairo_height;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	pango_layout_set_text( sdata->current_layout, text, -1 );
	my_utils_pango_layout_ellipsize( sdata->current_layout, max_size );
	cairo_move_to( sdata->current_context, x, y );
	pango_cairo_update_layout( sdata->current_context, sdata->current_layout );
	pango_cairo_show_layout( sdata->current_context, sdata->current_layout );

	pango_layout_get_size( sdata->current_layout, &pango_width, &pango_height );
	cairo_height = ( gdouble ) pango_height / PANGO_SCALE;

	return( cairo_height );
}

/**
 * ofa_irenderable_set_wrapped_text:
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
gdouble
ofa_irenderable_set_wrapped_text( ofaIRenderable *instance,
									gdouble x, gdouble y, gdouble width, const gchar *text, PangoAlignment align )
{
	sIRenderable *sdata;
	gdouble height;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	pango_layout_set_width( sdata->current_layout, width );
	pango_layout_set_wrap( sdata->current_layout, PANGO_WRAP_WORD );
	height = ofa_irenderable_set_text( instance, x, y, text, align );

	return( height );
}

/**
 * ofa_irenderable_draw_rubber:
 */
void
ofa_irenderable_draw_rubber( ofaIRenderable *instance, gdouble top, gdouble height )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );
	cairo_set_source_rgb( sdata->current_context, COLOR_LIGHT_GRAY );
	cairo_rectangle( sdata->current_context, 0, top, sdata->render_width, height );
	cairo_fill( sdata->current_context );
}

/**
 * ofa_irenderable_draw_rect:
 */
void
ofa_irenderable_draw_rect( ofaIRenderable *instance, gdouble x, gdouble y, gdouble width, gdouble height )
{
	sIRenderable *sdata;
	gdouble cx;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );
	cx = width < 0 ? sdata->render_width : width;
	cairo_set_line_width( sdata->current_context, 0.5 );

	cairo_move_to( sdata->current_context, x, y );				/* ---- */
	cairo_line_to( sdata->current_context, x+cx, y );
	cairo_stroke( sdata->current_context );

	cairo_move_to( sdata->current_context, x, y );				/* +---- */
	cairo_line_to( sdata->current_context, x, y+height );		/* |     */
	cairo_stroke( sdata->current_context );

	cairo_move_to( sdata->current_context, x+cx, y );			/* +---+ */
	cairo_line_to( sdata->current_context, x+cx, y+height );	/* |   | */
	cairo_stroke( sdata->current_context );

	cairo_move_to( sdata->current_context, x, y+height );		/* +---+ */
	cairo_line_to( sdata->current_context, x+cx, y+height );	/* +---+ */
	cairo_stroke( sdata->current_context );
}

/**
 * ofa_irenderable_draw_default_page_footer:
 * @instance: this #ofaIRenderable instance.
 *
 * Draws the default page footer.
 */
void
ofa_irenderable_draw_default_page_footer( ofaIRenderable *instance )
{
	static gdouble vspace_before_footer = 2.0;	/* points */
	static gdouble vspace_after_line = 1.0; 	/* points */
	sIRenderable *sdata;
	gchar *str, *stamp_str;
	GTimeVal stamp;
	gdouble y, text_height, r, g, b;

	sdata = get_instance_data( instance );

	/* page footer color */
	ofa_irenderable_get_footer_color( instance, &r, &g, &b );
	ofa_irenderable_set_color( instance, r, g, b );

	/* draw the separation line
	 * max_y is zero the first time and then set depending of the result */
	y = sdata->last_y;
	y += vspace_before_footer;
	cairo_set_line_width( sdata->current_context, 0.5 );
	cairo_move_to( sdata->current_context, 0, y );
	cairo_line_to( sdata->current_context, sdata->render_width, y );
	cairo_stroke( sdata->current_context );
	y += vspace_after_line;

	/* draw the footer line */
	ofa_irenderable_set_font( instance, ofa_irenderable_get_footer_font( instance ));
	text_height = ofa_irenderable_get_text_height( instance );

	str = g_strdup_printf( "%s v %s", PACKAGE_NAME, PACKAGE_VERSION );
	ofa_irenderable_set_text( instance, st_page_margin, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	str = g_strdup_printf(
			_( "Printed on %s - Page %d/%d" ), stamp_str, 1+sdata->page_num, sdata->pages_count );
	g_free( stamp_str );
	ofa_irenderable_set_text( instance, sdata->render_width-st_page_margin, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y += text_height;
	sdata->last_y = y;
}

/**
 * ofa_irenderable_draw_no_data:
 */
void
ofa_irenderable_draw_no_data( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble y, width, height;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_instance_data( instance );

	ofa_irenderable_set_color( instance, COLOR_NO_DATA );
	ofa_irenderable_set_font( instance, st_default_no_data_font );
	height = ofa_irenderable_get_text_height( instance );

	/* vertically centered */
	y = sdata->last_y + (sdata->max_y-sdata->last_y-height)/2;
	width = sdata->render_width;
	ofa_irenderable_set_text( instance, width/2, y, _( "Empty dataset" ), PANGO_ALIGN_CENTER );
	sdata->last_y = y+height;
}

/**
 * ofa_irenderable_get_header_columns_height
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the height of the surrounding rectangle of the columns
 * headers.
 */
gdouble
ofa_irenderable_get_header_columns_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_instance_data( instance );

	return( sdata->header_columns_height );
}

/**
 * ofa_irenderable_get_context:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the current cairo_t context.
 */
cairo_t *
ofa_irenderable_get_context( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_instance_data( instance );

	return( sdata->current_context );
}

/**
 * ofa_irenderable_set_temp_context:
 * @instance: this #ofaIRenderable instance.
 *
 * Save the current rendering context, and setup a new temp context to
 * let the user draw onto.
 *
 * This doesn't have any effect during the pagination, as we already use
 * a temporary context.
 */
void
ofa_irenderable_set_temp_context( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_instance_data( instance );

	if( !sdata->paginating && sdata->current_context == sdata->in_context && sdata->current_layout == sdata->in_layout ){
		sdata->current_context = sdata->temp_context;
		sdata->current_layout = sdata->temp_layout;
	}
}

/**
 * ofa_irenderable_restore_context:
 * @instance: this #ofaIRenderable instance.
 *
 * Restore the previously saved (pagination or rendering) context.
 *
 * This doesn't have any effect during the pagination, as we already use
 * a temporary context.
 */
void
ofa_irenderable_restore_context( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_instance_data( instance );

	if( !sdata->paginating && sdata->current_context == sdata->temp_context && sdata->current_layout == sdata->temp_layout ){
		sdata->current_context = sdata->in_context;
		sdata->current_layout = sdata->in_layout;
	}
}

/**
 * ofa_irenderable_get_page_margin:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the page margin in points.
 */
gdouble
ofa_irenderable_get_page_margin( const ofaIRenderable *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	return( st_page_margin );
}

/**
 * ofa_irenderable_get_columns_spacing:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the minimal space between columns in points.
 */
gdouble
ofa_irenderable_get_columns_spacing( const ofaIRenderable *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	return( st_column_hspacing );
}

/**
 * ofa_irenderable_get_dossier_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the dossier name.
 */
const gchar *
ofa_irenderable_get_dossier_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_dossier_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_header_dossier_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_dossier_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the dossier name.
 */
void
ofa_irenderable_get_dossier_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_dossier_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		set_rgb( r, g, b, COLOR_HEADER_DOSSIER );
	}
}

/**
 * ofa_irenderable_get_title_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the page title.
 */
const gchar *
ofa_irenderable_get_title_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_title_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_header_title_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_title_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the page title.
 */
void
ofa_irenderable_get_title_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_title_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_title_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		set_rgb( r, g, b, COLOR_HEADER_TITLE );
	}
}

/**
 * ofa_irenderable_get_columns_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the columns headers.
 */
const gchar *
ofa_irenderable_get_columns_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_columns_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_columns_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_columns_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_header_columns_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_columns_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the columns headers.
 */
void
ofa_irenderable_get_columns_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_columns_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_columns_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_columns_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		set_rgb( r, g, b, COLOR_HEADER_COLUMNS_FG );
	}
}

/**
 * ofa_irenderable_get_summary_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the summaries.
 */
const gchar *
ofa_irenderable_get_summary_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_summary_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_summary_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_summary_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_summary_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_summary_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the summaries.
 */
void
ofa_irenderable_get_summary_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_summary_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_summary_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_summary_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		ofa_irenderable_get_title_color( instance, r, g, b );
	}
}

/**
 * ofa_irenderable_get_group_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the summaries.
 */
const gchar *
ofa_irenderable_get_group_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_group_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_group_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_group_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_group_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_group_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the group header and footer.
 */
void
ofa_irenderable_get_group_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_group_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_group_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_group_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		ofa_irenderable_get_summary_color( instance, r, g, b );
	}
}

/**
 * ofa_irenderable_get_report_font:
 * @instance: this #ofaIRenderable instance.
 * @page_num: the page number, counted from zero.
 *
 * Returns: the font of the summaries.
 */
const gchar *
ofa_irenderable_get_report_font( const ofaIRenderable *instance, guint page_num )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_report_font( OFA_IRENDERER( it->data ), instance, page_num );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_report_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_report_font( instance, page_num );
		}
	}

	if( !my_strlen( font )){
		font = st_default_report_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_report_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the top and bottom reports.
 */
void
ofa_irenderable_get_report_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_report_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_report_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_report_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		ofa_irenderable_get_summary_color( instance, r, g, b );
	}
}

/**
 * ofa_irenderable_get_body_font:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the body font.
 */
const gchar *
ofa_irenderable_get_body_font( const ofaIRenderable *instance )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_body_font( OFA_IRENDERER( it->data ), instance );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_font( instance );
		}
	}

	if( !my_strlen( font )){
		font = st_default_body_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_body_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the body.
 */
void
ofa_irenderable_get_body_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_body_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		set_rgb( r, g, b, COLOR_BODY );
	}
}

/**
 * ofa_irenderable_get_body_vspace_rate:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the body.
 *
 * vspace_rate: the vertical space to add to the text line height to
 * have an interline - default to 25%
 */
gdouble
ofa_irenderable_get_body_vspace_rate( const ofaIRenderable *instance )
{
	gdouble rate;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_vspace_rate ){
		rate = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_vspace_rate( instance );

	} else {
		rate = st_body_vspace_rate;
	}

	return( rate );
}

/**
 * ofa_irenderable_get_footer_font:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the font used for the page footer.
 */
const gchar *
ofa_irenderable_get_footer_font( const ofaIRenderable *instance )
{
	sIRenderable *sdata;
	const gchar *font;
	GList *it;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	font = NULL;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		font = ofa_irenderer_get_footer_font( OFA_IRENDERER( it->data ), instance );
		if( my_strlen( font )){
			break;
		}
	}

	if( !my_strlen( font )){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_footer_font ){
			font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_footer_font( instance );
		}
	}

	if( !my_strlen( font )){
		font = st_default_footer_font;
	}

	return( font );
}

/**
 * ofa_irenderable_get_body_color:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the color used for the page footer.
 */
void
ofa_irenderable_get_footer_color( const ofaIRenderable *instance, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderable *sdata;
	GList *it;
	gboolean done;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));
	g_return_if_fail( r && g && b );

	sdata = get_instance_data( instance );
	done = FALSE;

	for( it=sdata->renderer_plugins ; it ; it=it->next ){
		done = ofa_irenderer_get_footer_color( OFA_IRENDERER( it->data ), instance, r, g, b );
		if( done ){
			break;
		}
	}

	if( !done ){
		if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_footer_color ){
			OFA_IRENDERABLE_GET_INTERFACE( instance )->get_footer_color( instance, r, g, b );
			done = TRUE;
		}
	}

	if( !done ){
		set_rgb( r, g, b, COLOR_FOOTER );
	}
}

static sIRenderable *
get_instance_data( const ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = ( sIRenderable * ) g_object_get_data( G_OBJECT( instance ), IRENDERABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIRenderable, 1 );
		g_object_set_data( G_OBJECT( instance ), IRENDERABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		sdata->line_mode = IRENDERABLE_MODE_NORMAL;
	}

	return( sdata );
}

static void
on_instance_finalized( sIRenderable *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_irenderable_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_list_free( sdata->renderer_plugins );

	g_clear_object( &sdata->in_layout );

	if( sdata->temp_context ){
		cairo_destroy( sdata->temp_context );
	}

	g_clear_object( &sdata->temp_layout );

	g_free( sdata );
}
