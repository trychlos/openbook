/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"

#include "ofa-mysql-connect.h"
#include "ofa-mysql-exercice-editor.h"
#include "ofa-mysql-exercice-meta.h"

/* priv instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime data
	 */
	gchar    *database;
}
	ofaMysqlExerciceMetaPrivate;

#define MYSQL_DATABASE_KEY_PREFIX       "mysql-db-"

static void     read_settings( ofaMysqlExerciceMeta *self, const gchar *key_id );
static void     write_settings( ofaMysqlExerciceMeta *self, const gchar *key_id );
static void     idbexercice_meta_iface_init( ofaIDBExerciceMetaInterface *iface );
static guint    idbexercice_meta_get_interface_version( void );
static void     idbexercice_meta_set_from_settings( ofaIDBExerciceMeta *instance, const gchar *key_id );
static void     idbexercice_meta_set_from_editor( ofaIDBExerciceMeta *instance, ofaIDBExerciceEditor *editor, const gchar *key_id );
static gchar   *idbexercice_meta_get_name( const ofaIDBExerciceMeta *instance );
static gint     idbexercice_meta_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b );
static void     idbexercice_meta_update_settings( const ofaIDBExerciceMeta *instance );
static gboolean idbexercice_meta_delete( ofaIDBExerciceMeta *period, ofaIDBConnect *connect, gchar **msgerr );
static void     idbexercice_meta_dump( const ofaIDBExerciceMeta *instance );

G_DEFINE_TYPE_EXTENDED( ofaMysqlExerciceMeta, ofa_mysql_exercice_meta, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlExerciceMeta )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEXERCICE_META, idbexercice_meta_iface_init ))

static void
mysql_exercice_meta_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_exercice_meta_finalize";
	ofaMysqlExerciceMetaPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_META( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	/* free data members here */
	g_free( priv->database );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_meta_parent_class )->finalize( instance );
}

static void
mysql_exercice_meta_dispose( GObject *instance )
{
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_meta_parent_class )->dispose( instance );
}

static void
ofa_mysql_exercice_meta_init( ofaMysqlExerciceMeta *self )
{
	static const gchar *thisfn = "ofa_mysql_exercice_meta_init";
	ofaMysqlExerciceMetaPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_exercice_meta_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_exercice_meta_class_init( ofaMysqlExerciceMetaClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_exercice_meta_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_exercice_meta_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_exercice_meta_finalize;
}

/**
 * ofa_mysql_exercice_meta_new:
 *
 * Returns: a reference to a new #ofaMysqlExerciceMeta object, which
 * implements the #ofaIDBExerciceMeta interface.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
ofaMysqlExerciceMeta *
ofa_mysql_exercice_meta_new( void )
{
	ofaMysqlExerciceMeta *meta;

	meta = g_object_new( OFA_TYPE_MYSQL_EXERCICE_META, NULL );

	return( meta );
}

/**
 * ofa_mysql_exercice_meta_get_database:
 * @period: this #ofaMysqlExerciceMeta object.
 *
 * Returns: the database name.
 *
 * The returned string is owned by the @period object, and should not
 * be freed by the caller.
 */
const gchar *
ofa_mysql_exercice_meta_get_database( ofaMysqlExerciceMeta *period )
{
	ofaMysqlExerciceMetaPrivate *priv;

	g_return_val_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ), NULL );

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->database );
}

/**
 * ofa_mysql_exercice_meta_set_database:
 * @period: this #ofaMysqlExerciceMeta object.
 * @database: [allow-none]: the database to be set.
 *
 * Set the database name.
 */
void
ofa_mysql_exercice_meta_set_database( ofaMysqlExerciceMeta *period, const gchar *database )
{
	ofaMysqlExerciceMetaPrivate *priv;

	g_return_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ));

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->database );
	priv->database = g_strdup( database );
}

/*
 * Settings are: "database(s);"
 */
