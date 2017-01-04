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

#include <stdlib.h>

#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"

#include "ofa-mysql-dossier-editor.h"
#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-editor-enter.h"
#include "ofa-mysql-exercice-meta.h"

/* priv instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* server connection infos
	 */
	gchar   *host;
	gchar   *socket;
	guint    port;
	gchar   *root_account;
}
	ofaMysqlDossierMetaPrivate;

#define MYSQL_HOST_KEY                  "mysql-host"
#define MYSQL_SOCKET_KEY                "mysql-socket"
#define MYSQL_PORT_KEY                  "mysql-port"

static void                idbdossier_meta_iface_init( ofaIDBDossierMetaInterface *iface );
static guint               idbdossier_meta_get_interface_version( void );
static void                idbdossier_meta_set_from_settings( ofaIDBDossierMeta *instance );
static void                idbdossier_meta_set_from_editor( ofaIDBDossierMeta *instance, ofaIDBDossierEditor *editor );
static ofaIDBExerciceMeta *idbdossier_meta_new_exercice_meta( ofaIDBDossierMeta *instance );
//static GList                *load_periods( ofaIDBDossierMeta *meta, myISettings *settings, const gchar *group );
//static ofaMysqlExerciceMeta *find_period( ofaMysqlExerciceMeta *period, GList *list );
static void                idbdossier_meta_update_period( ofaIDBDossierMeta *instance, ofaIDBExerciceMeta *period, gboolean current, const GDate *begin, const GDate *end );
static void                idbdossier_meta_remove_period( ofaIDBDossierMeta *instance, ofaIDBExerciceMeta *period );
static void                idbdossier_meta_dump( const ofaIDBDossierMeta *instance );
static void                read_settings( ofaMysqlDossierMeta *self );
static void                write_settings( ofaMysqlDossierMeta *self );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDossierMeta, ofa_mysql_dossier_meta, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlDossierMeta )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBDOSSIER_META, idbdossier_meta_iface_init ))

static void
mysql_dossier_meta_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_dossier_meta_finalize";
	ofaMysqlDossierMetaPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_META( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	priv = ofa_mysql_dossier_meta_get_instance_private( OFA_MYSQL_DOSSIER_META( instance ));

	g_free( priv->host );
	g_free( priv->socket );
	g_free( priv->root_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_meta_parent_class )->finalize( instance );
}

static void
mysql_dossier_meta_dispose( GObject *instance )
{
	ofaMysqlDossierMetaPrivate *priv;

	priv = ofa_mysql_dossier_meta_get_instance_private( OFA_MYSQL_DOSSIER_META( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_meta_parent_class )->dispose( instance );
}

static void
ofa_mysql_dossier_meta_init( ofaMysqlDossierMeta *self )
{
	static const gchar *thisfn = "ofa_mysql_dossier_meta_init";
	ofaMysqlDossierMetaPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_dossier_meta_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->host = NULL;
	priv->port = 0;
	priv->socket = NULL;
	priv->root_account = NULL;
}

static void
ofa_mysql_dossier_meta_class_init( ofaMysqlDossierMetaClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dossier_meta_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_dossier_meta_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_dossier_meta_finalize;
}

/*
 * ofaIDBDossierMeta interface management
 */
static void
idbdossier_meta_iface_init( ofaIDBDossierMetaInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dossier_meta_idbdossier_meta_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbdossier_meta_get_interface_version;
	iface->set_from_settings = idbdossier_meta_set_from_settings;
	iface->set_from_editor = idbdossier_meta_set_from_editor;
	iface->new_exercice_meta = idbdossier_meta_new_exercice_meta;
	iface->update_period = idbdossier_meta_update_period;
	iface->remove_period = idbdossier_meta_remove_period;
	iface->dump = idbdossier_meta_dump;
}

static guint
idbdossier_meta_get_interface_version( void )
{
	return( 1 );
}

static void
idbdossier_meta_set_from_settings( ofaIDBDossierMeta *meta )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ));

	priv = ofa_mysql_dossier_meta_get_instance_private( OFA_MYSQL_DOSSIER_META( meta ));

	g_return_if_fail( !priv->dispose_has_run );

	/* read connection informations from settings */
	read_settings( OFA_MYSQL_DOSSIER_META( meta ));

