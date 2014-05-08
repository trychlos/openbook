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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/my-string-list.h"

/* private class data
 */
struct _myStringListClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _myStringListPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	GSList  *string_list;
};

static       GObjectClass *st_parent_class = NULL;

static GType register_type( void );
static void  class_init( myStringListClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_dispose( GObject *string_list );
static void  instance_finalize( GObject *string_list );
static void  free_string_item( gchar *data );

GType
my_string_list_get_type( void )
{
	static GType st_string_list_type = 0;

	if( !st_string_list_type ){
		st_string_list_type = register_type();
	}

	return( st_string_list_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "my_string_list_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( myStringListClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( myStringList ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "myStringList", &info, 0 );

	return( type );
}

static void
class_init( myStringListClass *klass )
{
	static const gchar *thisfn = "my_string_list_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( myStringListClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *string_list, gpointer klass )
{
	static const gchar *thisfn = "my_string_list_instance_init";
	myStringList *self;

	g_return_if_fail( MY_IS_STRING_LIST( string_list ));

	g_debug( "%s: string_list=%p (%s), klass=%p",
			thisfn, ( void * ) string_list, G_OBJECT_TYPE_NAME( string_list ), ( void * ) klass );

	self = MY_STRING_LIST( string_list );

	self->private = g_new0( myStringListPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *string_list )
{
	static const gchar *thisfn = "my_string_list_instance_dispose";
	myStringListPrivate *priv;

	g_return_if_fail( MY_IS_STRING_LIST( string_list ));

	priv = MY_STRING_LIST( string_list )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: string_list=%p (%s)", thisfn, ( void * ) string_list, G_OBJECT_TYPE_NAME( string_list ));

		priv->dispose_has_run = TRUE;

		g_slist_free_full( priv->string_list, ( GDestroyNotify) free_string_item );

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( string_list );
		}
	}
}

static void
instance_finalize( GObject *string_list )
{
	static const gchar *thisfn = "my_string_list_instance_finalize";
	myStringListPrivate *priv;

	g_return_if_fail( MY_IS_STRING_LIST( string_list ));

	g_debug( "%s: string_list=%p (%s)",
			thisfn, ( void * ) string_list, G_OBJECT_TYPE_NAME( string_list ));

	priv = MY_STRING_LIST( string_list )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( string_list );
	}
}

static void
free_string_item( gchar *data )
{
	g_free( data );
}

/**
 *
 */
myStringList *
my_string_list_new_from_g_value( GValue *value )
{
	return( NULL );
}

/**
 *
 */
void
my_string_list_free( myStringList *list )
{
}

/**
 * Returns a newly allocated GSList
 * which must be my_utils_gslist_free() by the caller
 */
GSList *
my_string_list_get_gslist( myStringList *list )
{
	return( NULL );
}
