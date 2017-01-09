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

#include <cairo-pdf.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-irenderable.h"

/* data associated to each implementor object
 */
typedef struct {
	/* constant data
	 * they are set on begin rendering, from #begin_render() unless
	 * otherwise specified, and use through all the rest of the code
	 */
	gdouble      render_width;
	gdouble      render_height;
	gchar       *body_font;
	gdouble      body_vspace_rate;
	gdouble      page_header_columns_height;
	gdouble      page_footer_height;
	gdouble      max_y;
	gint         pages_count;
	gboolean     want_groups;			/* whether the implementation manages groups */
	gboolean     want_new_page;			/* whether if wants new page on new group */
	gboolean     want_line_separation;	/* whether if wants groups to be line-separated */
	GList       *dataset;

	/* data reset from each entry point of the api:
	 * #begin_render(), #render_page() and #end_render()
	 */
	cairo_t     *in_context;			/* the provided context with an associated layout */
	PangoLayout *in_layout;
	cairo_t     *temp_context;			/* new temp context and layout for pagination */
	PangoLayout *temp_layout;

	/* runtime data
	 */
	gboolean     paginating;
	cairo_t     *current_context;
	PangoLayout *current_layout;
	gdouble      last_y;
	GList       *last_printed;			/* reset_runtime */

	/* @group_footer_printed: is reset to %TRUE in #reset_runtime()
	 *  so that we do not try to draw it if the implementation does
	 *  not manage groups
	 */
	gboolean     group_footer_printed;	/* reset_runtime */
}
	sIRenderable;

#define IRENDERABLE_LAST_VERSION         1
#define IRENDERABLE_DATA                 "ofa-irenderable-data"

#define COLOR_BLACK                     0,      0,      0
#define COLOR_DARK_CYAN                 0,      0.5,    0.5
#define COLOR_DARK_RED                  0.5,    0,      0
#define COLOR_GRAY                      0.6,    0.6,    0.6			/* #999999 */
#define COLOR_LIGHT_GRAY                0.9375, 0.9375, 0.9375		/* #f0f0f0 */
#define COLOR_MIDDLE_GRAY               0.7688, 0.7688, 0.7688		/* #c4c4c4 */
#define COLOR_WHITE                     1,      1,      1

#define COLOR_HEADER_DOSSIER            COLOR_DARK_RED
#define COLOR_HEADER_TITLE              COLOR_DARK_CYAN
#define COLOR_HEADER_SUBTITLE           COLOR_DARK_CYAN
#define COLOR_HEADER_NOTES              COLOR_BLACK
#define COLOR_HEADER_COLUMNS_BG         COLOR_DARK_CYAN
#define COLOR_HEADER_COLUMNS_FG         COLOR_WHITE
#define COLOR_SUMMARY                   COLOR_DARK_CYAN
#define COLOR_GROUP                     COLOR_DARK_CYAN
#define COLOR_REPORT                    COLOR_DARK_CYAN
#define COLOR_BODY                      COLOR_BLACK
#define COLOR_FOOTER                    COLOR_GRAY
#define COLOR_NO_DATA                   COLOR_MIDDLE_GRAY

static const gchar  *st_default_body_font                  = "Sans 6";
static const gchar  *st_default_header_dossier_font        = "Sans Bold Italic 11";
static const gchar  *st_default_header_title_font          = "Sans Bold 10";
static const gchar  *st_default_header_subtitle_font       = "Sans Bold 8";
static const gchar  *st_default_header_columns_font        = "Sans Bold 5";
static const gchar  *st_default_summary_font               = "Sans Bold 7";
static const gchar  *st_default_group_font                 = "Sans Bold 6";
static const gchar  *st_default_report_font                = "Sans 6";
static const gchar  *st_default_footer_font                = "Sans Italic 5";
static const gchar  *st_default_no_data_font               = "Sans 18";

