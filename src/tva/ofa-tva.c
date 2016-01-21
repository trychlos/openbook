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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ofa-tva.h"
#include "ofa-tva-dbmodel.h"
#include "ofa-tva-execlose.h"

/* private instance data
 */
struct _ofaTvaPrivate {
	gboolean  dispose_has_run;
};

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void class_init( ofaTvaClass *klass );
static void instance_init( GTypeInstance *instance, gpointer klass );
static void instance_dispose( GObject *object );
static void instance_finalize( GObject *object );

GType
ofa_tva_get_type( void )
{
	return( st_module_type );
}

void
ofa_tva_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_tva_register_type";

	static GTypeInfo info = {
		sizeof( ofaTvaClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaTva ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo idbmodel_iface_info = {
		( GInterfaceInitFunc ) ofa_tva_dbmodel_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iexeclose_iface_info = {
		( GInterfaceInitFunc ) ofa_tva_execlose_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaTva", &info, 0 );

	g_type_module_add_interface(
			module, st_module_type, OFA_TYPE_IDBMODEL, &idbmodel_iface_info );

	g_type_module_add_interface(
			module, st_module_type, OFA_TYPE_IEXECLOSE_CLOSE, &iexeclose_iface_info );
}

static void
class_init( ofaTvaClass *klass )
{
	static const gchar *thisfn = "ofa_tva_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaTvaPrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_tva_instance_init";
	ofaTva *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_TVA( instance ));

	self = OFA_TVA( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA, ofaTvaPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaTvaPrivate *priv;

	g_return_if_fail( object && OFA_IS_TVA( object ));

	priv = OFA_TVA( object )->priv;

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
	static const gchar *thisfn = "ofa_tva_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_TVA( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}
