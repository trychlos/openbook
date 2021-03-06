/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-idoc.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-entry.h"

#include "tva/ofo-tva-record.h"

/* priv instance data
 */
enum {
	TFO_MNEMO = 1,
	TFO_END,
	TFO_HAS_CORRESPONDENCE,
	TFO_CRE_USER,
	TFO_CRE_STAMP,
	TFO_LABEL,
	TFO_CORRESPONDENCE,
	TFO_BEGIN,
	TFO_NOTES,
	TFO_UPD_USER,
	TFO_UPD_STAMP,
	TFO_DOPE,
	TFO_OPE_USER,
	TFO_OPE_STAMP,
	TFO_STATUS,
	TFO_STA_CLOSING,
	TFO_STA_USER,
	TFO_STA_STAMP,
	TFO_BOOL_ROW,
	TFO_BOOL_LABEL,
	TFO_BOOL_TRUE,
	TFO_DET_ROW,
	TFO_DET_CODE,
	TFO_DET_LABEL,
	TFO_DET_LEVEL,
	TFO_DET_HAS_BASE,
	TFO_DET_BASEF,
	TFO_DET_BASE,
	TFO_DET_HAS_AMOUNT,
	TFO_DET_AMOUNTF,
	TFO_DET_AMOUNT,
	TFO_DET_HAS_TEMPLATE,
	TFO_DET_TEMPLATE,
	TFO_DET_OPE_NUMBER,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_HAS_CORRESPONDENCE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( TFO_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_CORRESPONDENCE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BEGIN ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( TFO_DOPE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_OPE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_OPE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( TFO_STATUS ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_STA_CLOSING ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_STA_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_STA_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_bools_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BOOL_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BOOL_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BOOL_TRUE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_details_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_CODE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LEVEL ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_BASEF ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_BASE ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_AMOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_AMOUNTF ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_AMOUNT ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_OPE_NUMBER ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

typedef struct {

	/* the details of the record as a GList of GList fields
	 */
	GList *bools;
	GList *details;
}
	ofoTVARecordPrivate;

/* manage the validity status
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	ofeVatStatus    id;
	const gchar   *dbms;
	const gchar   *abr;
	const gchar   *label;
}
	sValid;

static sValid st_valid[] = {
		{ VAT_STATUS_NO,     "N", N_( "No" ),      N_( "Not validated" ) },
		{ VAT_STATUS_USER,   "U", N_( "User" ),    N_( "Validated by the user" ) },
		{ VAT_STATUS_PCLOSE, "C", N_( "Closing" ), N_( "Automatically validated on period closing" ) },
		{ 0 },
};

static void      vat_record_set_mnemo( ofoTVARecord *record, const gchar *mnemo );
static void      vat_record_set_has_correspondence( ofoTVARecord *record, gboolean has_correspondence );
static void      vat_record_set_cre_user( ofoTVARecord *record, const gchar *user );
static void      vat_record_set_cre_stamp( ofoTVARecord *record, const myStampVal *stamp );
static void      vat_record_set_ope_date( ofoTVARecord *record, const GDate *date );
static void      vat_record_set_ope_user( ofoTVARecord *record, const gchar *user );
static void      vat_record_set_ope_stamp( ofoTVARecord *record, const myStampVal *stamp );
static void      vat_record_set_status( ofoTVARecord *record, ofeVatStatus status );
static void      vat_record_set_sta_closing( ofoTVARecord *record, const GDate *date );
static void      vat_record_set_sta_user( ofoTVARecord *record, const gchar *user );
static void      vat_record_set_sta_stamp( ofoTVARecord *record, const myStampVal *stamp );
static void      vat_record_set_upd_user( ofoTVARecord *record, const gchar *user );
static void      vat_record_set_upd_stamp( ofoTVARecord *record, const myStampVal *stamp );
static void      vat_record_boolean_add( ofoTVARecord *record, const gchar *label, gboolean is_true );
static void      vat_record_detail_add( ofoTVARecord *record, ofxAmount base, ofxAmount amount );
static void      vat_record_detail_set_code( ofoTVARecord *record, guint idx, const gchar *code );
static void      vat_record_detail_set_label( ofoTVARecord *record, guint idx, const gchar *label );
static void      vat_record_detail_set_level( ofoTVARecord *record, guint idx, guint level );
static void      vat_record_detail_set_has_base( ofoTVARecord *record, guint idx, gboolean has_base );
static void      vat_record_detail_set_base_formula( ofoTVARecord *record, guint idx, const gchar *formula );
static void      vat_record_detail_set_has_amount( ofoTVARecord *record, guint idx, gboolean has_amount );
static void      vat_record_detail_set_amount_formula( ofoTVARecord *record, guint idx, const gchar *formula );
static void      vat_record_detail_set_has_template( ofoTVARecord *record, guint idx, gboolean has_template );
static void      vat_record_detail_set_template( ofoTVARecord *record, guint idx, const gchar *template );
static GList    *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean  record_do_insert( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_insert_main( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_delete_bools( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_delete_details( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_insert_details_ex( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_insert_bools( ofoTVARecord *record, const ofaIDBConnect *connect, guint idx );
static gboolean  record_insert_details( ofoTVARecord *record, const ofaIDBConnect *connect, guint idx );
static gboolean  record_do_update( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_update_main( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_do_update_notes( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean  record_do_update_dope( ofoTVARecord *record, ofaIDBConnect *connect, const GDate *dope );
static gboolean  record_do_delete( ofoTVARecord *record, const ofaIDBConnect *connect );
static gint      record_cmp_by_mnemo_end( const ofoTVARecord *a, const gchar *mnemo, const GDate *end );
static void      icollectionable_iface_init( myICollectionableInterface *iface );
static guint     icollectionable_get_interface_version( void );
static GList    *icollectionable_load_collection( void *user_data );
static void      idoc_iface_init( ofaIDocInterface *iface );
static guint     idoc_get_interface_version( void );
static void      isignalable_iface_init( ofaISignalableInterface *iface );
static void      isignalable_connect_to( ofaISignaler *signaler );
static gboolean  signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean  signaler_is_deletable_tva_form( ofaISignaler *signaler, ofoTVAForm *form );
static void      signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean  signaler_on_updated_tva_form_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id );
static void      signaler_on_period_close( ofaISignaler *signaler, ofeSignalerClosing ind, const GDate *closing, void *empty );
static void      free_collection( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoTVARecord, ofo_tva_record, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoTVARecord )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
details_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
details_list_free( ofoTVARecord *record )
{
	ofoTVARecordPrivate *priv;

	priv = ofo_tva_record_get_instance_private( record );

	g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
	priv->details = NULL;
}

static void
bools_list_free_bool( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
bools_list_free( ofoTVARecord *record )
{
	ofoTVARecordPrivate *priv;

	priv = ofo_tva_record_get_instance_private( record );

	g_list_free_full( priv->bools, ( GDestroyNotify ) bools_list_free_bool );
	priv->bools = NULL;
}

static void
tva_record_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_tva_record_finalize";
	ofoTVARecordPrivate *priv;
	gchar *str;

	priv = ofo_tva_record_get_instance_private( OFO_TVA_RECORD( instance ));

	str = my_date_to_str( ofa_box_get_date( OFO_BASE( instance )->prot->fields, TFO_END ), MY_DATE_SQL );
	g_debug( "%s: instance=%p (%s): %s %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, TFO_MNEMO ), str );
	g_free( str );

	g_return_if_fail( instance && OFO_IS_TVA_RECORD( instance ));

	/* free data members here */
	if( priv->bools ){
		bools_list_free( OFO_TVA_RECORD( instance ));
	}
	if( priv->details ){
		details_list_free( OFO_TVA_RECORD( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_record_parent_class )->finalize( instance );
}

static void
tva_record_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_TVA_RECORD( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_record_parent_class )->dispose( instance );
}

static void
ofo_tva_record_init( ofoTVARecord *self )
{
	static const gchar *thisfn = "ofo_tva_record_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_tva_record_class_init( ofoTVARecordClass *klass )
{
	static const gchar *thisfn = "ofo_tva_record_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_finalize;
}

/**
 * ofo_tva_record_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoTVARecord dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_tva_record_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_TVA_RECORD, getter ));
}

/**
 * ofo_tva_record_get_last_end:
 * @getter: a #ofaIGetter instance.
 * @mnemo: the VAT record mnemonic.
 * @date: [out]: the date to be set.
 *
 * Set the last end declaration for the @mnemo VAT form.
 *
 * Returns: the provided @date.
 */
GDate *
ofo_tva_record_get_last_end( ofaIGetter *getter, const gchar *mnemo, GDate *date )
{
	gchar *query;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	const gchar *cstr;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );
	g_return_val_if_fail( date, NULL );

	query = g_strdup_printf( "SELECT MAX(TFO_END) FROM TVA_T_RECORDS WHERE TFO_MNEMO='%s'", mnemo );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	my_date_clear( date );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		irow = result;
		icol = irow ? ( GSList * ) irow->data : NULL;
		cstr = icol ? ( const gchar * ) icol->data : NULL;
		if( my_strlen( cstr )){
			my_date_set_from_sql( date, ( const gchar * ) icol->data );
		}
		ofa_idbconnect_free_results( result );
	}

	return( date );
}

/**
 * ofo_tva_record_get_by_key:
 * @getter: a #ofaIGetter instance.
 * @mnemo:
 * @candidate_end: a valid date as candidate end.
 *
 * Returns: the first found recorded tva declaration for which the @end
 * date is inside of begin end end declaration dates, or %NULL.
 *
 * @end is the candidate date end for a new record. As a consequence,
 * any record with this same @end end date will make the candidate date
 * erroneous.
 *
 * The returned object is owned by the #ofoTVARecord class, and should
 * not be unreffed by the caller.
 */
ofoTVARecord *
ofo_tva_record_get_by_key( ofaIGetter *getter, const gchar *mnemo, const GDate *candidate_end )
{
	GList *dataset, *it;
	ofoTVARecord *record;
	const gchar *cmnemo;
	const GDate *dbegin, *dend;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );
	g_return_val_if_fail( my_date_is_valid( candidate_end ), NULL );

	dataset = ofo_tva_record_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		cmnemo = ofo_tva_record_get_mnemo( record );
		if( !my_collate( cmnemo, mnemo )){
			dbegin = ofo_tva_record_get_begin( record );
			dend = ofo_tva_record_get_end( record );
			if( my_date_compare_ex( dbegin, candidate_end, TRUE ) <= 0 &&
					my_date_compare( candidate_end, dend ) <= 0 ){
				return( record );
			}
		}
	}

	return( NULL );
}

/**
 * ofo_tva_record_get_overlap:
 * @getter: a #ofaIGetter instance.
 * @mnemo: the mnemonic identifier of the candidate VAT declaration.
 * @candidate_begin: a beginning date.
 * @candidate_end: an ending date.
 *
 * Returns: the first found recorded tva declaration which overlaps with
 * @candidate_begin and @candidate_end.
 *
 * The returned object is owned by the #ofoTVARecord class, and should
 * not be unreffed by the caller.
 */
ofoTVARecord *
ofo_tva_record_get_overlap( ofaIGetter *getter, const gchar *mnemo, const GDate *candidate_begin, const GDate *candidate_end )
{
	GList *dataset, *it;
	ofoTVARecord *record;
	const gchar *cmnemo;
	const GDate *dbegin, *dend;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );
	g_return_val_if_fail( my_date_is_valid( candidate_begin ), NULL );
	g_return_val_if_fail( my_date_is_valid( candidate_end ), NULL );