static const gdouble st_page_margin                        = 2.0;

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIRenderableInterface *klass );
static void          interface_base_finalize( ofaIRenderableInterface *klass );
static gchar        *irenderable_get_body_font( ofaIRenderable *instance );
static gdouble       irenderable_get_body_vspace_rate( ofaIRenderable *instance );
static gboolean      irenderable_want_groups( const ofaIRenderable *instance );
static gboolean      irenderable_want_new_page( const ofaIRenderable *instance );
static gboolean      irenderable_want_line_separation( const ofaIRenderable *instance );
static gboolean      draw_page( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          irenderable_draw_page_header( ofaIRenderable *instance, gint page_num );
static void          draw_page_header_dossier( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_title( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_subtitle( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_subtitle2( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_notes( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static gdouble       draw_page_header_columns( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static gdouble       get_page_header_columns_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_top_summary( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_page_top_report( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static gboolean      draw_line( ofaIRenderable *instance, gint page_num, gint line_num, GList *line, GList *next, sIRenderable *sdata );
static gboolean      is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static gboolean      irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev );
static void          draw_group_header( ofaIRenderable *instance, gint line_num, GList *line, sIRenderable *sdata );
static gdouble       get_group_header_height( ofaIRenderable *instance, gint line_num, GList *line, sIRenderable *sdata );
static void          draw_group_top_report( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_group_bottom_report( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_group_bottom_report_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_group_footer( ofaIRenderable *instance, gint line_num, sIRenderable *sdata );
static gdouble       get_group_footer_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_page_bottom_report( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static gdouble       get_page_bottom_report_height( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_bottom_summary( ofaIRenderable *instance, sIRenderable *sdata );
static gdouble       get_bottom_summary_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          draw_page_footer( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          irenderable_draw_page_footer( ofaIRenderable *instance, gint page_num );
static gdouble       get_page_footer_height( ofaIRenderable *instance, sIRenderable *sdata );
static void          reset_runtime( ofaIRenderable *instance, sIRenderable *sdata );
static sIRenderable *get_irenderable_data( ofaIRenderable *instance );
static void          set_irenderable_input_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata );
static void          on_instance_finalized( sIRenderable *sdata, void *instance );
static void          free_irenderable_data( sIRenderable *sdata );
static void          set_font( PangoLayout *layout, const gchar *font_str, gdouble *size );
static gdouble       set_text( PangoLayout *layout, cairo_t *context, gdouble x, gdouble y, const gchar *text, PangoAlignment align );

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

		klass->get_body_font = irenderable_get_body_font;
		klass->get_body_vspace_rate = irenderable_get_body_vspace_rate;
		klass->want_groups = irenderable_want_groups;
		klass->want_new_page = irenderable_want_new_page;
		klass->want_line_separation = irenderable_want_line_separation;
		klass->draw_page_header = irenderable_draw_page_header;
		klass->is_new_group = irenderable_is_new_group;
		klass->draw_page_footer = irenderable_draw_page_footer;
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
 * ofa_irenderable_begin_render:
 * @instance:
 * @cr:
 * @render_width:
 * @render_height:
 *
 * The first entry point of the interface.
 * This initialize all main variables, and paginates the render.
 * Must be called before any rendering.
 *
 * Returns: the pages count.
 */
gint
ofa_irenderable_begin_render( ofaIRenderable *instance, cairo_t *cr, gdouble render_width, gdouble render_height, GList *dataset )
{
	static const gchar *thisfn = "ofa_irenderable_begin_render";
	sIRenderable *sdata;
	gint page_num;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );
	sdata->render_width = render_width;
	sdata->render_height = render_height;
	sdata->dataset = dataset;
	set_irenderable_input_context( instance, cr, sdata );

	sdata->paginating = TRUE;
	sdata->current_context = sdata->temp_context;
	sdata->current_layout = sdata->temp_layout;

	sdata->body_font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_font( instance );
	sdata->body_vspace_rate = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_vspace_rate( instance );
	sdata->page_header_columns_height = get_page_header_columns_height( instance, sdata );
	sdata->page_footer_height = get_page_footer_height( instance, sdata );
	sdata->max_y = render_height - sdata->page_footer_height;

	sdata->want_groups = OFA_IRENDERABLE_GET_INTERFACE( instance )->want_groups( instance );
	if( sdata->want_groups ){
		sdata->want_new_page = OFA_IRENDERABLE_GET_INTERFACE( instance )->want_new_page( instance );
		sdata->want_line_separation = OFA_IRENDERABLE_GET_INTERFACE( instance )->want_line_separation( instance );
	}

	g_debug( "%s: instance=%p, cr=%p, render_width=%lf, render_height=%lf, max_y=%lf, dataset_count=%d",
			thisfn, ( void * ) instance, ( void * ) cr, render_width, render_height, sdata->max_y,
			g_list_length( sdata->dataset ));

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->begin_render ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->begin_render( instance, render_width, render_height );
	}

	reset_runtime( instance, sdata );
	page_num = 0;
	while( draw_page( instance, page_num++, sdata ));

	sdata->pages_count = page_num;
	g_debug( "%s: about to render %d page(s)", thisfn, sdata->pages_count );

	reset_runtime( instance, sdata );
	sdata->paginating = FALSE;

	return( sdata->pages_count );

}

/*
 * default implementation of some virtuals
 */
static gchar *
irenderable_get_body_font( ofaIRenderable *instance )
{
	return( g_strdup( st_default_body_font ));
}

static gdouble
irenderable_get_body_vspace_rate( ofaIRenderable *instance )
{
	static const gdouble st_vspace_rate = 0.35;

	return( st_vspace_rate );
}

static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	return( FALSE );
}

static gboolean
irenderable_want_new_page( const ofaIRenderable *instance )
{
	return( FALSE );
}

static gboolean
irenderable_want_line_separation( const ofaIRenderable *instance )
{
	return( TRUE );
}

/**
 * ofa_irenderable_render_page:
 * @instance:
 * @cr:
 * @page_number: counted from zero.
 *
 * The main entry point of the interface.
 * Must be called once for each page in order each page be rendered.
 */
void
ofa_irenderable_render_page( ofaIRenderable *instance, cairo_t *context, gint page_number )
{
	static const gchar *thisfn = "ofa_irenderable_render_page";
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );
	set_irenderable_input_context( instance, context, sdata );

	g_debug( "%s: instance=%p, context=%p, page_number=%d, count=%d",
			thisfn, ( void * ) instance, ( void * ) context, page_number, g_list_length( sdata->dataset ));

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata->current_context = sdata->in_context;
	sdata->current_layout = sdata->in_layout;

	draw_page( instance, page_number, sdata );

	/*g_debug( "%s: quitting with count=%d", thisfn, g_list_length( sdata->dataset ));*/
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
draw_page( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	GList *line, *next;
	gint count;
	gboolean is_last;
	gdouble req_height;

	/*g_debug( "draw_page: page_num=%d, count=%d", page_num, g_list_length( sdata->dataset ));*/
	sdata->last_y = 0;

	OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header( instance, page_num );

	if( page_num == 0 ){
		draw_top_summary( instance, sdata );
	}

	line = sdata->last_printed ? g_list_next( sdata->last_printed ) : sdata->dataset;

	for( count=0 ; line ; count+=1 ){
		next = g_list_next( line );		/* may be %NULL */
		if( !draw_line( instance, page_num, count, line, next, sdata )){
			/*g_debug( "draw_page: draw_line returned False, count=%d", g_list_length( sdata->dataset ));*/
			break;
		}
		sdata->last_printed = line;
		line = next;
	}

	/* end of the last page ? */
	is_last = FALSE;

	if( !line ){
		if( !sdata->group_footer_printed ){
			draw_group_footer( instance, count, sdata );
		}
		req_height = get_bottom_summary_height( instance, sdata );
		if( sdata->last_y + req_height <= sdata->max_y ){
			is_last = TRUE;
		}
		if( is_last ){
			draw_bottom_summary( instance, sdata );
			/*g_debug( "draw_page: page_num=%d, is_last=%s", page_num, is_last ? "True":"False" );*/
		}
	}

	/*g_debug( "draw_page: page_num=%d, is_last=%s, last_y=%lf, max_y=%lf",
			page_num, is_last ? "True":"False", sdata->last_y, sdata->max_y );*/

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_footer ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_footer( instance, page_num );
	}

	/*g_debug( "draw_page: is_last=%s, count=%d", is_last ? "True":"False", g_list_length( sdata->dataset ));*/

	return( !is_last );
}

/*
 * default implementation of #draw_page_header() virtual
 */
static void
irenderable_draw_page_header( ofaIRenderable *instance, gint page_num )
{
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );

	draw_page_header_dossier( instance, page_num, sdata );
	draw_page_header_title( instance, page_num, sdata );
	draw_page_header_subtitle( instance, page_num, sdata );
	draw_page_header_subtitle2( instance, page_num, sdata );
	draw_page_header_notes( instance, page_num, sdata );
	draw_page_header_columns( instance, page_num, sdata );
}

/*
 * default implementation of #draw_page_header() virtual
 * - dossier name
 */
static void
draw_page_header_dossier( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	gchar *dossier_name;
	gdouble y, height;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_name ){
		dossier_name = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_name( instance );
		y = sdata->last_y;

		ofa_irenderable_set_color( instance, COLOR_HEADER_DOSSIER );
		ofa_irenderable_set_font( instance, st_default_header_dossier_font );
		height = ofa_irenderable_set_text( instance, 0, y, dossier_name, PANGO_ALIGN_LEFT );

		y += height;
		sdata->last_y = y;
		g_free( dossier_name );
	}
}

/*
 * default implementation of #draw_page_header() virtual
 * - report title
 */
static void
draw_page_header_title( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	gdouble y, height;
	gchar *title;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_title ){
		title = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_title( instance );
		if( my_strlen( title )){
			y = sdata->last_y;

			ofa_irenderable_set_color( instance, COLOR_HEADER_TITLE );
			ofa_irenderable_set_font( instance, st_default_header_title_font );
			height = ofa_irenderable_set_text(
					instance, sdata->render_width/2, y, title, PANGO_ALIGN_CENTER );

			y += height;
			sdata->last_y = y;
		}
		g_free( title );
	}
}

/*
 * default implementation of #draw_page_header() virtual
 * - report subtitle
 */
static void
draw_page_header_subtitle( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	gdouble y, height;
	gchar *title;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_subtitle ){
		title = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_subtitle( instance );
		if( my_strlen( title )){
			y = sdata->last_y;

			ofa_irenderable_set_color( instance, COLOR_HEADER_SUBTITLE );
			ofa_irenderable_set_font( instance, st_default_header_subtitle_font );
			height = ofa_irenderable_set_text(
					instance, sdata->render_width/2, y, title, PANGO_ALIGN_CENTER );

			y += height;
			sdata->last_y = y;
		}
		g_free( title );
	}
}

static void
draw_page_header_subtitle2( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	static const gdouble st_vspace_rate_before = 0.15;
	gdouble y, height;
	gchar *title;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_subtitle2 ){
		title = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_page_header_subtitle2( instance );
		if( my_strlen( title )){
			y = sdata->last_y;

			ofa_irenderable_set_color( instance, COLOR_HEADER_SUBTITLE );
			ofa_irenderable_set_font( instance, st_default_header_subtitle_font );
			y += ofa_irenderable_get_text_height( instance ) * st_vspace_rate_before;

			height = ofa_irenderable_set_text(
					instance, sdata->render_width/2, y, title, PANGO_ALIGN_CENTER );

			y += height;
			sdata->last_y = y;
		}
		g_free( title );
	}
}

static void
draw_page_header_notes( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	static const gdouble st_vspace_rate_before = 0.5;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes ){

		ofa_irenderable_set_color( instance, COLOR_HEADER_NOTES );
		ofa_irenderable_set_font( instance, sdata->body_font );
		sdata->last_y += ofa_irenderable_get_text_height( instance ) * st_vspace_rate_before;

		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes( instance, page_num );
	}
}

/*
 * default implementation of #draw_page_header() virtual
 * - columns header
 *
 * May be called first with @page_num=-1 when computing the height of
 * the columns headers
 *
 * Returns: the height of the columns header from the *user* point of
 * view, i.e. excluding before and after vertical spaces added by the
 * interface itself.
 * This is actually the height of the colored rectangle which acts as
 * the columns headers background.
 */
static gdouble
draw_page_header_columns( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	static const gdouble st_vspace_rate_before = 0.5;
	static const gdouble st_vspace_rate_after = 0.5;
	gdouble cy_before, cy_after, prev_y, rc_height;

	rc_height = 0;
	cy_before = 0;
	cy_after = 0;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns ){
		ofa_irenderable_set_font( instance, st_default_header_columns_font );
		cy_before = ofa_irenderable_get_text_height( instance ) * st_vspace_rate_before;
		cy_after = ofa_irenderable_get_text_height( instance ) * st_vspace_rate_after;
	}

	/* draw and paint a rectangle
	 * this must be done before writing the columns headers */
	if( page_num >= 0 ){
		ofa_irenderable_set_color( instance, COLOR_HEADER_COLUMNS_BG );
		cairo_rectangle( sdata->current_context,
				0, sdata->last_y + cy_before, sdata->render_width, sdata->page_header_columns_height );
		cairo_fill( sdata->current_context );
	}

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns ){
		sdata->last_y += cy_before;
		ofa_irenderable_set_color( instance, COLOR_HEADER_COLUMNS_FG );
		ofa_irenderable_set_font( instance, st_default_header_columns_font );
		prev_y = sdata->last_y;
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns( instance, page_num );
		rc_height = sdata->last_y - prev_y;
		sdata->last_y += cy_after;
	}

	return( rc_height );
}

