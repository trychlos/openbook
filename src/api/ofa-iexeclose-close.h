/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IEXECLOSE_CLOSE_H__
#define __OPENBOOK_API_OFA_IEXECLOSE_CLOSE_H__

/**
 * SECTION: iexeclose_close
 * @title: ofaIExeCloseClose
 * @short_description: The Exercice Closing Interface
 * @include: openbook/ofa-iexeclose-close.h
 *
 * The #ofaIExeClosexxx interfaces serie lets a plugin balance, close
 * and archive its data on exercice closing.
 *
 * In particular, this #ofaIExeCloseClose lets a plugin insert its
 * tasks either in the closing exercice, or in the opening exercice.
 *
 * When the plugin wants insert some tasks either before closing the
 * exercice N, or after opening the exercice N+1, it must:
 * - provides a label that the assistant will take care of inserting
 *   in the ad-hoc page of the assistant,
 * - does its tasks, updating the provided GtkBox at its convenience.
 *
 * When closing the exercice N, the tasks are executed before the
 * program does anything (validating lines, balancing accounts, closing
 * ledgers), but possibly after other plugins.
 *
 * When opening the exercice N+1, the tasks are executed after the
 * program has archived all its data, and set the future entries in
 * the new exercice.
 *
 * Please note that the order in which plugins are called in not
 * guaranteed to be consistent between several executions of the
 * program.
 */

#include <gtk/gtk.h>

#include "api/ofa-hub.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXECLOSE_CLOSE                      ( ofa_iexeclose_close_get_type())
#define OFA_IEXECLOSE_CLOSE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXECLOSE_CLOSE, ofaIExeCloseClose ))
#define OFA_IS_IEXECLOSE_CLOSE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXECLOSE_CLOSE ))
#define OFA_IEXECLOSE_CLOSE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXECLOSE_CLOSE, ofaIExeCloseCloseInterface ))

typedef struct _ofaIExeCloseClose                     ofaIExeCloseClose;

/**
 * ofaIExeCloseCloseInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @add_row: [should]: insert a row on the page when closing the exercice.
 * @do_task: [should]: do the task.
 *
 * This defines the interface that an #ofaIExeCloseClose may/should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIExeCloseClose instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIExeCloseClose interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint    ( *get_interface_version )( const ofaIExeCloseClose *instance );

	/**
	 * add_row:
	 * @instance: the #ofaIExeCloseClose instance.
	 * @rowtype: whether we insert on closing exercice N, or on opening
	 *  exercice N+1.
	 *
	 * Ask @instance the text to be inserted as the row label if it
	 * wants do some tasks at the moment specified by @rowtype.
	 *
	 * If the plugin returns a %NULL or empty string, then it will not
	 * be called later to do any task.
	 *
	 * Returns: a string which will be #g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *  ( *add_row )              ( ofaIExeCloseClose *instance,
											guint rowtype );

	/**
	 * do_task:
	 * @instance: the #ofaIExeCloseClose instance.
	 * @rowtype: whether we insert on closing exercice N, or on opening
	 *  exercice N+1.
	 * @box: a #GtkBox available to the plugin.
	 * @hub: the current #ofaHub object.
	 *
	 * Returns: %TRUE if the plugin tasks are successful, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *do_task )              ( ofaIExeCloseClose *instance,
											guint rowtype,
											GtkWidget *box,
											ofaHub *hub );
}
	ofaIExeCloseCloseInterface;

/**
 * Whether this row concerns the closing exercice N, or the opening
 * exercice N+1.
 */
enum {
	EXECLOSE_CLOSING = 1,
	EXECLOSE_OPENING
};

GType    ofa_iexeclose_close_get_type                  ( void );

guint    ofa_iexeclose_close_get_interface_last_version( void );

guint    ofa_iexeclose_close_get_interface_version     ( const ofaIExeCloseClose *instance );

gchar   *ofa_iexeclose_close_add_row                   ( ofaIExeCloseClose *instance,
															guint rowtype );

gboolean ofa_iexeclose_close_do_task                   ( ofaIExeCloseClose *instance,
															guint rowtype,
															GtkWidget *box,
															ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXECLOSE_CLOSE_H__ */
