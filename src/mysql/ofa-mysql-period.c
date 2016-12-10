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

#include "api/ofa-idbexercice-meta.h"

#include "ofa-mysql-period.h"

/* priv instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime data
	 */
	gchar    *database;
}
	ofaMySQLPeriodPrivate;

#define MYSQL_DATABASE_KEY_PREFIX       "mysql-db-"

static void            idbperiod_iface_init( ofaIDBExerciceMetaInterface *iface );
static guint           idbperiod_get_interface_version( void );
static gchar          *idbperiod_get_name( const ofaIDBExerciceMeta *instance );
static gint            idbperiod_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b );
static void            idbperiod_dump( const ofaIDBExerciceMeta *instance );
static ofaMySQLPeriod *read_from_settings( myISettings *settings, const gchar *group, const gchar *key );
static void            write_to_settings( ofaMySQLPeriod *period, myISettings *settings, const gchar *group );

G_DEFINE_TYPE_EXTENDED( ofaMySQLPeriod, ofa_mysql_period, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMySQLPeriod )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEXERCICE_META, idbperiod_iface_init ))

static void
mysql_period_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_period_finalize";
	ofaMySQLPeriodPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_PERIOD( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( instance ));

	/* free data members here */
	g_free( priv->database );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_period_parent_class )->finalize( instance );
}

static void
mysql_period_dispose( GObject *instance )
{
	ofaMySQLPeriodPrivate *priv;

	priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_period_parent_class )->dispose( instance );
}

static void
ofa_mysql_period_init( ofaMySQLPeriod *self )
{
	static const gchar *thisfn = "ofa_mysql_period_init";
	ofaMySQLPeriodPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_period_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_period_class_init( ofaMySQLPeriodClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_period_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_period_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_period_finalize;
}

/*
 * ofaIDBExerciceMeta interface management
 */
static void
idbperiod_iface_init( ofaIDBExerciceMetaInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_period_idbperiod_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbperiod_get_interface_version;
	iface->get_name = idbperiod_get_name;
	iface->compare = idbperiod_compare;
	iface->dump = idbperiod_dump;
}

static guint
idbperiod_get_interface_version( void )
{
	return( 1 );
}

static gchar *
idbperiod_get_name( const ofaIDBExerciceMeta *instance )
{
	ofaMySQLPeriodPrivate *priv;

	priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( priv->database ));
}

static gint
idbperiod_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b )
{
	ofaMySQLPeriodPrivate *a_priv, *b_priv;
	gint cmp;

	a_priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( a ));
	b_priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( b ));

	cmp = g_utf8_collate( a_priv->database, b_priv->database );

	return( cmp );
}

static void
idbperiod_dump( const ofaIDBExerciceMeta *instance )
{
	static const gchar *thisfn = "ofa_mysql_period_dump";
	ofaMySQLPeriodPrivate *priv;

	priv = ofa_mysql_period_get_instance_private( OFA_MYSQL_PERIOD( instance ));

	g_debug( "%s: period=%p", thisfn, ( void * ) instance );
	g_debug( "%s:   database=%s", thisfn, priv->database );
}

/**
 * ofa_mysql_period_new_from_settings:
 * @settings: the dossier settings file provided by the application.
 * @group: the settings group name.
 * @key: the key to be examined.
 *
 * Returns: a reference to a new #ofaMySQLPeriod object, which
 * implements the #ofaIDBExerciceMeta interface, if the provided @key is
 * suitable to define a financial period (an exercice), or %NULL.
 *
 * When non null, the returned reference should be #g_object_unref()
 * by the caller.
 */
ofaMySQLPeriod *
ofa_mysql_period_new_from_settings( myISettings *settings, const gchar *group, const gchar *key )
{
	ofaMySQLPeriod *period;

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
static ofaMySQLPeriod *
read_from_settings( myISettings *settings, const gchar *group, const gchar *key )
{
	//static const gchar *thisfn = "ofa_mysql_period_read_from_settings";
	ofaMySQLPeriod *period;
	ofaMySQLPeriodPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;

	period = g_object_new( OFA_TYPE_MYSQL_PERIOD, NULL );

	priv = ofa_mysql_period_get_instance_private( period );

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
 * ofa_mysql_period_new_to_settings:
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
 * Returns: a reference to a new #ofaMySQLPeriod object, which
 * implements the #ofaIDBExerciceMeta interface.
 */
ofaMySQLPeriod *
ofa_mysql_period_new_to_settings( myISettings *settings, const gchar *group,
									gboolean current, const GDate *begin, const GDate *end, const gchar *database )
{
	ofaMySQLPeriod *period;
	ofaMySQLPeriodPrivate *priv;
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

	period = g_object_new( OFA_TYPE_MYSQL_PERIOD, NULL );
	priv = ofa_mysql_period_get_instance_private( period );
	priv->database = g_strdup( database );
	ofa_idbexercice_meta_set_current( OFA_IDBEXERCICE_META( period ), current );
	ofa_idbexercice_meta_set_begin_date( OFA_IDBEXERCICE_META( period ), begin );
	ofa_idbexercice_meta_set_end_date( OFA_IDBEXERCICE_META( period ), end );

	return( period );
}

/**
 * ofa_mysql_period_get_database:
 * @period: this #ofaMySQLPeriod object.
 *
 * Returns: the database name.
 *
 * The returned string is owned by the @period object, and should not
 * be freed by the caller.
 */
const gchar *
ofa_mysql_period_get_database( ofaMySQLPeriod *period )
{
	ofaMySQLPeriodPrivate *priv;

	g_return_val_if_fail( period && OFA_IS_MYSQL_PERIOD( period ), NULL );

	priv = ofa_mysql_period_get_instance_private( period );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->database );
}

/**
 * ofa_mysql_period_update:
 * @period: this #ofaMySQLPeriod object.
 * @settings: the #myISettings object.
 * @group: the group name in the settings.
 * @current: whether the financial period is current.
 * @begin: [allow-none]: the beginning date.
 * @end: [allow-none]: the ending date.
 *
 * Update the dossier settings for this @period with the specified datas.
 */
void
ofa_mysql_period_update( ofaMySQLPeriod *period,
		myISettings *settings, const gchar *group, gboolean current, const GDate *begin, const GDate *end )
{
	ofaMySQLPeriodPrivate *priv;

	priv = ofa_mysql_period_get_instance_private( period );

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
 * ofa_mysql_period_remove:
 * @period: this #ofaMySQLPeriod object.
 * @settings: the #myISettings object.
 * @group: the group name in the settings.
 *
 * Removes the @period from dossier settings.
 */
void
ofa_mysql_period_remove( ofaMySQLPeriod *period, myISettings *settings, const gchar *group )
{
	ofaMySQLPeriodPrivate *priv;
	gchar *key;

	priv = ofa_mysql_period_get_instance_private( period );

	g_return_if_fail( !priv->dispose_has_run );

	key = g_strdup_printf( "%s%s", MYSQL_DATABASE_KEY_PREFIX, priv->database );
	my_isettings_remove_key( settings, group, key );
	g_free( key );
}

static void
write_to_settings( ofaMySQLPeriod *period, myISettings *settings, const gchar *group )
{
	ofaMySQLPeriodPrivate *priv;
	gchar *key, *content, *begin, *end;

	priv = ofa_mysql_period_get_instance_private( period );

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
