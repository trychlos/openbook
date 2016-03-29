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

#include <glib/gi18n.h>

#include "my/my-date.h"

#include "api/ofa-idbperiod.h"
#include "api/ofa-preferences.h"

/* some data attached to each IDBPeriod instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	GDate    begin;
	GDate    end;
	gboolean current;
}
	sIDBPeriod;

#define IDBPERIOD_LAST_VERSION          1
#define IDBPERIOD_DATA                  "idbperiod-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaIDBPeriodInterface *klass );
static void        interface_base_finalize( ofaIDBPeriodInterface *klass );
static sIDBPeriod *get_idbperiod_data( const ofaIDBPeriod *period );
static void        on_period_finalized( sIDBPeriod *data, GObject *finalized_period );

/**
 * ofa_idbperiod_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbperiod_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbperiod_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbperiod_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBPeriodInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBPeriod", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBPeriodInterface *klass )
{
	static const gchar *thisfn = "ofa_idbperiod_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBPeriodInterface *klass )
{
	static const gchar *thisfn = "ofa_idbperiod_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbperiod_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbperiod_get_interface_last_version( void )
{
	return( IDBPERIOD_LAST_VERSION );
}

/**
 * ofa_idbperiod_get_interface_version:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_idbperiod_get_interface_version( const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_idbperiod_get_interface_version";

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), 0 );

	if( OFA_IDBPERIOD_GET_INTERFACE( period )->get_interface_version ){
		return( OFA_IDBPERIOD_GET_INTERFACE( period )->get_interface_version( period ));
	}

	g_info( "%s: ofaIDBPeriod's %s implementation does not provide 'get_interface_version() method",
			thisfn, G_OBJECT_TYPE_NAME( period ));
	return( 1 );
}

/**
 * ofa_idbperiod_get_begin_date:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: the beginning date of the @period.
 */
const GDate *
ofa_idbperiod_get_begin_date( const ofaIDBPeriod *period )
{
	sIDBPeriod *data;

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );

	data = get_idbperiod_data( period );
	return(( const GDate * ) &data->begin );
}

/**
 * ofa_idbperiod_set_begin_date:
 * @period: this #ofaIDBPeriod instance.
 * @date: the beginning date to be set.
 *
 * Set the beginning date of the @period.
 */
void
ofa_idbperiod_set_begin_date( ofaIDBPeriod *period, const GDate *date )
{
	sIDBPeriod *data;

	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	data = get_idbperiod_data( period );
	my_date_set_from_date( &data->begin, date );
}

/**
 * ofa_idbperiod_get_end_date:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: the ending date of the @period.
 */
const GDate *
ofa_idbperiod_get_end_date( const ofaIDBPeriod *period )
{
	sIDBPeriod *data;

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );

	data = get_idbperiod_data( period );
	return(( const GDate * ) &data->end );
}

/**
 * ofa_idbperiod_set_end_date:
 * @period: this #ofaIDBPeriod instance.
 * @date: the endning date to be set.
 *
 * Set the ending date of the @period.
 */
void
ofa_idbperiod_set_end_date( ofaIDBPeriod *period, const GDate *date )
{
	sIDBPeriod *data;

	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	data = get_idbperiod_data( period );
	my_date_set_from_date( &data->end, date );
}

/**
 * ofa_idbperiod_get_current:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: %TRUE if the financial period is current, i.e. may be
 * modified, %FALSE else.
 */
gboolean
ofa_idbperiod_get_current( const ofaIDBPeriod *period )
{
	sIDBPeriod *data;

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), FALSE );

	data = get_idbperiod_data( period );
	return( data->current );
}

/**
 * ofa_idbperiod_set_current:
 * @period: this #ofaIDBPeriod instance.
 * @current: whether this @period is current.
 *
 * Set the @current flag.
 */
void
ofa_idbperiod_set_current( ofaIDBPeriod *period, gboolean current )
{
	sIDBPeriod *data;

	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	data = get_idbperiod_data( period );
	data->current = current;
}

/**
 * ofa_idbperiod_get_status:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: the localized status string, as a newly allocated string
 * which should be g_free() by the caller.
 *
 * English example:
 * - 'Current' for the currently opened period
 * - 'Archived' for any closed period.
 */
gchar *
ofa_idbperiod_get_status( const ofaIDBPeriod *period )
{
	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );

	return( g_strdup( ofa_idbperiod_get_current( period ) ? _( "Current") : _( "Archived" )));
}

/**
 * ofa_idbperiod_get_label:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: a localized string which describes and qualifies the @period,
 *  as a newly allocated string which should be g_free() by the caller.
 *
 * English example:
 * - 'Current exercice to 31/12/2013' for the currently opened period
 * - 'Archived exercice from 01/01/2012 to 31/12/2012'.
 */
gchar *
ofa_idbperiod_get_label( const ofaIDBPeriod *period )
{
	GString *svalue;
	gchar *sdate;
	const GDate *begin, *end;

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );

	svalue = g_string_new( ofa_idbperiod_get_current( period )
					? _( "Current exercice" )
					: _( "Archived exercice" ));

	begin = ofa_idbperiod_get_begin_date( period );
	if( my_date_is_valid( begin )){
		sdate = my_date_to_str( begin, ofa_prefs_date_display());
		g_string_append_printf( svalue, _( " from %s" ), sdate );
		g_free( sdate );
	}

	end = ofa_idbperiod_get_end_date( period );
	if( my_date_is_valid( end )){
		sdate = my_date_to_str( end, ofa_prefs_date_display());
		g_string_append_printf( svalue, _( " to %s" ), sdate );
		g_free( sdate );
	}

	return( g_string_free( svalue, FALSE ));
}

