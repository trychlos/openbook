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
 * standardized rendering system of Openbook.
 *
 * The #ofaIRenderable interface is meant to be implemented by the final
 * class, itself being derived from #ofaRenderPage.
 *
 * Architecture
 * ------------
 *
 *       ofaRenderPage
 *       |
 *       +- implements ofaIPrintable      ->     ofa<Something>Render
 *       |                                       |
 *       +- render via ofaIRenderable            +- is derived from ofaRenderPage
 *          client API interface                 |
 *                                               +- implements ofaIRenderable
 *                                                             |
 *                                                             +- defines 'ofa-render-page' signal
 *
 * Signals
 * -------
 * - 'ofa-render-page': is emitted during pagination, both when
 *   computing the pages count (paginating), then when actually drawing
 *   the pages (rendering);
 *   i.e. if the final rendering has 5 pages, the signal will be
 *   emitted 10 times.
 */

#include <glib.h>

G_BEGIN_DECLS

#define OFA_TYPE_IRENDERABLE                      ( ofa_irenderable_get_type())
#define OFA_IRENDERABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IRENDERABLE, ofaIRenderable ))
#define OFA_IS_IRENDERABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IRENDERABLE ))
#define OFA_IRENDERABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IRENDERABLE, ofaIRenderableInterface ))

