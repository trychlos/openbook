/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IPRINTABLE_H__
#define __OPENBOOK_API_OFA_IPRINTABLE_H__

/**
 * SECTION: iprintable
 * @title: ofaIPrintable
 * @short_description: The IPrintable Interface
 * @include: api/ofa-iprintable.h
 *
 * The #ofaIPrintable interface lets its users benefit of the
 * standardized printing system of Openbook.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_IPRINTABLE                      ( ofa_iprintable_get_type())
#define OFA_IPRINTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IPRINTABLE, ofaIPrintable ))
#define OFA_IS_IPRINTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IPRINTABLE ))
#define OFA_IPRINTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IPRINTABLE, ofaIPrintableInterface ))

typedef struct _ofaIPrintable                    ofaIPrintable;

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
	guint              ( *get_interface_version )( const ofaIPrintable *instance );

	/**
	 * get_paper_size:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * Returns: the paper name.
	 */
	const gchar *      ( *get_paper_name )       ( ofaIPrintable *instance );

	/**
	 * get_page_orientation:
	 * @instance: the #ofaIPrintable provider.
	 *
	 * Returns: the page orientation.
	 */
	GtkPageOrientation ( *get_page_orientation ) ( ofaIPrintable *instance );

	/**
	 * get_print_settings:
	 * @instance: the #ofaIPrintable provider.
	 * @keyfile: [out]:
	 * @group_name: [out]:
	 *
	 * The implementation should set the GKeyFile and the group name to
	 * load/save the print settings.
	 */
	void               ( *get_print_settings )   ( ofaIPrintable *instance,
														GKeyFile **keyfile,
														gchar **group_name );

	/**
	 * begin_print:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface in response to the
	 * "begin-print" message, before the beginning of the pagination
	 * process.
	 */
	void               ( *begin_print )          ( ofaIPrintable *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );

	/**
	 * paginate:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface during the
	 * pagination process.
	 */
	gboolean           ( *paginate )             ( ofaIPrintable *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );

	/**
	 * draw_page:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: a #GtkPrintOperation operation, %NULL while
	 *  pagination phase.
	 * @context: a #GtkPrintContext context, %NULL while pagination
	 *  phase.
	 * @page_num: the page number, counted from zero.
	 */
	void               ( *draw_page )            ( ofaIPrintable *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context,
														gint page_num );

	/**
	 * end_print:
	 * @instance: the #ofaIPrintable provider.
	 * @operation: the #GtkPrintOperation operation.
	 * @context: the #GtkPrintContext context.
	 *
	 * This method is called by the interface on end printing.
	 */
	void               ( *end_print )            ( ofaIPrintable *instance,
														GtkPrintOperation *operation,
														GtkPrintContext *context );
}
	ofaIPrintableInterface;

GType    ofa_iprintable_get_type     ( void );

guint    ofa_iprintable_get_interface_last_version
                                      ( const ofaIPrintable *instance );

gboolean ofa_iprintable_print        ( ofaIPrintable *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IPRINTABLE_H__ */
