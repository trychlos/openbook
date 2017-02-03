/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-iident.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iproperties.h"

#include "mysql/ofa-mysql-prefs-bin.h"
#include "mysql/ofa-mysql-properties.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaMysqlPropertiesPrivate;

#define IPROPERTIES_CANON_NAME           "MySQL"

static void       iident_iface_init( myIIdentInterface *iface );
static gchar     *iident_get_canon_name( const myIIdent *instance, void *user_data );
static void       iproperties_iface_init( ofaIPropertiesInterface *iface );
static GtkWidget *iproperties_init( ofaIProperties *instance, ofaIGetter *getter );
static gboolean   iproperties_is_valid( const ofaIProperties *instance, GtkWidget *widget, gchar **msgerr );
static void       iproperties_apply( const ofaIProperties *instance, GtkWidget *widget );

G_DEFINE_TYPE_EXTENDED( ofaMysqlProperties, ofa_mysql_properties, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IPROPERTIES, iproperties_iface_init ))

static void
mysql_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_properties_parent_class )->finalize( instance );
}

static void
mysql_properties_dispose( GObject *instance )
{
	ofaMysqlPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_PROPERTIES( instance ));

	priv = ofa_mysql_properties_get_instance_private( OFA_MYSQL_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_properties_parent_class )->dispose( instance );
}

static void
ofa_mysql_properties_init( ofaMysqlProperties *self )
{
	static const gchar *thisfn = "ofa_mysql_properties_init";
	ofaMysqlPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_properties_class_init( ofaMysqlPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_properties_finalize;
}

/*
 * #myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_properties_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IPROPERTIES_CANON_NAME ));
}

/*
 * ofaIProperties interface management
 */
static void
iproperties_iface_init( ofaIPropertiesInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_prefs_bin_iproperties_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iproperties_init;
	iface->is_valid = iproperties_is_valid;
	iface->apply = iproperties_apply;
}

static GtkWidget *
iproperties_init( ofaIProperties *instance, ofaIGetter *getter )
{
	GtkWidget *bin;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PROPERTIES( instance ), NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = ofa_mysql_prefs_bin_new( getter );

	return( bin );
}

static gboolean
iproperties_is_valid( const ofaIProperties *instance, GtkWidget *widget, gchar **message )
{
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PROPERTIES( instance ), FALSE );
	g_return_val_if_fail( widget && OFA_IS_MYSQL_PREFS_BIN( widget ), FALSE );

	ok = ofa_mysql_prefs_bin_is_valid( OFA_MYSQL_PREFS_BIN( widget ), message );

	return( ok );
}

static void
iproperties_apply( const ofaIProperties *instance, GtkWidget *widget )
{
	g_return_if_fail( instance && OFA_IS_MYSQL_PROPERTIES( instance ));
	g_return_if_fail( widget && OFA_IS_MYSQL_PREFS_BIN( widget ));

	ofa_mysql_prefs_bin_apply( OFA_MYSQL_PREFS_BIN( widget ));
}