/**
 * ofa_idbperiod_get_name:
 * @period: this #ofaIDBPeriod instance.
 *
 * Returns: a plugin-specific which qualifies the @period,
 *  as a newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_idbperiod_get_name( const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_idbperiod_get_name";

	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), NULL );

	if( OFA_IDBPERIOD_GET_INTERFACE( period )->get_name ){
		return( OFA_IDBPERIOD_GET_INTERFACE( period )->get_name( period ));
	}

	g_info( "%s: ofaIDBPeriod's %s implementation does not provide 'get_name() method",
			thisfn, G_OBJECT_TYPE_NAME( period ));
	return( NULL );
}

/**
 * ofa_idbperiod_compare:
 * @a: a #ofaIDBPeriod instance.
 * @b: another #ofaIDBPeriod instance.
 *
 * Compare the two periods by their dates.
 *
 * Returns: -1 if @a < @b, +1 if @a > @b, 0 if they are equal.
 */
gint
ofa_idbperiod_compare( const ofaIDBPeriod *a, const ofaIDBPeriod *b )
{
	gint cmp;
	const GDate *a_begin, *a_end, *b_begin, *b_end;

	cmp = 0;

	if( a ){
		a_begin = ofa_idbperiod_get_begin_date( a );
		a_end = ofa_idbperiod_get_end_date( a );

		if( b ){
			b_begin = ofa_idbperiod_get_begin_date( b );
			b_end = ofa_idbperiod_get_end_date( b );

			cmp = my_date_compare_ex( a_begin, b_begin, TRUE );
			if( cmp == 0 ){
				cmp = my_date_compare_ex( a_end, b_end, FALSE );
			}
			if( cmp == 0 ){
				if( OFA_IDBPERIOD_GET_INTERFACE( a )->compare ){
					cmp = OFA_IDBPERIOD_GET_INTERFACE( a )->compare( a, b );
				}
			}
			return( cmp );
		}
		/* if a is set, and b is not set, then a > b */
		return( 1 );

	} else if( b ) {
		/* if a is not set and b is set, then a < b */
		return( -1 );

	} else {
		/* both a and b are unset */
		return( 0 );
	}
}

/**
 * ofa_idbperiod_is_suitable:
 * @period: a #ofaIDBPeriod instance.
 * @begin: [allow-none]: the beginning date of a period.
 * @end: [allow-none]: the ending date of a period.
 *
 * Returns: %TRUE if @period is compatible with @begin and @end.
 */
gboolean
ofa_idbperiod_is_suitable( const ofaIDBPeriod *period, const GDate *begin, const GDate *end )
{
	sIDBPeriod *data;
	gboolean begin_ok, end_ok;

	begin_ok = FALSE;
	end_ok = FALSE;
	data = get_idbperiod_data( period );

	begin_ok = begin ? my_date_compare_ex( begin, &data->begin, TRUE ) : TRUE;
	end_ok = end ? my_date_compare_ex( end, &data->end, FALSE ) : TRUE;

	return( begin_ok && end_ok );
}

/**
 * ofa_idbperiod_dump:
 * @period: this #ofaIDBPeriod instance.
 *
 * Dump the object.
 */
void
ofa_idbperiod_dump( const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_idbperiod_dump";
	sIDBPeriod *data;
	gchar *begin, *end;

	g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

	if( OFA_IDBPERIOD_GET_INTERFACE( period )->dump ){
		OFA_IDBPERIOD_GET_INTERFACE( period )->dump( period );
	}

	data = get_idbperiod_data( period );
	begin = my_date_to_str( &data->begin, MY_DATE_SQL );
	end = my_date_to_str( &data->end, MY_DATE_SQL );

	g_debug( "%s: period=%p (%s)",
			thisfn, ( void * ) period, G_OBJECT_TYPE_NAME( period ));
	g_debug( "%s:   begin=%s", thisfn, begin );
	g_debug( "%s:   end=%s", thisfn, end );
	g_debug( "%s:   current=%s", thisfn, data->current ? "True":"False" );

	g_free( begin );
	g_free( end );
}

static sIDBPeriod *
get_idbperiod_data( const ofaIDBPeriod *period )
{
	sIDBPeriod *data;

	data = ( sIDBPeriod * ) g_object_get_data( G_OBJECT( period ), IDBPERIOD_DATA );

	if( !data ){
		data = g_new0( sIDBPeriod, 1 );
		g_object_set_data( G_OBJECT( period ), IDBPERIOD_DATA, data );
		g_object_weak_ref( G_OBJECT( period ), ( GWeakNotify ) on_period_finalized, data );
	}

	return( data );
}

static void
on_period_finalized( sIDBPeriod *data, GObject *finalized_period )
{
	static const gchar *thisfn = "ofa_idbperiod_on_period_finalized";

	g_debug( "%s: data=%p, finalized_period=%p", thisfn, ( void * ) data, ( void * ) finalized_period );

	g_free( data );
}