	dataset = ofo_tva_record_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		cmnemo = ofo_tva_record_get_mnemo( record );
		/* not the same mnemo */
		if( my_collate( cmnemo, mnemo ) != 0 ){
			continue;
		}
		/* if this is the same end date, then this is the same record */
		dend = ofo_tva_record_get_end( record );
		if( my_date_compare( dend, candidate_end ) == 0 ){
			continue;
		}
		if( my_date_compare( dend, candidate_begin ) >= 0 && my_date_compare( dend, candidate_end ) <= 0 ){
			return( record );
		}
		dbegin = ofo_tva_record_get_begin( record );
		if( my_date_compare( dbegin, candidate_begin ) >= 0 && my_date_compare( dbegin, candidate_end ) <= 0 ){
			return( record );
		}
	}

	return( NULL );
}

/**
 * ofo_tva_record_new:
 * @form: the source #ofoTVAForm to be initialized from.
 *
 * Returns: a new #ofoTVARecords object, initializing it with data
 * from #ofoTVAForm @form source.
 */
ofoTVARecord *
ofo_tva_record_new( ofoTVAForm *form )
{
	ofaIGetter *getter;
	ofoTVARecord *record;
	guint count, i;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	getter = ofo_base_get_getter( OFO_BASE( form ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	record = g_object_new( OFO_TYPE_TVA_RECORD, "ofo-base-getter", getter, NULL );

	OFO_BASE( record )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	vat_record_set_mnemo( record, ofo_tva_form_get_mnemo( form ));
	vat_record_set_has_correspondence( record, ofo_tva_form_get_has_correspondence( form ));
	ofo_tva_record_set_label( record, ofo_tva_form_get_label( form ));
	vat_record_set_status( record, VAT_STATUS_NO );

	count = ofo_tva_form_boolean_get_count( form );
	bools_list_free( record );
	for( i=0 ; i<count ; ++i ){
		cstr = ofo_tva_form_boolean_get_label( form, i );
		vat_record_boolean_add( record, cstr, FALSE );
	}

	count = ofo_tva_form_detail_get_count( form );
	details_list_free( record );
	for( i=0 ; i<count ; ++i ){
		vat_record_detail_add( record, 0, 0 );
		cstr = ofo_tva_form_detail_get_code( form, i );
		if( my_strlen( cstr )){
			vat_record_detail_set_code( record, i, cstr );
		}
		cstr = ofo_tva_form_detail_get_label( form, i );
		if( my_strlen( cstr )){
			vat_record_detail_set_label( record, i, cstr );
		}
		vat_record_detail_set_level( record, i, ofo_tva_form_detail_get_level( form, i ));
		vat_record_detail_set_has_base( record, i, ofo_tva_form_detail_get_has_base( form, i ));
		cstr = ofo_tva_form_detail_get_base( form, i );
		if( my_strlen( cstr )){
			vat_record_detail_set_base_formula( record, i, cstr );
		}
		vat_record_detail_set_has_amount( record, i, ofo_tva_form_detail_get_has_amount( form, i ));
		cstr = ofo_tva_form_detail_get_amount( form, i );
		if( my_strlen( cstr )){
			vat_record_detail_set_amount_formula( record, i, cstr );
		}
		vat_record_detail_set_has_template( record, i, ofo_tva_form_detail_get_has_template( form, i ));
		cstr = ofo_tva_form_detail_get_template( form, i );
		if( my_strlen( cstr )){
			vat_record_detail_set_template( record, i, cstr );
		}
	}

	return( record );
}

/**
 * ofo_tva_record_dump:
 */
void
ofo_tva_record_dump( const ofoTVARecord *record )
{
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	ofa_box_dump_fields_list( "ofo_tva_record_dump", OFO_BASE( record )->prot->fields );
}

/**
 * ofo_tva_record_get_mnemo:
 * @record:
 */
const gchar *
ofo_tva_record_get_mnemo( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_MNEMO );
}

/**
 * ofo_tva_record_get_end:
 */
const GDate *
ofo_tva_record_get_end( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, date, NULL, TFO_END );
}