typedef struct _ofaIRenderable                    ofaIRenderable;
typedef enum   _ofeIRenderableBreak               ofeIRenderableBreak;

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
	guint         ( *get_interface_version )    ( const ofaIRenderable *instance );

	/**
	 * begin_render:
	 * @instance: the #ofaIRenderable instance.
	 * @cr: the target #cairo_t context.
	 * @render_width: the maximum rendering width in points.
	 * @render_height: the maximum rendering height in points.
	 * @dataset: the set of records to be rendered.
	 *
	 * This method is called by the interface when about to begin the
	 * pagination.
	 *
	 * When #ofaRenderPage calls ofa_irenderable_begin_render(), the
	 * child class has already been asked for its dataset, which has
	 * been provided to this #ofaIRenderable interface.
	 *
	 * The implementation may take advantage of this method to compute
	 * and adjust its page layout accordingly.
	 */
	void          ( *begin_render )             ( ofaIRenderable *instance,
														cairo_t *cr,
														gdouble render_width,
														gdouble render_height,
														GList *dataset );

	/**
	 * render_page:
	 * @instance: the #ofaIRenderable instance.
	 * @cr: the target #cairo_t context.
	 * @page_num: the page number to be rendered, counted from zero.
	 *
	 * This method is called by the interface for rendering a page.
	 *
	 * If not implemented, the interface defaults to draw the page to
	 * the provided @cr context.
	 */
	void          ( *render_page )              ( ofaIRenderable *instance,
														cairo_t *cr,
														guint page_num );

	/**
	 * end_render:
	 * @instance: the #ofaIRenderable instance.
	 * @cr: the target #cairo_t context.
	 *
	 * This method is called by the interface after having rendered all
	 * pages.
	 *
	 * If not implemented, the interface defaults to do nothing.
	 */
	void          ( *end_render )               ( ofaIRenderable *instance,
														cairo_t *cr );

	/**
	 * draw_page_header_dossier:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Called when beginning to draw each page, this method let the
	 * implementation render specific informations about the dossier.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * If not implemented, and the get_dossier_label() method is, then
	 * the interface defaults to render the dossier's label, with dossier
	 * font and color properties.
	 *
	 * In this later case, the interface draws the dossier label left-
	 * aligned at the x=0,y=0 coordinates, and leaves the last_y ordonate
	 * just at the bottom of the text, without any more margin.
	 */
	void          ( *draw_page_header_dossier ) ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_dossier_label:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the label to be rendered for the dossier required as
	 * part of the default implementation of draw_page_header_dossier() method,
	 * as a newly allocated string will be g_free() by the interface.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_dossier().
	 */
	gchar *       ( *get_dossier_label )        ( const ofaIRenderable *instance );

	/**
	 * get_dossier_font:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the font family description to be used for the page dossier.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_dossier().
	 *
	 * Default is "Sans Bold Italic 11".
	 */
	const gchar * ( *get_dossier_font )         ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_dossier_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the dossier header.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_dossier().
	 *
	 * Default is dark red: (r,g,b) = (0.5, 0, 0) = #800000
	 */
	void          ( *get_dossier_color )        ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * draw_page_header_title:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Called after having drawn the dossier label, this method let the
	 * implementation render the title of the printings.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * If not implemented, and the get_title_label() method is, then
	 * the interface defaults to render the title's label, with title
	 * font and color properties.
	 *
	 * In this later case, the interface draws the title label
	 * horizontally centered at the ordonate last_y as left from
	 * draw_page_header_dossier(), and leaves this last_y ordonates just at
	 * the bottom of the text, without any more margin.
	 */
	void          ( *draw_page_header_title )   ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_title_label:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the label to be rendered for the title required as
	 * part of the default implementation of draw_page_header_title() method,
	 * as a newly allocated string will be g_free() by the interface.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_title().
	 */
	gchar *       ( *get_title_label )          ( const ofaIRenderable *instance );

	/**
	 * get_title_font:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the font family description to be used for the page title.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_title().
	 *
	 * Default is "Sans Bold 10".
	 */
	const gchar * ( *get_title_font )           ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_title_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the page title.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_title().
	 *
	 * Default is dark cyan: (r,g,b) = (0, 0.3765, 0.5) = #006080
	 */
	void          ( *get_title_color )          ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * draw_page_header_notes:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * Called after having drawn the title(s), this method let the
	 * implementation insert some text notes between the title(s)
	 * and the columns headers.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * The interface does not provide any default.
	 */
	void          ( *draw_page_header_notes )   ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * draw_page_header_columns:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * Called after having drawn the titles and the header notes (if
	 * any), this method let the implementation render the headers of
	 * the columns.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * If not implemented, and the draw_header_column_names() method is,
	 * then the interface defaults to render the headers in a painted
	 * background.
	 */
	void          ( *draw_page_header_columns ) ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * draw_header_column_names:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * Called as part of the default implementation of the
	 * draw_page_header_columns() method.
	 *
	 * On call, the last_y ordonate is set on top of the painted
	 * background, and the color and font of the header names are set.
	 * It is up to the implementation to draw the names at the right
	 * place, taking into account the margins on each side.
	 *
	 * As a recall, the horizontal internal margin is the page margin
	 * (by definition). The vertical internal margin is up to the
	 * implementation.
	 *
	 * The painted background will span until the last_y ordonate, as
	 * provided on return of this function.
	 *
	 * The interface provides a vertical space before and after the
	 * painted background.
	 */
	void          ( *draw_header_column_names ) ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_columns_font:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the font family description to be used for the columns
	 * headers.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_columns().
	 *
	 * Default is "Sans Bold 5".
	 */
	const gchar * ( *get_columns_font )         ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_columns_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the columns header names.
	 *
	 * This method is part of default implementation of
	 * draw_page_header_columns().
	 *
	 * Default is white: (r,g,b) = (1, 1, 1)
	 */
	void          ( *get_columns_color )        ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * draw_top_summary:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * Called after having drawn the columns headers, this method let
	 * the implementation have a top summary on each page.
	 *
	 * On call, the summary font and color are set.
	 *
	 * Note that from #ofaIRenderable point of view, this method has
	 * nothing to do with the groups management.
	 *
	 * Please also note that, while draw_top_summary() is called on
	 * top on each page, draw_last_summary() is only called on the
	 * last page.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * The interface does not provide any default.
	 */
	void          ( *draw_top_summary )         ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_summary_font:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the font family description to be used for the summaries
	 * of the page.
	 *
	 * Default is "Sans Bold 7".
	 */
	const gchar * ( *get_summary_font )         ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_summary_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the summaries of the page.
	 *
	 * Default is title color.
	 */
	void          ( *get_summary_color )        ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * is_new_group:
	 * @instance: the #ofaIRenderable instance.
	 * @prev: [allow-none]: the last rendered element;
	 *  may be %NULL when dealing with the first element.
	 * @line: [allow-none]: the current candidate to be rendered;
	 *  may be %NULL when dealing with the last element.
	 * @sep: [out]: the type of separation between the groups.
	 *
	 * Returns: If implemented, this method should return %TRUE if the
	 * @current element does not belong to the same group than @prev.
	 *
	 * When %TRUE (the implementation has detected a group break between
	 * @prev and @line), then the interface will take care of drawing
	 * group header and footer.
	 *
	 * Defaults to FALSE (no group).
	 */
	gboolean      ( *is_new_group )             ( const ofaIRenderable *instance,
														GList *prev,
														GList *line,
														ofeIRenderableBreak *sep );

	/**
	 * draw_group_header:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 * @line: the first element of the group.
	 *
	 * If implemented, this method must draw the group header.
	 *
	 * The group header is drawn on top on each group of lines.
	 * This is also the good time to initialize the data specific
	 * to this new group.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used in order
	 * for the interface to auto-detect its height.
	 */
	void          ( *draw_group_header )        ( ofaIRenderable *instance,
														guint page_num,
														GList *line );

	/**
	 * get_group_font:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the font family description to be used for the groups
	 * headers and footers.
	 *
	 * Default is "Sans Bold 6".
	 */
	const gchar * ( *get_group_font )           ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_group_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the group header and footer.
	 *
	 * Default is summary color
	 */
	void          ( *get_group_color )          ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * draw_top_report:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 * @prev: [allow-none]: the last rendered element;
	 *  may be %NULL when dealing with the first element.
	 * @line: the next element to be rendered.
	 *
	 * If implemented, this method must draw the top report for the
	 * new page.
	 *
	 * The top report is drawn on the top of the page, after the top
	 * summary above, as a recall of the previous page.
	 * It may be associated with a bottom report on the end of the page.
	 *
	 * On call, the report font and color are set.
	 *
	 * Note that from #ofaIRenderable point of view, this method has
	 * nothing to do with the groups management.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used.
	 */
	void          ( *draw_top_report )          ( ofaIRenderable *instance,
														guint page_num,
														GList *prev,
														GList *line );

	/**
	 * get_report_font:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the font family description to be used for the top and
	 * bottom page reports.
	 *
	 * Default is "Sans 6".
	 */
	const gchar * ( *get_report_font )          ( const ofaIRenderable *instance,
														guint page_num );

	/**
	 * get_report_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the top and bottom reports of the
	 * page.
	 *
	 * Default is summary color.
	 */
	void          ( *get_report_color )         ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * draw_line:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the page number, counted from zero.
	 * @line_num: the line number in this @page_num, counted from zero.
	 * @line: the element to be printed on this line.
	 *
	 * If implemented, this method must draw the @current line on the
	 * current context.
	 *
	 * On call, the body font and color are set.
	 *
	 * The interface takes itself care of upating the 'last_y' ordonate.
	 */
	void          ( *draw_line )                ( ofaIRenderable *instance,
														guint page_num,
														guint line_num,
														GList *line );

	/**
	 * draw_bottom_report:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, this method must draw the bottom report for
	 * the page.
	 *
	 * The bottom report is drawn on the bottom of the page, as a recall
	 * of the current page. It may be associated with a top report on
	 * the beginning of the pages.
	 *
	 * On call, the report font (associated with summary color) are set.
	 *
	 * Note that from #ofaIRenderable point of view, this method has
	 * nothing to do with the groups management.
	 *
	 * The application must take care itself of updating the 'last_y'
	 * ordonate according to the vertical space it has used.
	 */
	void          ( *draw_bottom_report )       ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * draw_group_footer:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 * @line: the line just rendered.
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
	void          ( *draw_group_footer )        ( ofaIRenderable *instance,
														guint page_num,
														GList *line );

	/**
	 * draw_last_summary:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * Called at the end of the last page, after having rendered the
	 * last record, and maybe the groups subtotals.
	 *
	 * On call, the summary font and color are set.
	 *
	 * Please also note that, while draw_last_summary() is only called
	 * on the end of the last page, draw_top_summary() is called on top
	 * on each page.
	 *
	 * If provided, the implementation must update the last_y ordonate
	 * accordingly.
	 *
	 * The interface does not provide any default.
	 */
	void          ( *draw_last_summary )        ( ofaIRenderable *instance,
														guint page_num );

	/**
	 * draw_page_footer:
	 * @instance: the #ofaIRenderable instance.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, this method must draw the page footer.
	 *
	 * The page footer is expected to always have the same height in
	 * all pages.
	 *
	 * The default is to have a gray separation line, and
	 * a footer line which includes the application name and version,
	 * the rendering timestamp, the page number.
	 */
	void          ( *draw_page_footer )         ( ofaIRenderable *instance,
															gint page_num );

	/**
	 * get_body_font:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the font family description to be used for the standard line.
	 *
	 * Default is "Sans 6".
	 */
	const gchar * ( *get_body_font )            ( const ofaIRenderable *instance );

	/**
	 * get_body_vspace_rate:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the vspace rate to be applied to body line.
	 *
	 * Default is 0.25.
	 */
	gdouble       ( *get_body_vspace_rate )     ( const ofaIRenderable *instance );

	/**
	 * get_body_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the body line.
	 *
	 * Default is black: (r,g,b) = (0, 0, 0).
	 */
	void          ( *get_body_color )           ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * get_footer_font:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Returns: the font family description to be used for the page footer.
	 *
	 * Default is "Sans Italic 5".
	 */
	const gchar * ( *get_footer_font )          ( const ofaIRenderable *instance );

	/**
	 * get_footer_color:
	 * @instance: the #ofaIRenderable instance.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
	 *
	 * Set the @r, @g, @b color of the page footer.
	 *
	 * Default is gray: (r,g,b) = (0.6, 0.6, 0.6) = #999999.
	 */
	void          ( *get_footer_color )         ( const ofaIRenderable *instance,
														gdouble *r,
														gdouble *g,
														gdouble *b );

	/**
	 * clear_runtime_data:
	 * @instance: the #ofaIRenderable instance.
	 *
	 * Called a first time between begin_render() and the pagination,
	 * and a second time before the rendering.
	 *
	 * Let the implementation clear its runtime data.
	 */
	void          ( *clear_runtime_data )       ( ofaIRenderable *instance );
}
	ofaIRenderableInterface;

