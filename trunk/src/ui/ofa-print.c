/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-print.h"

#define COLOR_DARK_RED              0.5,    0,      0
#define COLOR_DARK_CYAN             0,      0.5,    0.5
#define COLOR_GRAY                  0.6,    0.6,    0.6
#define COLOR_LIGHT_GRAY            0.9375, 0.9375, 0.9375

#define COLOR_HEADER_DOSSIER        COLOR_DARK_RED
#define COLOR_HEADER_TITLE          COLOR_DARK_CYAN
#define COLOR_FOOTER                COLOR_GRAY

static const gchar *st_font_family                       = "Sans";
static const gint   st_header_dossier_name_font_size     = 14; /* 18 */
#define             st_header_dossier_name_line_spacing    1 /*st_header_dossier_name_font_size*.25*/
static const gint   st_header_dossier_label_font_size    = 14;

static const gint   st_summary_title_font_size           = 14;
#define             st_summary_title_line_spacing_before   st_summary_subtitle_font_size
static const gint   st_summary_title_line_spacing_after  = 3;
static const gint   st_summary_subtitle_font_size        = 10;
#define             st_summary_subtitle_line_spacing       st_summary_subtitle_font_size

static const gint   st_footer_font_size                  = 7;
#define             st_footer_before_line_vspacing         2 /*st_footer_font_size*/
static const gint   st_footer_after_line_vspacing        = 1;

static const gint   st_margin                            = 2;

static void    header_dossier_page1_line1_render( GtkPrintContext *context, PangoLayout *layout, gdouble y, const ofoDossier *dossier );
static void    header_dossier_page1_line2_render( GtkPrintContext *context, PangoLayout *layout, gdouble y, const ofoDossier *dossier );

/**
 * ofa_print_header_dossier_render:
 *
 * The dossier header is printed the same way in all pages
 * so we can safely ignore the pages indicators
 */
void
ofa_print_header_dossier_render( GtkPrintContext *context, PangoLayout *layout,
										gint page_num, gboolean is_last,
										gdouble y, const ofoDossier *dossier )
{
	header_dossier_page1_line1_render( context, layout, y, dossier );

	if( 0 ){
		y += st_header_dossier_name_font_size;
		y += st_header_dossier_name_line_spacing;
		header_dossier_page1_line2_render( context, layout, y, dossier );
	}
}

static void
header_dossier_page1_line1_render( GtkPrintContext *context, PangoLayout *layout,
										gdouble y, const ofoDossier *dossier )
{
	cairo_t *cr;
	gdouble width;
	gchar *str;

	/* dossier name on line 1 */
	str = g_strdup_printf( "%s Bold Italic %d", st_font_family, st_header_dossier_name_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );

	ofa_print_set_color( context, layout, COLOR_HEADER_DOSSIER );

	ofa_print_set_text( context, layout, 0, y, ofo_dossier_get_name( dossier ), PANGO_ALIGN_LEFT );

	if( 0 ){
		cr = gtk_print_context_get_cairo_context( context );
		width = gtk_print_context_get_width( context );
		cairo_set_line_width( cr, 0.25 );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, width, y );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y+st_header_dossier_name_font_size );
		cairo_line_to( cr, width, y+st_header_dossier_name_font_size );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, 0, y+st_header_dossier_name_font_size );
		cairo_stroke( cr );

		cairo_move_to( cr, width, y );
		cairo_line_to( cr, width, y+st_header_dossier_name_font_size );
		cairo_stroke( cr );
	}
}

static void
header_dossier_page1_line2_render( GtkPrintContext *context, PangoLayout *layout,
										gdouble y, const ofoDossier *dossier )
{
	gchar *str;

	/* dossier label on line 2 */
	str = g_strdup_printf( "%s Bold %d", st_font_family, st_header_dossier_label_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );
	ofa_print_set_color( context, layout, COLOR_HEADER_DOSSIER );
	ofa_print_set_text( context, layout, 0, y, ofo_dossier_get_label( dossier ), PANGO_ALIGN_LEFT );
}