/**
 * ofo_tva_record_get_has_correspondence:
 */
gboolean
ofo_tva_record_get_has_correspondence( const ofoTVARecord *record )
{
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( record )->prot->fields, TFO_HAS_CORRESPONDENCE );

	return( my_utils_boolean_from_str( cstr ));
}

/**
 * ofo_tva_record_get_cre_user:
 */
const gchar *
ofo_tva_record_get_cre_user( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_CRE_USER );
}

/**
 * ofo_tva_record_get_cre_stamp:
 */
const myStampVal *
ofo_tva_record_get_cre_stamp( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, timestamp, NULL, TFO_CRE_STAMP );
}

/**
 * ofo_tva_record_get_label:
 */
const gchar *
ofo_tva_record_get_label( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_LABEL );
}

/**
 * ofo_tva_record_get_correspondence:
 */
const gchar *
ofo_tva_record_get_correspondence( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_CORRESPONDENCE );
}

/**
 * ofo_tva_record_get_begin:
 */
const GDate *
ofo_tva_record_get_begin( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, date, NULL, TFO_BEGIN );
}

/**
 * ofo_tva_record_get_notes:
 */
const gchar *
ofo_tva_record_get_notes( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_NOTES );
}

/**
 * ofo_tva_record_get_upd_user:
 */
const gchar *
ofo_tva_record_get_upd_user( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_UPD_USER );
}

/**
 * ofo_tva_record_get_upd_stamp:
 */
const myStampVal *
ofo_tva_record_get_upd_stamp( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, timestamp, NULL, TFO_UPD_STAMP );
}

/**
 * ofo_tva_record_get_dope:
 */
const GDate *
ofo_tva_record_get_dope( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, date, NULL, TFO_DOPE );
}

/**
 * ofo_tva_record_get_ope_user:
 */
const gchar *
ofo_tva_record_get_ope_user( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_OPE_USER );
}

/**
 * ofo_tva_record_get_ope_stamp:
 */
const myStampVal *
ofo_tva_record_get_ope_stamp( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, timestamp, NULL, TFO_OPE_STAMP );
}

/**
 * ofo_tva_record_get_status:
 * @record: this #ofoTVARecord object.
 *
 * Returns: the validity status of @record.
 */
ofeVatStatus
ofo_tva_record_get_status( const ofoTVARecord * record )
{
	static const gchar *thisfn = "ofo_tva_record_get_status";
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), VAT_STATUS_NO );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, VAT_STATUS_NO );

	cstr = ofa_box_get_string( OFO_BASE( record )->prot->fields, TFO_STATUS );

	for( i=0 ; st_valid[i].id ; ++i ){
		if( !my_collate( st_valid[i].dbms, cstr )){
			return( st_valid[i].id );
		}
	}

	g_warning( "%s: unknown or invalid dbms status: %s", thisfn, cstr );

	return( VAT_STATUS_NO );
}

/**
 * ofo_tva_record_status_get_dbms:
 *
 * Returns: the dbms string corresponding to the status.
 */
