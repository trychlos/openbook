/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* priv instance data
 */
enum {
	RAT_MNEMO = 1,
	RAT_CRE_USER,
	RAT_CRE_STAMP,
	RAT_LABEL,
	RAT_NOTES,
	RAT_UPD_USER,
	RAT_UPD_STAMP,
	RAT_VAL_ROW,
	RAT_VAL_BEGIN,
	RAT_VAL_END,
	RAT_VAL_RATE,
	RAT_DOC_ID,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order.
 * So:
 * 1/ the class default import should expect these fields in this same
 *    order.
 * 2/ new datas should be added to the end of the list.
 * 3/ a removed column should be replaced by an empty one to stay
 *    compatible with the class default import.
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( RAT_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( RAT_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( RAT_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
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

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( RAT_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( RAT_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define RATE_TABLES_COUNT               3
#define RATE_EXPORT_VERSION             2

typedef struct {
	GList *validities;					/* the validities of the rate as a GList of GList fields */
	GList *docs;
}
	ofoRatePrivate;

static ofoRate  *rate_find_by_mnemo( GList *set, const gchar *mnemo );
static void      rate_set_cre_user( ofoRate *rate, const gchar *user );
static void      rate_set_cre_stamp( ofoRate *rate, const myStampVal *stamp );
static void      rate_set_upd_user( ofoRate *rate, const gchar *user );
static void      rate_set_upd_stamp( ofoRate *rate, const myStampVal *stamp );
static GList    *rate_val_new_detail( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value );
static void      rate_val_add_detail( ofoRate *rate, GList *detail );
static GList    *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean  rate_do_insert( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_main( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_delete_validities( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_validities( ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_insert_validity( ofoRate *rate, GList *detail, gint count, const ofaIDBConnect *connect );
static gboolean  rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean  rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaIDBConnect *connect );
static gboolean  rate_do_delete( ofoRate *rate, const ofaIDBConnect *connect );
static gint      rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo );
static gint      rate_cmp_by_validity( const ofsRateValidity *a, const ofsRateValidity *b, gboolean *consistent );
static void      icollectionable_iface_init( myICollectionableInterface *iface );
static guint     icollectionable_get_interface_version( void );
static GList    *icollectionable_load_collection( void *user_data );
static void      idoc_iface_init( ofaIDocInterface *iface );
static guint     idoc_get_interface_version( void );
static void      iexportable_iface_init( ofaIExportableInterface *iface );
static guint     iexportable_get_interface_version( void );
static gchar    *iexportable_get_label( const ofaIExportable *instance );
static gboolean  iexportable_get_published( const ofaIExportable *instance );
static gboolean  iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean  iexportable_export_default( ofaIExportable *exportable );
static void      iimportable_iface_init( ofaIImportableInterface *iface );
static guint     iimportable_get_interface_version( void );
static gchar    *iimportable_get_label( const ofaIImportable *instance );
static guint     iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList    *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoRate  *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static GList    *iimportable_import_parse_validity( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo );
static void      iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean  rate_get_exists( const ofoRate *rate, const ofaIDBConnect *connect );
static gboolean  rate_drop_content( const ofaIDBConnect *connect );
static void      isignalable_iface_init( ofaISignalableInterface *iface );
static void      isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoRate, ofo_rate, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRate )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoRatePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_rate_get_instance_private( self );

	priv->validities = NULL;
	priv->docs = NULL;
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
 * ofo_rate_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoRate dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_rate_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_RATE, getter ));
}

/**
 * ofo_rate_get_by_mnemo:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched rate, or %NULL.
 *
 * The returned object is owned by the #ofoRate class, and should
 * not be unreffed by the caller.
 */
ofoRate *
ofo_rate_get_by_mnemo( ofaIGetter *getter, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	dataset = ofo_rate_get_dataset( getter );

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
 * @getter: a #ofaIGetter instance.
 */
ofoRate *
ofo_rate_new( ofaIGetter *getter )
{
	ofoRate *rate;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	rate = g_object_new( OFO_TYPE_RATE, "ofo-base-getter", getter, NULL );
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
 * ofo_rate_get_cre_user:
 */
const gchar *
ofo_rate_get_cre_user( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_CRE_USER );
}

/**
 * ofo_rate_get_cre_stamp:
 */
const myStampVal *
ofo_rate_get_cre_stamp( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, timestamp, NULL, RAT_CRE_STAMP );
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
const myStampVal *
ofo_rate_get_upd_stamp( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, timestamp, NULL, RAT_UPD_STAMP );
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
ofo_rate_get_rate_at_date( ofoRate *rate, const GDate *date )
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
		sdate = my_date_to_str( date, MY_DATE_SQL );
		sbegin = my_date_to_str( val_begin, MY_DATE_SQL );
		send = my_date_to_str( val_end, MY_DATE_SQL );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( rate ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, rate, &deletable );
	}

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
			*msgerr = g_strdup( _( "Mnemonic is empty" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
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

/*
 * rate_set_cre_user:
 */
static void
rate_set_cre_user( ofoRate *rate, const gchar *user )
{
	ofo_base_setter( RATE, rate, string, RAT_CRE_USER, user );
}

/*
 * rate_set_cre_stamp:
 */
static void
rate_set_cre_stamp( ofoRate *rate, const myStampVal *stamp )
{
	ofo_base_setter( RATE, rate, timestamp, RAT_CRE_STAMP, stamp );
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
 * rate_set_upd_user:
 */
static void
rate_set_upd_user( ofoRate *rate, const gchar *user )
{
	ofo_base_setter( RATE, rate, string, RAT_UPD_USER, user );
}

/*
 * rate_set_upd_stamp:
 */
static void
rate_set_upd_stamp( ofoRate *rate, const myStampVal *stamp )
{
	ofo_base_setter( RATE, rate, timestamp, RAT_UPD_STAMP, stamp );
}

/**
 * ofo_rate_valid_get_count:
 *
 * Returns: the count of validity rows for this @rate
 */
gint
ofo_rate_valid_get_count( ofoRate *rate )
{
	ofoRatePrivate *priv;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, 0 );

	priv = ofo_rate_get_instance_private( rate );

	return( g_list_length( priv->validities ));
}

/**
 * ofo_rate_valid_get_begin:
 */
const GDate *
ofo_rate_valid_get_begin( ofoRate *rate, gint idx )
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
 * ofo_rate_valid_get_end:
 */
const GDate *
ofo_rate_valid_get_end( ofoRate *rate, gint idx )
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
 * ofo_rate_valid_get_rate:
 */
ofxAmount
ofo_rate_valid_get_rate( ofoRate *rate, gint idx )
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
 * ofo_rate_valid_get_min_date:
 *
 * Returns the smallest beginning date, all validities included.
 * If the returned #GDate struct is invalid, then it must be considered
 * as infinite in the past.
 */
const GDate *
ofo_rate_valid_get_min_date( ofoRate *rate )
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
 * ofo_rate_valid_get_max_date:
 *
 * Returns the greatest ending date, all validities included.
 * The returned #GDate object may be %NULL or invalid if it is
 * infinite in the future.
 */
const GDate *
ofo_rate_valid_get_max_date( ofoRate *rate )
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
 * ofo_rate_valid_reset:
 *
 * Clear all validities of the rate object.
 * This is normally done just before adding new validities, when
 * preparing for a dbms update.
 */
void
ofo_rate_valid_reset( ofoRate *rate )
{

	g_return_if_fail( rate && OFO_IS_RATE( rate ));
	g_return_if_fail( !OFO_BASE( rate )->prot->dispose_has_run );

	rate_free_validities( rate );
}

/**
 * ofo_rate_valid_add:
 * @rate: the #ofoRate to which the new validity must be added
 * @begin: the beginning of the validity, or %NULL
 * @end: the end of the validity, or %NULL
 * @value: the value of the rate for this validity.
 *
 * Add a validity record to the rate.
 */
void
ofo_rate_valid_add( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value )
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
	ofa_box_set_int( fields, RAT_VAL_ROW, 1+ofo_rate_valid_get_count( rate ));
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
 * ofo_rate_valid_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown rates in OFA_T_RATES_VAL
 * child table.
 *
 * The returned list should be ofo_rate_free_orphans() by the
 * caller.
 */
GList *
ofo_rate_valid_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_RATES_VAL" ));
}

/**
 * ofo_rate_doc_get_count:
 * @rate: this #ofoRate object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_rate_doc_get_count( ofoRate *rate )
{
	ofoRatePrivate *priv;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, 0 );

	priv = ofo_rate_get_instance_private( rate );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_rate_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown rates in OFA_T_RATES_DOC
 * child table.
 *
 * The returned list should be ofo_rate_free_orphans() by the
 * caller.
 */
GList *
ofo_rate_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_RATES_DOC" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(RAT_MNEMO) FROM %s "
			"	WHERE RAT_MNEMO NOT IN (SELECT RAT_MNEMO FROM OFA_T_RATES)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			orphans = g_list_prepend( orphans, g_strdup(( const gchar * ) icol->data ));
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_rate_insert:
 *
 * First creation of a new rate. This may contain zéro to n validity
 * datail rows. But, if it doesn't, then we take care of removing all
 * previously existing old validity rows.
 */
gboolean
ofo_rate_insert( ofoRate *rate )
{
	static const gchar *thisfn = "ofo_rate_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: rate=%p", thisfn, ( void * ) rate );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( rate ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_rate_get_dataset( getter );

	if( rate_do_insert( rate, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( rate ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, rate );
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
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	myStampVal *stamp;
	const gchar *userid;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_rate_get_label( rate ));
	notes = my_utils_quote_sql( ofo_rate_get_notes( rate ));
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_RATES" );

	g_string_append_printf( query,
			"	(RAT_MNEMO,RAT_CRE_USER,RAT_CRE_STAMP,RAT_LABEL,RAT_NOTES) "
			"	VALUES ('%s','%s','%s','%s',",
			ofo_rate_get_mnemo( rate ),
			userid,
			stamp_str,
			label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		rate_set_cre_user( rate, userid );
		rate_set_cre_stamp( rate, stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	my_stamp_free( stamp );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: rate=%p, prev_mnemo=%s",
			thisfn, ( void * ) rate, prev_mnemo );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( rate ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( rate_do_update( rate, prev_mnemo, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, rate, prev_mnemo );
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
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	myStampVal *stamp;
	const gchar *userid;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_rate_get_label( rate ));
	notes = my_utils_quote_sql( ofo_rate_get_notes( rate ));
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

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
		rate_set_upd_stamp( rate, stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_rate_delete:
 */
gboolean
ofo_rate_delete( ofoRate *rate )
{
	static const gchar *thisfn = "ofo_rate_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: rate=%p", thisfn, ( void * ) rate );

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( rate )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( rate ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( rate_do_delete( rate, ofa_hub_get_connect( hub ))){
		g_object_ref( rate );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( rate ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, rate );
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
	return( my_collate( ofo_rate_get_mnemo( a ), mnemo ));
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
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( void )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( void *user_data )
{
	ofoRatePrivate *priv;
	GList *dataset, *it;
	ofoRate *rate;
	gchar *from;
	ofaHub *hub;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_RATES",
					OFO_TYPE_RATE,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		priv = ofo_rate_get_instance_private( rate );
		from = g_strdup_printf(
				"OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
				ofo_rate_get_mnemo( rate ));
		priv->validities =
				ofo_base_load_rows( st_validity_defs, ofa_hub_get_connect( hub ), from );
		g_free( from );
	}

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_idoc_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idoc_get_interface_version;
}

static guint
idoc_get_interface_version( void )
{
	return( 1 );
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
	iface->get_label = iexportable_get_label;
	iface->get_published = iexportable_get_published;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "Reference : _rates" )));
}

static gboolean
iexportable_get_published( const ofaIExportable *instance )
{
	return( TRUE );
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the rates.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_rate_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTER_DEFAULT_FORMAT_ID )){
		return( iexportable_export_default( exportable ));
	}

	g_warning( "%s: format_id=%s unmanaged here", thisfn, format_id );

	return( FALSE );
}

static gboolean
iexportable_export_default( ofaIExportable *exportable )
{
	ofaIGetter *getter;
	ofaStreamFormat *stformat;
	ofoRatePrivate *priv;
	GList *dataset, *it, *det, *itd;
	ofoRate *rate;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_rate_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += RATE_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		count += ofo_rate_valid_get_count( rate );
		count += ofo_rate_doc_get_count( rate );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, RATE_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					RATE_TABLES_COUNT, st_boxed_defs, st_validity_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		rate = OFO_RATE( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( rate )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_rate_get_instance_private( rate );

		for( det=priv->validities ; det && ok ; det=det->next ){
			str1 = ofa_box_csv_get_line( det->data, stformat, NULL );
			str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
			str2 = g_strdup_printf( "1%c3%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}
	}

	return( ok );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->get_label = iimportable_get_label;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iimportable_get_label( const ofaIImportable *instance )
{
	return( iexportable_get_label( OFA_IEXPORTABLE( instance )));
}

/*
 * ofo_rate_iimportable_import:
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
 * Returns: the total count of errors.
 *
 * As the table may have been dropped between import phase and insert
 * phase, if an error occurs during insert phase, then the table is
 * changed and only contains the successfully inserted records.
 */
static guint
iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	ofaISignaler *signaler;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *dataset;
	gchar *bck_table, *bck_det_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_RATES" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "OFA_T_RATES_VAL" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_RATE );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_RATE );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_RATES" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "OFA_T_RATES_VAL" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
	}

	if( dataset ){
		ofo_rate_free_dataset( dataset );
	}

	return( parms->parse_errs+parms->insert_errs );
}

/*
 * parse to a dataset
 */
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	guint numline, total;
	gchar *str, *mnemo;
	gint type;
	GList *detail;
	ofoRate *rate;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;

		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = cstr ? atoi( cstr ) : 0;

		switch( type ){
			case 1:
				rate = iimportable_import_parse_main( importer, parms, numline, itf );
				if( rate ){
					dataset = g_list_prepend( dataset, rate );
					parms->parsed_count += 1;
					ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = iimportable_import_parse_validity( importer, parms, numline, itf, &mnemo );
				if( detail ){
					rate = rate_find_by_mnemo( dataset, mnemo );
					if( rate ){
						ofa_box_set_int( detail, RAT_VAL_ROW, 1+ofo_rate_valid_get_count( rate ));
						rate_val_add_detail( rate, detail );
						total -= 1;
						ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
					}
					g_free( mnemo );
				}
				break;
			default:
				str = g_strdup_printf( _( "invalid line type: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
		}
	}

	return( dataset );
}

static ofoRate *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoRate *rate;
	myStampVal *stamp;

	rate = ofo_rate_new( parms->getter );

	/* rate mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty rate mnemonic" ));
		parms->parse_errs += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_mnemo( rate, cstr );

	/* creation user */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		rate_set_cre_user( rate, cstr );
	}

	/* creation timestamp */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		stamp = my_stamp_new_from_sql( cstr );
		rate_set_cre_stamp( rate, stamp );
		my_stamp_free( stamp );
	}

	/* rate label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty rate label" ));
		parms->parse_errs += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_label( rate, cstr );

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_rate_set_notes( rate, splitted );
	g_free( splitted );

	return( rate );
}

static GList *
iimportable_import_parse_validity( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;
	GDate date;
	ofxAmount amount;

	detail = ofa_box_init_fields_list( st_validity_defs );
	itf = fields ? fields->next : NULL;

	/* rate mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty operation template mnemonic" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );
	ofa_box_set_string( detail, RAT_MNEMO, cstr );

	/* row number (placeholder) */
	itf = itf ? itf->next : NULL;

	/* rate begin validity */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_date( detail, RAT_VAL_BEGIN, my_date_set_from_sql( &date, cstr ));

	/* rate end validity */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_date( detail, RAT_VAL_END, my_date_set_from_sql( &date, cstr ));

	/* rate rate */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	amount = my_double_set_from_csv( cstr, ofa_stream_format_get_decimal_sep( parms->format ));
	ofa_box_set_amount( detail, RAT_VAL_RATE, amount );

	return( detail );
}
/*
 * insert records
 */
static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	const gchar *mnemo;
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoRate *rate;
	ofaHub *hub;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		rate_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		rate = OFO_RATE( it->data );

		if( rate_get_exists( rate, connect )){
			parms->duplicate_count += 1;
			mnemo = ofo_rate_get_mnemo( rate );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate rate, replacing previous one" ), mnemo );
					rate_do_delete( rate, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate rate, ignored (skipped)" ), mnemo );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate rate" ), mnemo );
					type = MY_PROGRESS_ERROR;
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}

			ofa_iimporter_progress_text( importer, parms, type, str );
			g_free( str );
		}

		if( insert ){
			if( rate_do_insert( rate, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
rate_get_exists( const ofoRate *rate, const ofaIDBConnect *connect )
{
	const gchar *rate_id;
	gint count;
	gchar *str;

	count = 0;
	rate_id = ofo_rate_get_mnemo( rate );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_RATES WHERE RAT_MNEMO='%s'", rate_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
rate_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_RATES", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_RATES_VAL", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_rate_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