static void
read_settings( ofaMysqlExerciceMeta *self, const gchar *key_id )
{
	ofaMysqlExerciceMetaPrivate *priv;
	ofaIDBDossierMeta *dossier_meta;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *group, *cstr;
	gchar *key;

	priv = ofa_mysql_exercice_meta_get_instance_private( self );

	dossier_meta = ofa_idbexercice_meta_get_dossier_meta( OFA_IDBEXERCICE_META( self ));
	settings = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, key_id );

	strlist = my_isettings_get_string_list( settings, group, key );

	/* database */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->database );
		priv->database = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaMysqlExerciceMeta *self, const gchar *key_id )
{
	ofaMysqlExerciceMetaPrivate *priv;
	ofaIDBDossierMeta *dossier_meta;
	myISettings *settings;
	const gchar *group;
	gchar *str, *key;

	priv = ofa_mysql_exercice_meta_get_instance_private( self );

	str = g_strdup_printf( "%s;",
			priv->database );

	dossier_meta = ofa_idbexercice_meta_get_dossier_meta( OFA_IDBEXERCICE_META( self ));
	settings = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, key_id );

	my_isettings_set_string( settings, group, key, str );

	g_free( key );
	g_free( str );
}

/*
 * ofaIDBExerciceMeta interface management
 */
static void
idbexercice_meta_iface_init( ofaIDBExerciceMetaInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_exercice_meta_idbexercice_meta_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbexercice_meta_get_interface_version;
	iface->set_from_settings = idbexercice_meta_set_from_settings;
	iface->set_from_editor = idbexercice_meta_set_from_editor;
	iface->get_name = idbexercice_meta_get_name;
	iface->compare = idbexercice_meta_compare;
	iface->update_settings = idbexercice_meta_update_settings;
	iface->delete = idbexercice_meta_delete;
	iface->dump = idbexercice_meta_dump;
}

static guint
idbexercice_meta_get_interface_version( void )
{
	return( 1 );
}

static void
idbexercice_meta_set_from_settings( ofaIDBExerciceMeta *instance, const gchar *key_id )
{
	read_settings( OFA_MYSQL_EXERCICE_META( instance ), key_id );
}

static void
idbexercice_meta_set_from_editor( ofaIDBExerciceMeta *instance, ofaIDBExerciceEditor *editor, const gchar *key_id )
{
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	g_free( priv->database );
	priv->database = g_strdup( ofa_mysql_exercice_editor_get_database( OFA_MYSQL_EXERCICE_EDITOR( editor )));

	write_settings( OFA_MYSQL_EXERCICE_META( instance ), key_id );
}

static gchar *
idbexercice_meta_get_name( const ofaIDBExerciceMeta *instance )
{
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( priv->database ));
}

static gint
idbexercice_meta_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b )
{
	ofaMysqlExerciceMetaPrivate *a_priv, *b_priv;
	gint cmp;
	glong a_len, b_len;

	a_priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( a ));
	b_priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( b ));

	a_len = my_strlen( a_priv->database );
	b_len = my_strlen( b_priv->database );

	cmp = ( a_len < b_len ? -1 : ( a_len > b_len ? 1 : 0 ));

	return( cmp );
}

static void
idbexercice_meta_update_settings( const ofaIDBExerciceMeta *instance )
{
	const gchar *key_id;

	key_id = ofa_idbexercice_meta_get_settings_id( instance );
	write_settings( OFA_MYSQL_EXERCICE_META( instance ), key_id );
}

static gboolean
idbexercice_meta_delete( ofaIDBExerciceMeta *instance, ofaIDBConnect *connect, gchar **msgerr )
{
	ofaMysqlExerciceMetaPrivate *priv;
	ofaIDBDossierMeta *dossier_meta;
	myISettings *settings;
	const gchar *group, *key_id;
	gchar *key;
	gboolean ok;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	/* remove the period from the settings */
	dossier_meta = ofa_idbexercice_meta_get_dossier_meta( instance );
	settings = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	key_id = ofa_idbexercice_meta_get_settings_id( instance );
	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, key_id );
	my_isettings_remove_key( settings, group, key );
	g_free( key );

	/* drop the database */
	ok = ofa_mysql_connect_drop_database( OFA_MYSQL_CONNECT( connect ), priv->database, msgerr );

	return( ok );
}

static void
idbexercice_meta_dump( const ofaIDBExerciceMeta *instance )
{
	static const gchar *thisfn = "ofa_mysql_exercice_meta_dump";
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	g_debug( "%s: period=%p", thisfn, ( void * ) instance );
	g_debug( "%s:   database=%s", thisfn, priv->database );
}
