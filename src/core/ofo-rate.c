/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-idataset.h"
#include "api/ofa-iexportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* priv instance data
 */
struct _ofoRatePrivate {

	/* dbms data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
	GList     *validities;
};

static GList           *rate_load_dataset( ofoDossier *dossier, GType type );
static ofoRate         *rate_find_by_mnemo( GList *set, const gchar *mnemo );
static void             rate_set_upd_user( ofoRate *rate, const gchar *user );
static void             rate_set_upd_stamp( ofoRate *rate, const GTimeVal *stamp );
static void             rate_val_add_detail( ofoRate *rate, ofsRateValidity *detail );
static gboolean         rate_do_insert( ofoRate *rate, const ofaDbms *dbms, const gchar *user );
static gboolean         rate_insert_main( ofoRate *rate, const ofaDbms *dbms, const gchar *user );
static gboolean         rate_delete_validities( ofoRate *rate, const ofaDbms *dbms );
static gboolean         rate_insert_validities( ofoRate *rate, const ofaDbms *dbms );
static gboolean         rate_insert_validity( ofoRate *rate, ofsRateValidity *sdet, const ofaDbms *dbms );
static gboolean         rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user );
static gboolean         rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user );
static gboolean         rate_do_delete( ofoRate *rate, const ofaDbms *dbms );
static gint             rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo );
static gint             rate_cmp_by_ptr( const ofoRate *a, const ofoRate *b );
static gint             rate_cmp_by_validity( ofsRateValidity *a, ofsRateValidity *b, gboolean *consistent );
static void             iexportable_iface_init( ofaIExportableInterface *iface );
static guint            iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean         iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier );
static ofoRate         *rate_import_csv_rate( GSList *fields, gint count, gint *errors );
static ofsRateValidity *rate_import_csv_validity( GSList *fields, gint count, gint *errors, gchar **mnemo );
static gboolean         rate_do_drop_content( const ofaDbms *dbms );

G_DEFINE_TYPE_EXTENDED( ofoRate, ofo_rate, OFO_TYPE_BASE, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IEXPORTABLE, iexportable_iface_init ));

OFA_IDATASET_LOAD( RATE, rate );

static void
rate_free_validity( ofsRateValidity *sval )
{
	g_free( sval );
}

static void
rate_free_validities( ofoRate *rate )
{
	g_list_free_full( rate->priv->validities, ( GDestroyNotify ) rate_free_validity );
	rate->priv->validities = NULL;
}

static void
rate_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_rate_finalize";
	ofoRatePrivate *priv;

	priv = OFO_RATE( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	g_return_if_fail( instance && OFO_IS_RATE( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->upd_user );

	rate_free_validities( OFO_RATE( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rate_parent_class )->finalize( instance );
}

static void
rate_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RATE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rate_parent_class )->dispose( instance );
}

static void
ofo_rate_init( ofoRate *self )
{
	static const gchar *thisfn = "ofo_rate_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_RATE, ofoRatePrivate );
}

static void
ofo_rate_class_init( ofoRateClass *klass )
{
	static const gchar *thisfn = "ofo_rate_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_finalize;

	g_type_class_add_private( klass, sizeof( ofoRatePrivate ));
}

static GList *
rate_load_dataset( ofoDossier *dossier, GType type )
{
	const ofaDbms *dbms;
	GSList *result, *irow, *icol;
	ofoRate *rate;
	GList *dataset, *it;
	ofsRateValidity *valid;
	gchar *query;
	GTimeVal timeval;

	dataset = NULL;
	dbms = ofo_dossier_get_dbms( dossier );

	result = ofa_dbms_query_ex( dbms,
			"SELECT RAT_MNEMO,RAT_LABEL,RAT_NOTES,"
			"	RAT_UPD_USER,RAT_UPD_STAMP"
			"	FROM OFA_T_RATES", TRUE );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		rate = ofo_rate_new();
		ofo_rate_set_mnemo( rate, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_rate_set_label( rate, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_rate_set_notes( rate, ( gchar * ) icol->data );
		icol = icol->next;
		rate_set_upd_user( rate, ( gchar * ) icol->data );
		icol = icol->next;
		rate_set_upd_stamp( rate,
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, rate );
	}

	ofa_dbms_free_results( result );

	for( it=dataset ; it ; it=it->next ){

		rate = OFO_RATE( it->data );
		rate->priv->validities = NULL;

		query = g_strdup_printf(
					"SELECT RAT_VAL_BEG,RAT_VAL_END,RAT_VAL_RATE"
					"	FROM OFA_T_RATES_VAL "
					"	WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

		result = ofa_dbms_query_ex( dbms, query, TRUE );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			valid = g_new0( ofsRateValidity, 1 );
			my_date_set_from_sql( &valid->begin, ( const gchar * ) icol->data );
			icol = icol->next;
			my_date_set_from_sql( &valid->end, ( const gchar * ) icol->data );
			icol = icol->next;
			valid->rate = my_double_set_from_sql(( const gchar * ) icol->data );
			rate_val_add_detail( rate, valid );
		}

		ofa_dbms_free_results( result );
		g_free( query );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_rate_get_by_mnemo:
 *
 * Returns: the searched rate, or %NULL.
 *
 * The returned object is owned by the #ofoRate class, and should
 * not be unreffed by the caller.
 */
ofoRate *
ofo_rate_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	OFA_IDATASET_GET( dossier, RATE, rate );

