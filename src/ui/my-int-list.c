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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/my-int-list.h"

/* private class data
 */
struct _myIntListClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _myIntListPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	GList   *int_list;
};

static       GObjectClass *st_parent_class = NULL;

static GType register_type( void );
static void  class_init( myIntListClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_dispose( GObject *int_list );
static void  instance_finalize( GObject *int_list );

GType
my_int_list_get_type( void )
{
	static GType int_list_type = 0;

	if( !int_list_type ){
		int_list_type = register_type();
	}

	return( int_list_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "my_int_list_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( myIntListClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( myIntList ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "myIntList", &info, 0 );

	return( type );
}

static void
class_init( myIntListClass *klass )
{
	static const gchar *thisfn = "my_int_list_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( myIntListClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *int_list, gpointer klass )
{
	static const gchar *thisfn = "my_int_list_instance_init";
	myIntList *self;

	g_return_if_fail( MY_IS_INT_LIST( int_list ));

	g_debug( "%s: int_list=%p (%s), klass=%p",
			thisfn, ( void * ) int_list, G_OBJECT_TYPE_NAME( int_list ), ( void * ) klass );

	self = MY_INT_LIST( int_list );

	self->private = g_new0( myIntListPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *int_list )
{
	static const gchar *thisfn = "my_int_list_instance_dispose";
	myIntListPrivate *priv;

	g_return_if_fail( MY_IS_INT_LIST( int_list ));

	priv = MY_INT_LIST( int_list )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: int_list=%p (%s)", thisfn, ( void * ) int_list, G_OBJECT_TYPE_NAME( int_list ));

		priv->dispose_has_run = TRUE;

		g_list_free( priv->int_list );

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( int_list );
		}
	}
}

static void
instance_finalize( GObject *int_list )
{
	static const gchar *thisfn = "my_int_list_instance_finalize";
	myIntListPrivate *priv;

	g_return_if_fail( MY_IS_INT_LIST( int_list ));

	g_debug( "%s: int_list=%p (%s)",
			thisfn, ( void * ) int_list, G_OBJECT_TYPE_NAME( int_list ));

	priv = MY_INT_LIST( int_list )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( int_list );
	}
}

/**
 *
 */
myIntList *
my_int_list_new_from_g_value( GValue *value )
{
	return( NULL );
}

/**
 *
 */
void
my_int_list_free( myIntList *list )
{
}

/**
 * Returns a newly allocated GList
 * which must be g_list_free() by the caller
 */
GList *
my_int_list_get_glist( myIntList *list )
{
	return( NULL );
}