/**
 * ofeIRenderableBreak:
 *
 * The type of separation between two groups.
 */
enum _ofeIRenderableBreak {
	IRENDERABLE_BREAK_NONE = 0,
	IRENDERABLE_BREAK_NEW_PAGE,
	IRENDERABLE_BREAK_BLANK_LINE,
	IRENDERABLE_BREAK_SEP_LINE
};

/**
 * ofeIRenderableMode:
 *
 * What to do with the provided lines ?
 */
typedef enum {
	IRENDERABLE_MODE_NORMAL = 1,
	IRENDERABLE_MODE_NOPRINT
}
	ofeIRenderableMode;

GType              ofa_irenderable_get_type                  ( void );

guint              ofa_irenderable_get_interface_last_version( const ofaIRenderable *instance );

/* three main entry points for this interface
 */
gint               ofa_irenderable_begin_render              ( ofaIRenderable *instance,
																	cairo_t *cr,
																	gdouble render_width,
																	gdouble render_height,
																	GList *dataset );

void               ofa_irenderable_render_page               ( ofaIRenderable *instance,
																	cairo_t *cr,
																	guint page_number );

void               ofa_irenderable_end_render                ( ofaIRenderable *instance,
																	cairo_t *cr );

/* runtime
 */
gboolean           ofa_irenderable_is_paginating             ( ofaIRenderable *instance );

