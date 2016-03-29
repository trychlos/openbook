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

#include "my/my-iident.h"

#include "ofa-tva.h"
#include "ofa-tva-dbmodel.h"
#include "ofa-tva-execlose.h"
#include "ofo-tva-form.h"
#include "ofo-tva-record.h"

/* private instance data
 */
struct _ofaTvaPrivate {
	gboolean  dispose_has_run;

	/* an instanciation of each class managed by the module
	 * needed in order to dynamically get exportables/importables
	 */
	GList    *fakes;
};

#define MODULE_DISPLAY_NAME              "VAT declarations"
#define MODULE_VERSION                    PACKAGE_VERSION

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void   instance_finalize( GObject *object );
static void   instance_dispose( GObject *object );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   class_init( ofaTvaClass *klass );
static void   iident_iface_init( myIIdentInterface *iface );
static gchar *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_version( const myIIdent *instance, void *user_data );
static GList *tva_register_types( void );

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

	static const GInterfaceInfo iident_iface_info = {
		( GInterfaceInitFunc ) iident_iface_init,
		NULL,
		NULL
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

	g_type_module_add_interface( module, st_module_type, MY_TYPE_IIDENT, &iident_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IDBMODEL, &idbmodel_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IEXECLOSE_CLOSE, &iexeclose_iface_info );
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

static void
instance_dispose( GObject *object )
{
	ofaTvaPrivate *priv;

	g_return_if_fail( object && OFA_IS_TVA( object ));

	priv = OFA_TVA( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_list_free_full( priv->fakes, ( GDestroyNotify ) g_object_unref );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
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
	self->priv->fakes = tva_register_types();
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

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( MODULE_DISPLAY_NAME ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( MODULE_VERSION ));
}

static GList *
tva_register_types( void )
{
	GList *list;

	list = NULL;
	list = g_list_prepend( list, g_object_new( OFO_TYPE_TVA_FORM, NULL ));
	list = g_list_prepend( list, g_object_new( OFO_TYPE_TVA_RECORD, NULL ));

	return( list );
}

GList *
ofa_tva_get_registered_types( const ofaTva *module )
{
	ofaTvaPrivate *priv;

	g_return_val_if_fail( module && OFA_IS_TVA( module ), NULL );

	priv = module->priv;

	return( priv->fakes );
}
