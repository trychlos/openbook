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
 * Open Freelance Accounting is distributed in the hdope that it will be
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

#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"

#include "core/ofa-iconcil.h"

/* priv instance data
 */
struct _ofoBatLinePrivate {

	/* dbms data
	 */
	ofxCounter bat_id;						/* bat (imported file) id */
	ofxCounter line_id;						/* line id */
	GDate      deffect;
	GDate      dope;
	gchar     *ref;
	gchar     *label;
	gchar     *currency;
	ofxAmount  amount;
};

static GList       *bat_line_load_dataset( ofxCounter bat_id, ofaHub *hub );
static void         bat_line_set_line_id( ofoBatLine *batline, ofxCounter id );
static gboolean     bat_line_do_insert( ofoBatLine *bat, const ofaIDBConnect *connect );
static gboolean     bat_line_insert_main( ofoBatLine *bat, const ofaIDBConnect *connect );
static void         iconcil_iface_init( ofaIConcilInterface *iface );
static guint        iconcil_get_interface_version( const ofaIConcil *instance );
static ofxCounter   iconcil_get_object_id( const ofaIConcil *instance );
static const gchar *iconcil_get_object_type( const ofaIConcil *instance );

G_DEFINE_TYPE_EXTENDED( ofoBatLine, ofo_bat_line, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoBatLine )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONCIL, iconcil_iface_init ))

static void
bat_line_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_line_finalize";
	ofoBatLinePrivate *priv;

	priv = ofo_bat_line_get_instance_private( OFO_BAT_LINE( instance ));

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->label );

	g_return_if_fail( instance && OFO_IS_BAT_LINE( instance ));

	/* free data members here */
	g_free( priv->ref );
	g_free( priv->label );
	g_free( priv->currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_line_parent_class )->finalize( instance );
}

static void
bat_line_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_BAT_LINE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_line_parent_class )->dispose( instance );
}

static void
ofo_bat_line_init( ofoBatLine *self )
{
	static const gchar *thisfn = "ofo_bat_line_init";
	ofoBatLinePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_bat_line_get_instance_private( self );

	priv->bat_id = OFO_BASE_UNSET_ID;
	priv->line_id = OFO_BASE_UNSET_ID;
	my_date_clear( &priv->deffect );
	my_date_clear( &priv->dope );
}

static void
ofo_bat_line_class_init( ofoBatLineClass *klass )
{
	static const gchar *thisfn = "ofo_bat_line_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_line_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_line_finalize;
}

/**
 * ofo_bat_line_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the list of lines imported in the specified bank account
 * transaction list.
 */
GList *
ofo_bat_line_get_dataset( ofaHub *hub, ofxCounter bat_id )
{
	static const gchar *thisfn = "ofo_bat_line_get_dataset";
	GList *dataset;

	g_debug( "%s: hub=%p, bat_id=%lu", thisfn, ( void * ) hub, bat_id );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	dataset = bat_line_load_dataset( bat_id, hub );

	return( dataset );
}

