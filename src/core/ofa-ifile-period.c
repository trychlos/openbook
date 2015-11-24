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

#include <api/ofa-ifile-period.h>

#define IFILE_PERIOD_LAST_VERSION       1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIFilePeriodInterface *klass );
static void  interface_base_finalize( ofaIFilePeriodInterface *klass );

/**
 * ofa_ifile_period_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ifile_period_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ifile_period_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ifile_period_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIFilePeriodInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIFilePeriod", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIFilePeriodInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_period_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIFilePeriodInterface *klass )
{
	static const gchar *thisfn = "ofa_ifile_period_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ifile_period_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ifile_period_get_interface_last_version( void )
{
	return( IFILE_PERIOD_LAST_VERSION );
}

/**
 * ofa_ifile_period_get_interface_version:
 * @period: this #ofaIFilePeriod instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_ifile_period_get_interface_version( const ofaIFilePeriod *period )
{
	g_return_val_if_fail( period && OFA_IS_IFILE_PERIOD( period ), 0 );

	if( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_interface_version ){
		return( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_interface_version( period ));
	}

	return( 1 );
}

/**
 * ofa_ifile_period_get_begin_date:
 * @period: this #ofaIFilePeriod instance.
 * @date: a pointer to a GDate storage space, to set the date.
 *
 * Returns: the same @date pointer, in order the function may be chained.
 */
GDate *
ofa_ifile_period_get_begin_date( const ofaIFilePeriod *period, GDate *date )
{
	g_return_val_if_fail( period && OFA_IS_IFILE_PERIOD( period ), NULL );
	g_return_val_if_fail( date, NULL );

	if( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_begin_date ){
		return( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_begin_date( period, date ));
	}

	return( NULL );
}

/**
 * ofa_ifile_period_get_end_date:
 * @period: this #ofaIFilePeriod instance.
 * @date: a pointer to a GDate storage space, to set the date.
 *
 * Returns: the same @date pointer, in order the function may be chained.
 */
GDate *
ofa_ifile_period_get_end_date( const ofaIFilePeriod *period, GDate *date )
{
	g_return_val_if_fail( period && OFA_IS_IFILE_PERIOD( period ), NULL );
	g_return_val_if_fail( date, NULL );

	if( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_end_date ){
		return( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_end_date( period, date ));
	}

	return( NULL );
}

/**
 * ofa_ifile_period_get_current:
 * @period: this #ofaIFilePeriod instance.
 *
 * Returns: %TRUE if the financier period is current, i.e. may be
 * modified, %FALSE else.
 */
gboolean
ofa_ifile_period_get_current( const ofaIFilePeriod *period )
{
	g_return_val_if_fail( period && OFA_IS_IFILE_PERIOD( period ), FALSE );

	if( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_current ){
		return( OFA_IFILE_PERIOD_GET_INTERFACE( period )->get_current( period ));
	}

	return( FALSE );
}

/**
 * ofa_ifile_period_get_status:
 * @period: this #ofaIFilePeriod instance.
 *
 * Returns: the localized status string, as a newly allocated string
 * which should be g_free() by the caller.
 *
 * English example:
 * - 'Current' for the currently opened period
 * - 'Archived' for any closed period.
 */
gchar *
ofa_ifile_period_get_status( const ofaIFilePeriod *period )
{
	g_return_val_if_fail( period && OFA_IS_IFILE_PERIOD( period ), NULL );

	return( g_strdup( ofa_ifile_period_get_current( period ) ? _( "Current") : _( "Archived" )));
}
