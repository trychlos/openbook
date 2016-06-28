/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-dbprovider.h"
#include "mysql/ofa-mysql-editor-display.h"
#include "mysql/ofa-mysql-editor-enter.h"
#include "mysql/ofa-mysql-meta.h"
#include "mysql/ofa-mysql-period.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaMysqlDBProviderPrivate;

#define DBPROVIDER_CANON_NAME            "MySQL"
#define DBPROVIDER_DISPLAY_NAME          "MySQL DBMS Provider"
#define DBPROVIDER_VERSION                PACKAGE_VERSION

static void           iident_iface_init( myIIdentInterface *iface );
static gchar         *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar         *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar         *iident_get_version( const myIIdent *instance, void *user_data );
static void           idbprovider_iface_init( ofaIDBProviderInterface *iface );
static ofaIDBMeta    *idbprovider_new_meta( ofaIDBProvider *instance );
static ofaIDBConnect *idbprovider_new_connect( ofaIDBProvider *instance );
static ofaIDBEditor  *idbprovider_new_editor( ofaIDBProvider *instance, gboolean editable );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDBProvider, ofa_mysql_dbprovider, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlDBProvider )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBPROVIDER, idbprovider_iface_init ))

static void
mysql_dbprovider_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_DBPROVIDER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dbprovider_parent_class )->finalize( instance );
}

static void
mysql_dbprovider_dispose( GObject *instance )
{
	ofaMysqlDBProviderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_DBPROVIDER( instance ));

	priv = ofa_mysql_dbprovider_get_instance_private( OFA_MYSQL_DBPROVIDER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dbprovider_parent_class )->dispose( instance );
}

static void
ofa_mysql_dbprovider_init( ofaMysqlDBProvider *self )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_init";
	ofaMysqlDBProviderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_dbprovider_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_dbprovider_class_init( ofaMysqlDBProviderClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_dbprovider_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_dbprovider_finalize;
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
idbprovider_new_meta( ofaIDBProvider *instance )
{
	ofaMySQLMeta *meta;

	meta = ofa_mysql_meta_new();

	return( OFA_IDBMETA( meta ));
}

/*
 * instanciates a new ofaIDBConnect object
 */
static ofaIDBConnect *
idbprovider_new_connect( ofaIDBProvider *instance )
{
	ofaMySQLConnect *connect;

	connect = ofa_mysql_connect_new();

	return( OFA_IDBCONNECT( connect ));
}

static ofaIDBEditor *
idbprovider_new_editor( ofaIDBProvider *instance, gboolean editable )
{
	GtkWidget *widget;

	widget = editable ? ofa_mysql_editor_enter_new() : ofa_mysql_editor_display_new();

	return( OFA_IDBEDITOR( widget ));
}
