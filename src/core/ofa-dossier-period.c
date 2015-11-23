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

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/ofa-ifile-period.h"

#include "ofa-dossier-period.h"

/* priv instance data
 */
struct _ofaDossierPeriodPrivate {
	gboolean  dispose_has_run;

	/* initialization data
	 */
	//const ofaIFileCollection *collection;

	/* runtime data
	 */
	GDate     begin;
	GDate     end;
	gboolean  current;
};

static void     ifile_period_iface_init( ofaIFilePeriodInterface *iface );
static guint    ifile_period_get_interface_version( const ofaIFilePeriod *instance );
static GDate   *ifile_period_get_begin_date( const ofaIFilePeriod *instance, GDate *date );
static GDate   *ifile_period_get_end_date( const ofaIFilePeriod *instance, GDate *date );
static gboolean ifile_period_get_current( const ofaIFilePeriod *instance );
static gchar   *ifile_period_get_status( const ofaIFilePeriod *instance );

G_DEFINE_TYPE_EXTENDED( ofaDossierPeriod, ofa_dossier_period, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IFILE_PERIOD, ifile_period_iface_init ));

static void
dossier_period_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_period_finalize";

	g_return_if_fail( instance && OFA_IS_DOSSIER_PERIOD( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_period_parent_class )->finalize( instance );
}

static void
dossier_period_dispose( GObject *instance )
{
	ofaDossierPeriodPrivate *priv;

	priv = OFA_DOSSIER_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_period_parent_class )->dispose( instance );
}

static void
ofa_dossier_period_init( ofaDossierPeriod *self )
{
	static const gchar *thisfn = "ofa_dossier_period_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_PERIOD, ofaDossierPeriodPrivate );
}

static void
ofa_dossier_period_class_init( ofaDossierPeriodClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_period_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_period_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_period_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierPeriodPrivate ));
}

/*
 * ofaIFilePeriod interface management
 */
static void
ifile_period_iface_init( ofaIFilePeriodInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_period_ifile_period_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ifile_period_get_interface_version;
	iface->get_begin_date = ifile_period_get_begin_date;
	iface->get_end_date = ifile_period_get_end_date;
	iface->get_current = ifile_period_get_current;
	iface->get_status = ifile_period_get_status;
}

static guint
ifile_period_get_interface_version( const ofaIFilePeriod *instance )
{
	return( 1 );
}

static GDate *
ifile_period_get_begin_date( const ofaIFilePeriod *instance, GDate *date )
{
	ofaDossierPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_PERIOD( instance ), NULL );

	priv = OFA_DOSSIER_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( date, &priv->begin );
		return( date );
	}

	return( NULL );
}

static GDate *
ifile_period_get_end_date( const ofaIFilePeriod *instance, GDate *date )
{
	ofaDossierPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_PERIOD( instance ), NULL );

	priv = OFA_DOSSIER_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( date, &priv->end );
		return( date );
	}

	return( NULL );
}

static gboolean
ifile_period_get_current( const ofaIFilePeriod *instance )
{
	ofaDossierPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_PERIOD( instance ), FALSE );

	priv = OFA_DOSSIER_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		return( priv->current );
	}

	return( FALSE );
}

static gchar *
ifile_period_get_status( const ofaIFilePeriod *instance )
{
	ofaDossierPeriodPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_PERIOD( instance ), NULL );

	priv = OFA_DOSSIER_PERIOD( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->current ? _( "Current" ) : _( "Archived" )));
	}

	return( NULL );
}

/**
 * ofa_dossier_period_new:
 *
 * Returns: a new reference to a #ofaDossierPeriod instance.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
ofaDossierPeriod *
ofa_dossier_period_new( void )
{
	ofaDossierPeriod *period;

	period = g_object_new( OFA_TYPE_DOSSIER_PERIOD, NULL );

	return( period );
}

/**
 * ofa_dossier_period_set_begin_date:
 * @instance: this #ofaDossierPeriod instance.
 * @date: the beginning date of the @instance period.
 *
 * Set the beginning date of the period.
 */
void
ofa_dossier_period_set_begin_date( ofaDossierPeriod *period, const GDate *date )
{
	ofaDossierPeriodPrivate *priv;

	g_return_if_fail( period && OFA_IS_DOSSIER_PERIOD( period ));

	priv = period->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( &priv->begin, date );
	}
}

/**
 * ofa_dossier_period_set_end_date:
 * @instance: this #ofaDossierPeriod instance.
 * @date: the ending date of the @instance period.
 *
 * Set the ending date of the period.
 */
void
ofa_dossier_period_set_end_date( ofaDossierPeriod *period, const GDate *date )
{
	ofaDossierPeriodPrivate *priv;

	g_return_if_fail( period && OFA_IS_DOSSIER_PERIOD( period ));

	priv = period->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( &priv->end, date );
	}
}

/**
 * ofa_dossier_period_set_current:
 * @instance: this #ofaDossierPeriod instance.
 * @current: whether this period is current.
 *
 * Set the current status of the period.
 */
void
ofa_dossier_period_set_current( ofaDossierPeriod *period, gboolean current )
{
	ofaDossierPeriodPrivate *priv;

	g_return_if_fail( period && OFA_IS_DOSSIER_PERIOD( period ));

	priv = period->priv;

	if( !priv->dispose_has_run ){
		priv->current = current;
	}
}