/*
 * columns header is expected to have a fixed height
 * so it is worth to caching it
 * so it is called once from begin_render() on start of the pagination phase.
 */
static gdouble
get_page_header_columns_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	static gboolean in_function = FALSE;	/* against recursion */
	gdouble height;

	height = 0;

	if( !in_function ){
		in_function = TRUE;
		g_return_val_if_fail( sdata->paginating, 0 );

		height = draw_page_header_columns( instance, -1, sdata );

		in_function = FALSE;
	}

	return( height );
}

/**
 * ofa_irenderable_get_page_header_columns_height
 *
 * Returns: the height of the surrounding rectangle of the columns
 * header.
 *
 * This height is computed by just drawing on a context, and then
 * measuring the 'last_y' difference.
 *
 * It is expected this function returns 0 when called more or less
 * directly from #draw_page_header_columns() while computing the height.
 * This should happen only the first time it is called, only during the
 * pagination phase.
 */
gdouble
ofa_irenderable_get_page_header_columns_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );

	return( sdata->page_header_columns_height );
}

static void
draw_top_summary( ofaIRenderable *instance, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_summary ){
		ofa_irenderable_set_color( instance, COLOR_SUMMARY );
		ofa_irenderable_set_font( instance, st_default_summary_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_top_summary( instance );
	}
}