#if 0
	GList *periods;
	/* reload defined periods */
	periods = load_periods( meta, settings, group );
	ofa_idbdossier_meta_set_periods( meta, periods );
	ofa_idbdossier_meta_free_periods( periods );
}

/*
 * returns the list the defined periods, making its best to reuse
 *  existing references
 */
static GList *
load_periods( ofaIDBDossierMeta *meta, myISettings *settings, const gchar *group )
{
	GList *outlist, *prev_list;
	GList *keys, *itk;
	const gchar *cstr;
	ofaMysqlExerciceMeta *new_period, *exist_period, *period;

	keys = my_isettings_get_keys( settings, group );
	prev_list = ofa_idbdossier_meta_get_periods( meta );
	outlist = NULL;

	for( itk=keys ; itk ; itk=itk->next ){
		cstr = ( const gchar * ) itk->data;
		/* define a new period with the settings */
		new_period = ofa_mysql_exercice_meta_new_from_settings( settings, group, cstr );
		if( new_period ){
			/* search for this period in the previous list */
			exist_period = find_period( new_period, prev_list );
			if( exist_period ){
				period = exist_period;
				g_object_unref( new_period );
			} else {
				period = new_period;
			}
			outlist = g_list_prepend( outlist, period );
		}
	}

	ofa_idbdossier_meta_free_periods( prev_list );
	my_isettings_free_keys( settings, keys );

	return( g_list_reverse( outlist ));
}

static ofaMysqlExerciceMeta *
find_period( ofaMysqlExerciceMeta *period, GList *list )
{
	GList *it;
	ofaMysqlExerciceMeta *current;

	for( it=list ; it ; it=it->next ){
		current = ( ofaMysqlExerciceMeta * ) it->data;
		if( ofa_idbexercice_meta_compare( OFA_IDBEXERCICE_META( current ), OFA_IDBEXERCICE_META( period )) == 0 ){
			return( g_object_ref( current ));
		}
	}

	return( NULL );
#endif
}

static void
idbdossier_meta_set_from_editor( ofaIDBDossierMeta *meta, ofaIDBDossierEditor *editor )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ));
	g_return_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ));

	priv = ofa_mysql_dossier_meta_get_instance_private( OFA_MYSQL_DOSSIER_META( meta ));

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->host );
	priv->host = g_strdup( ofa_mysql_dossier_editor_get_host( OFA_MYSQL_DOSSIER_EDITOR( editor )));
	priv->port = ofa_mysql_dossier_editor_get_port( OFA_MYSQL_DOSSIER_EDITOR( editor ));
	g_free( priv->socket );
	priv->socket = g_strdup( ofa_mysql_dossier_editor_get_socket( OFA_MYSQL_DOSSIER_EDITOR( editor )));
	g_free( priv->root_account );
	priv->root_account = g_strdup( ofa_mysql_dossier_editor_get_remembered_account( OFA_MYSQL_DOSSIER_EDITOR( editor )));

	write_settings( OFA_MYSQL_DOSSIER_META( meta ));
}

/*
 * instanciates a new ofaIDBExerciceMeta object
 */
static ofaIDBExerciceMeta *
idbdossier_meta_new_exercice_meta( ofaIDBDossierMeta *meta )
{
	ofaMysqlExerciceMeta *exercice_meta;

	exercice_meta = ofa_mysql_exercice_meta_new();

	return( OFA_IDBEXERCICE_META( exercice_meta ));
}

static void
idbdossier_meta_update_period( ofaIDBDossierMeta *instance,
		ofaIDBExerciceMeta *period, gboolean current, const GDate *begin, const GDate *end )
{
	myISettings *settings;
	const gchar *group;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_META( instance ));
	g_return_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ));

	settings = ofa_idbdossier_meta_get_settings_iface( instance );
	group = ofa_idbdossier_meta_get_settings_group( instance );
	ofa_mysql_exercice_meta_update( OFA_MYSQL_EXERCICE_META( period ), settings, group, current, begin, end );
}