ofeIRenderableMode ofa_irenderable_get_line_mode             ( const ofaIRenderable *instance );

void               ofa_irenderable_set_line_mode             ( ofaIRenderable *instance,
																	ofeIRenderableMode mode );

void               ofa_irenderable_set_last_y                ( ofaIRenderable *instance,
																	gdouble y );

gdouble            ofa_irenderable_get_last_y                ( ofaIRenderable *instance );

void               ofa_irenderable_set_last_y                ( ofaIRenderable *instance,
																	gdouble y );

gdouble            ofa_irenderable_get_max_y                 ( ofaIRenderable *instance );

gdouble            ofa_irenderable_get_text_height           ( ofaIRenderable *instance );

gdouble            ofa_irenderable_get_text_width            ( ofaIRenderable *instance,
																	const gchar *text );

gdouble            ofa_irenderable_get_line_height           ( ofaIRenderable *instance );

void               ofa_irenderable_set_font                  ( ofaIRenderable *instance,
																	const gchar *font_str );

void               ofa_irenderable_set_color                 ( ofaIRenderable *instance,
																	gdouble r,
																	gdouble g,
																	gdouble b );

gdouble            ofa_irenderable_set_text                  ( ofaIRenderable *instance,
																	gdouble x,
																	gdouble y,
																	const gchar *text,
																	PangoAlignment align );

