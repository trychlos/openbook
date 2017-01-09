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

#ifndef __OPENBOOK_API_OFA_IRENDERABLE_H__
#define __OPENBOOK_API_OFA_IRENDERABLE_H__

/**
 * SECTION: irenderable
 * @title: ofaIRenderable
 * @short_description: The IRenderable Interface
 * @include: api/ofa-irenderable.h
 *
 * The #ofaIRenderable interface lets its users benefit of the
 * standardized printing system of Openbook.
 */

#include <glib.h>

G_BEGIN_DECLS

#define OFA_TYPE_IRENDERABLE                      ( ofa_irenderable_get_type())
#define OFA_IRENDERABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IRENDERABLE, ofaIRenderable ))
#define OFA_IS_IRENDERABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IRENDERABLE ))
#define OFA_IRENDERABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IRENDERABLE, ofaIRenderableInterface ))

typedef struct _ofaIRenderable                    ofaIRenderable;

/**
 * ofaIRenderableInterface:
 *
 * This defines the interface that an #ofaIRenderable should implement.
 *
 * A #IRenderable summary is built on top on a standard
 * #GtkPrintOperation, where each page contains a page header, a page
 * body and a page footer.
 *
 * The printing itself contains a printing header, printed on top on
 * the first page, and a printing summary, printed on bottom of the
 * last page.
 *
 * The page body may contains one to any groups, where each group may
 * have a group header, zero to any group lines and a group summary.
 * If a group layouts on several pages, then a bottom group report may
 * be printed on bottom of the page, and a top group report be printed
 * on the top of the next page.
 *
 * The #IRenderable interface does not actually send the report to a
 * printer. Instead, it exports it to a named PDF file.
 *
 * Application page setup (size and orientation) should be done just
 * after init_dialog() method returns.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIRenderable interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint              ( *get_interface_version )    ( const ofaIRenderable *instance );

	/**
	 * get_paper_name:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the name of the used paper.
	 *
	 * Default to GTK_PAPER_NAME_A4.
	 */
	const gchar *      ( *get_paper_name )           ( ofaIRenderable *instance );

	/**
	 * get_page_orientation:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the used orientation.
	 *
	 * Default to GTK_PAGE_ORIENTATION_PORTRAIT.
	 */
	GtkPageOrientation ( *get_page_orientation )     ( ofaIRenderable *instance );

	/**
	 * begin_render:
	 * @instance: the #ofaIRenderable instance.
	 * @context: the #cairo_t context.
	 *
	 * This method is called by the interface when about to begin the
	 * rendering.
	 */
	void               ( *begin_render )             ( ofaIRenderable *instance,
															gdouble render_width,
															gdouble render_height );

	/**
	 * get_body_font:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the font family description as a newly allocated string
	 * which will be freed by the interface.
	 *
	 * Default is "Sans 8".
	 */
	gchar *            ( *get_body_font )            ( ofaIRenderable *instance );

	/**
	 * get_body_vspace_rate:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the vspace rate to be applied to body line.
	 *
	 * Default is 0.35.
	 */
	gdouble            ( *get_body_vspace_rate )     ( ofaIRenderable *instance );

	/**
	 * draw_page_header:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * The page header is drawn on top of each page.
	 *
	 * If implemented, this method must draw the page header on the
	 * provided @context. In this case, it is the responsability of the
	 * application to updating the 'last_y' ordonate at the end of the
	 * page header drawing.
	 *
	 * The interface code provide a suitable default which includes:
	 * - requiring the implementation and drawing the dossier name
	 *   (see #get_dossier_name())
	 * - requiring the implementation and drawing a title
	 *   (see #get_page_header_title())
	 * - requiring the implementation and drawing a subtitle
	 *   (see #get_page_header_subtitle())
	 * - requiring the implementation and drawing somes introduction/notes
	 *   text (see #get_page_header_notes())
	 * - drawing headers columns
	 * See infra get_page_header_title() and get_page_header_subtitle()
	 * virtuals.
	 */
	void               ( *draw_page_header )         ( ofaIRenderable *instance,
															gint page_num );

	/**
	 * get_dossier_name:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the name of the dossier required as part of the default
	 * implementation of #draw_page_header() virtual, as a newly
	 * allocated string which should be g_free() by the caller.
	 */
	gchar *            ( *get_dossier_name )         ( const ofaIRenderable *instance );

	/**
	 * get_page_header_title:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the title of the report as a newly allocated string.
	 *
	 * This title is required as part of the default implementation of
	 * #draw_page_header() virtual.
	 *
	 * The interface code takes care itself of :
	 * - g_free-ing() the returned string
	 * - updating the 'last_y' ordonate at the end of the drawing.
	 */
	gchar *            ( *get_page_header_title )    ( const ofaIRenderable *instance );

	/**
	 * get_page_header_subtitle:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the subtitle of the report as a newly allocated string.
	 *
	 * This subtitle is required as part of the default implementation
	 * of #draw_page_header() virtual.
	 *
	 * The interface code takes care itself of :
	 * - g_free-ing() the returned string
	 * - updating the 'last_y' ordonate at the end of the drawing.
	 */
	gchar *            ( *get_page_header_subtitle ) ( const ofaIRenderable *instance );

	/**
	 * get_page_header_subtitle2:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: a second subtitle as a newly allocated string.
	 *
	 * The interface code takes care itself of :
	 * - g_free-ing() the returned string
	 * - updating the 'last_y' ordonate at the end of the drawing.
	 */
	gchar *            ( *get_page_header_subtitle2 )( const ofaIRenderable *instance );

	/**
	 * draw_page_header_notes:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, let us insert some notes between the subtitle and
	 * the columns headers.
	 *
	 * This method is called as part of the default implementation of
	 * the #draw_header() virtual. When called, the cairo context is
	 * setup with body font, color and background.
	 *
	 * The application must take care of updating the 'last_y' ordonate
	 * according to the count of lines it has been printed, and of the
	 * vertical space it may have added between these rows, and it wishes
	 * to be added after the block of text.
	 */
	void               ( *draw_page_header_notes )   ( ofaIRenderable *instance,
															gint page_num );

	/**
	 * draw_page_header_columns:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, should write the column headers.
	 *
	 * This method is called as part of the default implementation of
	 * the #draw_header() virtual. When called, the cairo context has
	 * already been filled with a colored rectangle, and the layout is
	 * set to suitable text font color and size.
	 *
	 * The application must take care of updating the 'last_y' ordonate
	 * according to the count of lines it has been printed, and of the
	 * vertical space it may have added between these rows.
	 *
	 * Note that this method is called once with @page_num=-1 when
	 * computing the height of the columns headers.
	 */
	void               ( *draw_page_header_columns ) ( ofaIRenderable *instance,
															gint page_num );

	/**
	 * draw_top_summary:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * If implemented, this method must draw the top summary on the
	 * provided @context.
	 *
	 * The top summary is drawn on the first page after the page header.
	 *
	 * The application must take care of updating the 'last_y' ordonate
	 * according to the vertical space it has used, and depending of the
	 * vertical space it wants to set before the first line.
	 */
	void               ( *draw_top_summary )         ( ofaIRenderable *instance );

	/**
	 * want_groups:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: %TRUE if the implementation manages groups
	 *
	 * Defaults to %FALSE.
	 */
	gboolean           ( *want_groups )              ( const ofaIRenderable *instance );

	/**
	 * want_new_page:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: If implemented, this method should return %TRUE if the
	 * implementation wishes to begin a new page on new groups.
	 *
	 * This is only called if #want_groups() has returned %TRUE.
	 *
	 * Defaults to %FALSE (on the same page while there is enough place).
	 */
	gboolean           ( *want_new_page )            ( const ofaIRenderable *instance );

	/**
	 * want_line_separation:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: If implemented, this method should return %TRUE if the
	 * implementation wishes to separate groups on the same page by a line
	 * separation/.
	 *
	 * This is only called if #want_groups() has returned %TRUE and
	 * #want_new_page() has returned %FALSE.
	 *
	 * Defaults to %TRUE (groups are line-separated).
	 */
	gboolean           ( *want_line_separation )     ( const ofaIRenderable *instance );

	/**
	 * is_new_group:
	 * @instance: the #ofaIRenderable instance.
	 * @current: the element currently candidate for printing (not nul).
	 * @prev: the last printed element,
	 *  may be %NULL when dealing with the first element.
	 *
	 * Returns: If implemented, this method should return %TRUE if the
	 * @current element does not belong to the same group than @prev.
	 *
	 * Defaults to FALSE (no group).
	 */
	gboolean           ( *is_new_group )             ( const ofaIRenderable *instance,
															GList *current,
															GList *prev );

	/**
	 * draw_group_header:
	 * @instance: the #ofaIRenderable instance.
	 * @current: the first element of the group.
	 *
	 * If implemented, this method must draw the group header on the
	 * provided @context.
	 *
	 * The group header is drawn on top on each group of lines.
	 * This is also the good time to initialize the data specific
	 * to this new group.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used in order
	 * for the interface to auto-detect its height.
	 */
	void               ( *draw_group_header )       ( ofaIRenderable *instance,
															GList *current );

	/**
	 * draw_group_top_report:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * If implemented, this method must draw the top report for
	 * the group on the provided @context.
	 *
	 * The top report is drawn on the top of the page, as a recall
	 * of the current group. It is usually associated with a group
	 * bottom report on the previous page.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used.
	 */
	void               ( *draw_group_top_report )   ( ofaIRenderable *instance );

	/**
	 * draw_line:
	 * @instance: the #ofaIRenderable instance.
	 * @current: the element to be printed on this line.
	 *
	 * If implemented, this method must draw the line on the provided
	 * @context.
	 *
	 * The interface code takes care itself of upating the 'last_y'
	 * coordinate of the height of one standard line.
	 */
	void               ( *draw_line )               ( ofaIRenderable *instance,
															GList *current );

	/**
	 * draw_group_bottom_report:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * If implemented, this method must draw the bottom report for
	 * the group on the provided @context.
	 *
	 * The bottom report is drawn on the bottom of the page, as a recall
	 * of the current group. It is usually associated with a group top
	 * report on the next page.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used.
	 */
	void               ( *draw_group_bottom_report )( ofaIRenderable *instance );

	/**
	 * draw_group_footer:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * If implemented, this method must draw the footer summary for
	 * the current group.
	 *
	 * The group footer is drawn at the end of each group, just before
	 * the group header of the next group (if any).
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used.
	 */
	void               ( *draw_group_footer )       ( ofaIRenderable *instance );

	/**
	 * draw_bottom_summary:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * If implemented, this method must draw the bottom summary on the
	 * provided @context.
	 *
	 * The bottom summary is drawn of the last page, at the end of the
	 * report.
	 */
	void               ( *draw_bottom_summary )     ( ofaIRenderable *instance );

	/**
	 * draw_page_footer:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, this method must draw the page footer on the
	 * provided @context.
	 */
	void               ( *draw_page_footer )        ( ofaIRenderable *instance,
															gint page_num );

	/**
	 * reset_runtime:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * This method is called by the interface between begin_render()
	 * and draw_page() calls, in order to let the implementation
	 * reinitializes its own internal datas.
	 */
	void               ( *reset_runtime )           ( ofaIRenderable *instance );

	/**
	 * end_render:
	 * @instance: the #ofaIRenderable instance.
	 */
	void               ( *end_render )              ( ofaIRenderable *instance );
}
	ofaIRenderableInterface;

