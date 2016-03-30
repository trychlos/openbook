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

#include "api/ofa-extension.h"

#include "mysql/ofa-mysql-dbmodel.h"
#include "mysql/ofa-mysql-dbprovider.h"

/*
 * The part below defines and implements the GTypeModule-derived class
 *  for this library.
 * See infra for the software extension API implementation.
 */
#define OFA_TYPE_MYSQL_ID                ( ofa_mysql_id_get_type())
#define OFA_MYSQL_ID( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_ID, ofaMysqlId ))
#define OFA_MYSQL_ID_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_ID, ofaMysqlIdClass ))
#define OFA_IS_MYSQL_ID( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_ID ))
#define OFA_IS_MYSQL_ID_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_ID ))
#define OFA_MYSQL_ID_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_ID, ofaMysqlIdClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlId;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlIdClass;

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaMysqlIdPrivate;

GType ofa_mysql_id_get_type( void );

static void   iident_iface_init( myIIdentInterface *iface );
static gchar *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_version( const myIIdent *instance, void *user_data );

G_DEFINE_DYNAMIC_TYPE_EXTENDED( ofaMysqlId, ofa_mysql_id, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE_DYNAMIC( ofaMysqlId )
		G_IMPLEMENT_INTERFACE_DYNAMIC( MY_TYPE_IIDENT, iident_iface_init ))

static void
ofa_mysql_id_class_finalize( ofaMysqlIdClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_id_class_finalize";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
}

static void
mysql_id_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_id_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_ID( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_id_parent_class )->finalize( instance );
}

static void
mysql_id_dispose( GObject *instance )
{
	ofaMysqlIdPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_ID( instance ));

	priv = ofa_mysql_id_get_instance_private( OFA_MYSQL_ID( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_id_parent_class )->dispose( instance );
}

static void
ofa_mysql_id_init( ofaMysqlId *self )
{
	static const gchar *thisfn = "ofa_mysql_id_init";
	ofaMysqlIdPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_id_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_id_class_init( ofaMysqlIdClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_id_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_id_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_id_finalize;
}

/*
 * #myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_id_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "MySQL" ));
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "MySQL Library" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( PACKAGE_VERSION ));
}

/*
 * The part below implements the software extension API
 */

/*
 * The count of GType types provided by this extension.
 * Each of these GType types must be addressed in #ofa_extension_list_types().
 * Only the GTypeModule has to be registered from #ofa_extension_startup().
 */
#define TYPES_COUNT	 3

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, ofaIGetter *getter )
{
	static const gchar *thisfn = "mysql/ofa_extension_startup";

	g_debug( "%s: module=%p, getter=%p", thisfn, ( void * ) module, ( void * ) getter  );

	ofa_mysql_id_register_type( module );

	return( TRUE );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with API v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "mysql/ofa_extension_list_types";
	static GType types_list [1+TYPES_COUNT];
	gint i = 0;

	g_debug( "%s: types=%p, count=%u", thisfn, ( void * ) types, TYPES_COUNT );

	types_list[i++] = OFA_TYPE_MYSQL_ID;
	types_list[i++] = OFA_TYPE_MYSQL_DBMODEL;
	types_list[i++] = OFA_TYPE_MYSQL_DBPROVIDER;

	g_return_val_if_fail( i == TYPES_COUNT, 0 );
	types_list[i] = 0;
	*types = types_list;

	return( TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * optional as of API v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "mysql/ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
