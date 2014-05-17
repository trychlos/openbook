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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/ofo-base.h"

/* private class data
 */
struct _ofoBaseClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofoBasePrivate {
	gboolean dispose_has_run;
};

static GObjectClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoBaseClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );

GType
ofo_base_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_base_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoBaseClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoBase ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofoBase", &info, 0 );

	return( type );
}

static void
class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoBaseClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	/*static const gchar *thisfn = "ofo_base_instance_init";*/
	ofoBase *self;

	g_return_if_fail( OFO_IS_BASE( instance ));

	/*g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );*/

	self = OFO_BASE( instance );

	self->private = g_new0( ofoBasePrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	/*static const gchar *thisfn = "ofo_base_instance_dispose";*/
	ofoBasePrivate *priv;

	g_return_if_fail( OFO_IS_BASE( instance ));

	priv = ( OFO_BASE( instance ))->private;

	if( !priv->dispose_has_run ){

		/*g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));*/

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	/*static const gchar *thisfn = "ofo_base_instance_finalize";*/
	ofoBasePrivate *priv;

	g_return_if_fail( OFO_IS_BASE( instance ));

	/*g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));*/

	priv = OFO_BASE( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}