GType        ofa_irenderable_get_type             ( void );

guint        ofa_irenderable_get_interface_last_version
                                                  ( const ofaIRenderable *instance );

/* three main entry points for this interface
 */
gint         ofa_irenderable_begin_render         ( ofaIRenderable *instance,
															cairo_t *cr,
															gdouble render_width,
															gdouble render_height,
															GList *dataset );

void         ofa_irenderable_render_page          ( ofaIRenderable *instance,
															cairo_t *cr,
															gint page_number );

void         ofa_irenderable_end_render           ( ofaIRenderable *instance,
															cairo_t *cr );

/* helpers
 */
gboolean     ofa_irenderable_is_paginating        ( ofaIRenderable *instance );

void         ofa_irenderable_get_paper_size       ( ofaIRenderable *instance,
															gdouble *width,
															gdouble *height );

gdouble      ofa_irenderable_get_page_header_columns_height
                                                  ( ofaIRenderable *instance );

gdouble      ofa_irenderable_get_page_margin      ( ofaIRenderable *instance );

cairo_t     *ofa_irenderable_get_context          ( ofaIRenderable *instance );

void         ofa_irenderable_set_font             ( ofaIRenderable *instance,
															const gchar *font_str );

void         ofa_irenderable_set_summary_font     ( ofaIRenderable *instance );