const gchar *
ofo_tva_record_status_get_dbms( ofeVatStatus status )
{
	static const gchar *thisfn = "ofo_tva_record_status_get_dbms";
	gint i;

	for( i=0 ; st_valid[i].id ; ++i ){
		if( st_valid[i].id == status ){
			return( st_valid[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_tva_record_status_get_abr:
 *
 * Returns: the abbreviated localized string corresponding to the status.
 */
const gchar *
ofo_tva_record_status_get_abr( ofeVatStatus status )
{
	static const gchar *thisfn = "ofo_tva_record_status_get_abr";
	gint i;

	for( i=0 ; st_valid[i].id ; ++i ){
		if( st_valid[i].id == status ){
			return( gettext( st_valid[i].abr ));
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_tva_record_status_get_label:
 *
 * Returns: the abbreviated localized label corresponding to the status.
 */
const gchar *
ofo_tva_record_status_get_label( ofeVatStatus status )
{
	static const gchar *thisfn = "ofo_tva_record_status_get_label";
	gint i;

	for( i=0 ; st_valid[i].id ; ++i ){
		if( st_valid[i].id == status ){
			return( gettext( st_valid[i].label ));
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_tva_record_get_sta_closing:
 * @record: this #ofoTVARecord object.
 *
 * Returns: the closing date of the validation if the record has been
 * automatically validated on period closing.
 *
 * Validation status must be VAT_STATUS_PCLOSE.
 */
const GDate *
ofo_tva_record_get_sta_closing( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, date, NULL, TFO_STA_CLOSING );
}

/**
 * ofo_tva_record_get_sta_user:
 * @record: this #ofoTVARecord object.
 *
 * Returns: the user responsible of the validation.
 */
const gchar *
ofo_tva_record_get_sta_user( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_STA_USER );
}

/**
 * ofo_tva_record_get_sta_stamp:
 * @record: this #ofoTVARecord object.
 *
 * Returns: the validation timestamp.
 */
const myStampVal *
ofo_tva_record_get_sta_stamp( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, timestamp, NULL, TFO_STA_STAMP );
}

/**
 * ofo_tva_record_get_accounting_opes:
 * @record: this #ofoTVARecord object.
 *
 * Returns: the list of generated accounting operations number.
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
ofo_tva_record_get_accounting_opes( ofoTVARecord *record )
{
	guint count, idx;
	GList *list;
	ofxCounter number;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	count = ofo_tva_record_detail_get_count( record );
	list = NULL;

	for( idx=0 ; idx<count ; ++idx ){
		number = ofo_tva_record_detail_get_ope_number( record, idx );
		if( number > 0 ){
			list = g_list_prepend( list, ( gpointer ) number );
		}
	}

	return( list );
}

/**
 * ofo_tva_record_delete_accounting_entries:
 * @record: this #ofoTVARecord object.
 * @opes: the accounting operations.
 *
 * Delete the accouting entries generated by the @opes.
 *
 * The caller MUST have previously made sure that these entries are
 * not validated nor already deleted.
 */
void
ofo_tva_record_delete_accounting_entries( ofoTVARecord *record, GList *opes )
{
	ofaIGetter *getter;
	GList *entries, *it;
	ofoEntry *entry;
	ofeEntryStatus status;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	getter = ofo_base_get_getter( OFO_BASE( record ));

	entries = ofo_entry_get_by_ope_numbers( getter, opes );
	for( it=entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		//g_debug( "ofo_tva_record_delete_accounting_entries: number=%lu", ofo_entry_get_number( entry ));
		status = ofo_entry_get_status( entry );
		g_return_if_fail( status == ENT_STATUS_ROUGH );
		ofo_entry_delete( entry );
	}

	g_list_free( entries );
}

/**
 * ofo_tva_record_boolean_get_count:
 */
guint
ofo_tva_record_boolean_get_count( ofoTVARecord *record )
{
	ofoTVARecordPrivate *priv;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	return( g_list_length( priv->bools ));
}

/**
 * ofo_tva_record_boolean_get_label:
 * @idx is the index in the booleans list, starting with zero
 */
const gchar *
ofo_tva_record_boolean_get_label( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->bools, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_BOOL_LABEL ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_boolean_get_true:
 * @idx is the index in the booleans list, starting with zero
 */
gboolean
ofo_tva_record_boolean_get_true( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->bools, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_BOOL_TRUE ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_record_detail_get_count:
 */
guint
ofo_tva_record_detail_get_count( ofoTVARecord *record )
{
	ofoTVARecordPrivate *priv;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	return( g_list_length( priv->details ));
}

/**
 * ofo_tva_record_detail_get_code:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_code( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_CODE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_label( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_LABEL ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_level:
 * @idx is the index in the details list, starting with zero
 */
guint
ofo_tva_record_detail_get_level( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	guint level;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	level = nth ? ofa_box_get_int( nth->data, TFO_DET_LEVEL ) : 0;

	return( level );
}

/**
 * ofo_tva_record_detail_get_has_base:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_record_detail_get_has_base( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_BASE ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_record_detail_get_base_formula:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_base_formula( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_BASEF ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_base:
 * @idx is the index in the details list, starting with zero
 */
ofxAmount
ofo_tva_record_detail_get_base( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	ofxAmount amount;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	amount = nth ? ofa_box_get_amount( nth->data, TFO_DET_BASE ) : 0;

	return( amount );
}

/**
 * ofo_tva_record_detail_get_has_amount:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_record_detail_get_has_amount( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_AMOUNT ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_record_detail_get_amount_formula:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_amount_formula( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_AMOUNTF ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_amount:
 * @idx is the index in the details list, starting with zero
 */
ofxAmount
ofo_tva_record_detail_get_amount( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	ofxAmount amount;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	amount = nth ? ofa_box_get_amount( nth->data, TFO_DET_AMOUNT ) : 0;

	return( amount );
}

/**
 * ofo_tva_record_detail_get_has_template:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_record_detail_get_has_template( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_TEMPLATE ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_record_detail_get_template:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_template( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_TEMPLATE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_ope_number:
 * @idx is the index in the details list, starting with zero
 */
ofxCounter
ofo_tva_record_detail_get_ope_number( ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	ofxCounter number;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	number = nth ? ofa_box_get_counter( nth->data, TFO_DET_OPE_NUMBER ) : 0;

	return( number );
}

/**
 * ofo_tva_record_is_deletable:
 * @record: the tva record.
 * @gen_opes: the generated accounting operations.
 *
 * Returns: %TRUE if the tva record is deletable.
 *
 * A TVA record may be deleted while it is not validated, and none of
 * the generated accounting entries has been validated.
 */
gboolean
ofo_tva_record_is_deletable( const ofoTVARecord *record, GList *gen_opes )
{
	ofaIGetter *getter;
	GList *entries, *it;
	guint count;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	if( ofo_tva_record_get_status( record ) != VAT_STATUS_NO ){
		return( FALSE );
	}

	count = 0;
	getter = ofo_base_get_getter( OFO_BASE( record ));
	entries = ofo_entry_get_by_ope_numbers( getter, gen_opes );
	for( it=entries ; it ; it=it->next ){
		if( ofo_entry_get_status( OFO_ENTRY( it->data )) == ENT_STATUS_VALIDATED ){
			count += 1;
		}
	}
	g_list_free( entries );

	return( count == 0 );
}

/**
 * ofo_tva_record_is_valid_data:
 * @mnemo: an identifier mnemonic.
 * @label: the label of the declaration.
 * @begin: [allow-none]: the begin date of the declaration.
 * @end: the end date of the declaration.
 * @msgerr: [out]: error message placeholder.
 *
 * Returns: %TRUE if the provided datas make a TVA record valid (which
 * may be recorded).
 *
 * We accept here that the begin date be not set.
 * However, if the begin date is set, then it must be less or equal to
 * the end date.
 */
gboolean
ofo_tva_record_is_valid_data( const gchar *mnemo, const gchar *label, const GDate *begin, const GDate *end, gchar **msgerr )
{
	gint cmp;

	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( mnemo)){
		if( msgerr ){
			*msgerr = g_strdup( _( "Mnemonic identifier is empty" ));
		}
		return( FALSE );
	}
	if( !my_strlen( label)){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
		return( FALSE );
	}
	if( !my_date_is_valid( end )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Ending date is not set or invalid" ));
		}
		return( FALSE );
	}
	if( !my_date_is_valid( begin )){
		return( TRUE );
	}
	cmp = my_date_compare( begin, end );
	if( cmp > 0 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Beginning date is greater than ending date" ));
		}
	}
	return( cmp <= 0 );
}

/**
 * ofo_tva_record_compare_by_key:
 * @record:
 * @mnemo:
 * @end:
 *
 * Returns: -1 if @record'ids are lesser than @mnemo and @end,
 *  0 if they are equal, +1 if they are greater.
 */
gint
ofo_tva_record_compare_by_key( const ofoTVARecord *record, const gchar *mnemo, const GDate *end )
{
gint cmp;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( my_strlen( mnemo ), 0 );
	g_return_val_if_fail( my_date_is_valid( end ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	cmp = record_cmp_by_mnemo_end( record, mnemo, end );

	return( cmp );
}

/*
 * ofo_tva_vat_record_set_mnemo:
 */
static void
vat_record_set_mnemo( ofoTVARecord *record, const gchar *mnemo )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_MNEMO, mnemo );
}

/**
 * ofo_tva_record_set_end:
 */
void
ofo_tva_record_set_end( ofoTVARecord *record, const GDate *date )
{
	ofo_base_setter( TVA_RECORD, record, date, TFO_END, date );
}

/*
 * vat_record_set_has_correspondence:
 */
static void
vat_record_set_has_correspondence( ofoTVARecord *record, gboolean has_correspondence )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_HAS_CORRESPONDENCE, has_correspondence ? "Y":"N" );
}

/*
 * ofo_tva_record_set_cre_user:
 */
static void
vat_record_set_cre_user( ofoTVARecord *record, const gchar *user )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_CRE_USER, user );
}

/*
 * ofo_tva_record_set_cre_stamp:
 */
static void
vat_record_set_cre_stamp( ofoTVARecord *record, const myStampVal *stamp )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_CRE_STAMP, stamp );
}

/**
 * ofo_tva_record_set_label:
 */
void
ofo_tva_record_set_label( ofoTVARecord *record, const gchar *label )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_LABEL, label );
}

/**
 * ofo_tva_record_set_correspondence:
 */
void
ofo_tva_record_set_correspondence( ofoTVARecord *record, const gchar *correspondence )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_CORRESPONDENCE, correspondence );
}

/**
 * ofo_tva_record_set_begin:
 */
void
ofo_tva_record_set_begin( ofoTVARecord *record, const GDate *date )
{
	ofo_base_setter( TVA_RECORD, record, date, TFO_BEGIN, date );
}

/**
 * ofo_tva_record_set_notes:
 */
void
ofo_tva_record_set_notes( ofoTVARecord *record, const gchar *notes )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_NOTES, notes );
}

/*
 * ofo_tva_record_set_upd_user:
 */
static void
vat_record_set_upd_user( ofoTVARecord *record, const gchar *user )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_UPD_USER, user );
}

/*
 * ofo_tva_record_set_upd_stamp:
 */
static void
vat_record_set_upd_stamp( ofoTVARecord *record, const myStampVal *stamp )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_UPD_STAMP, stamp );
}

/*
 * ofo_tva_record_set_ope_date:
 */
static void
vat_record_set_ope_date( ofoTVARecord *record, const GDate *date )
{
	ofo_base_setter( TVA_RECORD, record, date, TFO_DOPE, date );
}