static void
draw_page_top_report( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
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
draw_line( ofaIRenderable *instance,
					gint page_num, gint line_num, GList *line, GList *next, sIRenderable *sdata )
{
	static const gchar *thisfn = "ofa_irenderable_draw_line";
	gdouble y, req_height, end_height, line_height, font_height;

	/* must be set before any height computing as this is the main
	 * parameter  */
	ofa_irenderable_set_color( instance, COLOR_BODY );
	ofa_irenderable_set_font( instance, sdata->body_font );
	font_height = ofa_irenderable_get_text_height( instance );
	line_height = ofa_irenderable_get_line_height( instance );

	if( 0 ){
		g_debug( "%s: paginating=%s, page_num=%d, line_num=%d, line=%p, next=%p, last_y=%lf, font_height=%lf, line_height=%lf, count=%d",
				thisfn, sdata->paginating ? "True":"False",
				page_num, line_num, ( void * ) line, ( void * ) next,
				sdata->last_y, font_height, line_height, g_list_length( sdata->dataset ));
	}

	/* this line + a group bottom report or a group footer or a page bottom report */
	end_height =
			line_height +
			( sdata->want_groups ?
					(( !next || is_new_group( instance, next, line )) ?
							get_group_footer_height( instance, sdata ) :
							get_group_bottom_report_height( instance, sdata )) :
					get_page_bottom_report_height( instance, page_num, sdata ));

	/* does the group change ? */
	if( sdata->want_groups &&
			is_new_group( instance, line, sdata->last_printed )){

		/* do we have a previous group footer not yet printed ? */
		if( sdata->last_printed && !sdata->group_footer_printed ){
			draw_group_footer( instance, line_num, sdata );
		}
		/* is the group header requested on a new page ? */
		if( line_num > 0 && sdata->want_new_page ){
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
		draw_group_header( instance, line_num, line, sdata );

	} else if( line_num == 0 ){
		if( sdata->want_groups ){
			draw_group_top_report( instance, sdata );
		} else {
			draw_page_top_report( instance, page_num, sdata );
		}

	} else {
		/* either no groups or no new group */
		/* do we have enough vertical space for this line, and a group
		 * bottom report or a group footer or a page bottom report ? */
		req_height = end_height;
		if( sdata->last_y + req_height > sdata->max_y ){
			if( sdata->want_groups ){
				draw_group_bottom_report( instance, sdata );
			} else {
				draw_page_bottom_report( instance, page_num, sdata );
			}
			/*g_debug( "draw_line: last_y=%lf, font_height=%lf, line_height=%lf, req_height=%lf, max_y=%lf",
					sdata->last_y, font_height, line_height, req_height, sdata->max_y );*/
			return( FALSE );
		}
	}

	/* so, we are OK to draw the line ! */
	/* we are using a unique font to draw the lines */
	y = sdata->last_y;

	/* have a rubber every other line */
	if( line_num % 2 ){
		ofa_irenderable_draw_rubber( instance, y-(line_height-font_height)*0.5, line_height );
	}

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_line ){
		ofa_irenderable_set_color( instance, COLOR_BODY );
		ofa_irenderable_set_font( instance, sdata->body_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_line( instance, line );
	}

	y += line_height;
	sdata->last_y = y;

	return( TRUE );
}

/*
 * Do not put here any other default value than FALSE as it is used to
 * draw group header and footer (so do not even test here for prev=NULL).
 * It is up to the implementor to decide if it will make use of groups
 * and to return TRUE on a new group.
 */
static gboolean
is_new_group( const ofaIRenderable *instance, GList *current, GList *prev )
{
	return( OFA_IRENDERABLE_GET_INTERFACE( instance )->is_new_group( instance, current, prev ));
}

static gboolean
irenderable_is_new_group( const ofaIRenderable *instance, GList *current, GList *prev )
{
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
draw_group_header( ofaIRenderable *instance, gint line_num, GList *line, sIRenderable *sdata )
{
	gdouble y, text_height;

	ofa_irenderable_set_color( instance, COLOR_GROUP );
	ofa_irenderable_set_font( instance, st_default_group_font );
	text_height = ofa_irenderable_get_text_height( instance );

	/* separation line */
	if( line_num > 0 && sdata->want_line_separation ){
		y = sdata->last_y;
		cairo_set_line_width( sdata->current_context, 0.5 );
		cairo_move_to( sdata->current_context, 0, y );
		cairo_line_to( sdata->current_context, sdata->render_width, y );
		cairo_stroke( sdata->current_context );
		y += 1.5;
		cairo_move_to( sdata->current_context, 0, y );
		cairo_line_to( sdata->current_context, sdata->render_width, y );
		cairo_stroke( sdata->current_context );
		y += sdata->body_vspace_rate * text_height;
		sdata->last_y = y;
	}

	/* display the group header */
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_header ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_header( instance, line );
	}

	/* setup the group properties */
	sdata->group_footer_printed = FALSE;
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_group_header_height( ofaIRenderable *instance, gint line_num, GList *line, sIRenderable *sdata )
{
	gdouble height, prev_y;
	gboolean prev_printed, prev_paginating;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	prev_y = sdata->last_y;
	prev_printed = sdata->group_footer_printed;
	sdata->last_y = 0;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	draw_group_header( instance, line_num, line, sdata );

	height = sdata->last_y;
	sdata->group_footer_printed = prev_printed;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

/*
 * draw the report for the group at the top of the next body page
 */
static void
draw_group_top_report( ofaIRenderable *instance, sIRenderable *sdata )
{
	ofa_irenderable_set_color( instance, COLOR_REPORT );
	ofa_irenderable_set_font( instance, st_default_report_font );

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_top_report ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_top_report( instance );
	}
}

/*
 * draw the report for the group at the bottom of the current body page
 */
static void
draw_group_bottom_report( ofaIRenderable *instance, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_bottom_report ){
		ofa_irenderable_set_color( instance, COLOR_REPORT );
		ofa_irenderable_set_font( instance, st_default_report_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_bottom_report( instance );
	}
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_group_bottom_report_height( ofaIRenderable *instance, sIRenderable *sdata )
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

	draw_group_bottom_report( instance, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

/*
 * only called if there is actually a group footer in this report
 */
static void
draw_group_footer( ofaIRenderable *instance, gint line_num, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_footer ){
		ofa_irenderable_set_color( instance, COLOR_GROUP );
		ofa_irenderable_set_font( instance, st_default_group_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_group_footer( instance );
	}

	sdata->group_footer_printed = TRUE;
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_group_footer_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gboolean prev_printed, prev_paginating;
	gdouble height, prev_y;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	prev_y = sdata->last_y;
	prev_printed = sdata->group_footer_printed;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;
	prev_paginating = sdata->paginating;
	sdata->paginating = TRUE;

	draw_group_footer( instance, -1, sdata );

	height = sdata->last_y - prev_y;
	sdata->last_y = prev_y;
	sdata->group_footer_printed = prev_printed;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}
static void
draw_page_bottom_report( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{

}

static gdouble
get_page_bottom_report_height( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	draw_page_bottom_report( instance, page_num, sdata );
	return( 0 );
}

/*
 * Returns %TRUE if we have been able to print the bottom summary,
 * %FALSE else (and we need another page)
 */
static void
draw_bottom_summary( ofaIRenderable *instance, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_bottom_summary ){
		ofa_irenderable_set_color( instance, COLOR_SUMMARY );
		ofa_irenderable_set_font( instance, st_default_summary_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_bottom_summary( instance );
	}
}

/*
 * the value cannot be cached as the height may depend of the
 * position of the group header in the sheet, and of the content of
 * the data
 */
static gdouble
get_bottom_summary_height( ofaIRenderable *instance, sIRenderable *sdata )
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

	draw_bottom_summary( instance, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;
	sdata->paginating = prev_paginating;

	return( height );
}

static void
draw_page_footer( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_footer( instance, page_num );
}

static void
irenderable_draw_page_footer( ofaIRenderable *instance, gint page_num )
{
	static gdouble vspace_before_footer = 2.0; /* points */
	static gdouble vspace_after_line = 1.0; /* points */
	sIRenderable *sdata;
	gchar *str, *stamp_str;
	GTimeVal stamp;
	gdouble y, height;

	sdata = get_irenderable_data( instance );

	ofa_irenderable_set_color( instance, COLOR_FOOTER );
	ofa_irenderable_set_font( instance, st_default_footer_font );

	y = sdata->max_y;
	y += vspace_before_footer;
	cairo_set_line_width( sdata->current_context, 0.5 );
	cairo_move_to( sdata->current_context, 0, y );
	cairo_line_to( sdata->current_context, sdata->render_width, y );
	cairo_stroke( sdata->current_context );
	y += vspace_after_line;

	str = g_strdup_printf( "%s v %s", PACKAGE_NAME, PACKAGE_VERSION );
	height = ofa_irenderable_set_text( instance, st_page_margin, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_DMYYHM );
	str = g_strdup_printf(
			_( "Printed on %s - Page %d/%d" ), stamp_str, 1+page_num, sdata->pages_count );
	g_free( stamp_str );
	ofa_irenderable_set_text( instance, sdata->render_width-st_page_margin, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y += height;
	sdata->last_y = y;
}

/*
 * the height of the default page footer is computed by just drawing it
 * at y=0, and then requiring Pango the layout height
 *
 * page footer is expected to have a fixed height
 * so it is worth to caching it
 * so it is called once from begin_render() on start of the pagination phase.
 */
static gdouble
get_page_footer_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble prev_y, prev_render_height, height;

	if(( height = sdata->page_footer_height ) == 0 ){

		prev_y = sdata->last_y;
		sdata->last_y = 0;
		prev_render_height = sdata->render_height;
		sdata->render_height = 0;

		draw_page_footer( instance, 0, sdata );

		height = sdata->last_y;
		sdata->last_y = prev_y;
		sdata->render_height = prev_render_height;
	}

	return( height );
}

/**
 * ofa_irenderable_end_render:
 * @instance:
 * @cr:
 *
 * The last entry point of the interface.
 * Must be called after all pages have been rendered.
 */
void
ofa_irenderable_end_render( ofaIRenderable *instance, cairo_t *context )
{
	static const gchar *thisfn = "ofa_irenderable_end_render";
	sIRenderable *sdata;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );
	set_irenderable_input_context( instance, context, sdata );

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render( instance );
	}

	/* #795 - sIRenderable struct should have the same life cycle
	 * than the implementation page */
	/*
	free_irenderable_data( sdata );
	g_object_set_data( G_OBJECT( instance ), IRENDERABLE_DATA, NULL );
	g_object_weak_unref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	*/
}

/*
 * Note that our own reset runtime is not a default that the
 * application would supersede. Instead, it is actually part of the
 * whole algorithm, and the application may only add its own code.
 */
static void
reset_runtime( ofaIRenderable *instance, sIRenderable *sdata )
{
	sdata->last_printed = NULL;
	sdata->group_footer_printed = TRUE;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->reset_runtime ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->reset_runtime( instance );
	}
}

static sIRenderable *
get_irenderable_data( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = ( sIRenderable * ) g_object_get_data( G_OBJECT( instance ), IRENDERABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIRenderable, 1 );
		g_object_set_data( G_OBJECT( instance ), IRENDERABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

/*
 * save the provided context, and create an associated layout
 * this is called on each entry point of the interface:
 * - begin_render(), render_page() and end_render().
 *
 * simultaneously create a temp context which is used:
 * - first during pagination phase
 * - then each time we need to compute some dimension without actually
 *   drawing anything
 */
static void
set_irenderable_input_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata )
{
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
	/*
	set_font( sdata->in_layout, "Sans 8", NULL );
	gdouble height = set_text( sdata->in_layout, sdata->in_context, 0, 0, "This is a text", PANGO_ALIGN_LEFT );
	g_debug( "set_irenderable_input_context: input context/layout: height=%lf", height );
	set_font( sdata->temp_layout, "Sans 8", NULL );
	height = set_text( sdata->temp_layout, sdata->temp_context, 0, 0, "This is a text", PANGO_ALIGN_LEFT );
	g_debug( "set_irenderable_input_context: temp context/layout: height=%lf", height );
	*/
}

static void
on_instance_finalized( sIRenderable *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_irenderable_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	free_irenderable_data( sdata );
}

static void
free_irenderable_data( sIRenderable *sdata )
{
	g_clear_object( &sdata->in_layout );

	g_free( sdata->body_font );

	if( sdata->temp_context ){
		cairo_destroy( sdata->temp_context );
	}

	g_clear_object( &sdata->temp_layout );

	g_free( sdata );
}

/**
 * ofa_irenderable_is_paginating:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: %TRUE if while pagination.
 */
gboolean
ofa_irenderable_is_paginating( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );

	return( sdata->paginating );
}

/**
 * ofa_irenderable_get_paper_size:
 * @instance: this #ofaIRenderable instance.
 * @width: [out]:
 * @height: [out]:
 */
void
ofa_irenderable_get_paper_size( ofaIRenderable *instance, gdouble *width, gdouble *height )
{
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );

	*width = sdata->render_width;
	*height = sdata->render_height;
}

/**
 * ofa_irenderable_get_page_margin:
 * @instance: this #ofaIRenderable instance.
 */
gdouble
ofa_irenderable_get_page_margin( ofaIRenderable *instance )
{
	return( st_page_margin );
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

	sdata = get_irenderable_data( instance );

	return( sdata->current_context );
}

/**
 * ofa_irenderable_set_font:
 */
void
ofa_irenderable_set_font( ofaIRenderable *instance, const gchar *font_str )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );
	set_font( sdata->in_layout, font_str, NULL );
	set_font( sdata->temp_layout, font_str, NULL );
}

