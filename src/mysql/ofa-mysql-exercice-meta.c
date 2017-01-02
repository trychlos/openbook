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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"

#include "ofa-mysql-exercice-meta.h"

/* priv instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* initialization
	 */
	gchar    *settings_id;

	/* runtime data
	 */
	gchar    *database;
}
	ofaMysqlExerciceMetaPrivate;

#define MYSQL_DATABASE_KEY_PREFIX       "mysql-db-"

static void            idbexercice_meta_iface_init( ofaIDBExerciceMetaInterface *iface );
static guint           idbexercice_meta_get_interface_version( void );
static void            idbexercice_meta_set_from_settings( ofaIDBExerciceMeta *instance, const gchar *key_id );
static gchar          *idbexercice_meta_get_name( const ofaIDBExerciceMeta *instance );
static gint            idbexercice_meta_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b );
static void            idbexercice_meta_dump( const ofaIDBExerciceMeta *instance );
static ofaMysqlExerciceMeta *read_from_settings( myISettings *settings, const gchar *group, const gchar *key );
static void            read_settings( ofaMysqlExerciceMeta *self );
static void            write_to_settings( ofaMysqlExerciceMeta *period, myISettings *settings, const gchar *group );

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
	g_free( priv->settings_id );
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
	iface->get_name = idbexercice_meta_get_name;
	iface->compare = idbexercice_meta_compare;
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
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( instance ));

	g_free( priv->settings_id );
	priv->settings_id = g_strdup( key_id );

	read_settings( OFA_MYSQL_EXERCICE_META( instance ));
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

	a_priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( a ));
	b_priv = ofa_mysql_exercice_meta_get_instance_private( OFA_MYSQL_EXERCICE_META( b ));

	cmp = g_utf8_collate( a_priv->database, b_priv->database );

	return( cmp );
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
 * ofa_mysql_exercice_meta_new_from_settings:
 * @settings: the dossier settings file provided by the application.
 * @group: the settings group name.
 * @key: the key to be examined.
 *
 * Returns: a reference to a new #ofaMysqlExerciceMeta object, which
 * implements the #ofaIDBExerciceMeta interface, if the provided @key is
 * suitable to define a financial period (an exercice), or %NULL.
 *
 * When non null, the returned reference should be #g_object_unref()
 * by the caller.
 */
ofaMysqlExerciceMeta *
ofa_mysql_exercice_meta_new_from_settings( myISettings *settings, const gchar *group, const gchar *key )
{
	ofaMysqlExerciceMeta *period;

	period = NULL;

	if( g_str_has_prefix( key, MYSQL_DATABASE_KEY_PREFIX )){
		period = read_from_settings( settings, group, key );
	}

	return( period );
}

/*
 * a financial period is set in settings as:
 * key = <PREFIX> + <database_name>
 * string = current / begin / end
 */
static ofaMysqlExerciceMeta *
read_from_settings( myISettings *settings, const gchar *group, const gchar *key )
{
	//static const gchar *thisfn = "ofa_mysql_exercice_meta_read_from_settings";
	ofaMysqlExerciceMeta *period;
	ofaMysqlExerciceMetaPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;

	period = g_object_new( OFA_TYPE_MYSQL_EXERCICE_META, NULL );

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	priv->database = g_strdup( key+my_strlen( MYSQL_DATABASE_KEY_PREFIX ));
	//g_debug( "%s: key=%s, database=%s", thisfn, key, priv->database );

	strlist = my_isettings_get_string_list( settings, group, key );

	/* first element: current as a True/False string */
	it = strlist;
	cstr = it ? it->data : NULL;
	ofa_idbexercice_meta_set_current(
			OFA_IDBEXERCICE_META( period ), my_utils_boolean_from_str( cstr ));

	/* second element: beginning date as YYYYMMDD */
	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_idbexercice_meta_set_begin_date(
				OFA_IDBEXERCICE_META( period ), my_date_set_from_str( &date, cstr, MY_DATE_YYMD ));
	}

	/* third element: ending date as YYYYMMDD */
	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_idbexercice_meta_set_end_date(
				OFA_IDBEXERCICE_META( period ), my_date_set_from_str( &date, cstr, MY_DATE_YYMD ));
	}

	if( strlist ){
		my_isettings_free_string_list( settings, strlist );
	}

	return( period );
}

