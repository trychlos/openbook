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
 *
 * This defines the interface that an #myIProgress must/should/may implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: an #myIProgress instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the myIProgress interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( const myIProgress *instance );

	/**
	 * work_start:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the start
	 *  of the work.
	 *
	 * Display the @widget.
	 *
	 * Since: version 1.
	 */
	void     ( *work_start )           ( myIProgress *instance,
												void *worker,
												GtkWidget *widget );

	/**
	 * work_end:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the end
	 *  of the work.
	 *
	 * Display the @widget.
	 *
	 * Since: version 1.
	 */
	void     ( *work_end )             ( myIProgress *instance,
												void *worker,
												GtkWidget *widget );

	/**
	 * progress_start:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the start
	 *  of the progress.
	 *
	 * Display the @widget.
	 *
	 * It is expected that the implementation displays a progress bar
	 * on the right of the @widget.
	 *
	 * Since: version 1.
	 */
	void     ( *progress_start )       ( myIProgress *instance,
												void *worker,
												GtkWidget *widget );

	/**
	 * progress_pulse:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @progress: the progress, from 0 to 1.
	 * @text: [allow-none]: a text to be set with the progress.
	 *
	 * Increments the progress bar.
	 *
	 * Since: version 1.
	 */
	void     ( *progress_pulse )       ( myIProgress *instance,
												void *worker,
												gdouble progress,
												const gchar *text );

	/**
	 * progress_end:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @widget: [allow-none]: a widget to be displayed to mark the end
	 *  of the progress.
	 * @errs_count: the count of errors.
	 *
	 * Display the @widget.
	 *
	 * It is expected that the implementation displays a "OK" label
	 * on the right of the progress bar.
	 *
	 * Since: version 1.
	 */
	void     ( *progress_end )         ( myIProgress *instance,
												void *worker,
												GtkWidget *widget,
												gulong errs_count );

	/**
	 * text:
	 * @instance: the #myIProgress instance.
	 * @worker: any worker.
	 * @text: [allow-none]: a text to be added in a text view.
	 *
	 * Display the @text.
	 *
	 * Since: version 1.
	 */
	void     ( *text )                 ( myIProgress *instance,
												void *worker,
												const gchar *text );
}
	myIProgressInterface;

GType           my_iprogress_get_type                  ( void );

guint           my_iprogress_get_interface_last_version( void );

guint           my_iprogress_get_interface_version     ( const myIProgress *instance );

void            my_iprogress_work_start                ( myIProgress *instance,
																void *worker,
																GtkWidget *widget );

void            my_iprogress_work_end                  ( myIProgress *instance,
																void *worker,
																GtkWidget *widget );

void            my_iprogress_progress_start            ( myIProgress *instance,
																void *worker,
																GtkWidget *widget );

void            my_iprogress_progress_pulse            ( myIProgress *instance,
																void *worker,
																gdouble progress,
																const gchar *text );

void            my_iprogress_progress_end              ( myIProgress *instance,
																void *worker,
																GtkWidget *widget,
																gulong errs_count );

void            my_iprogress_text                      ( myIProgress *instance,
																void *worker,
																const gchar *text );

G_END_DECLS

#endif /* __MY_API_MY_IPROGRESS_H__ */
