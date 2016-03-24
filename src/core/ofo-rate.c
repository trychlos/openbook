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
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* priv instance data
 */
enum {
	RAT_MNEMO = 1,
	RAT_LABEL,
	RAT_NOTES,
	RAT_UPD_USER,
	RAT_UPD_STAMP,
	RAT_VAL_ROW,
	RAT_VAL_BEGIN,
	RAT_VAL_END,
	RAT_VAL_RATE,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( RAT_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( RAT_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( RAT_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( RAT_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( RAT_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_validity_defs[] = {
		{ RAT_MNEMO,
				"RAT_MNEMO",
				NULL,
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ RAT_VAL_ROW,
				"RAT_VAL_ROW",
				"RatValidityRow",
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ RAT_VAL_BEGIN,
				"RAT_VAL_BEGIN",
				"RatValidityBegin",
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ RAT_VAL_END,
				"RAT_VAL_END",
				"RatValidityEnd",
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ RAT_VAL_RATE,
				"RAT_VAL_RATE",
				"RatRate",
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ 0 }
};

typedef struct {

	/* the validities of the rate as a GList of GList fields
	 */
	GList *validities;
}
	ofoRatePrivate;

static ofoRate  *rate_find_by_mnemo( GList *set, const gchar *mnemo );
static void      rate_set_upd_user( ofoRate *rate, const gchar *user );
static void      rate_set_upd_stamp( ofoRate *rate, const GTimeVal *stamp );
static GList    *rate_val_new_detail( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value );
static void      rate_val_add_detail( ofoRate *rate, GList *detail );
static gboolean  rate_do_insert( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_main( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_delete_validities( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_validities( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_validity( ofoRate *rate, GList *detail, gint count, const ofaIDBConnect *connect );
static gboolean  rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean  rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean  rate_do_delete( ofoRate *rate, const ofaIDBConnect *connect );
static gint      rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo );
static gint      rate_cmp_by_ptr( const ofoRate *a, const ofoRate *b );
static gint      rate_cmp_by_validity( const ofsRateValidity *a, const ofsRateValidity *b, gboolean *consistent );
static void      icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint     icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList    *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void      iexportable_iface_init( ofaIExportableInterface *iface );
static guint     iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean  iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void      iimportable_iface_init( ofaIImportableInterface *iface );
static guint     iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean  iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub );
static ofoRate  *rate_import_csv_rate( ofaIImportable *exportable, GSList *fields, const ofaFileFormat *settings, guint count, guint *errors );
static GList    *rate_import_csv_validity( ofaIImportable *exportable, GSList *fields, const ofaFileFormat *settings, guint count, guint *errors, gchar **mnemo );
static gboolean  rate_do_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoRate, ofo_rate, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRate )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

static void
rate_free_validity( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
rate_free_validities( ofoRate *rate )
{
	ofoRatePrivate *priv;

	priv = ofo_rate_get_instance_private( rate );

	g_list_free_full( priv->validities, ( GDestroyNotify ) rate_free_validity );
	priv->validities = NULL;
}

static void
rate_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_rate_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, RAT_MNEMO ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, RAT_LABEL ));

	g_return_if_fail( instance && OFO_IS_RATE( instance ));

	/* free data members here */

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
}

static void
ofo_rate_class_init( ofoRateClass *klass )
{
	static const gchar *thisfn = "ofo_rate_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_finalize;
}

/**
 * ofo_rate_connect_to_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_rate_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_rate_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}

/**
 * ofo_rate_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoRate dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_rate_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_RATE ));
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
ofo_rate_get_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	dataset = ofo_rate_get_dataset( hub );

	return( rate_find_by_mnemo( dataset, mnemo ));
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
	OFO_BASE( rate )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( rate );
}

/**
 * ofo_rate_get_mnemo:
 */
const gchar *
ofo_rate_get_mnemo( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_MNEMO );
}

/**
 * ofo_rate_get_label:
 */
const gchar *
ofo_rate_get_label( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_LABEL );
}

/**
 * ofo_rate_get_notes:
 */
const gchar *
ofo_rate_get_notes( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_NOTES );
}

/**
 * ofo_rate_get_upd_user:
 */
const gchar *
ofo_rate_get_upd_user( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_UPD_USER );
}

/**
 * ofo_rate_get_upd_stamp:
 */
const GTimeVal *
ofo_rate_get_upd_stamp( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, timestamp, NULL, RAT_UPD_STAMP );
}