	return( rate_find_by_mnemo( rate_dataset, mnemo ));
}

static ofoRate *
rate_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) rate_cmp_by_mnemo );
	if( found ){
		return( OFO_RATE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_rate_new:
 */
ofoRate *
ofo_rate_new( void )
{
	ofoRate *rate;

	rate = g_object_new( OFO_TYPE_RATE, NULL );

	return( rate );
}

/**
 * ofo_rate_get_mnemo:
 */
const gchar *
ofo_rate_get_mnemo( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return(( const gchar * ) rate->priv->mnemo );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_label:
 */
const gchar *
ofo_rate_get_label( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return(( const gchar * ) rate->priv->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_notes:
 */
const gchar *
ofo_rate_get_notes( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return(( const gchar * ) rate->priv->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_upd_user:
 */
const gchar *
ofo_rate_get_upd_user( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return(( const gchar * ) rate->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_upd_stamp:
 */
const GTimeVal *
ofo_rate_get_upd_stamp( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &rate->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_min_valid:
 *
 * Returns the smallest beginning date, all validities included.
 * The returned #GDate object may be invalid if it is
 * infinite in the past.
 */
const GDate *
ofo_rate_get_min_valid( const ofoRate *rate )
{
	GList *iv;
	const GDate *min;
	ofsRateValidity *sval;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for (min=NULL, iv=rate->priv->validities ; iv ; iv=iv->next ){
			sval = ( ofsRateValidity * ) iv->data;
			if( !min ){
				min = &sval->begin;
			} else if( my_date_compare_ex( &sval->begin, min, TRUE ) < 0 ){
				min = &sval->begin;
			}
		}

		return( min );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_max_valid:
 *
 * Returns the greatest ending date, all validities included.
 * The returned #GDate object may be %NULL or invalid if it is
 * infinite in the future.
 */
const GDate *
ofo_rate_get_max_valid( const ofoRate *rate )
{
	GList *iv;
	const GDate *max;
	ofsRateValidity *sval;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for (max=NULL, iv=rate->priv->validities ; iv ; iv=iv->next ){
			sval = ( ofsRateValidity * ) iv->data;
			if( !max ){
				max = &sval->end;
			} else if( my_date_compare_ex( &sval->end, max, FALSE ) > 0 ){
				max = &sval->end;
			}
		}

		return( max );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_val_count:
 */
gint
ofo_rate_get_val_count( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return( g_list_length( rate->priv->validities ));
	}

	return( 0 );
}

/**
 * ofo_rate_get_val_begin:
 */
const GDate *
ofo_rate_get_val_begin( const ofoRate *rate, gint idx )
{
	GList *nth;
	ofsRateValidity *validity;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			validity = ( ofsRateValidity * ) nth->data;
			return(( const GDate * ) &validity->begin );
		}
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_end:
 */
const GDate *
ofo_rate_get_val_end( const ofoRate *rate, gint idx )
{
	GList *nth;
	ofsRateValidity *validity;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			validity = ( ofsRateValidity * ) nth->data;
			return(( const GDate * ) &validity->end );
		}
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_rate:
 */
ofxAmount
ofo_rate_get_val_rate( const ofoRate *rate, gint idx )
{
	GList *nth;
	ofsRateValidity *validity;

	g_return_val_if_fail( OFO_IS_RATE( rate ), 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			validity = ( ofsRateValidity * ) nth->data;
			return( validity->rate );
		}
	}

	return( 0 );
}

/**
 * ofo_rate_get_rate_at_date:
 * @rate: the #ofoRate object.
 * @date: the date at which we are searching the rate value; this must
 *  be a valid date.
 *
 * Returns the value of the rate at the given date, or zero.
 */
ofxAmount
ofo_rate_get_rate_at_date( const ofoRate *rate, const GDate *date )
{
	GList *iva;
	ofsRateValidity *svalid;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( date, 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for( iva=rate->priv->validities ; iva ; iva=iva->next ){
			svalid = ( ofsRateValidity * ) iva->data;
			if( my_date_compare_ex( &svalid->begin, date, TRUE ) > 0 ){
				continue;
			}
			if( my_date_compare_ex( &svalid->end, date, FALSE ) >= 0 ){
				return( svalid->rate );
			}
		}
	}

	return( 0 );
}

/**
 * ofo_rate_is_deletable:
 *
 * A rate cannot be deleted if it is referenced in the debit or the
 * credit formulas of a model detail line.
 */
gboolean
ofo_rate_is_deletable( const ofoRate *rate, ofoDossier *dossier )
{
	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return( !ofo_ope_template_use_rate( dossier, ofo_rate_get_mnemo( rate )));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_rate_is_valid:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate mnemo or so.
 *
 * In order to check that all provided periods of validity are
 * consistent between each others, we are trying to sort them from the
 * infinite past to the infinite future - if this doesn't work
 * (probably because overlapping each others), then the provided data
 * is not valid
 */
gboolean
ofo_rate_is_valid( const gchar *mnemo, const gchar *label, GList *validities )
{
	gboolean ok;
	gboolean consistent;

	ok = mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 );

	consistent = TRUE;
	validities = g_list_sort_with_data(
			validities, ( GCompareDataFunc ) rate_cmp_by_validity, &consistent );
	ok &= consistent;

	return( ok );
}

/**
 * ofo_rate_set_mnemo:
 */
void
ofo_rate_set_mnemo( ofoRate *rate, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_free( rate->priv->mnemo );
		rate->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_rate_set_label:
 */
void
ofo_rate_set_label( ofoRate *rate, const gchar *label )
{
	g_return_if_fail( OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_free( rate->priv->label );
		rate->priv->label = g_strdup( label );
	}
}

/**
 * ofo_rate_set_notes:
 */
void
ofo_rate_set_notes( ofoRate *rate, const gchar *notes )
{
	g_return_if_fail( OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_free( rate->priv->notes );
		rate->priv->notes = g_strdup( notes );
	}
}

/*
 * ofo_rate_set_upd_user:
 */
static void
rate_set_upd_user( ofoRate *rate, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_free( rate->priv->upd_user );
		rate->priv->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_rate_set_upd_stamp:
 */
static void
rate_set_upd_stamp( ofoRate *rate, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &rate->priv->upd_stamp, upd_stamp );
	}
}

/**
 * ofo_rate_free_all_val:
 *
 * Clear all validities of the rate object.
 * This is normally done just before adding new validities, when
 * preparing for a dbms update.
 */
void
ofo_rate_free_all_val( ofoRate *rate )
{

	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		rate_free_validities( rate );
	}
}

/**
 * ofo_rate_add_val:
 * @rate: the #ofoRate to which the new validity must be added
 * @begin: the beginning of the validity, or %NULL
 * @end: the end of the validity, or %NULL
 * @value: the value of the rate for this validity.
 *
 * Add a validity record to the rate.
 */
void
ofo_rate_add_val( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value )
{
	ofsRateValidity *sval;

	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		sval = g_new0( ofsRateValidity, 1 );
		my_date_set_from_date( &sval->begin, begin );
		my_date_set_from_date( &sval->end, end );
		sval->rate = value;
		rate_val_add_detail( rate, sval );
	}
}

static void
rate_val_add_detail( ofoRate *rate, ofsRateValidity *detail )
{
	rate->priv->validities = g_list_append( rate->priv->validities, detail );
}

/**
 * ofo_rate_insert:
 *
 * First creation of a new rate. This may contain zéro to n validity
 * datail rows. But, if it doesn't, then we take care of removing all
 * previously existing old validity rows.
 */
gboolean
ofo_rate_insert( ofoRate *rate, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_rate_insert";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p", thisfn, ( void * ) rate );

		if( rate_do_insert(
					rate,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, RATE, rate );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_insert( ofoRate *rate, const ofaDbms *dbms, const gchar *user )
{
	return( rate_insert_main( rate, dbms, user ) &&
			rate_delete_validities( rate, dbms ) &&
			rate_insert_validities( rate, dbms ));
}

static gboolean
rate_insert_main( ofoRate *rate, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_rate_get_label( rate ));
	notes = my_utils_quote( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_RATES" );

	g_string_append_printf( query,
			"	(RAT_MNEMO,RAT_LABEL,RAT_NOTES,"
			"	RAT_UPD_USER, RAT_UPD_STAMP) VALUES ('%s','%s',",
			ofo_rate_get_mnemo( rate ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	if( ofa_dbms_query( dbms, query->str, TRUE )){

		rate_set_upd_user( rate, user );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
rate_delete_validities( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
rate_insert_validities( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	GList *idet;
	ofsRateValidity *sdet;

	ok = TRUE;
	for( idet=rate->priv->validities ; idet ; idet=idet->next ){
		sdet = ( ofsRateValidity * ) idet->data;
		ok &= rate_insert_validity( rate, sdet, dbms );
	}

	return( ok );
}

static gboolean
rate_insert_validity( ofoRate *rate, ofsRateValidity *sdet, const ofaDbms *dbms )
{
	gboolean ok;
	GString *query;
	gchar *dbegin, *dend, *amount;

	dbegin = my_date_to_str( &sdet->begin, MY_DATE_SQL );
	dend = my_date_to_str( &sdet->end, MY_DATE_SQL );
	amount = my_double_to_sql( sdet->rate );

	query = g_string_new( "INSERT INTO OFA_T_RATES_VAL " );

	g_string_append_printf( query,
			"	(RAT_MNEMO,"
			"	RAT_VAL_BEG,RAT_VAL_END,RAT_VAL_RATE) "
			"	VALUES ('%s',",
					ofo_rate_get_mnemo( rate ));

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "'%s',", dbegin );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "'%s',", dend );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%s)", amount );

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( dbegin );
	g_free( dend );
	g_free( amount );

	return( ok );
}

/**
 * ofo_rate_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_rate_update( ofoRate *rate, ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_rate_update";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p, prev_mnemo=%s", thisfn, ( void * ) rate, prev_mnemo );

		if( rate_do_update(
					rate,
					prev_mnemo,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_UPDATE( dossier, RATE, rate, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user )
{
	return( rate_update_main( rate, prev_mnemo, dbms, user ) &&
			rate_delete_validities( rate, dbms ) &&
			rate_insert_validities( rate, dbms ));
}

static gboolean
rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_rate_get_label( rate ));
	notes = my_utils_quote( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_RATES SET " );

	g_string_append_printf( query, "RAT_MNEMO='%s',", ofo_rate_get_mnemo( rate ));
	g_string_append_printf( query, "RAT_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "RAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "RAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	RAT_UPD_USER='%s',RAT_UPD_STAMP='%s'"
			"	WHERE RAT_MNEMO='%s'",
					user, stamp_str, prev_mnemo );

	if( ofa_dbms_query( dbms, query->str, TRUE )){

		rate_set_upd_user( rate, user );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_rate_delete:
 */
gboolean
ofo_rate_delete( ofoRate *rate, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_rate_delete";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_rate_is_deletable( rate, dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p", thisfn, ( void * ) rate );

		if( rate_do_delete(
					rate,
					ofo_dossier_get_dbms( dossier ))){

			OFA_IDATASET_REMOVE( dossier, RATE, rate );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_delete( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok &= ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_rate_get_mnemo( a ), mnemo ));
}

static gint
rate_cmp_by_ptr( const ofoRate *a, const ofoRate *b )
{
	return( rate_cmp_by_mnemo( a, ofo_rate_get_mnemo( b )));
}

/*
 * sorting two period of validities
 * setting consistent to %FALSE if the two overlaps each other
 *
 * A period "a" is said lesser than a period "b" if "a" begins before "b".
 * If "a" and "b" begins on the same date, (this is an inconsistent
 * case), then "a" is said lesser than "b" if "a" ends before "b".
 * If "a" and "b" ends on the same date, then periods are said equal.
 *
 *                     +----------------------------------------------+
 *                     |                    "b"                       |
 *                     |         begin                  end           |
 *                     |    set      invalid      set      invalid    |
 * +-------------------+-----------+---------+-----------+------------+
 * | "a" begin set     |   bs-bs   |  bs-bi  |           |            |
 * | "a" begin invalid |   bi-bs   |  bi-bi  |           |            |
 * | "a" end set       |           |         |  es-es    |   es-ei    |
 * | "a" end invalid   |           |         |  ei-es    |   ei-ei    |
 * +-----+-------------+-----------+---------+-----------+------------+
 */
static gint
rate_cmp_by_validity( ofsRateValidity *a, ofsRateValidity *b, gboolean *consistent )
{
	/* does 'a' start from the infinite ? */
	if( !my_date_is_valid( &a->begin )){
		/* 'a' starts from the infinite */
		if( !my_date_is_valid( &b->begin )){
			/* 'bi-bi' case
			 * the two dates start from the infinite: this is not
			 * consistent - compare the end dates */
			if( consistent ){
				*consistent = FALSE;
			}
			if( !my_date_is_valid( &a->end )){
				return( my_date_is_valid( &b->end ) ? -1 : 0 );

			} else if( !my_date_is_valid( &b->end )){
				return( 1 );
			} else {
				return( my_date_compare_ex( &a->end, &b->end, FALSE ));
			}
		}
		/* 'bi-bs' case
		 *  'a' starts from the infinite while 'b-begin' is set
		 * for this be consistant, a must ends before b starts
		 * whatever be the case, 'a' is said lesser than 'b' */
		if( !my_date_is_valid( &a->end ) || my_date_compare_ex( &a->end, &b->begin, TRUE ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( -1 );
	}

	/* a starts from a fixed date */
	if( !my_date_is_valid( &b->begin )){
		/* 'bs-bi' case
		 * 'b' is said lesser than 'a'
		 * for this be consistent, 'b' must ends before 'a' starts */
		if( !my_date_is_valid( &b->end ) || my_date_compare_ex( &b->end, &a->begin, TRUE ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( 1 );
	}

	/* 'bs-bs' case
	 * 'a' and 'b' both starts from a set date: b must ends before 'a' starts */
	if( !my_date_is_valid( &b->end ) || my_date_compare_ex( &b->end, &a->begin, TRUE ) >= 0 ){
		if( consistent ){
			*consistent = FALSE;
		}
	}

	g_return_val_if_fail( my_date_is_valid( &a->begin ), 0 );
	g_return_val_if_fail( my_date_is_valid( &a->end ), 0 );

	return( my_date_compare( &a->begin, &a->end ));
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier )
{
	GList *it, *det;
	GSList *lines;
	gchar *str, *stamp;
	ofoRate *rate;
	const gchar *muser;
	ofsRateValidity *sdet;
	gchar *sbegin, *send, *notes;
	gboolean ok, with_headers;
	gulong count;

	OFA_IDATASET_GET( dossier, RATE, rate );

	with_headers = ofa_export_settings_get_headers( settings );

	count = ( gulong ) g_list_length( rate_dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=rate_dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		count += g_list_length( rate->priv->validities );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "1;Mnemo;Label;Notes;MajUser;MajStamp" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = g_strdup_printf( "2;Mnemo;Begin;End;Rate" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=rate_dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );

		notes = my_utils_export_multi_lines( ofo_rate_get_notes( rate ));
		muser = ofo_rate_get_upd_user( rate );
		stamp = my_utils_stamp_to_str( ofo_rate_get_upd_stamp( rate ), MY_STAMP_YYMDHMS );

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s",
				ofo_rate_get_mnemo( rate ),
				ofo_rate_get_label( rate ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		for( det=rate->priv->validities ; det ; det=det->next ){
			sdet = ( ofsRateValidity * ) det->data;

			sbegin = my_date_to_str( &sdet->begin, MY_DATE_SQL );
			send = my_date_to_str( &sdet->end, MY_DATE_SQL );

			str = g_strdup_printf( "2;%s;%s;%s;%.2lf",
					ofo_rate_get_mnemo( rate ),
					sbegin,
					send,
					sdet->rate );

			g_free( sbegin );
			g_free( send );

			lines = g_slist_prepend( NULL, str );
			ok = ofa_iexportable_export_lines( exportable, lines );
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
			if( !ok ){
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

/**
 * ofo_rate_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - rate mnemo
 * - label
 * - notes (opt)
 *
 * - 2:
 * - rate mnemo
 * - begin validity (opt - yyyy-mm-dd)
 * - end validity (opt - yyyy-mm-dd)
 * - rate
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'rate' records, then all 'validity' records...
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_rate_import_csv( ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_rate_import_csv";
	gint type;
	GSList *ili, *ico;
	ofoRate *rate;
	ofsRateValidity *sdet;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;
	gchar *mnemo;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			ico=ili->data;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty line type", thisfn, count );
				errors += 1;
				continue;
			}
			type = atoi( str );
			switch( type ){
				case 1:
					rate = rate_import_csv_rate( ico, count, &errors );
					if( rate ){
						new_set = g_list_prepend( new_set, rate );
					}
					break;
				case 2:
					mnemo = NULL;
					sdet = rate_import_csv_validity( ico, count, &errors, &mnemo );
					if( sdet ){
						rate = rate_find_by_mnemo( new_set, mnemo );
						if( rate ){
							rate_val_add_detail( rate, sdet );
						}
						g_free( mnemo );
					}
					break;
				default:
					g_warning( "%s: (line %d) invalid line type: %d", thisfn, count, type );
					errors += 1;
					continue;
			}
		}
	}

	if( !errors ){
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_RATE, FALSE );

		rate_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			rate_do_insert(
					OFO_RATE( ise->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		ofa_idataset_free_dataset( dossier, OFO_TYPE_RATE );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_RATE );

		g_list_free( new_set );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_RATE, TRUE );
	}
}

static ofoRate *
rate_import_csv_rate( GSList *fields, gint count, gint *errors )
{
	static const gchar *thisfn = "ofo_rate_import_csv_rate";
	ofoRate *rate;
	const gchar *str;
	GSList *ico;
	gchar *splitted;

	rate = ofo_rate_new();

	/* rate mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_mnemo( rate, str );

	/* rate label */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty label", thisfn, count );
		*errors += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_label( rate, str );

	/* notes
	 * we are tolerant on the last field... */
	ico = ico->next;
	if( ico ){
		str = ( const gchar * ) ico->data;
		if( str && g_utf8_strlen( str, -1 )){
			splitted = my_utils_import_multi_lines( str );
			ofo_rate_set_notes( rate, splitted );
			g_free( splitted );
		}
	}

	return( rate );
}

static ofsRateValidity *
rate_import_csv_validity( GSList *fields, gint count, gint *errors, gchar **mnemo )
{
	static const gchar *thisfn = "ofo_rate_import_csv_validity";
	ofsRateValidity *detail;
	const gchar *str;
	GSList *ico;

	detail = g_new0( ofsRateValidity, 1 );

	/* rate mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( str );

	/* rate begin validity */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	my_date_set_from_sql( &detail->begin, str );

	/* rate end validity */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	my_date_set_from_sql( &detail->end, str );

	/* rate rate */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	detail->rate = g_ascii_strtod( str, NULL );

	return( detail );
}

static gboolean
rate_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_RATES", TRUE ) &&
			ofa_dbms_query( dbms, "DELETE FROM OFA_T_RATES_VAL", TRUE ));
}