/**
 * ofa_irenderable_set_summary_font:
 *
 * Set summary font and color.
 */
void
ofa_irenderable_set_summary_font( ofaIRenderable *instance )
{
	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	ofa_irenderable_set_color( instance, COLOR_SUMMARY );
	ofa_irenderable_set_font( instance, st_default_summary_font );
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
 * ofa_irenderable_get_text_height:
 * @instance: this #ofaIRenderable instance.
 *
 * Returns: the height in cairo units used by a text drawn in the current font.
 */
gdouble
ofa_irenderable_get_text_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble height;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );

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
 * @text:
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

	sdata = get_irenderable_data( instance );

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
 * Returns: the height in cairo units used by a line.
 */
gdouble
ofa_irenderable_get_line_height( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble text_height, line_height;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );
	text_height = ofa_irenderable_get_text_height( instance );
	line_height = text_height * ( 1+sdata->body_vspace_rate );

	return( line_height );
}

/**
 * ofa_irenderable_get_max_y:
 * @instance: this #ofaIRenderable instance.
 */
gdouble
ofa_irenderable_get_max_y( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );

	return( sdata->max_y );
}

/**
 * ofa_irenderable_get_last_y:
 * @instance: this #ofaIRenderable instance.
 */
gdouble
ofa_irenderable_get_last_y( ofaIRenderable *instance )
{
	sIRenderable *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );

	return( sdata->last_y );
}