/**
 * ofo_rate_get_min_valid:
 *
 * Returns the smallest beginning date, all validities included.
 * If the returned #GDate struct is invalid, then it must be considered
 * as infinite in the past.
 */
const GDate *
ofo_rate_get_min_valid( const ofoRate *rate )
{
	ofoRatePrivate *priv;
	GList *it;
	const GDate *val_begin, *min;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), NULL );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, NULL );

	priv = ofo_rate_get_instance_private( rate );

	for( min=NULL, it=priv->validities ; it ; it=it->next ){
		val_begin = ofa_box_get_date( it->data, RAT_VAL_BEGIN );
		if( !min ){
			min = val_begin;
		} else if( my_date_compare_ex( val_begin, min, TRUE ) < 0 ){
			min = val_begin;
		}
	}

	return( min );
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
	ofoRatePrivate *priv;
	GList *it;
	const GDate *val_end, *max;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), NULL );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, NULL );

	priv = ofo_rate_get_instance_private( rate );

	for( max=NULL, it=priv->validities ; it ; it=it->next ){
		val_end = ofa_box_get_date( it->data, RAT_VAL_END );
		if( !max ){
			max = val_end;
		} else if( my_date_compare_ex( val_end, max, FALSE ) > 0 ){
			max = val_end;
		}
	}

	return( max );
}

/**
 * ofo_rate_get_val_count:
 *
 * Returns: the count of validity rows for this @rate
 */
gint
ofo_rate_get_val_count( const ofoRate *rate )
{
	ofoRatePrivate *priv;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, 0 );

	priv = ofo_rate_get_instance_private( rate );

	return( g_list_length( priv->validities ));
}

/**
 * ofo_rate_get_val_begin:
 */
const GDate *
ofo_rate_get_val_begin( const ofoRate *rate, gint idx )
{
	ofoRatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), NULL );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, NULL );

	priv = ofo_rate_get_instance_private( rate );

	nth = g_list_nth( priv->validities, idx );
	if( nth ){
		return( ofa_box_get_date( nth->data, RAT_VAL_BEGIN ));
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_end:
 */
const GDate *
ofo_rate_get_val_end( const ofoRate *rate, gint idx )
{
	ofoRatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), NULL );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, NULL );

	priv = ofo_rate_get_instance_private( rate );

	nth = g_list_nth( priv->validities, idx );
	if( nth ){
		return( ofa_box_get_date( nth->data, RAT_VAL_END ));
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_rate:
 */
ofxAmount
ofo_rate_get_val_rate( const ofoRate *rate, gint idx )
{
	ofoRatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, 0 );

	priv = ofo_rate_get_instance_private( rate );

	nth = g_list_nth( priv->validities, idx );
	if( nth ){
		return( ofa_box_get_amount( nth->data, RAT_VAL_RATE ));
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
	ofoRatePrivate *priv;
	GList *it;
	const GDate *val_begin, *val_end;
	ofxAmount amount;
	gchar *sdate, *sbegin, *send;
	gint cmp;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( date, 0 );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, 0 );

	priv = ofo_rate_get_instance_private( rate );

	for( it=priv->validities ; it ; it=it->next ){
		val_begin = ofa_box_get_date( it->data, RAT_VAL_BEGIN );
		val_end = ofa_box_get_date( it->data, RAT_VAL_END );
		sdate = my_date_to_str( date, ofa_prefs_date_display());
		sbegin = my_date_to_str( val_begin, ofa_prefs_date_display());
		send = my_date_to_str( val_end, ofa_prefs_date_display());
		g_debug( "ofo_rate_get_rate_at_date: date=%s, begin=%s, end=%s", sdate, sbegin, send );
		g_free( sdate );
		g_free( sbegin );
		g_free( send );
		cmp = my_date_compare_ex( val_begin, date, TRUE );
		g_debug( "my_date_compare_ex( val_begin, date, TRUE ): cmp=%d", cmp );
		if( cmp > 0 ){
			continue;
		}
		cmp = my_date_compare_ex( val_end, date, FALSE );
		g_debug( "my_date_compare_ex( val_end, date, FALSE ): cmp=%d", cmp );
		if( cmp >= 0 ){
			amount = ofa_box_get_amount( it->data, RAT_VAL_RATE );
			/*g_debug( "amount=%.5lf", amount );*/
			return( amount );
		}
	}

	return( 0 );
}