/**
 * ofa_print_header_dossier_get_height:
 *
 * Returns the height of the dossier header, without the trailing line
 * spacing (which will to be prepended when printing the summary title)
 */
gdouble
ofa_print_header_dossier_get_height( gint page_num, gboolean is_last )
{
	gdouble y;

	y = 0;
	y += st_header_dossier_name_font_size;

	if( 0 ){
		y += st_header_dossier_name_line_spacing;
		y += st_header_dossier_label_font_size;
	}

	return( y );
}

/**
 * ofa_print_header_title_render:
 */
void
ofa_print_header_title_render( GtkPrintContext *context, PangoLayout *layout,
									gint page_num, gboolean is_last, gdouble y, const gchar *title )
{
	cairo_t *cr;
	gchar *str;
	gdouble width;

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_summary_title_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );

	ofa_print_header_title_set_color( context, layout );

	width = gtk_print_context_get_width( context );
	y += st_summary_title_line_spacing_before;

	ofa_print_set_text( context, layout, width/2, y, title, PANGO_ALIGN_CENTER );

	if( 0 ){
		cr = gtk_print_context_get_cairo_context( context );
		cairo_set_line_width( cr, 0.25 );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, width, y );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y+st_summary_title_font_size );
		cairo_line_to( cr, width, y+st_summary_title_font_size );
		cairo_stroke( cr );

		cairo_move_to( cr, 0, y );
		cairo_line_to( cr, 0, y+st_summary_title_font_size );
		cairo_stroke( cr );

		cairo_move_to( cr, width, y );
		cairo_line_to( cr, width, y+st_summary_title_font_size );
		cairo_stroke( cr );
	}
}

/**
 * ofa_print_header_title_get_height:
 */
gdouble
ofa_print_header_title_get_height( gint page_num, gboolean is_last )
{
	gdouble y;

	y = 0;
	y += st_summary_title_line_spacing_before;
	y += st_summary_title_font_size;
	y += st_summary_title_line_spacing_after;

	return( y );
}

/**
 * ofa_print_header_subtitle_render:
 */
void
ofa_print_header_subtitle_render( GtkPrintContext *context, PangoLayout *layout,
									gint page_num, gboolean is_last, gdouble y, const gchar *title )
{
	gchar *str;
	gdouble width;

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_summary_subtitle_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );

	ofa_print_header_title_set_color( context, layout );

	width = gtk_print_context_get_width( context );
	ofa_print_set_text( context, layout, width/2, y, title, PANGO_ALIGN_CENTER );
}

/**
 * ofa_print_header_subtitle_get_height:
 */
gdouble
ofa_print_header_subtitle_get_height( gint page_num, gboolean is_last )
{
	gdouble y;

	y = 0;
	y += st_summary_subtitle_font_size;
	y += st_summary_subtitle_line_spacing;

	return( y );
}

/**
 * ofa_print_header_title_set_color:
 */
void
ofa_print_header_title_set_color( GtkPrintContext *context, PangoLayout *layout )
{
	ofa_print_set_color( context, layout, COLOR_HEADER_TITLE );
}

/**
 * ofa_print_footer_render:
 * @page_num: page number, counted from zero
 *
 * The footer is built with:
 * - a vertical space between the bottom of the body and the separator
 * - a line separator
 * - one or more lines which are the said footer
 *
 * The last line of the footer is printable at y=page_height.
 */