/**
 * ofa_mysql_exercice_meta_new_to_settings:
 * @settings: the #myISettings instance which holds the dossier settings
 *  file.
 * @group: the group name for this dossier.
 * @current: whether the financial period is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 * @database: the database name.
 *
 * Defines a new financial period in the dossier settings
 *
 * Returns: a reference to a new #ofaMysqlExerciceMeta object, which
 * implements the #ofaIDBExerciceMeta interface.
 */
ofaMysqlExerciceMeta *
ofa_mysql_exercice_meta_new_to_settings( myISettings *settings, const gchar *group,
									gboolean current, const GDate *begin, const GDate *end, const gchar *database )
{
	ofaMysqlExerciceMeta *period;
	ofaMysqlExerciceMetaPrivate *priv;
	gchar *key, *sbegin, *send, *content;

	g_return_val_if_fail( settings && MY_IS_ISETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( database ), NULL );

	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, database );
	sbegin = my_date_to_str( begin, MY_DATE_YYMD );
	send = my_date_to_str( end, MY_DATE_YYMD );
	content = g_strdup_printf( "%s;%s;%s;", current ? "True":"False", sbegin, send );

	my_isettings_set_string( settings, group, key, content );

	g_free( content );
	g_free( send );
	g_free( sbegin );
	g_free( key );

	period = g_object_new( OFA_TYPE_MYSQL_EXERCICE_META, NULL );
	priv = ofa_mysql_exercice_meta_get_instance_private( period );
	priv->database = g_strdup( database );
	ofa_idbexercice_meta_set_current( OFA_IDBEXERCICE_META( period ), current );
	ofa_idbexercice_meta_set_begin_date( OFA_IDBEXERCICE_META( period ), begin );
	ofa_idbexercice_meta_set_end_date( OFA_IDBEXERCICE_META( period ), end );

	return( period );
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
 * ofa_mysql_exercice_meta_update:
 * @period: this #ofaMysqlExerciceMeta object.
 * @settings: the #myISettings object.
 * @group: the group name in the settings.
 * @current: whether the financial period is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Update the dossier settings for this @period with the specified datas.
 */
void
ofa_mysql_exercice_meta_update( ofaMysqlExerciceMeta *period,
		myISettings *settings, const gchar *group, gboolean current, const GDate *begin, const GDate *end )
{
	ofaMysqlExerciceMetaPrivate *priv;

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	g_return_if_fail( !priv->dispose_has_run );

	/* we update the internal data of the object through this is
	 * pretty useless as writing into dossier settings file will
	 * trigger a reload of all data (through the myFileMonitor) */
	ofa_idbexercice_meta_set_current( OFA_IDBEXERCICE_META( period ), current );
	ofa_idbexercice_meta_set_begin_date( OFA_IDBEXERCICE_META( period ), begin );
	ofa_idbexercice_meta_set_end_date( OFA_IDBEXERCICE_META( period ), end );

	/* next update the settings */
	write_to_settings( period, settings, group );
}

/**
 * ofa_mysql_exercice_meta_remove:
 * @period: this #ofaMysqlExerciceMeta object.
 * @settings: the #myISettings object.
 * @group: the group name in the settings.
 *
 * Removes the @period from dossier settings.
 */
void
ofa_mysql_exercice_meta_remove( ofaMysqlExerciceMeta *period, myISettings *settings, const gchar *group )
{
	ofaMysqlExerciceMetaPrivate *priv;
	gchar *key;

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	g_return_if_fail( !priv->dispose_has_run );

	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, priv->database );
	my_isettings_remove_key( settings, group, key );
	g_free( key );
}

/*
 * Settings are: "database(s);"
 */
static void
read_settings( ofaMysqlExerciceMeta *self )
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
	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, priv->settings_id );

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
write_to_settings( ofaMysqlExerciceMeta *period, myISettings *settings, const gchar *group )
{
	ofaMysqlExerciceMetaPrivate *priv;
	gchar *key, *content, *begin, *end;

	priv = ofa_mysql_exercice_meta_get_instance_private( period );

	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, priv->database );

	begin = my_date_to_str( ofa_idbexercice_meta_get_begin_date( OFA_IDBEXERCICE_META( period )), MY_DATE_YYMD );
	end = my_date_to_str( ofa_idbexercice_meta_get_end_date( OFA_IDBEXERCICE_META( period )), MY_DATE_YYMD );
	content = g_strdup_printf( "%s;%s;%s;",
					ofa_idbexercice_meta_get_current( OFA_IDBEXERCICE_META( period )) ? "True":"False",
					begin, end );
	my_isettings_set_string( settings, group, key, content );

	g_free( begin );
	g_free( end );
	g_free( content );
	g_free( key );
}