/*
 * ofo_tva_record_set_ope_user:
 */
static void
vat_record_set_ope_user( ofoTVARecord *record, const gchar *user )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_OPE_USER, user );
}

/*
 * ofo_tva_record_set_ope_stamp:
 */
static void
vat_record_set_ope_stamp( ofoTVARecord *record, const myStampVal *stamp )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_OPE_STAMP, stamp );
}

/*
 * ofo_tva_record_set_status:
 * @record: this #ofoTVARecord object.
 * @status: the validations status of @record.
 *
 * Set @status.
 */
static void
vat_record_set_status( ofoTVARecord *record, ofeVatStatus status )
{
	const gchar *cstr;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	cstr = ofo_tva_record_status_get_dbms( status );
	ofa_box_set_string( OFO_BASE( record )->prot->fields, TFO_STATUS, cstr );
}

/*
 * ofo_tva_record_set_sta_closing:
 * @record: this #ofoTVARecord object.
 * @status: the validations status of @record.
 */
static void
vat_record_set_sta_closing( ofoTVARecord *record, const GDate *date )
{
	ofo_base_setter( TVA_RECORD, record, date, TFO_STA_CLOSING, date );
}

/*
 * ofo_tva_record_set_sta_user:
 */
static void
vat_record_set_sta_user( ofoTVARecord *record, const gchar *user )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_STA_USER, user );
}

/*
 * ofo_tva_record_set_sta_stamp:
 */
static void
vat_record_set_sta_stamp( ofoTVARecord *record, const myStampVal *stamp )
{
	ofo_base_setter( TVA_RECORD, record, timestamp, TFO_STA_STAMP, stamp );
}

/*
 * vat_record_boolean_add:
 * @record:
 * @label:
 * @is_true:
 */
static void
vat_record_boolean_add( ofoTVARecord *record, const gchar *label, gboolean is_true )
{
	ofoTVARecordPrivate *priv;
	GList *fields;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( my_strlen( label ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	fields = ofa_box_init_fields_list( st_bools_defs );
	ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_record_get_mnemo( record ));
	ofa_box_set_int( fields, TFO_BOOL_ROW, 1+ofo_tva_record_boolean_get_count( record ));
	ofa_box_set_string( fields, TFO_BOOL_LABEL, label );
	ofa_box_set_string( fields, TFO_BOOL_TRUE, is_true ? "Y":"N" );

	priv->bools = g_list_append( priv->bools, fields );
}

/**
 * ofo_tva_record_boolean_set_true:
 * @record:
 * @idx:
 * @is_true:
 *
 * @idx is the index in the details list, starting with zero.
 */
void
ofo_tva_record_boolean_set_true( ofoTVARecord *record, guint idx, gboolean is_true )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->bools, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_BOOL_TRUE, is_true ? "Y":"N" );
}

static void
vat_record_detail_add( ofoTVARecord *record, ofxAmount base, ofxAmount amount )
{
	ofoTVARecordPrivate *priv;
	GList *fields;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	fields = ofa_box_init_fields_list( st_details_defs );
	ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_record_get_mnemo( record ));
	ofa_box_set_date( fields, TFO_END, ofo_tva_record_get_end( record ));
	ofa_box_set_int( fields, TFO_DET_ROW, 1+ofo_tva_record_detail_get_count( record ));
	ofa_box_set_amount( fields, TFO_DET_BASE, base );
	ofa_box_set_amount( fields, TFO_DET_AMOUNT, amount );

	priv->details = g_list_append( priv->details, fields );
}

/*
 * vat_record_detail_set_code:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_code( ofoTVARecord *record, guint idx, const gchar *code )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_CODE, code );
}

/*
 * vat_record_detail_set_label:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_label( ofoTVARecord *record, guint idx, const gchar *label )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_LABEL, label );
}

/*
 * vat_record_detail_set_level:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_level( ofoTVARecord *record, guint idx, guint level )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_int( nth->data, TFO_DET_LEVEL, level );
}

/*
 * vat_record_detail_set_has_base:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_has_base( ofoTVARecord *record, guint idx, gboolean has_base )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_HAS_BASE, has_base ? "Y":"N" );
}

/*
 * vat_record_detail_set_base_formula:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_base_formula( ofoTVARecord *record, guint idx, const gchar *formula )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_BASEF, formula );
}

/**
 * ofo_tva_record_detail_set_base:
 * @idx is the index in the details list, starting with zero
 */
void
ofo_tva_record_detail_set_base( ofoTVARecord *record, guint idx, ofxAmount base )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_amount( nth->data, TFO_DET_BASE, base );
}

/*
 * vat_record_detail_set_has_amount:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_has_amount( ofoTVARecord *record, guint idx, gboolean has_amount )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_HAS_AMOUNT, has_amount ? "Y":"N" );
}

/*
 * vat_record_detail_set_amount_formula:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_amount_formula( ofoTVARecord *record, guint idx, const gchar *formula )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_AMOUNTF, formula );
}

/**
 * ofo_tva_record_detail_set_amount:
 * @idx is the index in the details list, starting with zero
 */
void
ofo_tva_record_detail_set_amount( ofoTVARecord *record, guint idx, ofxAmount amount )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_amount( nth->data, TFO_DET_AMOUNT, amount );
}

/*
 * vat_record_detail_set_has_template:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_has_template( ofoTVARecord *record, guint idx, gboolean has_template )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_HAS_TEMPLATE, has_template ? "Y":"N" );
}

/*
 * vat_record_detail_set_template:
 * @idx is the index in the details list, starting with zero
 */
static void
vat_record_detail_set_template( ofoTVARecord *record, guint idx, const gchar *template )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_string( nth->data, TFO_DET_TEMPLATE, template );
}

/**
 * ofo_tva_record_detail_set_ope_number:
 * @idx is the index in the details list, starting with zero
 */
void
ofo_tva_record_detail_set_ope_number( ofoTVARecord *record, guint idx, ofxCounter number )
{
	ofoTVARecordPrivate *priv;
	GList *nth;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	g_return_if_fail( nth );
	ofa_box_set_counter( nth->data, TFO_DET_OPE_NUMBER, number );
}

/**
 * ofo_tva_record_get_bool_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_RECORDS_BOOL
 * child table.
 *
 * The returned list should be #ofo_tva_record_free_bool_orphans() by the
 * caller.
 */
GList *
ofo_tva_record_get_bool_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_RECORDS_BOOL" ));
}

/**
 * ofo_tva_record_get_det_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_RECORDS_DET
 * child table.
 *
 * The returned list should be #ofo_tva_record_free_det_orphans() by the
 * caller.
 */
GList *
ofo_tva_record_get_det_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_RECORDS_DET" ));
}

/**
 * ofo_tva_record_get_doc_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_RECORDS_DOC
 * child table.
 *
 * The returned list should be #ofo_tva_record_free_doc_orphans() by the
 * caller.
 */
GList *
ofo_tva_record_get_doc_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_RECORDS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(TFO_MNEMO) FROM %s "
			"	WHERE TFO_MNEMO NOT IN (SELECT TFO_MNEMO FROM TVA_T_RECORDS)", table );

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
 * ofo_tva_record_validate:
 * @record: this #ofoTVARecord object.
 * @status: the new validation status,
 *  either VAT_STATUS_USER or VAT_STATUS_PCLOSE.
 * @closing: [allow-none]: closing date;
 *  must be set if @status is VAT_STATUS_PCLOSE.
 *
 * Validate the @record, along with the generated accounting entries..
 *
 * Current user and timestamp are recorded in corresponding 'status' columns.
 */
