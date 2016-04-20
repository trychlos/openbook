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

#ifndef __MY_API_MY_BOOK_DND_H__
#define __MY_API_MY_BOOK_DND_H__

/* @title: myBookDnd
 * @short_description: The myBookDnd Class Definition
 * @include: my/my-dnd-book.h
 *
 * The #myBookDnd class is an extension of #GtkNotebook which let
 * detach its tabs. It implements #myIBookAttach and #myIBookDetach
 * interfaces.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_BOOK_DND                ( my_book_dnd_get_type())
#define MY_BOOK_DND( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_BOOK_DND, myBookDnd ))
#define MY_BOOK_DND_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_BOOK_DND, myBookDndClass ))
#define MY_IS_BOOK_DND( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_BOOK_DND ))
#define MY_IS_BOOK_DND_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_BOOK_DND ))
#define MY_BOOK_DND_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_BOOK_DND, myBookDndClass ))

typedef struct {
	/*< public members >*/
	GtkNotebook      parent;
}
	myBookDnd;

typedef struct {
	/*< public members >*/
	GtkNotebookClass parent;
}
	myBookDndClass;

GType      my_book_dnd_get_type( void ) G_GNUC_CONST;

myBookDnd *my_book_dnd_new     ( void );

G_END_DECLS

#endif /* __MY_API_MY_BOOK_DND_H__ */
