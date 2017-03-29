/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hdope that it will be
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
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-iconcil.h"

/* priv instance data
 */
typedef struct {

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
}
	ofoBatLinePrivate;

static GList       *bat_line_load_dataset( ofaIGetter *getter, const gchar *where );
static gchar       *intlist_to_str( GList *list );
static const GDate *bat_line_get_dope( ofoBatLine *bat );
static const gchar *bat_line_get_label( ofoBatLine *bat );
static void         bat_line_set_line_id( ofoBatLine *batline, ofxCounter id );
static gboolean     bat_line_do_insert( ofoBatLine *bat, ofaIGetter *getter );
static gboolean     bat_line_insert_main( ofoBatLine *bat, ofaIGetter *getter );
static void         iconcil_iface_init( ofaIConcilInterface *iface );
static guint        iconcil_get_interface_version( void );
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
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of lines imported in the specified bank account
 * transaction list.
 */
GList *
ofo_bat_line_get_dataset( ofaIGetter *getter, ofxCounter bat_id )
{
	static const gchar *thisfn = "ofo_bat_line_get_dataset";
	GList *dataset;
	gchar *where;

	g_debug( "%s: getter=%p, bat_id=%lu", thisfn, ( void * ) getter, bat_id );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	where = g_strdup_printf( "WHERE BAT_ID=%ld", bat_id );

	dataset = bat_line_load_dataset( getter, where );

	g_free( where );

	return( dataset );
}

/*
 * this function read all columns
 * where and/or order clauses may be provided by the caller
 */