static GList *
bat_line_load_dataset( ofxCounter bat_id, ofaHub *hub )
{
	const ofaIDBConnect *connect;
	GString *query;
	GSList *result, *irow, *icol;
	GList *dataset;
	ofoBatLine *line;
	GDate date;

	dataset = NULL;
	connect = ofa_hub_get_connect( hub );

	query = g_string_new(
					"SELECT BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,"
					"	BAT_LINE_LABEL,BAT_LINE_REF,BAT_LINE_CURRENCY,"
					"	BAT_LINE_AMOUNT "
					"	FROM OFA_T_BAT_LINES " );

	g_string_append_printf( query, "WHERE BAT_ID=%ld ", bat_id );

	query = g_string_append( query, "ORDER BY BAT_LINE_DEFFECT ASC" );

	if( ofa_idbconnect_query_ex( connect, query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			line = ofo_bat_line_new( bat_id );
			bat_line_set_line_id( line, atol(( gchar * ) icol->data ));
			icol = icol->next;
			my_date_set_from_sql( &date, ( const gchar * ) icol->data );
			ofo_bat_line_set_deffect( line, &date );
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &date, ( const gchar * ) icol->data );
				ofo_bat_line_set_dope( line, &date );
			}
			icol = icol->next;
			ofo_bat_line_set_label( line, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_ref( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_currency( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			ofo_bat_line_set_amount( line,
					my_double_set_from_sql(( const gchar * ) icol->data ));

			ofo_base_set_hub( OFO_BASE( line ), hub );
			dataset = g_list_prepend( dataset, line );
		}
		ofa_idbconnect_free_results( result );
	}
	g_string_free( query, TRUE );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_bat_line_get_bat_id_from_bat_line_id:
 * @hub: the current #ofaHub object.
 * @line_id: the #ofoBatLine identifier.
 *
 * Returns: the BAT file identifier to which the @id line is attached.
 */
ofxCounter
ofo_bat_line_get_bat_id_from_bat_line_id( ofaHub *hub, ofxCounter line_id )
{
	const ofaIDBConnect *connect;
	GString *query;
	GSList *result, *icol;
	ofxCounter bat_id;

	bat_id = 0;
	connect = ofa_hub_get_connect( hub );

	query = g_string_new( "SELECT BAT_ID FROM OFA_T_BAT_LINES " );
	g_string_append_printf( query, "WHERE BAT_LINE_ID=%ld ", line_id );

	if( ofa_idbconnect_query_ex( connect, query->str, &result, TRUE )){
		icol = ( GSList * ) result->data;
		bat_id = atol(( gchar * ) icol->data );
		ofa_idbconnect_free_results( result );
	}
	g_string_free( query, TRUE );

	return( bat_id );
}

/**
 * ofo_bat_line_new:
 */
ofoBatLine *
ofo_bat_line_new( gint bat_id )
{
	ofoBatLine *bat;
	ofoBatLinePrivate *priv;

	bat = g_object_new( OFO_TYPE_BAT_LINE, NULL );
	priv = ofo_bat_line_get_instance_private( bat );
	priv->bat_id = bat_id;

	return( bat );
}

/**
 * ofo_bat_line_get_bat_id:
 */
ofxCounter
ofo_bat_line_get_bat_id( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, OFO_BASE_UNSET_ID );

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->bat_id );
}

/**
 * ofo_bat_line_get_line_id:
 */
ofxCounter
ofo_bat_line_get_line_id( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, OFO_BASE_UNSET_ID );

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->line_id );
}

/**
 * ofo_bat_line_get_deffect:
 */
const GDate *
ofo_bat_line_get_deffect( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const GDate * ) &priv->deffect );
}

/**
 * ofo_bat_line_get_dope:
 */
const GDate *
ofo_bat_line_get_dope( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const GDate * ) &priv->dope );
}

/**
 * ofo_bat_line_get_ref:
 */
const gchar *
ofo_bat_line_get_ref( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const gchar * ) priv->ref );
}

/**
 * ofo_bat_line_get_label:
 */
const gchar *
ofo_bat_line_get_label( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const gchar * ) priv->label );
}

/**
 * ofo_bat_line_get_currency:
 */
const gchar *
ofo_bat_line_get_currency( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const gchar * ) priv->currency );
}

/**
 * ofo_bat_line_get_amount:
 */
ofxAmount
ofo_bat_line_get_amount( const ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->amount );
}

/*
 * bat_line_set_line_id:
 */
static void
bat_line_set_line_id( ofoBatLine *bat, ofxCounter id )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	priv->line_id = id;
}

/**
 * ofo_bat_line_set_deffect:
 */
void
ofo_bat_line_set_deffect( ofoBatLine *bat, const GDate *date )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	my_date_set_from_date( &priv->deffect, date );
}