gboolean
ofo_tva_record_validate( ofoTVARecord *record, ofeVatStatus status, const GDate *closing )
{
	static const gchar *thisfn = "ofo_tva_record_validate";
	ofaIGetter *getter;
	ofaHub *hub;
	ofaIDBConnect *connect;
	const gchar *user, *cstr;
	gchar *stamp_str, *sdate, *send;
	myStampVal *stamp;
	GString *gstr;
	gboolean ok;
	ofaISignaler *signaler;
	GList *opes;

	g_debug( "%s: record=%p, status=%u, closing=%p",
			thisfn, ( void * ) record, status, ( void * ) closing );

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );


	getter = ofo_base_get_getter( OFO_BASE( record ));
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	gstr = g_string_new( "UPDATE TVA_T_RECORDS SET " );

	vat_record_set_status( record, status );
	cstr = ofa_box_get_string( OFO_BASE( record )->prot->fields, TFO_STATUS );
	g_string_append_printf( gstr, "TFO_STATUS='%s',", cstr );

	user = ofa_idbconnect_get_account( connect );
	vat_record_set_sta_user( record, user );
	g_string_append_printf( gstr, "TFO_STA_USER='%s',", user );

	stamp = my_stamp_new_now();
	vat_record_set_sta_stamp( record, stamp );
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( gstr, "TFO_STA_STAMP='%s',", stamp_str );
	g_free( stamp_str );

	if( status == VAT_STATUS_PCLOSE || status == VAT_STATUS_USER ){
		g_return_val_if_fail( my_date_is_valid( closing ), FALSE );
		vat_record_set_sta_closing( record, closing );
		sdate = my_date_to_str( closing, MY_DATE_SQL );
		g_string_append_printf( gstr, "TFO_STA_CLOSING='%s'", sdate );
		g_free( sdate );

	} else {
		vat_record_set_sta_closing( record, NULL );
		gstr = g_string_append( gstr, "TFO_STA_CLOSING=NULL" );
	}

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
	g_string_append_printf(
			gstr, "	WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
					ofo_tva_record_get_mnemo( record ), send );
	g_free( send );

	ok = ofa_idbconnect_query( connect, gstr->str, TRUE );

	g_string_free( gstr, TRUE );

	signaler = ofa_igetter_get_signaler( getter );
	g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, record, NULL );

	opes = ofo_tva_record_get_accounting_opes( record );
	ofo_entry_validate_by_opes( getter, opes );
	g_list_free( opes );

	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_tva_record_validate_all:
 * @getter: the #ofaIGetter of the application.
 * @closing: a closing date.
 *
 * Validate all remaining VAT declarations until @closing date.
 *
 * Returns: the count of declarations validated here.
 */
guint
ofo_tva_record_validate_all( ofaIGetter *getter, const GDate *closing )
{
	static const gchar *thisfn = "ofo_tva_record_validate_all";
	GList *dataset, *it;
	ofoTVARecord *record;
	const GDate *end;
	ofeVatStatus status;
	guint count;

	g_debug( "%s: getter=%p, closing=%p",
			thisfn, ( void * ) getter, ( void * ) closing );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );

	count = 0;
	dataset = ofo_tva_record_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		status = ofo_tva_record_get_status( record );
		/* if already validated, nothing to do */
		if( status != VAT_STATUS_NO ){
			continue;
		}
		/* only validate the declarations before the closing date */
		end = ofo_tva_record_get_end( record );
		if( my_date_compare( end, closing ) <= 0 ){
			if( ofo_tva_record_validate( record, VAT_STATUS_PCLOSE, closing )){
				count += 1;
			}
		}
	}

	return( count );
}

/**
 * ofo_tva_record_insert:
 * @record: this #ofoTVARecord object.
 *
 * Insert the @record new VAT declaration in the DBMS.
 *
 * This first insertion does not consider the status data group.
 * See ofo_tva_record_validate() for this.
 */
gboolean
ofo_tva_record_insert( ofoTVARecord *record )
{
	static const gchar *thisfn = "ofo_record_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) record );

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( record ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_tva_record_get_dataset( getter );

	if( record_do_insert( record, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( record ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, record );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
record_do_insert( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	return( record_insert_main( record, connect ) &&
			record_insert_details_ex( record, connect ));
}

static gboolean
record_insert_main( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *notes, *label, *corresp, *sbegin, *send, *stamp_str;
	myStampVal *stamp;
	const gchar *userid, *cstr;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_record_get_label( record ));
	corresp = my_utils_quote_sql( ofo_tva_record_get_correspondence( record ));
	notes = my_utils_quote_sql( ofo_tva_record_get_notes( record ));
	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), MY_DATE_SQL );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO TVA_T_RECORDS" );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_END,TFO_HAS_CORRESPONDENCE,TFO_CRE_USER,TFO_CRE_STAMP,"
			"	 TFO_LABEL,TFO_CORRESPONDENCE,TFO_BEGIN,TFO_NOTES,"
			"	 TFO_STATUS,TFO_STA_USER,TFO_STA_STAMP)"
			"	VALUES ('%s','%s','%s','%s','%s'",
			ofo_tva_record_get_mnemo( record ),
			send,
			ofo_tva_record_get_has_correspondence( record ) ? "Y":"N",
			userid, stamp_str );

	if( my_strlen( label )){
		g_string_append_printf( query, ",'%s'", label );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( corresp )){
		g_string_append_printf( query, ",'%s'", corresp );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( sbegin )){
		g_string_append_printf( query, ",'%s'", sbegin );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( notes )){
		g_string_append_printf( query, ",'%s'", notes );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	/* status group */
	cstr = ofa_box_get_string( OFO_BASE( record )->prot->fields, TFO_STATUS );
	g_string_append_printf( query, ",'%s','%s','%s')", cstr, userid, stamp_str );
	vat_record_set_sta_user( record, userid );
	vat_record_set_sta_stamp( record, stamp );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	vat_record_set_cre_user( record, userid );
	vat_record_set_cre_stamp( record, stamp );

	g_free( send );
	g_free( sbegin );
	g_free( notes );
	g_free( label );
	g_free( corresp );
	g_free( stamp_str );
	my_stamp_free( stamp );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
record_delete_bools( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gchar *query, *send;
	gboolean ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_strdup_printf(
			"DELETE FROM TVA_T_RECORDS_BOOL WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
			ofo_tva_record_get_mnemo( record ), send );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );
	g_free( send );

	return( ok );
}

static gboolean
record_delete_details( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gchar *query, *send;
	gboolean ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_strdup_printf(
			"DELETE FROM TVA_T_RECORDS_DET WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
			ofo_tva_record_get_mnemo( record ), send );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );
	g_free( send );

	return( ok );
}

