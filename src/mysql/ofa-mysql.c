/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "ofa-mysql.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-ipreferences.h"

/* private instance data
 */
struct _ofaMysqlPrivate {
	gboolean dispose_has_run;
};

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void         class_init( ofaMysqlClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );

GType
ofa_mysql_get_type( void )
{
	return( st_module_type );
}

void
ofa_mysql_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_mysql_register_type";

	static GTypeInfo info = {
		sizeof( ofaMysqlClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMysql ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo idbprovider_iface_info = {
		( GInterfaceInitFunc ) ofa_mysql_idbprovider_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo ipreferences_iface_info = {
		( GInterfaceInitFunc ) ofa_mysql_ipreferences_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaMysql", &info, 0 );

	g_type_module_add_interface(
			module, st_module_type, OFA_TYPE_IDBPROVIDER, &idbprovider_iface_info );

	g_type_module_add_interface(
			module, st_module_type, OFA_TYPE_IPREFERENCES, &ipreferences_iface_info );
}

static void
class_init( ofaMysqlClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaMysqlPrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_mysql_instance_init";
	ofaMysql *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_MYSQL( instance ));

	self = OFA_MYSQL( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL, ofaMysqlPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaMysqlPrivate *priv;

	g_return_if_fail( object && OFA_IS_MYSQL( object ));

	priv = OFA_MYSQL( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_mysql_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_MYSQL( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}
