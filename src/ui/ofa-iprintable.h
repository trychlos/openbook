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

#ifndef __OFA_IPRINTABLE_H__
#define __OFA_IPRINTABLE_H__

/**
 * SECTION: iprintable
 * @title: ofaIPrintable
 * @short_description: The IPrintable Interface
 * @include: ui/ofa-iprintable.h
 *
 * The #ofaIPrintable interface lets its users benefit of the
 * standardized printing system of Openbook.
 */

#include "core/my-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_IPRINTABLE                      ( ofa_iprintable_get_type())
#define OFA_IPRINTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPRINTABLE, ofaIPrintable ))
#define OFA_IS_IPRINTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPRINTABLE ))
#define OFA_IPRINTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPRINTABLE, ofaIPrintableInterface ))

typedef struct _ofaIPrintabe                     ofaIPrintable;

/**
 * ofaIPrintableInterface:
 *
 * This defines the interface that an #ofaIPrintable should implement.
 *
 * A #IPrintable summary is built on top on a standard
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
 * The #IPrintable interface does not actually send the report to a
 * printer. Instead, it exports it to a named PDF file.
 *
 * To use it, the client class should be derived from myDialog, and
 * have a #GtkNotebook as one of the topmost children.
 * The #ofaIPrintable will add a tab to this notebook, letting the
 * user choose an exported filename.
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
	 * @instance: the #ofaIPrintable provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIPrintable interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )   ( const ofaIPrintable *instance );

	/**
	 * on_print_operation_new:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the newly created #GtkPrintOperation operation.
	 *
	 * This method is called by the interface after having allocated a
	 * new #GtkPrintOperation, and having set on it the default page
	 * setup.
	 */
	void     ( *on_print_operation_new )  ( const ofaIPrintable *instance,
												GtkPrintOperation *operation );

	/**
	 * get_dataset:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * This method is called by the interface in order to get the
	 * #GList list of elements to be printed.
	 *
	 * It is an error for the implementation to not provide this method,
	 * even if it returns an empty dataset.
	 */
	GList *  ( *get_dataset )             ( const ofaIPrintable *instance );

	/**
	 * on_begin_print:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface in response to the
	 * "begin-print" message, before the beginning of the pagination
	 * process.
	 */
	void     ( *on_begin_print )          ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * reset_runtime:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * This method is called by the interface first when beginning the
	 * pagination process, and then when about to begin the actual data
	 * generation.
	 *
	 * The interface code take advantage of this virtual to reset its
	 * own runtime data.
	 */
	void     ( *reset_runtime )           ( ofaIPrintable *instance );

	/**
	 * on_begin_paginate:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface when beginning the
	 * pagination process.
	 */
	void     ( *on_begin_paginate )       ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * on_end_paginate:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface at the end of the
	 * pagination process, after the pages count has been computed.
	 */
	void     ( *on_end_paginate )         ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_page_header:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 * @page_num: the page number, counted from zero.
	 *
	 * The page header is drawn on top of each page.
	 *
	 * If implemented, this method must draw the page header on the
	 * provided @context. In this case, it is the responsability of the
	 * application to updating the 'last_y' ordonate at the end of the
	 * page header drawing.
	 *
	 * The interface code provide a suitable default.
	 * See infra get_page_header_title() and get_page_header_subtitle()
	 * virtuals.
	 */
	void     ( *draw_page_header )        ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context,
												gint page_num );

	/**
	 * get_page_header_title:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * Returns: the title of the report.
	 *
	 * This title is drawn as part of the default page header provided
	 * by the interface code.
	 *
	 * The interface code takes care itself of :
	 * - g_free-ing() the returned string
	 * - updating the 'last_y' ordonate at the end of the drawing.
	 */
	gchar *  ( *get_page_header_title )   ( const ofaIPrintable *instance );

	/**
	 * get_page_header_subtitle:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * Returns: the subtitle of the report.
	 *
	 * This subtitle is drawn as part of the default page header
	 * provided by the interface code.
	 *
	 * The interface code takes care itself of :
	 * - g_free-ing() the returned string
	 * - updating the 'last_y' ordonate at the end of the drawing.
	 */
	gchar *  ( *get_page_header_subtitle )( const ofaIPrintable *instance );

	/**
	 * draw_page_header_columns:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: the #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 *
	 * If implemented, should write the column headers.
	 *
	 * This method is called as part of the default implementation of
	 * the "draw_header" virtual. When called, the cairo context has
	 * already been filled with a colored rectangle, and the layout is
	 * set to suitable text font color and size.
	 *
	 * The application must take care of updating the 'last_y' ordonate
	 * according to the count of lines it has been printed, and of the
	 * vertical space it may have added between these rows.
	 */
	void     ( *draw_page_header_columns )( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_top_summary:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
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
	void     ( *draw_top_summary )        ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * is_new_group:
	 * @instance: the #ofaIPrintable provider.
	 * @current: the element currently candidate for printing (not nul).
	 * @prev: the last printed element,
	 *  may be %NULL when dealing with the first element.
	 *
	 * Returns: If implemented, this method should return %TRUE if the
	 * @current element does not belong to the same group than @prev.
	 *
	 * Defaults to FALSE (no group).
	 */
	gboolean ( *is_new_group )            ( const ofaIPrintable *instance,
																GList *current,
																GList *prev );

	/**
	 * draw_group_header:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
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
	void     ( *draw_group_header )       ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context,
												GList *current );

	/**
	 * draw_group_top_report:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 * phase.
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
	void     ( *draw_group_top_report )   ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_line:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 * @current: the element to be printed on this line.
	 *
	 * If implemented, this method must draw the line on the provided
	 * @context.
	 *
	 * The interface code takes care itself of upating the 'last_y'
	 * coordinate of the height of one standard line.
	 */
	void     ( *draw_line )               ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context,
												GList *current );

	/**
	 * draw_group_bottom_report:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
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
	void     ( *draw_group_bottom_report )( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_group_footer:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
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
	void     ( *draw_group_footer )       ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_bottom_summary:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 *
	 * If implemented, this method must draw the bottom summary on the
	 * provided @context.
	 *
	 * The bottom summary is drawn of the last page, at the end of the
	 * report.
	 */
	void     ( *draw_bottom_summary )     ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * draw_page_footer:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 * @page_num: the current page number, counted from zero.
	 *
	 * If implemented, this method must draw the page footer on the
	 * provided @context.
	 */
	void     ( *draw_page_footer )        ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context,
												gint page_num );

	/**
	 * on_end_print:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface on end printing.
	 */
	void     ( *on_end_print )            ( ofaIPrintable *instance,
												GtkPrintOperation *operation,
												GtkPrintContext *context );

	/**
	 * get_success_msg:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * Returns: if implemented, this method must return a message to be
	 * displayed after a successful print.
	 */
	const gchar *      ( *get_success_msg )               ( const ofaIPrintable *instance );

	/**
	 * free_dataset:
	 * @dataset: the dataset provided by the get_dataset() virtual.
	 *
	 * Frees the dataset.
	 *
	 * The interface code doesn't provide any suitable default, not
	 * even g_list_free(). It is up to the implementation to free
	 * the dataset it has provided at the beginning of the run.
	 */
	void               ( *free_dataset )                  ( GList *dataset );
}
	ofaIPrintableInterface;