/**
 * ofo_rate_is_deletable:
 * @rate: the rate
 *
 * A rate cannot be deleted if it is referenced in the debit or the
 * credit formulas of a model detail line.
 *
 * Returns: %TRUE if the rate is deletable.
 */
gboolean
ofo_rate_is_deletable( const ofoRate *rate )
{
	ofaHub *hub;
	gboolean deletable;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( rate ));

	deletable = !ofo_ope_template_use_rate( hub, ofo_rate_get_mnemo( rate ));

	deletable &= ofa_idbmodel_get_is_deletable( hub, OFO_BASE( rate ));

	return( deletable );
}

/**
 * ofo_rate_is_valid_data:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate mnemo or so.
 *
 * In order to check that all provided periods of validity are
 * consistent between each others, we are trying to sort them from the
 * infinite past to the infinite future - if this doesn't work
 * (probably because overlapping each others), then the provided data
 * is considered as not valid
 */
gboolean
ofo_rate_is_valid_data( const gchar *mnemo, const gchar *label, GList *validities, gchar **msgerr )
{
	gboolean consistent;

	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( mnemo )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty mnemonic" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
		}
		return( FALSE );
	}

	consistent = TRUE;
	validities = g_list_sort_with_data(
			validities, ( GCompareDataFunc ) rate_cmp_by_validity, &consistent );
	if( !consistent ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Validities are not consistent" ));
		}
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_rate_set_mnemo:
 */
void
ofo_rate_set_mnemo( ofoRate *rate, const gchar *mnemo )
{
	ofo_base_setter( RATE, rate, string, RAT_MNEMO, mnemo );
}

/**
 * ofo_rate_set_label:
 */
void
ofo_rate_set_label( ofoRate *rate, const gchar *label )
{
	ofo_base_setter( RATE, rate, string, RAT_LABEL, label );
}

/**
 * ofo_rate_set_notes:
 */
void
ofo_rate_set_notes( ofoRate *rate, const gchar *notes )
{
	ofo_base_setter( RATE, rate, string, RAT_NOTES, notes );
}

/*
 * ofo_rate_set_upd_user:
 */
static void
rate_set_upd_user( ofoRate *rate, const gchar *upd_user )
{
	ofo_base_setter( RATE, rate, string, RAT_UPD_USER, upd_user );
}

/*
 * ofo_rate_set_upd_stamp:
 */
static void
rate_set_upd_stamp( ofoRate *rate, const GTimeVal *upd_stamp )
{
	ofo_base_setter( RATE, rate, timestamp, RAT_UPD_STAMP, upd_stamp );
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
	g_return_if_fail( !OFO_BASE( rate )->prot->dispose_has_run );

	rate_free_validities( rate );
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
	GList *fields;

	g_return_if_fail( rate && OFO_IS_RATE( rate ));
	g_return_if_fail( !OFO_BASE( rate )->prot->dispose_has_run );

	fields = rate_val_new_detail( rate, begin, end, value );
	rate_val_add_detail( rate, fields );
}

static GList *
rate_val_new_detail( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value )
{
	GList *fields;

	fields = ofa_box_init_fields_list( st_validity_defs );
	ofa_box_set_string( fields, RAT_MNEMO, ofo_rate_get_mnemo( rate ));
	ofa_box_set_int( fields, RAT_VAL_ROW, 1+ofo_rate_get_val_count( rate ));
	ofa_box_set_date( fields, RAT_VAL_BEGIN, begin );
	ofa_box_set_date( fields, RAT_VAL_END, end );
	g_debug( "ofo_rate_val_new_detail: ofa_box_set_amount with value=%.5lf", value );
	ofa_box_set_amount( fields, RAT_VAL_RATE, value );

	return( fields );
}

static void
rate_val_add_detail( ofoRate *rate, GList *detail )
{
	ofoRatePrivate *priv;

	priv = ofo_rate_get_instance_private( rate );

	priv->validities = g_list_append( priv->validities, detail );
}

/**
 * ofo_rate_insert:
 *
 * First creation of a new rate. This may contain zéro to n validity
 * datail rows. But, if it doesn't, then we take care of removing all
 * previously existing old validity rows.
 */
