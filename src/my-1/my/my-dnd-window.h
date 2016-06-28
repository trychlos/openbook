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

#ifndef __MY_API_MY_DND_WINDOW_H__
#define __MY_API_MY_DND_WINDOW_H__

/**
 * SECTION: my_dnd_window
 * @short_description: #myDndWindow class definition.
 * @include: my/my-dnd-window.h
 *
 * Open a non-modal window as a DnD source and target for #myDndBook.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_DND_WINDOW                ( my_dnd_window_get_type())
#define MY_DND_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DND_WINDOW, myDndWindow ))
#define MY_DND_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DND_WINDOW, myDndWindowClass ))
#define MY_IS_DND_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DND_WINDOW ))
#define MY_IS_DND_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DND_WINDOW ))
#define MY_DND_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DND_WINDOW, myDndWindowClass ))

typedef struct {
	/*< public members >*/
	GtkWindow      parent;
}
	myDndWindow;

typedef struct {
	/*< public members >*/
	GtkWindowClass parent;
}
	myDndWindowClass;

GType        my_dnd_window_get_type       ( void ) G_GNUC_CONST;

myDndWindow *my_dnd_window_new            ( GtkWidget *widget,
												const gchar *title,
												gint x,
												gint y,
												gint width,
												gint height );

gboolean     my_dnd_window_present_by_type( GType type );

void         my_dnd_window_close_all      ( void );

G_END_DECLS

#endif /* __MY_API_MY_DND_WINDOW_H__ */