GType        ofa_iprintable_get_type             ( void );

guint        ofa_iprintable_get_interface_last_version
                                                 ( const ofaIPrintable *instance );

void         ofa_iprintable_init                 ( ofaIPrintable *instance );

void         ofa_iprintable_set_paper_size       ( ofaIPrintable *instance, const gchar *size );

void         ofa_iprintable_set_paper_orientation( ofaIPrintable *instance, GtkPageOrientation orientation );

gboolean     ofa_iprintable_print_to_pdf         ( ofaIPrintable *instance, const gchar *filename );

void         ofa_iprintable_set_group_on_new_page( const ofaIPrintable *instance,
                                                		gboolean new_page );

gdouble      ofa_iprintable_get_page_header_columns_height
                                                 ( const ofaIPrintable *instance );

gdouble      ofa_iprintable_get_page_margin      ( const ofaIPrintable *instance );

void         ofa_iprintable_set_font             ( ofaIPrintable *instance,
														const gchar *name, gint font_size );

gint         ofa_iprintable_get_default_font_size( const ofaIPrintable *instance );

void         ofa_iprintable_set_default_font_size( ofaIPrintable *instance, gint font_size );

gint         ofa_iprintable_get_current_font_size( const ofaIPrintable *instance );

gdouble      ofa_iprintable_get_default_line_vspace
                                                 ( const ofaIPrintable *instance );

gdouble      ofa_iprintable_get_current_line_vspace
                                                 ( const ofaIPrintable *instance );

gdouble      ofa_iprintable_get_current_line_height
                                                 ( const ofaIPrintable *instance );

void         ofa_iprintable_set_color            ( const ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble r, gdouble g, gdouble b );

gdouble      ofa_iprintable_get_last_y           ( const ofaIPrintable *instance );

void         ofa_iprintable_set_last_y           ( ofaIPrintable *instance,
														gdouble y );

gdouble      ofa_iprintable_get_max_y            ( const ofaIPrintable *instance );

gint         ofa_iprintable_get_pages_count      ( const ofaIPrintable *instance );

void         ofa_iprintable_draw_rubber          ( ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble top, gdouble height );

void         ofa_iprintable_draw_rect            ( ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble x, gdouble y,
														gdouble width, gdouble height );

void         ofa_iprintable_draw_no_data         ( ofaIPrintable *instance,
														GtkPrintContext *context );

void         ofa_iprintable_set_text             ( const ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble x, gdouble y,
														const gchar *text, PangoAlignment align );

void         ofa_iprintable_ellipsize_text       ( const ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble x, gdouble y,
														const gchar *text, gdouble max_size );

void         ofa_iprintable_set_wrapped_text     ( const ofaIPrintable *instance,
														GtkPrintContext *context,
														gdouble x, gdouble y,
														gdouble width,
														const gchar *text, PangoAlignment align );

G_END_DECLS

#endif /* __OFA_IPRINTABLE_H__ */
