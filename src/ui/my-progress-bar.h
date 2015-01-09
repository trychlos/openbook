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

#ifndef __MY_PROGRESS_BAR_H__
#define __MY_PROGRESS_BAR_H__

/**
 * SECTION: my_progress_bar
 * @short_description: #myProgressBar class definition.
 * @include: ui/my-progress-bar.h
 *
 * A convenience class to manage the progress bars.
 *
 * This defines two action signals to let the user display its
 * progression in the bar:
 * - double
 * - text.
 */

#include <gtk/gtk.h>

#include "api/my-date.h"

G_BEGIN_DECLS

#define MY_TYPE_PROGRESS_BAR                ( my_progress_bar_get_type())
#define MY_PROGRESS_BAR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_PROGRESS_BAR, myProgressBar ))
#define MY_PROGRESS_BAR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_PROGRESS_BAR, myProgressBarClass ))
#define MY_IS_PROGRESS_BAR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_PROGRESS_BAR ))
#define MY_IS_PROGRESS_BAR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_PROGRESS_BAR ))
#define MY_PROGRESS_BAR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_PROGRESS_BAR, myProgressBarClass ))

typedef struct _myProgressBarPrivate        myProgressBarPrivate;

typedef struct {
	/*< public members >*/
	GtkProgressBar        parent;

	/*< private members >*/
	myProgressBarPrivate *priv;
}
	myProgressBar;

typedef struct {
	/*< public members >*/
	GtkProgressBarClass   parent;
}
	myProgressBarClass;

GType          my_progress_bar_get_type ( void ) G_GNUC_CONST;

myProgressBar *my_progress_bar_new      ( void );

void           my_progress_bar_attach_to( myProgressBar *bar,
													GtkContainer *parent );

G_END_DECLS

#endif /* __MY_PROGRESS_BAR_H__ */