void
ofa_print_footer_render( GtkPrintContext *context,
								PangoLayout *layout, gint page_num, gint pages_count )
{
	gchar *str, *stamp_str;
	GTimeVal stamp;
	gdouble width, y;
	cairo_t *cr;

	width = gtk_print_context_get_width( context );
	y = gtk_print_context_get_height( context );

	str = g_strdup_printf( "%s Italic %d", st_font_family, st_footer_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );

	ofa_print_set_color( context, layout, COLOR_FOOTER );

	str = g_strdup_printf( "%s v %s", PACKAGE_NAME, PACKAGE_VERSION );
	ofa_print_set_text( context, layout, st_margin, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_DMYYHM );
	str = g_strdup_printf( _( "Printed on %s - Page %d/%d" ), stamp_str, 1+page_num, pages_count );
	g_free( stamp_str );
	ofa_print_set_text( context, layout, width-st_margin, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y -= st_footer_after_line_vspacing;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_line_width( cr, 0.5 );
	cairo_move_to( cr, 0, y );
	cairo_line_to( cr, width, y );
	cairo_stroke( cr );
}

/**
 * ofa_print_footer_get_height:
 *
 * Returns the height of the page footer, including a leading line
 * spacing to make sure the summary body doesn't come to close of the
 * footer
 */
gdouble
ofa_print_footer_get_height( gint page_num, gboolean is_last )
{
	gdouble y;

	y = 0;
	y += st_footer_before_line_vspacing;
	y += st_footer_after_line_vspacing;

	return( y );
}

/**
 * ofa_print_ruler:
 */
void
ofa_print_ruler( GtkPrintContext *context, PangoLayout *layout, gdouble y )
{
	cairo_t *cr;
	gdouble x, width;
	gchar *str;

	str = g_strdup_printf( "%s %d", st_font_family, st_footer_font_size );
	ofa_print_set_font( context, layout, str );
	g_free( str );

	ofa_print_set_color( context, layout, COLOR_FOOTER );

	cr = gtk_print_context_get_cairo_context( context );
	width = gtk_print_context_get_width( context );
	cairo_set_line_width( cr, 0.25 );

	cairo_move_to( cr, 0, y );
	cairo_line_to( cr, width, y );
	cairo_stroke( cr );

	x = 0;
	while( x < width ){
		cairo_move_to( cr, x, y );
		cairo_line_to( cr, x, y+10 );
		cairo_stroke( cr );
		x += 10;
	}
}

/**
 * ofa_print_rubber:
 */
void
ofa_print_rubber( GtkPrintContext *context, PangoLayout *layout, gdouble top, gdouble height )
{
	cairo_t *cr;
	gdouble width;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, COLOR_LIGHT_GRAY );
	width = gtk_print_context_get_width( context );
	cairo_rectangle( cr, 0, top, width, height );
	cairo_fill( cr );
}

/**
 * ofa_print_set_color:
 */
void
ofa_print_set_color( GtkPrintContext *context, PangoLayout *layout,
								double red, double green, double blue )
{
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, red, green, blue );
}

/**
 * ofa_print_set_font:
 */
void
ofa_print_set_font( GtkPrintContext *context, PangoLayout *layout, const gchar *font_desc )
{
	PangoFontDescription *desc;

	desc = pango_font_description_from_string( font_desc );
	pango_layout_set_font_description( layout, desc );
	pango_font_description_free( desc );
}

/**
 * ofa_print_set_text:
 *
 * the 'x' abcisse must point to the tab reference:
 * - when aligned on left, to the left
 * - when aligned on right, to the right
 * - when centered, to the middle point
 */
void
ofa_print_set_text( GtkPrintContext *context, PangoLayout *layout,
							gdouble x, gdouble y, const gchar *text, PangoAlignment align )
{
	static const gchar *thisfn = "ofa_print_set_text";
	cairo_t *cr;
	PangoRectangle rc;

	pango_layout_set_text( layout, text, -1 );
	cr = gtk_print_context_get_cairo_context( context );

	if( align == PANGO_ALIGN_LEFT ){
		cairo_move_to( cr, x, y );

	} else if( align == PANGO_ALIGN_RIGHT ){
		pango_layout_get_pixel_extents( layout, NULL, &rc );
		cairo_move_to( cr, x-rc.width, y );

	} else if( align == PANGO_ALIGN_CENTER ){
		pango_layout_get_pixel_extents( layout, NULL, &rc );
		cairo_move_to( cr, x-rc.width/2, y );

	} else {
		g_warning( "%s: %d: unknown print alignment indicator", thisfn, align );
	}

	pango_cairo_update_layout( cr, layout );
	pango_cairo_show_layout( cr, layout );
}
