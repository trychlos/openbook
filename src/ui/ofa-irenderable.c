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

#include <cairo-pdf.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-utils.h"

#include "ui/ofa-irenderable.h"

/* data associated to each implementor object
 */
typedef struct {
	gdouble      render_width;
	gdouble      render_height;
	gdouble      max_y;
	gdouble      last_y;
	gint         pages_count;
	GList       *dataset;
	GList       *last_printed;
	cairo_t     *in_context;			/* save here the provided context with an associated layout */
	PangoLayout *in_layout;
	gchar       *body_font;
	cairo_t     *current_context;
	PangoLayout *current_layout;
	gdouble      current_font_size;
	gboolean     paginating;

	/* groups management
	 * @want_groups: is initialized to %FALSE on initialization,
	 *  then set to %TRUE during the pagination phase, when the first
	 *  line makes the implementation detect a new group (so tell us
	 *  that it actually manages groups)
	 * @group_footer_printed: is reset to %TRUE in #reset_runtime()
	 *  so that we do not try to draw it if the implementation does
	 *  not manage groups
	 */
	gboolean     want_groups;
	gboolean     want_new_page;
	gboolean     group_footer_printed;

	/* these are height of various portions of the printed
	 * they are computed only once (most probably while begin render)
	 * and cached here
	 */
	gdouble      page_header_columns_height;
	gdouble      page_footer_height;

	/* temporary context and layout to be used during pagination
	 */
	cairo_t     *temp_context;
	PangoLayout *temp_layout;
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
static const gdouble st_columns_vspace_rate_after          = 0.5;
static const gdouble st_body_vspace_rate                   = 0.35;

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIRenderableInterface *klass );
static void          interface_base_finalize( ofaIRenderableInterface *klass );
static gchar        *irenderable_get_body_font( ofaIRenderable *instance );
static gboolean      want_new_page( const ofaIRenderable *instance );
static gboolean      irenderable_want_new_page( const ofaIRenderable *instance );
static gboolean      want_groups( const ofaIRenderable *instance );
static gboolean      irenderable_want_groups( const ofaIRenderable *instance );
static gboolean      draw_page( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          irenderable_draw_page_header( ofaIRenderable *instance, gint page_num );
static void          draw_page_header_dossier( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_title( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_subtitle( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_notes( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
static void          draw_page_header_columns( ofaIRenderable *instance, gint page_num, sIRenderable *sdata );
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
static void          set_irenderable_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata );
static void          on_instance_finalized( sIRenderable *sdata, void *instance );
static void          free_irenderable_data( sIRenderable *sdata );
static cairo_t      *get_temp_context( ofaIRenderable *instance, sIRenderable *sdata );
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
		klass->want_groups = irenderable_want_groups;
		klass->want_new_page = irenderable_want_new_page;
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
 */
gint
ofa_irenderable_begin_render( ofaIRenderable *instance, cairo_t *cr, gdouble render_width, gdouble render_height )
{
	static const gchar *thisfn = "ofa_irenderable_begin_render";
	sIRenderable *sdata;
	gint page_num;

	g_return_val_if_fail( instance && OFA_IS_IRENDERABLE( instance ), 0 );

	sdata = get_irenderable_data( instance );
	set_irenderable_context( instance, cr, sdata );

	sdata->render_width = render_width;
	sdata->render_height = render_height;
	sdata->paginating = TRUE;

	sdata->body_font = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_body_font( instance );
	sdata->temp_context = get_temp_context( instance, sdata );
	sdata->temp_layout = pango_cairo_create_layout( sdata->temp_context );
	sdata->page_header_columns_height = get_page_header_columns_height( instance, sdata );
	sdata->page_footer_height = get_page_footer_height( instance, sdata );
	sdata->max_y = render_height - sdata->page_footer_height;
	sdata->current_context = sdata->temp_context;
	sdata->current_layout = sdata->temp_layout;

	sdata->want_groups = want_groups( instance );
	if( sdata->want_groups ){
		sdata->want_new_page = want_new_page( instance );
	}

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dataset ){
		sdata->dataset = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dataset( instance );
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
 * default implementation of #get_body_font() virtual
 */
static gchar *
irenderable_get_body_font( ofaIRenderable *instance )
{
	return( g_strdup( st_default_body_font ));
}

static gboolean
want_groups( const ofaIRenderable *instance )
{
	return( OFA_IRENDERABLE_GET_INTERFACE( instance )->want_groups( instance ));
}

static gboolean
irenderable_want_groups( const ofaIRenderable *instance )
{
	return( FALSE );
}

static gboolean
want_new_page( const ofaIRenderable *instance )
{
	return( OFA_IRENDERABLE_GET_INTERFACE( instance )->want_new_page( instance ));
}

static gboolean
irenderable_want_new_page( const ofaIRenderable *instance )
{
	return( FALSE );
}

/**
 * ofa_irenderable_render_page:
 * @instance:
 * @cr:
 * @page_number: counted from zero.
 */
void
ofa_irenderable_render_page( ofaIRenderable *instance, cairo_t *context, gint page_number )
{
	static const gchar *thisfn = "ofa_irenderable_render_page";
	sIRenderable *sdata;

	sdata = get_irenderable_data( instance );
	set_irenderable_context( instance, context, sdata );

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
	const gchar *dossier_name;
	gdouble y, height;

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_name ){
		dossier_name = OFA_IRENDERABLE_GET_INTERFACE( instance )->get_dossier_name( instance );
		y = sdata->last_y;

		ofa_irenderable_set_color( instance, COLOR_HEADER_DOSSIER );
		ofa_irenderable_set_font( instance, st_default_header_dossier_font );
		height = ofa_irenderable_set_text( instance, 0, y, dossier_name, PANGO_ALIGN_LEFT );

		y += height;
		sdata->last_y = y;
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
	static const gdouble st_vspace_rate_after = 0.4;
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
			y += ofa_irenderable_get_text_height( instance ) * st_vspace_rate_after;

			sdata->last_y = y;
		}
		g_free( title );
	}
}

static void
draw_page_header_notes( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes ){
		ofa_irenderable_set_color( instance, COLOR_HEADER_NOTES );
		ofa_irenderable_set_font( instance, sdata->body_font );
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_notes( instance, page_num );
	}
}

/*
 * default implementation of #draw_page_header() virtual
 * - columns header
 *
 * May be called first with @page_num=-1 when computing the height of
 * the columns headers
 */
static void
draw_page_header_columns( ofaIRenderable *instance, gint page_num, sIRenderable *sdata )
{
	gdouble height;

	/* draw and paint a rectangle
	 * this must be done before writing the columns headers */
	if( page_num >= 0 ){
		ofa_irenderable_set_color( instance, COLOR_HEADER_COLUMNS_BG );
		height = get_page_header_columns_height( instance, sdata );
		cairo_rectangle( sdata->current_context, 0, sdata->last_y, sdata->render_width, height );
		cairo_fill( sdata->current_context );
	}

	ofa_irenderable_set_color( instance, COLOR_HEADER_COLUMNS_FG );
	ofa_irenderable_set_font( instance, st_default_header_columns_font );

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->draw_page_header_columns( instance, page_num );
	}

	sdata->last_y += st_columns_vspace_rate_after * ofa_irenderable_get_text_height( instance );
}

/*
 * header columns have always the same height
 * so it is worth to caching it
 * This is thus called once from begin_render() during pagination phase.
 */
static gdouble
get_page_header_columns_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	static gboolean in_function = FALSE;	/* against recursion */
	gdouble prev_y, height;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	height = 0;

	if( !in_function ){
		in_function = TRUE;
		prev_context = sdata->current_context;
		sdata->current_context = sdata->temp_context;
		prev_layout = sdata->current_layout;
		sdata->current_layout = sdata->temp_layout;
		prev_y = sdata->last_y;

		draw_page_header_columns( instance, -1, sdata );

		height = sdata->last_y
				- prev_y
				- st_columns_vspace_rate_after * ofa_irenderable_get_text_height( instance );
		sdata->last_y = prev_y;
		sdata->current_context = prev_context;
		sdata->current_layout = prev_layout;
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

	if( 0 ){
		g_debug( "%s: page_num=%d, line_num=%d, line=%p, next=%p, count=%d",
				thisfn, page_num, line_num, ( void * ) line, ( void * ) next,
				g_list_length( sdata->dataset ));
	}

	/* must be set before any height computing as this is the main
	 * parameter  */
	ofa_irenderable_set_color( instance, COLOR_BODY );
	ofa_irenderable_set_font( instance, sdata->body_font );
	font_height = ofa_irenderable_get_text_height( instance );
	line_height = font_height * ( 1+st_body_vspace_rate );

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
		if( line_num > 0 && want_new_page( instance )){
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
			g_debug( "draw_line: last_y=%lf, font_height=%lf, line_height=%lf, req_height=%lf, max_y=%lf",
					sdata->last_y, font_height, line_height, req_height, sdata->max_y );
			return( FALSE );
		}
	}

	/* so, we are OK to draw the line ! */
	/* we are using a unique font to draw the lines */
	y = sdata->last_y;

	/* have a rubber every other line */
	if( line_num % 2 ){
		ofa_irenderable_draw_rubber( instance, y-(line_height-font_height)*0.25, line_height );
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
	if( line_num > 0 ){
		y = sdata->last_y;
		cairo_set_line_width( sdata->current_context, 0.5 );
		cairo_move_to( sdata->current_context, 0, y );
		cairo_line_to( sdata->current_context, sdata->render_width, y );
		cairo_stroke( sdata->current_context );
		y += 1.5;
		cairo_move_to( sdata->current_context, 0, y );
		cairo_line_to( sdata->current_context, sdata->render_width, y );
		cairo_stroke( sdata->current_context );
		y += st_body_vspace_rate * text_height;
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

	y = sdata->render_height - sdata->page_footer_height;
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
 * We consider here that the footer is of constant height, and so can
 * be cached. This is thus called once from begin_render() during
 * pagination phase.
 */
static gdouble
get_page_footer_height( ofaIRenderable *instance, sIRenderable *sdata )
{
	gdouble prev_y, prev_render_height, height;
	cairo_t *prev_context;
	PangoLayout *prev_layout;

	prev_y = sdata->last_y;
	sdata->last_y = 0;
	prev_render_height = sdata->render_height;
	sdata->render_height = 0;
	prev_context = sdata->current_context;
	sdata->current_context = sdata->temp_context;
	prev_layout = sdata->current_layout;
	sdata->current_layout = sdata->temp_layout;

	draw_page_footer( instance, 0, sdata );

	height = sdata->last_y;
	sdata->last_y = prev_y;
	sdata->render_height = prev_render_height;
	sdata->current_context = prev_context;
	sdata->current_layout = prev_layout;

	return( height );
}

/**
 * ofa_irenderable_end_render:
 * @instance:
 * @cr:
 */
void
ofa_irenderable_end_render( ofaIRenderable *instance, cairo_t *context )
{
	static const gchar *thisfn = "ofa_irenderable_end_render";
	sIRenderable *sdata;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_if_fail( instance && OFA_IS_IRENDERABLE( instance ));

	sdata = get_irenderable_data( instance );
	set_irenderable_context( instance, context, sdata );

	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->end_render( instance );
	}
	if( OFA_IRENDERABLE_GET_INTERFACE( instance )->free_dataset ){
		OFA_IRENDERABLE_GET_INTERFACE( instance )->free_dataset( instance, sdata->dataset );
	}

	free_irenderable_data( sdata );
	g_object_set_data( G_OBJECT( instance ), IRENDERABLE_DATA, NULL );
	g_object_weak_unref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
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

static void
set_irenderable_context( ofaIRenderable *instance, cairo_t *context, sIRenderable *sdata )
{
	/* save the provided context */
	sdata->in_context = context;

	/* create an associated pango layout */
	g_clear_object( &sdata->in_layout );
	sdata->in_layout = pango_cairo_create_layout( sdata->in_context );
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

/*
 * get a temporary context to be able to draw anything to compute its height
 * the returned context should be cairo_destroy() after use.
 */
static cairo_t *
get_temp_context( ofaIRenderable *instance, sIRenderable *sdata )
{
	cairo_surface_t *surf;
	cairo_t *new_cr;

	surf = cairo_pdf_surface_create( NULL, sdata->render_width, sdata->render_height );
	new_cr = cairo_create( surf );
	cairo_surface_destroy( surf );

	return( new_cr );
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
	set_font( sdata->current_layout, font_str, &sdata->current_font_size );
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