gdouble      ofa_irenderable_get_text_height      ( ofaIRenderable *instance );

gdouble      ofa_irenderable_get_text_width       ( ofaIRenderable *instance,
															const gchar *text );

gdouble      ofa_irenderable_get_line_height      ( ofaIRenderable *instance );

void         ofa_irenderable_set_color            ( ofaIRenderable *instance,
															gdouble r,
															gdouble g,
															gdouble b );

gdouble      ofa_irenderable_get_last_y           ( ofaIRenderable *instance );

void         ofa_irenderable_set_last_y           ( ofaIRenderable *instance,
															gdouble y );

gdouble      ofa_irenderable_get_max_y            ( ofaIRenderable *instance );

gint         ofa_irenderable_get_pages_count      ( ofaIRenderable *instance );

void         ofa_irenderable_draw_rubber          ( ofaIRenderable *instance,
															gdouble top,
															gdouble height );

void         ofa_irenderable_draw_rect            ( ofaIRenderable *instance,
															gdouble x,
															gdouble y,
															gdouble width,
															gdouble height );

void         ofa_irenderable_draw_no_data         ( ofaIRenderable *instance );

gdouble      ofa_irenderable_set_text             ( ofaIRenderable *instance,
															gdouble x,
															gdouble y,
															const gchar *text,
															PangoAlignment align );

gdouble      ofa_irenderable_ellipsize_text       ( ofaIRenderable *instance,
															gdouble x,
															gdouble y,
															const gchar *text,
															gdouble max_size );

gdouble      ofa_irenderable_set_wrapped_text     ( ofaIRenderable *instance,
															gdouble x,
															gdouble y,
															gdouble width,
															const gchar *text,
															PangoAlignment align );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IRENDERABLE_H__ */