static void
idbdossier_meta_remove_period( ofaIDBDossierMeta *instance, ofaIDBExerciceMeta *period )
{
	myISettings *settings;
	const gchar *group;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_META( instance ));
	g_return_if_fail( period && OFA_IS_MYSQL_EXERCICE_META( period ));

	settings = ofa_idbdossier_meta_get_settings_iface( instance );
	group = ofa_idbdossier_meta_get_settings_group( instance );
	ofa_mysql_exercice_meta_remove( OFA_MYSQL_EXERCICE_META( period ), settings, group );
}

static void
idbdossier_meta_dump( const ofaIDBDossierMeta *instance )
{
	static const gchar *thisfn = "ofa_mysql_dossier_meta_dump";
	ofaMysqlDossierMetaPrivate *priv;

	priv = ofa_mysql_dossier_meta_get_instance_private( OFA_MYSQL_DOSSIER_META( instance ));

	g_debug( "%s: meta=%p", thisfn, ( void * ) instance );
	g_debug( "%s:   host=%s", thisfn, priv->host );
	g_debug( "%s:   socket=%s", thisfn, priv->socket );
	g_debug( "%s:   port=%u", thisfn, priv->port );
	g_debug( "%s:   root_account=%s", thisfn, priv->root_account );
}

/**
 * ofa_mysql_dossier_meta_new:
 *
 * Returns: a newly allocated #ofaMysqlDossierMeta object, which should
 * be #g_object_unref() by the caller.
 */
ofaMysqlDossierMeta *
ofa_mysql_dossier_meta_new( void )
{
	ofaMysqlDossierMeta *meta;

	meta = g_object_new( OFA_TYPE_MYSQL_DOSSIER_META, NULL );

	return( meta );
}

/**
 * ofa_mysql_dossier_meta_get_host:
 * @meta: this #ofaMysqlDossierMeta object.
 *
 * Returns: the hostname which hosts the dataserver.
 *
 * The returned string is owned by the @meta object, and should not be
 * freed by the caller.
 */
const gchar *
ofa_mysql_dossier_meta_get_host( ofaMysqlDossierMeta *meta )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ), NULL );

	priv = ofa_mysql_dossier_meta_get_instance_private( meta );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->host );
}

/**
 * ofa_mysql_dossier_meta_get_port:
 * @meta: this #ofaMysqlDossierMeta object.
 *
 * Returns: the listening port of the dataserver, or zero for the
 * default value.
 */
guint
ofa_mysql_dossier_meta_get_port( ofaMysqlDossierMeta *meta )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ), 0 );

	priv = ofa_mysql_dossier_meta_get_instance_private( meta );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->port );
}

/**
 * ofa_mysql_dossier_meta_get_socket:
 * @meta: this #ofaMysqlDossierMeta object.
 *
 * Returns: the listening socket path of the dataserver, or %NULL.
 *
 * The returned string is owned by the @meta object, and should not be
 * freed by the caller.
 */
const gchar *
ofa_mysql_dossier_meta_get_socket( ofaMysqlDossierMeta *meta )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ), NULL );

	priv = ofa_mysql_dossier_meta_get_instance_private( meta );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->socket );
}

/**
 * ofa_mysql_dossier_meta_get_root_account:
 * @meta: this #ofaMysqlDossierMeta object.
 *
 * Returns: the root_account of the dataserver (if set), or %NULL.
 *
 * The returned string is owned by the @meta object, and should not be
 * freed by the caller.
 */
const gchar *
ofa_mysql_dossier_meta_get_root_account( ofaMysqlDossierMeta *meta )
{
	ofaMysqlDossierMetaPrivate *priv;

	g_return_val_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ), NULL );

	priv = ofa_mysql_dossier_meta_get_instance_private( meta );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->root_account );
}

/**
 * ofa_mysql_dossier_meta_add_period:
 * @meta: this #ofaMysqlDossierMeta instance.
 * @current: whether the financial period (exercice) is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 * @database: the database name.
 *
 * Defines a new financial period with the provided datas.
 */