/**
 * ofa_irenderable_set_last_y:
 * @instance: this #ofaIRenderable instance.
 * @y: the new ordinate to be set
 */
void
ofa_irenderable_set_last_y( ofaIRenderable *instance, gdouble y )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );

	sdata->last_y = y;
}

/**
 * ofa_irenderable_set_color:
 */
void
ofa_irenderable_set_color( ofaIRenderable *instance, double r, double g, double b )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );
	cairo_set_source_rgb( sdata->current_context, r, g, b );
}

/**
 * ofa_irenderable_draw_rubber:
 */
void
ofa_irenderable_draw_rubber( ofaIRenderable *instance, gdouble top, gdouble height )
{
	sIRenderable *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );
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

	sdata = get_irenderable_data( instance );
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
 * ofa_irenderable_draw_no_data:
 */
void
ofa_irenderable_draw_no_data( ofaIRenderable *instance )
{
	sIRenderable *sdata;
	gdouble y, width, height;

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );

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

	sdata = get_irenderable_data( instance );

	return( set_text( sdata->current_layout, sdata->current_context, x, y, text, align ));
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

	sdata = get_irenderable_data( instance );

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

	sdata = get_irenderable_data( instance );

	pango_layout_set_width( sdata->current_layout, width );
	pango_layout_set_wrap( sdata->current_layout, PANGO_WRAP_WORD );
	height = ofa_irenderable_set_text( instance, x, y, text, align );

	return( height );
}