static GList *
bat_line_load_dataset( ofaIGetter *getter, const gchar *where )
{
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query;
	GSList *result, *irow, *icol;
	GList *dataset;
	ofxCounter bat_id;
	ofoBatLine *line;
	GDate date;

	dataset = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"SELECT BAT_ID,BAT_LINE_ID,BAT_LINE_DEFFECT,BAT_LINE_DOPE,"
					"	BAT_LINE_REF,BAT_LINE_LABEL,BAT_LINE_CURRENCY,"
					"	BAT_LINE_AMOUNT "
					"	FROM OFA_T_BAT_LINES %s", where ? where : "" );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			bat_id = atol(( gchar * ) icol->data );
			line = ofo_bat_line_new( getter );
			ofo_bat_line_set_bat_id( line, bat_id );
			icol = icol->next;
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
			if( icol->data ){
				ofo_bat_line_set_ref( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			ofo_bat_line_set_label( line, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_bat_line_set_currency( line, ( gchar * ) icol->data );
			}
			icol = icol->next;
			ofo_bat_line_set_amount( line,
					my_double_set_from_sql(( const gchar * ) icol->data ));

			dataset = g_list_prepend( dataset, line );
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_bat_line_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of BAT identifiers which are referenced but unknwon.
 *
 * The returned list should not be #ofo_bat_line_free_orphans() by
 * the caller.
 */
GList *
ofo_bat_line_get_orphans( ofaIGetter *getter )
{
	ofaHub *hub;
	GList *orphans;
	GSList *result, *irow, *icol;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	hub = ofa_igetter_get_hub( getter );

	if( !ofa_idbconnect_query_ex(
			ofa_hub_get_connect( hub ),
			"SELECT DISTINCT(BAT_ID) FROM OFA_T_BAT_LINES "
			"	WHERE BAT_ID NOT IN (SELECT DISTINCT(BAT_ID) FROM OFA_T_BAT)"
			"	ORDER BY BAT_ID DESC", &result, FALSE )){
		return( NULL );
	}
	orphans = NULL;
	for( irow=result ; irow ; irow=irow->next ){
		icol = irow->data;
		orphans = g_list_prepend( orphans, g_strdup(( gchar * ) icol->data ));
	}
	ofa_idbconnect_free_results( result );

	return( orphans );
}

/**
 * ofo_bat_line_get_dataset_for_print_reconcil:
 * @getter: a #ofaIGetter instance.
 * @account: the reconciliated account.
 * @date: the reconciliation date.
 *
 * Returns: the list of lines on the specified account, unconciliated
 * at the given @date, ordered by ascending effect date.
 *
 * Find suitable BAT file: the first after the given date, or the most
 * recent.
 */
GList *
ofo_bat_line_get_dataset_for_print_reconcil( ofaIGetter *getter,
					const gchar *account_id, const GDate *date, ofxCounter *batid )
{
	static const gchar *thisfn = "ofo_bat_line_get_dataset_for_print_reconcil";
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *bats, *lines, *dataset;
	gchar *sdate, *slist, *query, *where;
	GSList *result, *irow, *icol;
	gboolean ok;
	GDate row_end;
	ofxCounter row_id;

	g_debug( "%s: getter=%p, account_id=%s, date%p, batid=%p",
			thisfn, ( void * ) getter, account_id, ( void * ) date, ( void * ) batid );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( account_id ), NULL );
	g_return_val_if_fail( my_date_is_valid( date ), NULL );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	*batid = -1;
	dataset = NULL;
	sdate = my_date_to_str( date, MY_DATE_SQL );
	bats = NULL;
	lines = NULL;

	/* get the list of candidates BAT files,
	 * stopping at the first after the requested date */
	query = g_strdup_printf(
				"SELECT BAT_ID,BAT_END FROM OFA_T_BAT WHERE BAT_ACCOUNT='%s' ORDER BY BAT_END",
				account_id );
	ok = ofa_idbconnect_query_ex( connect, query, &result, TRUE );
	if( ok ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			row_id = atol(( const gchar * ) icol->data );
			bats = g_list_append( bats, GUINT_TO_POINTER( row_id ));
			if( row_id > *batid ){
				*batid = row_id;
			}
			icol = icol->next;
			my_date_set_from_sql( &row_end, ( const gchar * ) icol->data );
			if( my_date_compare( &row_end, date ) >= 0 ){
				break;
			}
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	/* now get the list of bat lines id's
	 * which are not conciliated
	 * or which have been conciliated after the requested date
	 * NOTE: SELECT ... WHERE ... AND (... OR ( ... AND ...))  is very expensive (~ 2mn)
	 * Just have two requests */
	if( g_list_length( bats )){
		slist = intlist_to_str( bats );
		/* first not yet conciliated */
		query = g_strdup_printf(
					"SELECT BAT_LINE_ID FROM OFA_T_BAT_LINES,OFA_T_CONCIL_IDS "
					"	WHERE BAT_ID IN (%s)"
					"	AND BAT_LINE_ID NOT IN (SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS WHERE REC_IDS_TYPE='B') ",
					slist );
		ok = ofa_idbconnect_query_ex( connect, query, &result, TRUE );
		if( ok ){
			for( irow=result ; irow ; irow=irow->next ){
				icol = ( GSList * ) irow->data;
				row_id = atol(( const gchar * ) icol->data );
				lines = g_list_append( lines, GUINT_TO_POINTER( row_id ));
			}
			ofa_idbconnect_free_results( result );
		}
		g_free( query );

		/* second conciliated after the requested date */
		query = g_strdup_printf(
					"SELECT BAT_LINE_ID FROM OFA_T_BAT_LINES a,OFA_T_CONCIL_IDS b,OFA_T_CONCIL c "
					"	WHERE BAT_ID IN (%s)"
					"	AND BAT_LINE_ID=REC_IDS_OTHER "
					"	AND REC_IDS_TYPE='B' "
					"	AND b.REC_ID=c.REC_ID "
					"	AND REC_DVAL>'%s'",
					slist, sdate );
		ok = ofa_idbconnect_query_ex( connect, query, &result, TRUE );
		if( ok ){
			for( irow=result ; irow ; irow=irow->next ){
				icol = ( GSList * ) irow->data;
				row_id = atol(( const gchar * ) icol->data );
				lines = g_list_append( lines, GUINT_TO_POINTER( row_id ));
			}
			ofa_idbconnect_free_results( result );
		}
		g_free( query );
		g_free( slist );
	}

	/* last get the corresponding bat lines */
	if( g_list_length( lines )){
		slist = intlist_to_str( lines );
		where = g_strdup_printf(" WHERE BAT_LINE_ID IN (%s) ", slist );
		dataset = bat_line_load_dataset( getter, where );
		g_free( where );
		g_free( slist );
	}

	g_list_free( lines );
	g_list_free( bats );
	g_free( sdate );

	return( dataset );
}

static gchar *
intlist_to_str( GList *list )
{
	GString *str;
	GList *it;
	ofxCounter id;

	str = g_string_new( "" );
	for( it=list ; it ; it=it->next ){
		if( str->len ){
			str = g_string_append( str, "," );
		}
		id = ( ofxCounter ) GPOINTER_TO_UINT( it->data );
		g_string_append_printf( str, "%ld", id );
	}

	return( g_string_free( str, FALSE ));
}

/**
 * ofo_bat_line_get_bat_id_from_bat_line_id:
 * @getter: a #ofaIGetter instance.
 * @line_id: the #ofoBatLine identifier.
 *
 * Returns: the BAT file identifier to which the @id line is attached.
 */
ofxCounter
ofo_bat_line_get_bat_id_from_bat_line_id( ofaIGetter *getter, ofxCounter line_id )
{
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	GSList *result, *icol;
	ofxCounter bat_id;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );

	bat_id = 0;
	hub = ofa_igetter_get_hub( getter );
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
 * @getter: a #ofaIGetter instance.
 */
ofoBatLine *
ofo_bat_line_new( ofaIGetter *getter )
{
	ofoBatLine *bat;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bat = g_object_new( OFO_TYPE_BAT_LINE, "ofo-base-getter", getter, NULL );

	return( bat );
}

/**
 * ofo_bat_line_get_bat_id:
 */
ofxCounter
ofo_bat_line_get_bat_id( ofoBatLine *bat )
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
ofo_bat_line_get_line_id( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), OFO_BASE_UNSET_ID );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, OFO_BASE_UNSET_ID );

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->line_id );
}