gboolean
ofo_rate_insert( ofoRate *rate, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_rate_insert";
	gboolean ok;

	g_debug( "%s: rate=%p, hub=%p",
			thisfn, ( void * ) rate, ( void * ) hub );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( rate_do_insert( rate, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( rate ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( rate ), ( GCompareFunc ) rate_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, rate );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rate_do_insert( ofoRate *rate, const ofaIDBConnect *connect )
{
	return( rate_insert_main( rate, connect ) &&
			rate_delete_validities( rate, connect ) &&
			rate_insert_validities( rate, connect ));
}

static gboolean
rate_insert_main( ofoRate *rate, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_rate_get_label( rate ));
	notes = my_utils_quote_single( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_RATES" );

	g_string_append_printf( query,
			"	(RAT_MNEMO,RAT_LABEL,RAT_NOTES,"
			"	RAT_UPD_USER, RAT_UPD_STAMP) VALUES ('%s','%s',",
			ofo_rate_get_mnemo( rate ),
			label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		rate_set_upd_user( rate, userid );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

static gboolean
rate_delete_validities( ofoRate *rate, const ofaIDBConnect *connect )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
rate_insert_validities( ofoRate *rate, const ofaIDBConnect *connect )
{
	ofoRatePrivate *priv;
	gboolean ok;
	GList *it;
	gint count;

	priv = ofo_rate_get_instance_private( rate );
	ok = TRUE;
	count = 0;

	for( it=priv->validities ; it ; it=it->next ){
		ok &= rate_insert_validity( rate, it->data, ++count, connect );
	}

	return( ok );
}

static gboolean
rate_insert_validity( ofoRate *rate, GList *fields, gint count, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	const GDate *dbegin, *dend;
	ofxAmount amount;
	gchar *sdbegin, *sdend, *samount;

	dbegin = ofa_box_get_date( fields, RAT_VAL_BEGIN );
	sdbegin = my_date_to_str( dbegin, MY_DATE_SQL );

	dend = ofa_box_get_date( fields, RAT_VAL_END );
	sdend = my_date_to_str( dend, MY_DATE_SQL );

	amount = ofa_box_get_amount( fields, RAT_VAL_RATE );
	samount = my_double_to_sql( amount );

	query = g_string_new( "INSERT INTO OFA_T_RATES_VAL " );

	g_string_append_printf( query,
			"	(RAT_MNEMO,RAT_VAL_ROW,"
			"	RAT_VAL_BEGIN,RAT_VAL_END,RAT_VAL_RATE) "
			"	VALUES ('%s',%d,",
					ofo_rate_get_mnemo( rate ),
					count );

	if( my_strlen( sdbegin )){
		g_string_append_printf( query, "'%s',", sdbegin );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( sdend )){
		g_string_append_printf( query, "'%s',", sdend );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%s)", samount );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( sdbegin );
	g_free( sdend );
	g_free( samount );

	return( ok );
}

/**
 * ofo_rate_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_rate_update( ofoRate *rate, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_rate_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: rate=%p, prev_mnemo=%s",
			thisfn, ( void * ) rate, prev_mnemo );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( rate ));
	ok = FALSE;

	if( rate_do_update( rate, prev_mnemo, ofa_hub_get_connect( hub ))){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_RATE, ( GCompareFunc ) rate_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, rate, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect )
{
	return( rate_update_main( rate, prev_mnemo, connect ) &&
			rate_delete_validities( rate, connect ) &&
			rate_insert_validities( rate, connect ));
}

static gboolean
rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_rate_get_label( rate ));
	notes = my_utils_quote_single( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_RATES SET " );

	g_string_append_printf( query, "RAT_MNEMO='%s',", ofo_rate_get_mnemo( rate ));
	g_string_append_printf( query, "RAT_LABEL='%s',", label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "RAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "RAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	RAT_UPD_USER='%s',RAT_UPD_STAMP='%s'"
			"	WHERE RAT_MNEMO='%s'",
					userid, stamp_str, prev_mnemo );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		rate_set_upd_user( rate, userid );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_rate_delete:
 */
gboolean
ofo_rate_delete( ofoRate *rate )
{
	static const gchar *thisfn = "ofo_rate_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: rate=%p", thisfn, ( void * ) rate );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( rate ));
	ok = FALSE;

	if( rate_do_delete( rate, ofa_hub_get_connect( hub ))){
		g_object_ref( rate );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( rate ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, rate );
		g_object_unref( rate );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rate_do_delete( ofoRate *rate, const ofaIDBConnect *connect )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok &= ofa_idbconnect_query( connect, query, TRUE );

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
rate_cmp_by_validity( const ofsRateValidity *a, const ofsRateValidity *b, gboolean *consistent )
{
	gint cmp;

	g_return_val_if_fail( a, 0 );
	g_return_val_if_fail( b, 0 );
	g_return_val_if_fail( consistent, 0 );

	/* first deal with cases of inconsistency
	 * only one period may have begin/end unset */
	if( !my_date_is_valid( &a->begin ) && !my_date_is_valid( &b->begin )){
		*consistent = FALSE;
		return( my_date_compare_ex( &a->end, &b->end, FALSE ));
	}
	if( !my_date_is_valid( &a->end ) && !my_date_is_valid( &b->end )){
		*consistent = FALSE;
		return( my_date_compare_ex( &a->begin, &b->begin, TRUE ));
	}

	/* does 'a' start from the infinite ? */
	if( !my_date_is_valid( &a->begin )){
		/* 'a' starts from the infinite => 'b' begin is set */
		/* in order to be consistent, a_end must be set before b_begin */
		if( !my_date_is_valid( &a->end ) || my_date_compare_ex( &a->end, &b->begin, TRUE ) >= 0 ){
			*consistent = FALSE;
		}
		return( -1 );
	}

	/* does 'b' start from the infinite ? */
	if( !my_date_is_valid( &b->begin )){
		/* 'bs-bi' case
		 * 'b' is said lesser than 'a'
		 * for this be consistent, 'b' must ends before 'a' starts */
		if( !my_date_is_valid( &b->end ) || my_date_compare_ex( &b->end, &a->begin, TRUE ) >= 0 ){
			*consistent = FALSE;
		}
		return( 1 );
	}

	/* a_begin and b_begin are both set */
	cmp = my_date_compare( &a->begin, &b->begin );

	/* does 'a' end to the infinite ? */
	if( !my_date_is_valid( &a->end )){
		/* 'a' ends to the infinite => b_end is set */
		/* in order to be consistent, b_begin must be less than b_end,
		 * which must itself be less than a_begin */
		if( my_date_compare( &b->begin, &b->end ) >= 0 || my_date_compare( &b->end, &a->begin ) >= 0 ){
			*consistent = FALSE;
		}
		return( cmp );
	}

	/* does 'b' end to the infinite ? */
	if( !my_date_is_valid( &b->end )){
		/* 'b' ends to the infinite */
		/* in order to be consistent, a_begin must be less than a_end,
		 * which must itself be less than b_begin */
		if( my_date_compare( &a->begin, &a->end ) >= 0 || my_date_compare( &a->end, &b->begin ) >= 0 ){
			*consistent = FALSE;
		}
		return( cmp );
	}

	/* all dates are set */
	if( my_date_compare( &a->begin, &a->end ) >= 0 || my_date_compare( &b->begin, &b->end ) >= 0 ){
		*consistent = FALSE;
	}

	return( cmp );
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( const ofaICollectionable *instance )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub )
{
	ofoRatePrivate *priv;
	GList *dataset, *it;
	ofoRate *rate;
	gchar *from;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_RATES ORDER BY RAT_MNEMO ASC",
					OFO_TYPE_RATE,
					hub );

	for( it=dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		priv = ofo_rate_get_instance_private( rate );
		from = g_strdup_printf(
				"OFA_T_RATES_VAL WHERE RAT_MNEMO='%s' ORDER BY RAT_VAL_ROW ASC",
				ofo_rate_get_mnemo( rate ));
		priv->validities =
				ofo_base_load_rows( st_validity_defs, ofa_hub_get_connect( hub ), from );
		g_free( from );
	}

	return( dataset );
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
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	ofoRatePrivate *priv;
	GList *dataset, *it, *det;
	GSList *lines;
	ofoRate *rate;
	gchar *str;
	gboolean ok, with_headers;
	gulong count;
	gchar field_sep;

	dataset = ofo_rate_get_dataset( hub );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		count += ofo_rate_get_val_count( rate );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_validity_defs, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		rate = OFO_RATE( it->data );
		priv = ofo_rate_get_instance_private( rate );
		for( det=priv->validities ; det ; det=det->next ){
			str = ofa_box_csv_get_line( det->data, settings );
			lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
			g_free( str );
			ok = ofa_iexportable_export_lines( exportable, lines );
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
			if( !ok ){
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/*
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
 * - row number (placeholder, not imported, but recomputed)
 * - begin validity (opt - yyyy-mm-dd)
 * - end validity (opt - yyyy-mm-dd)
 * - rate
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'rate' records, then all 'validity' records.
 * Contrarily, validity lines for a given rate are imported in the order
 * they are found in the input file.
 *
 * Replace the whole table with the provided datas.
 *
 * Returns: 0 if no error has occurred, >0 if an error has been detected
 * during import phase (input file read), <0 if an error has occured
 * during insert phase.
 *
 * As the table is dropped between import phase and insert phase, if an
 * error occurs during insert phase, then the table is changed and only
 * contains the successfully inserted records.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoRate *rate;
	GList *dataset, *it;
	guint errors, line;
	gchar *msg, *mnemo;
	gint type;
	GList *detail;
	const ofaIDBConnect *connect;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = cstr ? atoi( cstr ) : 0;
		switch( type ){
			case 1:
				rate = rate_import_csv_rate( importable, fields, settings, line, &errors );
				if( rate ){
					dataset = g_list_prepend( dataset, rate );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = rate_import_csv_validity( importable, fields, settings, line, &errors, &mnemo );
				if( detail ){
					rate = rate_find_by_mnemo( dataset, mnemo );
					if( rate ){
						ofa_box_set_int( detail, RAT_VAL_ROW, 1+ofo_rate_get_val_count( rate ));
						rate_val_add_detail( rate, detail );
					}
					g_free( mnemo );
				}
				break;
			default:
				msg = g_strdup_printf( _( "invalid rate line type: %s" ), cstr );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				errors += 1;
				continue;
		}
	}

	if( !errors ){
		connect = ofa_hub_get_connect( hub );
		rate_do_drop_content( connect );

		for( it=dataset ; it ; it=it->next ){
			rate = OFO_RATE( it->data );
			if( !rate_do_insert( rate, connect )){
				errors -= 1;
			}
			ofa_iimportable_increment_progress(
					importable, IMPORTABLE_PHASE_INSERT, 1+ofo_rate_get_val_count( rate ));
		}

		ofo_rate_free_dataset( dataset );
		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_RATE );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_RATE );
	}

	return( errors );
}

static ofoRate *
rate_import_csv_rate( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint line, guint *errors )
{
	ofoRate *rate;
	gchar *str;
	GSList *itf;
	gchar *splitted;

	rate = ofo_rate_new();
	itf = fields ? fields->next : NULL;

	/* rate mnemo */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate mnemonic" ));
		*errors += 1;
		g_object_unref( rate );
		g_free( str );
		return( NULL );
	}
	ofo_rate_set_mnemo( rate, str );
	g_free( str );

	/* rate label */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate label" ));
		*errors += 1;
		g_object_unref( rate );
		g_free( str );
		return( NULL );
	}
	ofo_rate_set_label( rate, str );
	g_free( str );

	/* notes
	 * we are tolerant on the last field... */
	str = ofa_iimportable_get_string( &itf, settings );
	splitted = my_utils_import_multi_lines( str );
	ofo_rate_set_notes( rate, splitted );
	g_free( splitted );
	g_free( str );

	return( rate );
}

static GList *
rate_import_csv_validity( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint line, guint *errors, gchar **mnemo )
{
	GList *detail;
	gchar *str;
	GSList *itf;
	GDate date;
	ofxAmount amount;

	detail = ofa_box_init_fields_list( st_validity_defs );
	itf = fields ? fields->next : NULL;

	/* rate mnemo */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate mnemonic" ));
		*errors += 1;
		ofa_box_free_fields_list( detail );
		g_free( str );
		return( NULL );
	}
	*mnemo = g_strdup( str );
	ofa_box_set_string( detail, RAT_MNEMO, str );
	g_free( str );

	/* row number (placeholder) */
	itf = itf ? itf->next : NULL;

	/* rate begin validity */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_date( detail, RAT_VAL_BEGIN, my_date_set_from_sql( &date, str ));
	g_free( str );

	/* rate end validity */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_date( detail, RAT_VAL_END, my_date_set_from_sql( &date, str ));
	g_free( str );

	/* rate rate */
	str = ofa_iimportable_get_string( &itf, settings );
	amount = my_double_set_from_csv( str, ofa_file_format_get_decimal_sep( settings ));
	ofa_box_set_amount( detail, RAT_VAL_RATE, amount );
	g_free( str );

	return( detail );
}

static gboolean
rate_do_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_RATES", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_RATES_VAL", TRUE ));
}
