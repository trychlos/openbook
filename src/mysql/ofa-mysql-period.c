/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include <api/my-date.h>
#include <api/my-utils.h>
#include <api/ofa-ifile-period.h>

#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-period.h"

/* priv instance data
 */
struct _ofaMySQLPeriodPrivate {
	gboolean  dispose_has_run;

	/* initialization data
	 */
	GDate     begin;
	GDate     end;
	gboolean  current;
	gchar    *dbname;
};

static void     ifile_period_iface_init( ofaIFilePeriodInterface *iface );
static guint    ifile_period_get_interface_version( const ofaIFilePeriod *instance );
static GDate   *ifile_period_get_begin_date( const ofaIFilePeriod *instance, GDate *date );
static GDate   *ifile_period_get_end_date( const ofaIFilePeriod *instance, GDate *date );
static gboolean ifile_period_get_current( const ofaIFilePeriod *instance );

G_DEFINE_TYPE_EXTENDED( ofaMySQLPeriod, ofa_mysql_period, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IFILE_PERIOD, ifile_period_iface_init ));

static void
mysql_period_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_period_finalize";
	ofaMySQLPeriodPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_PERIOD( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFA_MYSQL_PERIOD( instance )->priv;

	/* free data members here */
	g_free( priv->dbname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_period_parent_class )->finalize( instance );
}

static void
mysql_period_dispose( GObject *instance )
{
	ofaMySQLPeriodPrivate *priv;

	priv = OFA_MYSQL_PERIOD( instance )->priv;

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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_MYSQL_PERIOD, ofaMySQLPeriodPrivate );
}

static void
ofa_mysql_period_class_init( ofaMySQLPeriodClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_period_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_period_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_period_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLPeriodPrivate ));
}

/*
 * ofaIFilePeriod interface management
 */
static void
ifile_period_iface_init( ofaIFilePeriodInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_period_ifile_period_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ifile_period_get_interface_version;
	iface->get_begin_date = ifile_period_get_begin_date;
	iface->get_end_date = ifile_period_get_end_date;
	iface->get_current = ifile_period_get_current;
}

static guint
ifile_period_get_interface_version( const ofaIFilePeriod *instance )
{
	return( 1 );
}

static GDate *
ifile_period_get_begin_date( const ofaIFilePeriod *instance, GDate *date )
{
	ofaMySQLPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PERIOD( instance ), NULL );

	priv = OFA_MYSQL_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( date, &priv->begin );
		return( date );
	}

	return( NULL );
}

static GDate *
ifile_period_get_end_date( const ofaIFilePeriod *instance, GDate *date )
{
	ofaMySQLPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PERIOD( instance ), NULL );

	priv = OFA_MYSQL_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( date, &priv->end );
		return( date );
	}

	return( NULL );
}

static gboolean
ifile_period_get_current( const ofaIFilePeriod *instance )
{
	ofaMySQLPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_PERIOD( instance ), FALSE );

	priv = OFA_MYSQL_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		return( priv->current );
	}

	return( FALSE );
}

/**
 * ofa_mysql_period_new:
 * @current: whether this period is current.
 * @begin: [allow-none]: the beginning date of the period.
 * @end: [allow-none]: the ending date of the period.
 * @database_name: the name of the database for this period.
 *
 * Returns: a new reference to a #ofaMySQLPeriod instance, which should
 * be #g_object_unref() by the caller.
 */
ofaMySQLPeriod *
ofa_mysql_period_new( gboolean current, const GDate *begin, const GDate *end, const gchar *database_name )
{
	ofaMySQLPeriod *period;
	ofaMySQLPeriodPrivate *priv;

	period = g_object_new( OFA_TYPE_MYSQL_PERIOD, NULL );
	priv = period->priv;
	priv->current = current;
	my_date_set_from_date( &priv->begin, begin );
	my_date_set_from_date( &priv->end, end );
	priv->dbname = g_strdup( database_name );

	return( period );
}