/**
 * ofo_bat_line_set_dope:
 */
void
ofo_bat_line_set_dope( ofoBatLine *bat, const GDate *date )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	my_date_set_from_date( &priv->dope, date );
}

/**
 * ofo_bat_line_set_ref:
 */
void
ofo_bat_line_set_ref( ofoBatLine *bat, const gchar *ref )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	g_free( priv->ref );
	priv->ref = g_strdup( ref );
}

/**
 * ofo_bat_line_set_label:
 */
void
ofo_bat_line_set_label( ofoBatLine *bat, const gchar *label )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	g_free( priv->label );
	priv->label = g_strdup( label );
}

/**
 * ofo_bat_line_set_currency:
 */
void
ofo_bat_line_set_currency( ofoBatLine *bat, const gchar *currency )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	g_free( priv->currency );
	priv->currency = g_strdup( currency );
}

/**
 * ofo_bat_line_set_amount:
 */
void
ofo_bat_line_set_amount( ofoBatLine *bat, ofxAmount amount )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	priv->amount = amount;
}

/**
 * ofo_bat_line_insert:
 *
 * When inserting a new BAT line, there has not yet been any
 *  reconciliation with an entry; this doesn't worth to try to insert
 *  these fields
 */
gboolean
ofo_bat_line_insert( ofoBatLine *bat_line, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_bat_line_insert";
	ofoDossier *dossier;
	gboolean ok;

	g_debug( "%s: bat=%p, hub=%p",
			thisfn, ( void * ) bat_line, ( void * ) hub );

	g_return_val_if_fail( bat_line && OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat_line )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	dossier = ofa_hub_get_dossier( hub );
	bat_line_set_line_id( bat_line, ofo_dossier_get_next_batline( dossier ));

	if( bat_line_do_insert( bat_line, ofa_hub_get_connect( hub ))){
		ok = TRUE;
	}

	return( ok );
}

static gboolean
bat_line_do_insert( ofoBatLine *bat, const ofaIDBConnect *connect )
{
	return( bat_line_insert_main( bat, connect ));
}

static gboolean
bat_line_insert_main( ofoBatLine *bat, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *str;
	const GDate *dope;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_BAT_LINES" );

	str = my_date_to_str( ofo_bat_line_get_deffect( bat ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,BAT_LINE_REF,"
			"	 BAT_LINE_LABEL,BAT_LINE_CURRENCY,BAT_LINE_AMOUNT) "
			"	VALUES (%ld,%ld,'%s',",
					ofo_bat_line_get_bat_id( bat ),
					ofo_bat_line_get_line_id( bat ),
					str );
	g_free( str );

	dope = ofo_bat_line_get_dope( bat );
	if( my_date_is_valid( dope )){
		str = my_date_to_str( dope, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote_single( ofo_bat_line_get_ref( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = my_utils_quote_single( ofo_bat_line_get_label( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = ( gchar * ) ofo_bat_line_get_currency( bat );
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_double_to_sql( ofo_bat_line_get_amount( bat ));
	g_string_append_printf( query, "%s", str );
	g_free( str );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/*
 * ofaIConcil interface management
 */
static void
iconcil_iface_init( ofaIConcilInterface *iface )
{
	static const gchar *thisfn = "ofo_bat_line_iconcil_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iconcil_get_interface_version;
	iface->get_object_id = iconcil_get_object_id;
	iface->get_object_type = iconcil_get_object_type;
}

static guint
iconcil_get_interface_version( const ofaIConcil *instance )
{
	return( 1 );
}

static ofxCounter
iconcil_get_object_id( const ofaIConcil *instance )
{
	return( ofo_bat_line_get_line_id( OFO_BAT_LINE( instance )));
}

static const gchar *
iconcil_get_object_type( const ofaIConcil *instance )
{
	return( CONCIL_TYPE_BAT );
}
