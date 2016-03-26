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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-iprefs-provider.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-dbprovider.h"
#include "mysql/ofa-mysql-editor-display.h"
#include "mysql/ofa-mysql-editor-enter.h"
#include "mysql/ofa-mysql-iprefs-provider.h"
#include "mysql/ofa-mysql-meta.h"
#include "mysql/ofa-mysql-period.h"

/* private instance data
 */
struct _ofaMysqlDBProviderPrivate {
	gboolean dispose_has_run;
};

#define DBPROVIDER_CANON_NAME            "MySQL"
#define DBPROVIDER_DISPLAY_NAME          "MySQL DBMS Provider"
#define DBPROVIDER_VERSION                PACKAGE_VERSION

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void           instance_finalize( GObject *object );
static void           instance_dispose( GObject *object );
static void           instance_init( GTypeInstance *instance, gpointer klass );
static void           class_init( ofaMysqlDBProviderClass *klass );
static void           iident_iface_init( myIIdentInterface *iface );
static gchar         *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar         *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar         *iident_get_version( const myIIdent *instance, void *user_data );
static void           idbprovider_iface_init( ofaIDBProviderInterface *iface );
static ofaIDBMeta    *idbprovider_new_meta( void );
static ofaIDBConnect *idbprovider_new_connect( void );
static ofaIDBEditor  *idbprovider_new_editor( gboolean editable );

GType
ofa_mysql_dbprovider_get_type( void )
{
	return( st_module_type );
}

void
ofa_mysql_dbprovider_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_register_type";

	static GTypeInfo info = {
		sizeof( ofaMysqlDBProviderClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMysqlDBProvider ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iident_iface_info = {
		( GInterfaceInitFunc ) iident_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo idbprovider_iface_info = {
		( GInterfaceInitFunc ) idbprovider_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iprefs_provider_iface_info = {
		( GInterfaceInitFunc ) ofa_mysql_iprefs_provider_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaMysqlDBProvider", &info, 0 );

	g_type_module_add_interface( module, st_module_type, MY_TYPE_IIDENT, &iident_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IDBPROVIDER, &idbprovider_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IPREFS_PROVIDER, &iprefs_provider_iface_info );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_MYSQL_DBPROVIDER( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
instance_dispose( GObject *object )
{
	ofaMysqlDBProviderPrivate *priv;

	g_return_if_fail( object && OFA_IS_MYSQL_DBPROVIDER( object ));

	priv = OFA_MYSQL_DBPROVIDER( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_instance_init";
	ofaMysqlDBProvider *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_MYSQL_DBPROVIDER( instance ));

	self = OFA_MYSQL_DBPROVIDER( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL_DBPROVIDER, ofaMysqlDBProviderPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
class_init( ofaMysqlDBProviderClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaMysqlDBProviderPrivate ));
}

/*
 * #myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( DBPROVIDER_CANON_NAME ));
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( DBPROVIDER_DISPLAY_NAME ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( DBPROVIDER_VERSION ));
}

/*
 * #ofaIDBProvider interface setup
 */
static void
idbprovider_iface_init( ofaIDBProviderInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_idbprovider_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->new_meta = idbprovider_new_meta;
	iface->new_connect = idbprovider_new_connect;
	iface->new_editor = idbprovider_new_editor;
}

/*
 * instanciates a new ofaIDBMeta object
 */
static ofaIDBMeta *
idbprovider_new_meta( void )
{
	ofaMySQLMeta *meta;

	meta = ofa_mysql_meta_new();

	return( OFA_IDBMETA( meta ));
}

/*
 * instanciates a new ofaIDBConnect object
 */
static ofaIDBConnect *
idbprovider_new_connect( void )
{
	ofaMySQLConnect *connect;

	connect = ofa_mysql_connect_new();

	return( OFA_IDBCONNECT( connect ));
}

static ofaIDBEditor *
idbprovider_new_editor( gboolean editable )
{
	GtkWidget *widget;

	widget = editable ? ofa_mysql_editor_enter_new() : ofa_mysql_editor_display_new();

	return( OFA_IDBEDITOR( widget ));
}
