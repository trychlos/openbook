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

#ifndef __MY_API_MY_DND_BOOK_H__
#define __MY_API_MY_DND_BOOK_H__

/* @title: myDndBook
 * @short_description: The myDndBook Class Definition
 * @include: my/my-dnd-book.h
 *
 * The #myDndBook class is an extension of #GtkNotebook which let
 * detach its tabs. It implements #myIBookAttach and #myIDndDetach
 * interfaces.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_DND_BOOK                ( my_dnd_book_get_type())
#define MY_DND_BOOK( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DND_BOOK, myDndBook ))
#define MY_DND_BOOK_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DND_BOOK, myDndBookClass ))
#define MY_IS_DND_BOOK( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DND_BOOK ))
#define MY_IS_DND_BOOK_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DND_BOOK ))
#define MY_DND_BOOK_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DND_BOOK, myDndBookClass ))

typedef struct {
	/*< public members >*/
	GtkNotebook      parent;
}
	myDndBook;

typedef struct {
	/*< public members >*/
	GtkNotebookClass parent;
}
	myDndBookClass;

GType      my_dnd_book_get_type( void ) G_GNUC_CONST;

myDndBook *my_dnd_book_new     ( void );

G_END_DECLS

#endif /* __MY_API_MY_DND_BOOK_H__ */
