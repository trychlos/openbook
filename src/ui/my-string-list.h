/*
 * Open Freelance Accounting
 * A double-entry accounting string_list for freelances.
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

#ifndef __MY_STRING_LIST_H__
#define __MY_STRING_LIST_H__

/**
 * SECTION: my_string_list
 * @title: myStringList
 * @short_description: The myStringList object class definition
 * @include: ui/ofa-int-list.h
 *
 * #myStringList is a #GObject -derived object which handles list of ints.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_STRING_LIST                ( my_string_list_get_type())
#define MY_STRING_LIST( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_STRING_LIST, myStringList ))
#define MY_STRING_LIST_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_STRING_LIST, myStringListClass ))
#define MY_IS_STRING_LIST( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_STRING_LIST ))
#define MY_IS_STRING_LIST_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_STRING_LIST ))
#define MY_STRING_LIST_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_STRING_LIST, myStringListClass ))

typedef struct _myStringListPrivate      myStringListPrivate;

typedef struct {
	/*< private >*/
	GObject              parent;
	myStringListPrivate *private;
}
	myStringList;

typedef struct _myStringListClassPrivate myStringListClassPrivate;

/**
 * myStringListClass:
 */
typedef struct {
	/*< private >*/
	GObjectClass              parent;
	myStringListClassPrivate *private;
}
	myStringListClass;

GType         my_string_list_get_type       ( void );

myStringList *my_string_list_new_from_string( const gchar *string );

void          my_string_list_free           ( myStringList *list );

GSList       *my_string_list_get_gslist     ( myStringList *list );

G_END_DECLS

#endif /* __MY_STRING_LIST_H__ */
