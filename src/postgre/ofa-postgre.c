/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ofa-postgre.h"

/* private instance data
 */
struct _ofaPostgrePrivate {
	gboolean  dispose_has_run;
};

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void class_init( ofaPostgreClass *klass );
static void instance_init( GTypeInstance *instance, gpointer klass );
static void instance_dispose( GObject *object );
static void instance_finalize( GObject *object );

GType
ofa_postgre_get_type( void )
{
	return( st_module_type );
}

void
ofa_postgre_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_postgre_register_type";

	static GTypeInfo info = {
		sizeof( ofaPostgreClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaPostgre ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaPostgre", &info, 0 );
}

static void
class_init( ofaPostgreClass *klass )
{
	static const gchar *thisfn = "ofa_postgre_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaPostgrePrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_postgre_instance_init";
	ofaPostgre *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_POSTGRE( instance ));

	self = OFA_POSTGRE( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_POSTGRE, ofaPostgrePrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaPostgre *self;

	g_return_if_fail( object && OFA_IS_POSTGRE( object ));

	self = OFA_POSTGRE( object );

	if( !self->priv->dispose_has_run ){

		self->priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_postgre_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_POSTGRE( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}