/**
 * ofo_bat_line_get_deffect:
 *
 * Returns: the effect date.
 */
const GDate *
ofo_bat_line_get_deffect( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return( &priv->deffect );
}

/**
 * ofo_bat_line_get_dope:
 *
 * Returns: the operation date.
 *
 * Defaults to the effect date if operation date is not valid.
 */
const GDate *
ofo_bat_line_get_dope( ofoBatLine *bat )
{
	const GDate *date, *dope;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	dope = bat_line_get_dope( bat );
	date = my_date_is_valid( dope ) ? dope : ofo_bat_line_get_deffect( bat );

	return( date );
}

/*
 * bat_line_get_dope:
 *
 * Returns: the operation date (no default).
 */
static const GDate *
bat_line_get_dope( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	priv = ofo_bat_line_get_instance_private( bat );

	return( &priv->dope );
}

/**
 * ofo_bat_line_get_ref:
 */
const gchar *
ofo_bat_line_get_ref( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_line_get_instance_private( bat );

	return(( const gchar * ) priv->ref );
}

/**
 * ofo_bat_line_get_label:
 *
 * Returns: the label of the BAT line.
 *
 * Defaults to the reference (hoping it is set).
 */
const gchar *
ofo_bat_line_get_label( ofoBatLine *bat )
{
	const gchar *label_orig, *label_out;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	label_orig = bat_line_get_label( bat );
	label_out = my_strlen( label_orig ) ? label_orig : ofo_bat_line_get_ref( bat );

	return( label_out );
}

/*
 * bat_line_get_label:
 *
 * Returns: the label of the BAT line (no default).
 */
static const gchar *
bat_line_get_label( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->label );
}

/**
 * ofo_bat_line_get_currency:
 */
const gchar *
ofo_bat_line_get_currency( ofoBatLine *bat )
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
ofo_bat_line_get_amount( ofoBatLine *bat )
{
	ofoBatLinePrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT_LINE( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	priv = ofo_bat_line_get_instance_private( bat );

	return( priv->amount );
}

/**
 * bat_line_set_bat_id:
 */
void
ofo_bat_line_set_bat_id( ofoBatLine *bat, ofxCounter id )
{
	ofoBatLinePrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT_LINE( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_line_get_instance_private( bat );

	priv->bat_id = id;
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
ofo_bat_line_insert( ofoBatLine *bat_line )
{
	static const gchar *thisfn = "ofo_bat_line_insert";
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	gboolean ok;

	g_debug( "%s: batline=%p", thisfn, ( void * ) bat_line );

	g_return_val_if_fail( bat_line && OFO_IS_BAT_LINE( bat_line ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat_line )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( bat_line ));
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	bat_line_set_line_id( bat_line, ofo_dossier_get_next_batline( dossier ));

	if( bat_line_do_insert( bat_line, getter )){
		ok = TRUE;
	}

	return( ok );
}

static gboolean
bat_line_do_insert( ofoBatLine *bat, ofaIGetter *getter )
{
	return( bat_line_insert_main( bat, getter ));
}

static gboolean
bat_line_insert_main( ofoBatLine *bat, ofaIGetter *getter )
{
	ofaHub *hub;
	GString *query;
	gchar *str;
	const GDate *dope;
	gboolean ok;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	cur_code = ofo_bat_line_get_currency( bat );
	cur_obj = my_strlen( cur_code ) ? ofo_currency_get_by_code( getter, cur_code ) : NULL;
	g_return_val_if_fail( !cur_obj || OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

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

	dope = bat_line_get_dope( bat );
	if( my_date_is_valid( dope )){
		str = my_date_to_str( dope, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote_sql( ofo_bat_line_get_ref( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	str = my_utils_quote_sql( bat_line_get_label( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	if( my_strlen( cur_code )){
		g_string_append_printf( query, "'%s',", cur_code );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = ofa_amount_to_sql( ofo_bat_line_get_amount( bat ), cur_obj );
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
iconcil_get_interface_version( void )
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
