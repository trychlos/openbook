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

#ifndef __OFA_IPRINTABLE2_H__
#define __OFA_IPRINTABLE2_H__

/**
 * SECTION: iprintable2
 * @title: ofaIPrintable2
 * @short_description: The IPrintable2 Interface
 * @include: ui/ofa-iprintable2.h
 *
 * The #ofaIPrintable2 interface lets its users benefit of the
 * standardized printing system of Openbook.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_IPRINTABLE2                      ( ofa_iprintable2_get_type())
#define OFA_IPRINTABLE2( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPRINTABLE2, ofaIPrintable2 ))
#define OFA_IS_IPRINTABLE2( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPRINTABLE2 ))
#define OFA_IPRINTABLE2_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPRINTABLE2, ofaIPrintable2Interface ))

typedef struct _ofaIPrintable2                    ofaIPrintable2;

/**
 * ofaIPrintable2Interface:
 *
 * This defines the interface that an #ofaIPrintable2 should implement.
 *
 * A #IPrintable2 summary is built on top on a standard
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
 * The #IPrintable2 interface does not actually send the report to a
 * printer. Instead, it exports it to a named PDF file.
 *
 * To use it, the client class should be derived from myDialog, and
 * have a #GtkNotebook as one of the topmost children.
 * The #ofaIPrintable2 will add a tab to this notebook, letting the
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
	 * @instance: the #ofaIPrintable2 provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIPrintable2 interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint              ( *get_interface_version )( const ofaIPrintable2 *instance );

	/**
	 * get_paper_size:
	 * @instance: the #ofaIPrintable2 provider.
	 *
	 * Returns: the paper name.
	 */
	const gchar *      ( *get_paper_name )       ( ofaIPrintable2 *instance );

	/**
	 * get_page_orientation:
	 * @instance: the #ofaIPrintable2 provider.
	 *
	 * Returns: the page orientation.
	 */
	GtkPageOrientation ( *get_page_orientation ) ( ofaIPrintable2 *instance );

	/**
	 * begin_print:
	 * @instance: the #ofaIPrintable2 provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface in response to the
	 * "begin-print" message, before the beginning of the pagination
	 * process.
	 */
	void               ( *begin_print )          ( ofaIPrintable2 *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );

	/**
	 * paginate:
	 * @instance: the #ofaIPrintable2 provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface during the
	 * pagination process.
	 */
	gboolean           ( *paginate )             ( ofaIPrintable2 *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );

	/**
	 * draw_page:
	 * @instance: the #ofaIPrintable2 provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 * @page_num: the page number, counted from zero.
	 */
	void               ( *draw_page )            ( ofaIPrintable2 *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context,
														gint page_num );

	/**
	 * end_print:
	 * @instance: the #ofaIPrintable2 provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface on end printing.
	 */
	void               ( *end_print )            ( ofaIPrintable2 *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );
}
	ofaIPrintable2Interface;

GType    ofa_iprintable2_get_type    ( void );

guint    ofa_iprintable2_get_interface_last_version
                                     ( const ofaIPrintable2 *instance );

gboolean ofa_iprintable2_preview     ( ofaIPrintable2 *instance );

gboolean ofa_iprintable2_print       ( ofaIPrintable2 *instance );

G_END_DECLS

#endif /* __OFA_IPRINTABLE2_H__ */
