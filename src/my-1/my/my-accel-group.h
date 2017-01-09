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

#ifndef __MY_API_MY_ACCEL_GROUP_H__
#define __MY_API_MY_ACCEL_GROUP_H__

/* @title: myAccelGroup
 * @short_description: The myAccelGroup Class Definition
 * @include: my/my-accel-group.h
 *
 * The #myAccelGroup class is an extension of #GtkAccelGroup to make
 * things easier in accelerators management.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_ACCEL_GROUP                ( my_accel_group_get_type())
#define MY_ACCEL_GROUP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_ACCEL_GROUP, myAccelGroup ))
#define MY_ACCEL_GROUP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_ACCEL_GROUP, myAccelGroupClass ))
#define MY_IS_ACCEL_GROUP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_ACCEL_GROUP ))
#define MY_IS_ACCEL_GROUP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_ACCEL_GROUP ))
#define MY_ACCEL_GROUP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_ACCEL_GROUP, myAccelGroupClass ))

typedef struct {
	/*< public members >*/
	GtkAccelGroup      parent;
}
	myAccelGroup;

typedef struct {
	/*< public members >*/
	GtkAccelGroupClass parent;
}
	myAccelGroupClass;

GType         my_accel_group_get_type              ( void ) G_GNUC_CONST;

myAccelGroup *my_accel_group_new                   ( void );

void          my_accel_group_setup_accels_from_menu( myAccelGroup *group,
															GActionMap *map,
															GMenuModel *menu );

G_END_DECLS

#endif /* __MY_API_MY_ACCEL_GROUP_H__ */