static gboolean
record_insert_details_ex( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gboolean ok;
	guint count, idx;

	ok = record_delete_bools( record, connect ) &&
			record_delete_details( record, connect );

	if( ok ){
		count = ofo_tva_record_boolean_get_count( record );
		for( idx=0 ; idx<count ; ++idx ){
			if( !record_insert_bools( record, connect, idx )){
				ok = FALSE;
				break;
			}
		}
		count = ofo_tva_record_detail_get_count( record );
		for( idx=0 ; idx<count ; ++idx ){
			if( !record_insert_details( record, connect, idx )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

/*
 * booleans and details insertion:
 * - idx is counted from zero, while TFO_ROW is counted from 1.
 */
static gboolean
record_insert_bools( ofoTVARecord *record, const ofaIDBConnect *connect, guint idx )
{
	GString *query;
	gboolean ok, is_true;
	gchar *send, *label;

	query = g_string_new( "INSERT INTO TVA_T_RECORDS_BOOL " );

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
	label = my_utils_quote_sql( ofo_tva_record_boolean_get_label( record, idx ));

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_END,TFO_BOOL_ROW,TFO_BOOL_LABEL,TFO_BOOL_TRUE) "
			"	VALUES('%s','%s',%u,'%s'",
			ofo_tva_record_get_mnemo( record ), send, idx+1, label );

	is_true = ofo_tva_record_boolean_get_true( record, idx );
	g_string_append_printf( query, ",'%s'", is_true ? "Y":"N" );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_free( label );
	g_free( send );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
record_insert_details( ofoTVARecord *record, const ofaIDBConnect *connect, guint idx )
{
	GString *query;
	gboolean ok, has;
	gchar *send, *formula, *samount, *code, *label, *template;
	ofxCounter number;

	query = g_string_new( "INSERT INTO TVA_T_RECORDS_DET " );

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
	code = my_utils_quote_sql( ofo_tva_record_detail_get_code( record, idx ));
	label = my_utils_quote_sql( ofo_tva_record_detail_get_label( record, idx ));

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_END,TFO_DET_ROW,"
			"	 TFO_DET_CODE,TFO_DET_LABEL,TFO_DET_LEVEL,"
			"	 TFO_DET_HAS_BASE,TFO_DET_BASEF,TFO_DET_BASE,"
			"	 TFO_DET_HAS_AMOUNT,TFO_DET_AMOUNTF,TFO_DET_AMOUNT,"
			"	 TFO_DET_HAS_TEMPLATE,TFO_DET_TEMPLATE,TFO_DET_OPE_NUMBER) "
			"	VALUES('%s','%s',%u",
			ofo_tva_record_get_mnemo( record ),
			send, idx+1 );

	if( my_strlen( code )){
		g_string_append_printf( query, ",'%s'", code );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( label )){
		g_string_append_printf( query, ",'%s'", label );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	g_string_append_printf( query, ",%u", ofo_tva_record_detail_get_level( record, idx ));

	has = ofo_tva_record_detail_get_has_base( record, idx );
	query = g_string_append( query, has ? ",'Y'":",'N'" );

	formula = has ? my_utils_quote_sql( ofo_tva_record_detail_get_base_formula( record, idx )) : NULL;
	if( my_strlen( formula )){
		g_string_append_printf( query, ",'%s'", formula );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( formula );

	samount = has ? my_double_to_sql( ofo_tva_record_detail_get_base( record, idx )) : NULL;
	if( my_strlen( samount )){
		g_string_append_printf( query, ",'%s'", samount );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( samount );

	has = ofo_tva_record_detail_get_has_amount( record, idx );
	query = g_string_append( query, has ? ",'Y'":",'N'" );

	formula = has ? my_utils_quote_sql( ofo_tva_record_detail_get_amount_formula( record, idx )) : NULL;
	if( my_strlen( formula )){
		g_string_append_printf( query, ",'%s'", formula );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( formula );

	samount = has ? my_double_to_sql( ofo_tva_record_detail_get_amount( record, idx )) : NULL;
	if( my_strlen( samount )){
		g_string_append_printf( query, ",'%s'", samount );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( samount );

	has = ofo_tva_record_detail_get_has_template( record, idx );
	query = g_string_append( query, has ? ",'Y'":",'N'" );

	template = has ? my_utils_quote_sql( ofo_tva_record_detail_get_template( record, idx )) : NULL;
	if( my_strlen( template )){
		g_string_append_printf( query, ",'%s'", template );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( template );

	number = has ? ofo_tva_record_detail_get_ope_number( record, idx ) : 0;
	if( number > 0 ){
		g_string_append_printf( query, ",%lu", number );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_free( send );
	g_free( code );
	g_free( label );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_tva_record_update:
 * @record: this #ofoTVARecord object.
 *
 * Update the properties of @record in DBMS.
 *
 * #ofaTVARecordProperties dialog refuses to modify mnemonic and end
 * date: they are set once and never modified.
 *
 * Notes are still updatable even after the declaration has been validated.
 *
 * Neither validation status nor operation date are updated here.
 * See ofo_tva_record_validate() and ofo_tva_record_update_dope().
 */
gboolean
ofo_tva_record_update( ofoTVARecord *record )
{
	static const gchar *thisfn = "ofo_tva_record_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) record );

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( record ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( record_do_update( record, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, record, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
record_do_update( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	return( record_update_main( record, connect ) &&
			record_insert_details_ex( record, connect ));
}

static gboolean
record_update_main( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *label, *corresp, *sbegin, *send, *stamp_str;
	const gchar *mnemo, *userid;
	myStampVal *stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_record_get_label( record ));
	corresp = my_utils_quote_sql( ofo_tva_record_get_correspondence( record ));
	mnemo = ofo_tva_record_get_mnemo( record );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), MY_DATE_SQL );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_string_new( "UPDATE TVA_T_RECORDS SET " );

	if( my_strlen( label )){
		g_string_append_printf( query, "TFO_LABEL='%s'", label );
	} else {
		query = g_string_append( query, "TFO_LABEL=NULL" );
	}

	if( my_strlen( corresp )){
		g_string_append_printf( query, ",TFO_CORRESPONDENCE='%s'", corresp );
	} else {
		query = g_string_append( query, ",TFO_CORRESPONDENCE=NULL" );
	}

	if( my_strlen( sbegin )){
		g_string_append_printf( query, ",TFO_BEGIN='%s'", sbegin );
	} else {
		query = g_string_append( query, ",TFO_BEGIN=NULL" );
	}

	g_string_append_printf( query,
			",TFO_UPD_USER='%s',TFO_UPD_STAMP='%s' "
			"	WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
					userid,
					stamp_str,
					mnemo, send );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	vat_record_set_upd_user( record, userid );
	vat_record_set_upd_stamp( record, stamp );

	g_string_free( query, TRUE );
	g_free( label );
	g_free( corresp );
	g_free( stamp_str );
	g_free( sbegin );
	g_free( send );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_tva_record_update_notes:
 * @record: this #ofoTVARecord object.
 *
 * Update only the notes of @record.
 *
 * Notes are still updatable even after the declaration has been validated.
 */
gboolean
ofo_tva_record_update_notes( ofoTVARecord *record )
{
	static const gchar *thisfn = "ofo_tva_record_update_notes";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) record );

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( record ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( record_do_update_notes( record, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, record, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
record_do_update_notes( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *notes, *send, *stamp_str;
	const gchar *mnemo, *userid;
	myStampVal *stamp;

	userid = ofa_idbconnect_get_account( connect );
	notes = my_utils_quote_sql( ofo_tva_record_get_notes( record ));
	mnemo = ofo_tva_record_get_mnemo( record );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_string_new( "UPDATE TVA_T_RECORDS SET " );

	if( my_strlen( notes )){
		g_string_append_printf( query, "TFO_NOTES='%s'", notes );
	} else {
		query = g_string_append( query, "TFO_NOTES=NULL" );
	}

	g_string_append_printf( query,
			",TFO_UPD_USER='%s',TFO_UPD_STAMP='%s' "
			"	WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
					userid,
					stamp_str,
					mnemo, send );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	vat_record_set_upd_user( record, userid );
	vat_record_set_upd_stamp( record, stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( send );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_tva_record_update_dope:
 * @record: this #ofoTVARecord object.
 * @dope: the operation date.
 *
 * Update the operation date in DBMS.
 *
 * This method is expected to be called on operation date update, and
 * when actually generating the operations.
 */
gboolean
ofo_tva_record_update_dope( ofoTVARecord *record, const GDate *dope )
{
	static const gchar *thisfn = "ofo_record_update_dope";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p, dope=%p", thisfn, ( void * ) record, ( void * ) dope );

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( record ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( record_do_update_dope( record, ofa_hub_get_connect( hub ), dope )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, record, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
record_do_update_dope( ofoTVARecord *record, ofaIDBConnect *connect, const GDate *dope )
{
	gboolean ok;
	GString *query;
	gchar *sdend, *sstamp, *sdope;
	const gchar *mnemo, *userid;
	myStampVal *stamp;

	mnemo = ofo_tva_record_get_mnemo( record );
	sdend = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_string_new( "UPDATE TVA_T_RECORDS SET " );

	if( my_date_is_valid( dope )){
		sdope = my_date_to_str( dope, MY_DATE_SQL );
		userid = ofa_idbconnect_get_account( connect );
		stamp = my_stamp_new_now();
		sstamp = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

		g_string_append_printf( query, "TFO_DOPE='%s',TFO_OPE_USER='%s',TFO_OPE_STAMP='%s'",
				sdope, userid, sstamp );

		g_free( sstamp );
		g_free( sdope );

		vat_record_set_ope_date( record, dope );
		vat_record_set_ope_user( record, userid );
		vat_record_set_ope_stamp( record, stamp );

		my_stamp_free( stamp );

	} else {
		query = g_string_append( query, "TFO_DOPE=NULL,TFO_OPE_USER=NULL,TFO_OPE_STAMP=NULL" );

		vat_record_set_ope_date( record, dope );
		vat_record_set_ope_user( record, NULL );
		vat_record_set_ope_stamp( record, NULL );
	}

	g_string_append_printf( query, " WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
			mnemo, sdend );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_free( sdend );
	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_tva_record_delete:
 */
gboolean
ofo_tva_record_delete( ofoTVARecord *tva_record )
{
	static const gchar *thisfn = "ofo_tva_record_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) tva_record );

	g_return_val_if_fail( tva_record && OFO_IS_TVA_RECORD( tva_record ), FALSE );
	g_return_val_if_fail( ofo_tva_record_is_deletable( tva_record, NULL ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tva_record ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( record_do_delete( tva_record, ofa_hub_get_connect( hub ))){
		g_object_ref( tva_record );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( tva_record ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, tva_record );
		g_object_unref( tva_record );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
record_do_delete( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	gchar *query, *send;
	gboolean ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_strdup_printf(
			"DELETE FROM TVA_T_RECORDS"
			"	WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
					ofo_tva_record_get_mnemo( record ), send );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );
	g_free( send );

	if( ok ){
		ok = record_delete_details( record, connect ) && record_delete_bools( record, connect );
	}

	return( ok );
}

static gint
record_cmp_by_mnemo_end( const ofoTVARecord *a, const gchar *mnemo, const GDate *end )
{
	gchar *aend, *akey, *bend, *bkey;
	gint cmp;

	aend = my_date_to_str( ofo_tva_record_get_end( a ), MY_DATE_SQL );
	akey = g_strdup_printf( "%s-%s", ofo_tva_record_get_mnemo( a ), aend );
	bend = my_date_to_str( end, MY_DATE_SQL );
	bkey = g_strdup_printf( "%s-%s", mnemo, bend );

	cmp = my_collate( akey, bkey );

	g_free( bkey );
	g_free( bend );
	g_free( akey );
	g_free( aend );

	return( cmp );
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_record_icollectionable_iface_init";

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
	static const gchar *thisfn = "ofo_tva_record_load_dataset";
	ofoTVARecordPrivate *priv;
	GList *dataset, *it, *ir;
	ofoTVARecord *record;
	gchar *from, *send;
	ofaHub *hub;
	const ofaIDBConnect *connect;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"TVA_T_RECORDS",
					OFO_TYPE_TVA_RECORD,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));
	connect = ofa_hub_get_connect( hub );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		priv = ofo_tva_record_get_instance_private( record );

		send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

		from = g_strdup_printf(
				"TVA_T_RECORDS_DET WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
				ofo_tva_record_get_mnemo( record ), send );
		priv->details = ofo_base_load_rows( st_details_defs, connect, from );
		g_free( from );

		/* dump the detail rows */
		if( 0 ){
			for( ir=priv->details ; ir ; ir=ir->next ){
				ofa_box_dump_fields_list( thisfn, ir->data );
			}
		}

		from = g_strdup_printf(
				"TVA_T_RECORDS_BOOL WHERE TFO_MNEMO='%s' AND TFO_END='%s'",
				ofo_tva_record_get_mnemo( record ), send );
		priv->bools = ofo_base_load_rows( st_bools_defs, connect, from );
		g_free( from );

		g_free( send );
	}

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_idoc_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idoc_get_interface_version;
}

static guint
idoc_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_form_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_tva_record_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
	g_signal_connect( signaler, SIGNALER_DOSSIER_PERIOD_CLOSED, G_CALLBACK( signaler_on_period_close ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_tva_record_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_TVA_FORM( object )){
		deletable = signaler_is_deletable_tva_form( signaler, OFO_TVA_FORM( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_tva_form( ofaISignaler *signaler, ofoTVAForm *form )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM TVA_T_RECORDS WHERE TFO_MNEMO='%s'",
			ofo_tva_form_get_mnemo( form ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_tva_record_signaler_on_updated_base";
	const gchar *mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_TVA_FORM( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_tva_form_get_mnemo( OFO_TVA_FORM( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_tva_form_mnemo( signaler, object, mnemo, prev_id );
			}
		}
	}
}

static gboolean
signaler_on_updated_tva_form_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_record_signaler_on_updated_tva_form_mnemo";
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE TVA_T_RECORDS "
					"	SET TFO_MNEMO='%s'"
					"	WHERE TFO_MNEMMO='%s'", mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	query = g_strdup_printf(
					"UPDATE TVA_T_RECORDS_BOOL "
					"	SET TFO_MNEMO='%s'"
					"	WHERE TFO_MNEMMO='%s'", mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	query = g_strdup_printf(
					"UPDATE TVA_T_RECORDS_DET "
					"	SET TFO_MNEMO='%s'"
					"	WHERE TFO_MNEMMO='%s'", mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	free_collection( signaler );

	return( ok );
}

/*
 * Auto-validate VAT declarations until this date.
 * Only deal here with intermediate period closing.
 *
 * Same action when closing the exercice is managed through the IExeClose
 * interface.
 */
static void
signaler_on_period_close( ofaISignaler *signaler, ofeSignalerClosing ind, const GDate *closing, void *empty )
{
	static const gchar *thisfn = "ofo_tva_record_signaler_on_period_close";
	ofaIGetter *getter;

	g_debug( "%s: signaler=%p, closing=%p, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) closing, empty );

	if( ind == SIGNALER_CLOSING_INTERMEDIATE ){

		getter = ofa_isignaler_get_getter( signaler );
		ofo_tva_record_validate_all( getter, closing );
	}
}

/*
 * #1558
 * not only the database must be updated with new values, but the in-memory
 * current collections should too also.
 * it is simpler to just free the collections to force a futur refresh
 */
static void
free_collection( ofaISignaler *signaler )
{
	ofaIGetter *getter;
	myICollector *collector;

	getter = ofa_isignaler_get_getter( signaler );
	collector = ofa_igetter_get_collector( getter );
	my_icollector_collection_free( collector, OFO_TYPE_TVA_RECORD );
}