gdouble            ofa_irenderable_ellipsize_text            ( ofaIRenderable *instance,
																	gdouble x,
																	gdouble y,
																	const gchar *text,
																	gdouble max_size );

gdouble            ofa_irenderable_set_wrapped_text          ( ofaIRenderable *instance,
																	gdouble x,
																	gdouble y,
																	gdouble width,
																	const gchar *text,
																	PangoAlignment align );

void               ofa_irenderable_draw_rubber               ( ofaIRenderable *instance,
																	gdouble top,
																	gdouble height );

void               ofa_irenderable_draw_rect                 ( ofaIRenderable *instance,
																	gdouble x,
																	gdouble y,
																	gdouble width,
																	gdouble height );

void               ofa_irenderable_draw_no_data              ( ofaIRenderable *instance );

gdouble            ofa_irenderable_get_header_columns_height ( ofaIRenderable *instance );

gdouble            ofa_irenderable_get_last_summary_height   ( ofaIRenderable *instance );

cairo_t           *ofa_irenderable_get_context               ( ofaIRenderable *instance );

void               ofa_irenderable_set_temp_context          ( ofaIRenderable *instance );

void               ofa_irenderable_restore_context           ( ofaIRenderable *instance );

/* helpers and default values
 */
gdouble            ofa_irenderable_get_page_margin           ( const ofaIRenderable *instance );

gdouble            ofa_irenderable_get_columns_spacing       ( const ofaIRenderable *instance );

const gchar       *ofa_irenderable_get_dossier_font          ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_dossier_color         ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_title_font            ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_title_color           ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_columns_font          ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_columns_color         ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_summary_font          ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_summary_color         ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_group_font            ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_group_color           ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_report_font           ( const ofaIRenderable *instance,
																	guint page_num );

void               ofa_irenderable_get_report_color          ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

const gchar       *ofa_irenderable_get_body_font             ( const ofaIRenderable *instance );

void               ofa_irenderable_get_body_color            ( const ofaIRenderable *instance,
																	gdouble *r,
																	gdouble *g,
																	gdouble *b );

gdouble            ofa_irenderable_get_body_vspace_rate      ( const ofaIRenderable *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IRENDERABLE_H__ */
