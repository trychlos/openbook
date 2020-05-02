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

#include "my/my-date.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-iextender-setter.h"
#include "api/ofa-igetter.h"

#include "mysql/ofa-mysql-dbprovider.h"
#include "mysql/ofa-mysql-dossier-editor.h"
#include "mysql/ofa-mysql-dossier-meta.h"
#include "mysql/ofa-mysql-exercice-editor.h"
#include "mysql/ofa-mysql-root-bin.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	ofaIGetter *getter;
}
	ofaMysqlDBProviderPrivate;

#define DBPROVIDER_CANON_NAME            "MySQL"
#define DBPROVIDER_DISPLAY_NAME          "MySQL DBMS Provider"
#define DBPROVIDER_VERSION                PACKAGE_VERSION

static void                  iident_iface_init( myIIdentInterface *iface );
static gchar                *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar                *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar                *iident_get_version( const myIIdent *instance, void *user_data );
static void                  idbprovider_iface_init( ofaIDBProviderInterface *iface );
static ofaIDBDossierMeta    *idbprovider_new_dossier_meta( ofaIDBProvider *instance );
static ofaIDBDossierEditor  *idbprovider_new_dossier_editor( ofaIDBProvider *instance, const gchar *settings_prefix, guint rule, gboolean with_su );
static ofaIDBExerciceEditor *idbprovider_new_exercice_editor( ofaIDBProvider *instance, const gchar *settings_prefix, guint rule );
static ofaIDBSuperuser      *idbprovider_new_superuser_bin( ofaIDBProvider *instance, guint rule );
static void                  iextender_setter_iface_init( ofaIExtenderSetterInterface *iface );
static ofaIGetter           *iextender_setter_get_getter( ofaIExtenderSetter *instance );
static void                  iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDBProvider, ofa_mysql_dbprovider, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlDBProvider )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBPROVIDER, idbprovider_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXTENDER_SETTER, iextender_setter_iface_init ))

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

	iface->new_dossier_meta = idbprovider_new_dossier_meta;
	iface->new_dossier_editor = idbprovider_new_dossier_editor;
	iface->new_exercice_editor = idbprovider_new_exercice_editor;
	iface->new_superuser_bin = idbprovider_new_superuser_bin;
}

/*
 * instanciates a new ofaIDBDossierMeta object
 */
static ofaIDBDossierMeta *
idbprovider_new_dossier_meta( ofaIDBProvider *instance )
{
	ofaMysqlDossierMeta *meta;

	meta = ofa_mysql_dossier_meta_new();

	return( OFA_IDBDOSSIER_META( meta ));
}

static ofaIDBDossierEditor *
idbprovider_new_dossier_editor( ofaIDBProvider *instance, const gchar *settings_prefix, guint rule, gboolean with_su )
{
	ofaMysqlDossierEditor *widget;

	widget = ofa_mysql_dossier_editor_new( instance, settings_prefix, rule, with_su );

	return( OFA_IDBDOSSIER_EDITOR( widget ));
}

static ofaIDBExerciceEditor *
idbprovider_new_exercice_editor( ofaIDBProvider *instance, const gchar *settings_prefix, guint rule )
{
	ofaMysqlExerciceEditor *widget;

	widget = ofa_mysql_exercice_editor_new( OFA_MYSQL_DBPROVIDER( instance ), settings_prefix, rule );

	return( OFA_IDBEXERCICE_EDITOR( widget ));
}

static ofaIDBSuperuser *
idbprovider_new_superuser_bin( ofaIDBProvider *instance, guint rule )
{
	ofaMysqlRootBin *bin;

	switch( rule ){
		case HUB_RULE_DOSSIER_NEW:
		case HUB_RULE_DOSSIER_RECOVERY:
		case HUB_RULE_DOSSIER_RESTORE:
		case HUB_RULE_EXERCICE_NEW:
		case HUB_RULE_EXERCICE_DELETE:
		case HUB_RULE_EXERCICE_CLOSE:
			bin = ofa_mysql_root_bin_new( OFA_MYSQL_DBPROVIDER( instance ), rule );
			break;

		default:
			bin = NULL;
			break;
	}

	return( bin ? OFA_IDBSUPERUSER( bin ) : NULL );
}

/*
 * #ofaIExtenderSetter interface setup
 */
static void
iextender_setter_iface_init( ofaIExtenderSetterInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_iextender_setter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_getter = iextender_setter_get_getter;
	iface->set_getter = iextender_setter_set_getter;
}

static ofaIGetter *
iextender_setter_get_getter( ofaIExtenderSetter *instance )
{
	ofaMysqlDBProviderPrivate *priv;

	priv = ofa_mysql_dbprovider_get_instance_private( OFA_MYSQL_DBPROVIDER( instance ));

	return( priv->getter );
}

static void
iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter )
{
	ofaMysqlDBProviderPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	priv = ofa_mysql_dbprovider_get_instance_private( OFA_MYSQL_DBPROVIDER( instance ));

	priv->getter = getter;
}
