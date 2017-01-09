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

#ifndef __MY_API_MY_IPROGRESS_H__
#define __MY_API_MY_IPROGRESS_H__

/**
 * SECTION: iprogress
 * @title: myIProgress
 * @short_description: The myIProgress Interface
 * @include: my/my-iprogress.h
 *
 * The #myIProgress may be implemented by any widget which would
 * wish display the progress of an external work.
 *
 * The caller (aka the worker) calls this interface with an unique
 * identifier, and may ask to set start and end labels, and pulse a
 * progress bar.
 *
 * The myIProgress implementation may also provide a text view for
 * possible error messages.
 *
 * Note that this #myIProgress interface is absolutely transparent,
 * i.e. does not do any work before of after the implementation. All
 * the behavior is fully dependant of the implementation.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_IPROGRESS                      ( my_iprogress_get_type())
#define MY_IPROGRESS( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IPROGRESS, myIProgress ))
#define MY_IS_IPROGRESS( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IPROGRESS ))
#define MY_IPROGRESS_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IPROGRESS, myIProgressInterface ))

typedef struct _myIProgress                    myIProgress;

/**
 * myIProgressInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @start_work: [should]: initialize the work.
 * @start_progress: [should]: initialize one progress instance.
 * @pulse: [should]: increment a progress bar.
 * @set_row: [should]: increment a grid row.
 * @set_ok: [should]: set OK/not OK.
 * @set_text: [should]: set a message.
 *
 * This defines the interface that an #myIProgress must/should/may implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * start_work:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the start
	 *  of the work.
	 *
	 * Display the @widget.
	 *
	 * Since: version 1.
	 */
	void     ( *start_work )           ( myIProgress *instance,
												const void *worker,
												GtkWidget *widget );

	/**
	 * start_progress:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the start
	 *  of the progress.
	 * @with_bar: whether to create a progress bar on the right of the
	 *  @widget.
	 *
	 * Display the @widget.
	 * Maybe create a progress bar.
	 *
	 * Since: version 1.
	 */
	void     ( *start_progress )       ( myIProgress *instance,
												const void *worker,
												GtkWidget *widget,
												gboolean with_bar );

	/**
	 * pulse:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @count: the current counter.
	 * @total: the expected total.
	 *
	 * Increments the progress bar.
	 *
	 * Since: version 1.
	 */
	void     ( *pulse )                ( myIProgress *instance,
												const void *worker,
												gulong count,
												gulong total );

	/**
	 * set_row:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed on the latest row.
	 *
	 * Display the @widget.
	 *
	 * Since: version 1.
	 */
	void     ( *set_row )              ( myIProgress *instance,
												const void *worker,
												GtkWidget *widget );

	/**
	 * set_ok:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the end
	 *  of the progress.
	 * @errs_count: the count of errors.
	 *
	 * Display the @widget.
	 * Display the result as 'OK' or the count of errors.
	 *
	 * Since: version 1.
	 */
	void     ( *set_ok )               ( myIProgress *instance,
												const void *worker,
												GtkWidget *widget,
												gulong errs_count );

	/**
	 * set_text:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @text: [allow-none]: a text to be added in a text view.
	 *
	 * Display the @text.
	 *
	 * Since: version 1.
	 */
	void     ( *set_text )             ( myIProgress *instance,
												const void *worker,
												const gchar *text );
}
	myIProgressInterface;

/*
 * Interface-wide
 */
GType           my_iprogress_get_type                  ( void );

guint           my_iprogress_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint           my_iprogress_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void            my_iprogress_start_work                ( myIProgress *instance,
																const void *worker,
																GtkWidget *widget );

void            my_iprogress_start_progress            ( myIProgress *instance,
																const void *worker,
																GtkWidget *widget,
																gboolean with_bar );

void            my_iprogress_pulse                     ( myIProgress *instance,
																const void *worker,
																gulong count,
																gulong total );

void            my_iprogress_set_row                   ( myIProgress *instance,
																const void *worker,
																GtkWidget *widget );

void            my_iprogress_set_ok                    ( myIProgress *instance,
																const void *worker,
																GtkWidget *widget,
																gulong errs_count );

void            my_iprogress_set_text                  ( myIProgress *instance,
																const void *worker,
																const gchar *text );

G_END_DECLS

#endif /* __MY_API_MY_IPROGRESS_H__ */
