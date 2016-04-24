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

#ifndef __MY_API_MY_DND_POPUP_H__
#define __MY_API_MY_DND_POPUP_H__

/**
 * SECTION: my_dnd_popup
 * @short_description: #myDndPopup class definition.
 * @include: my/my-dnd-popup.h
 *
 * A popup #GtkWindow which acts as DnD drag icon.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_DND_POPUP                ( my_dnd_popup_get_type())
#define MY_DND_POPUP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DND_POPUP, myDndPopup ))
#define MY_DND_POPUP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DND_POPUP, myDndPopupClass ))
#define MY_IS_DND_POPUP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DND_POPUP ))
#define MY_IS_DND_POPUP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DND_POPUP ))
#define MY_DND_POPUP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DND_POPUP, myDndPopupClass ))

typedef struct {
	/*< public members >*/
	GtkWindow      parent;
}
	myDndPopup;

typedef struct {
	/*< public members >*/
	GtkWindowClass parent;
}
	myDndPopupClass;

GType        my_dnd_popup_get_type        ( void ) G_GNUC_CONST;

myDndPopup  *my_dnd_popup_new             ( GtkWidget *source );

const gchar *my_dnd_popup_get_result_label( GtkDragResult result );

G_END_DECLS

#endif /* __MY_API_MY_DND_POPUP_H__ */
