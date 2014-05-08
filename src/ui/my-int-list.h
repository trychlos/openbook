/*
 * Open Freelance Accounting
 * A double-entry accounting int_list for freelances.
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

#ifndef __MY_INT_LIST_H__
#define __MY_INT_LIST_H__

/**
 * SECTION: my_int_list
 * @title: myIntList
 * @short_description: The myIntList object class definition
 * @include: ui/ofa-int-list.h
 *
 * #myIntList is a #GObject -derived object which handles list of ints.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_INT_LIST                ( my_int_list_get_type())
#define MY_INT_LIST( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_INT_LIST, myIntList ))
#define MY_INT_LIST_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_INT_LIST, myIntListClass ))
#define MY_IS_INT_LIST( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_INT_LIST ))
#define MY_IS_INT_LIST_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_INT_LIST ))
#define MY_INT_LIST_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_INT_LIST, myIntListClass ))

typedef struct _myIntListPrivate      myIntListPrivate;

typedef struct {
	/*< private >*/
	GObject           parent;
	myIntListPrivate *private;
}
	myIntList;

typedef struct _myIntListClassPrivate myIntListClassPrivate;

/**
 * myIntListClass:
 */
typedef struct {
	/*< private >*/
	GObjectClass           parent;
	myIntListClassPrivate *private;
}
	myIntListClass;

GType      my_int_list_get_type        ( void );

myIntList *my_int_list_new_from_g_value( GValue *value );

void       my_int_list_free            ( myIntList* list );

GList     *my_int_list_get_glist       ( myIntList* list );

G_END_DECLS

#endif /* __MY_INT_LIST_H__ */