void
ofa_mysql_dossier_meta_add_period( ofaMysqlDossierMeta *meta,
							gboolean current, const GDate *begin, const GDate *end, const gchar *database )
{
	ofaMysqlExerciceMeta *period;
	myISettings *settings;
	const gchar *group;

	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	settings = ofa_idbdossier_meta_get_settings_iface( OFA_IDBDOSSIER_META( meta ));
	group = ofa_idbdossier_meta_get_settings_group( OFA_IDBDOSSIER_META( meta ));

	period = ofa_mysql_exercice_meta_new_to_settings( settings, group, current, begin, end, database );
	//ofa_idbdossier_meta_add_period( OFA_IDBDOSSIER_META( meta ), OFA_IDBEXERCICE_META( period ));
	g_object_unref( period );
}

/**
 * ofa_mysql_dossier_meta_set_from_editor:
 * @meta: this #ofaMysqlDossierMeta instance.
 * @dossier_bin: the #ofaMysqlDossierBin dossier editor.
 * @root_bin: the #ofaMysqlRootBin root credentials editor.
 *
 * Defines data from the editor.
 * Writes them into dossier settings.
 */
void
ofa_mysql_dossier_meta_set_from_editor( ofaMysqlDossierMeta *meta, ofaMysqlDossierBin *dossier_bin, ofaMysqlRootBin *root_bin )
{
	ofaMysqlDossierMetaPrivate *priv;
	const gchar *cstr;

	g_return_if_fail( meta && OFA_IS_MYSQL_DOSSIER_META( meta ));
	g_return_if_fail( dossier_bin && OFA_IS_MYSQL_DOSSIER_BIN( dossier_bin ));
	g_return_if_fail( root_bin && OFA_IS_MYSQL_ROOT_BIN( root_bin ));

	priv = ofa_mysql_dossier_meta_get_instance_private( meta );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->host );
	priv->host = NULL;
	cstr = ofa_mysql_dossier_bin_get_host( dossier_bin );
	if( my_strlen( cstr )){
		priv->host = g_strdup( cstr );
	}

	priv->port = ofa_mysql_dossier_bin_get_port( dossier_bin );

	g_free( priv->socket );
	priv->socket = NULL;
	cstr = ofa_mysql_dossier_bin_get_socket( dossier_bin );
	if( my_strlen( cstr )){
		priv->socket = g_strdup( cstr );
	}

	g_free( priv->root_account );
	priv->root_account = NULL;
	cstr = ofa_mysql_root_bin_get_remembered_account( root_bin );
	if( my_strlen( cstr )){
		priv->root_account = g_strdup( cstr );
	}

	write_settings( meta );
}

/*
 * Settings are: "host(s); port(u); socket(s); root_account(s);"
 */
static void
read_settings( ofaMysqlDossierMeta *self )
{
	ofaMysqlDossierMetaPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *group, *cstr;

	priv = ofa_mysql_dossier_meta_get_instance_private( self );

	settings = ofa_idbdossier_meta_get_settings_iface( OFA_IDBDOSSIER_META( self ));
	group = ofa_idbdossier_meta_get_settings_group( OFA_IDBDOSSIER_META( self ));
	strlist = my_isettings_get_string_list( settings, group, "mysql-instance" );

	/* host */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->host );
		priv->host = g_strdup( cstr );
	}

	/* port */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->port = atoi( cstr );
	}

	/* socket */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->socket );
		priv->socket = g_strdup( cstr );
	}

	/* remembered root account */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->root_account );
		priv->root_account = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
write_settings( ofaMysqlDossierMeta *self )
{
	ofaMysqlDossierMetaPrivate *priv;
	myISettings *settings;
	const gchar *group;
	gchar *sport, *str;

	priv = ofa_mysql_dossier_meta_get_instance_private( self );

	sport = priv->port ? g_strdup_printf( "%u", priv->port ) : g_strdup( "" );

	str = g_strdup_printf( "%s;%s;%s;%s;",
			my_strlen( priv->host ) ? priv->host : "",
			sport,
			my_strlen( priv->socket ) ? priv->socket : "",
			my_strlen( priv->root_account ) ? priv->root_account : "" );

	settings = ofa_idbdossier_meta_get_settings_iface( OFA_IDBDOSSIER_META( self ));
	group = ofa_idbdossier_meta_get_settings_group( OFA_IDBDOSSIER_META( self ));

	my_isettings_set_string( settings, group, "mysql-instance", str );

	g_free( sport );
	g_free( str );
}
