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
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "tva/ofo-tva-record.h"

/* priv instance data
 */
enum {
	TFO_MNEMO = 1,
	TFO_LABEL,
	TFO_HAS_CORRESPONDENCE,
	TFO_NOTES,
	TFO_VALIDATED,
	TFO_BEGIN,
	TFO_END,
	TFO_UPD_USER,
	TFO_UPD_STAMP,
	TFO_BOOL_ROW,
	TFO_BOOL_LABEL,
	TFO_BOOL_TRUE,
	TFO_DET_ROW,
	TFO_DET_LEVEL,
	TFO_DET_CODE,
	TFO_DET_LABEL,
	TFO_DET_HAS_BASE,
	TFO_DET_BASE_RULE,
	TFO_DET_BASE,
	TFO_DET_HAS_AMOUNT,
	TFO_DET_AMOUNT_RULE,
	TFO_DET_AMOUNT,
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
		{ OFA_BOX_CSV( TFO_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_HAS_CORRESPONDENCE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_VALIDATED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BEGIN ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_END ),
				OFA_TYPE_DATE,
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
		{ OFA_BOX_CSV( TFO_DET_LEVEL ),
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
		{ OFA_BOX_CSV( TFO_DET_HAS_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_BASE_RULE ),
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
		{ OFA_BOX_CSV( TFO_DET_AMOUNT_RULE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_AMOUNT ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
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

typedef struct {

	/* the details of the record as a GList of GList fields
	 */
	GList *bools;
	GList *details;
}
	ofoTVARecordPrivate;

/* a structure to compare mnemos and end dates
 */
typedef struct {
	gchar *mnemo;
	GDate  date;
}
	sCompare;

static ofoTVARecord *record_find_by_mnemo_and_end( GList *set, const gchar *mnemo, const GDate *end );
static guint         record_count_for_account( const ofaIDBConnect *connect, const gchar *account );
static void          tva_record_set_upd_user( ofoTVARecord *record, const gchar *upd_user );
static void          tva_record_set_upd_stamp( ofoTVARecord *record, const GTimeVal *upd_stamp );
static gboolean      record_do_insert( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_insert_main( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_delete_details( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_delete_bools( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_insert_details_ex( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_insert_details( ofoTVARecord *record, const ofaIDBConnect *connect, guint rang, GList *details );
static gboolean      record_insert_bools( ofoTVARecord *record, const ofaIDBConnect *connect, guint rang, GList *details );
static gboolean      record_do_update( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_update_main( ofoTVARecord *record, const ofaIDBConnect *connect );
static gboolean      record_do_delete( ofoTVARecord *record, const ofaIDBConnect *connect );
static gint          record_cmp_by_compare( const ofoTVARecord *a, const sCompare *cmp );
static gint          record_cmp_by_mnemo_end( const ofoTVARecord *a, const gchar *mnemo, const GDate *end );
static gint          tva_record_cmp_by_ptr( const ofoTVARecord *a, const ofoTVARecord *b );
static void          icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint         icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList        *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoTVARecord, ofo_tva_record, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoTVARecord )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init ))

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
 * ofo_tva_record_connect_to_hub_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_tva_record_connect_to_hub_handlers( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_tva_record_connect_to_hub_handlers";

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	//g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), NULL );
}

/**
 * ofo_tva_record_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoTVARecord dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_tva_record_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_TVA_RECORD ));
}

/**
 * ofo_tva_record_get_by_key:
 * @dossier:
 * @mnemo:
 * @end:
 *
 * Returns: the searched recorded tva declaration, or %NULL.
 *
 * The returned object is owned by the #ofoTVARecord class, and should
 * not be unreffed by the caller.
 */
ofoTVARecord *
ofo_tva_record_get_by_key( ofaHub *hub, const gchar *mnemo, const GDate *end )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );
	g_return_val_if_fail( my_date_is_valid( end ), NULL );

	dataset = ofo_tva_record_get_dataset( hub );

	return( record_find_by_mnemo_and_end( dataset, mnemo, end ));
}

static ofoTVARecord *
record_find_by_mnemo_and_end( GList *set, const gchar *mnemo, const GDate *end )
{
	sCompare cmp;
	GList *found;

	cmp.mnemo = g_strdup( mnemo );
	my_date_set_from_date( &cmp.date, end );

	found = g_list_find_custom( set, &cmp, ( GCompareFunc ) record_cmp_by_compare );
	if( found ){
		return( OFO_TVA_RECORD( found->data ));
	}

	return( NULL );
}

/**
 * ofo_tva_record_get_is_deletable:
 * @hub: the current #ofaHub object of the application.
 * @object: the object to be tested.
 *
 * Returns: %TRUE if the @object is not used by ofoTVAForm, thus may be
 * deleted.
 */
gboolean
ofo_tva_record_get_is_deletable( const ofaHub *hub, const ofoBase *object )
{
	gboolean ok;
	const gchar *account_id;
	guint count;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( object && OFO_IS_BASE( object ), FALSE );

	ok = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		account_id = ofo_account_get_number( OFO_ACCOUNT( object ));
		count = record_count_for_account( ofa_hub_get_connect( hub ), account_id );
		ok = ( count == 0 );
	}

	return( ok );
}

static guint
record_count_for_account( const ofaIDBConnect *connect, const gchar *account )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM TVA_T_RECORDS_DET "
				"	WHERE TFO_DET_BASE_RULE LIKE '%%%s%%' OR TFO_DET_AMOUNT_RULE LIKE '%%%s%%'", account, account );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( abs( count ));
}

/**
 * ofo_tva_record_new:
 */
ofoTVARecord *
ofo_tva_record_new( void )
{
	ofoTVARecord *record;

	record = g_object_new( OFO_TYPE_TVA_RECORD, NULL );
	OFO_BASE( record )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( record );
}

/**
 * ofo_tva_record_new_from_record:
 * @form: the source #ofoTVAForm to be initiallized from.
 *
 * Allocates a new #ofoTVARecords object, initializing it with data
 * from @form #ofoTVAForm source.
 */
ofoTVARecord *
ofo_tva_record_new_from_form( const ofoTVAForm *form )
{
	ofoTVARecord *dest;
	gint count, i;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	dest = ofo_tva_record_new();

	ofo_tva_record_set_mnemo( dest, ofo_tva_form_get_mnemo( form ));
	ofo_tva_record_set_label( dest, ofo_tva_form_get_label( form ));
	ofo_tva_record_set_has_correspondence( dest, ofo_tva_form_get_has_correspondence( form ));

	count = ofo_tva_form_detail_get_count( form );
	for( i=0 ; i<count ; ++i ){
		ofo_tva_record_detail_add( dest,
				ofo_tva_form_detail_get_level( form, i ),
				ofo_tva_form_detail_get_code( form, i ),
				ofo_tva_form_detail_get_label( form, i ),
				ofo_tva_form_detail_get_has_base( form, i ),
				ofo_tva_form_detail_get_base( form, i ),
				0,
				ofo_tva_form_detail_get_has_amount( form, i ),
				ofo_tva_form_detail_get_amount( form, i ),
				0 );
	}

	count = ofo_tva_form_boolean_get_count( form );
	for( i=0 ; i<count ; ++i ){
		ofo_tva_record_boolean_add( dest,
				ofo_tva_form_boolean_get_label( form, i ),
				FALSE );
	}

	return( dest );
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
 * ofo_tva_record_get_label:
 */
const gchar *
ofo_tva_record_get_label( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_LABEL );
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
 * ofo_tva_record_get_notes:
 */
const gchar *
ofo_tva_record_get_notes( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, string, NULL, TFO_NOTES );
}

/**
 * ofo_tva_record_get_is_validated:
 */
gboolean
ofo_tva_record_get_is_validated( const ofoTVARecord *record )
{
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( record )->prot->fields, TFO_VALIDATED );

	return( my_utils_boolean_from_str( cstr ));
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
 * ofo_tva_record_get_end:
 */
const GDate *
ofo_tva_record_get_end( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, date, NULL, TFO_END );
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
const GTimeVal *
ofo_tva_record_get_upd_stamp( const ofoTVARecord *record )
{
	ofo_base_getter( TVA_RECORD, record, timestamp, NULL, TFO_UPD_STAMP );
}

/**
 * ofo_tva_record_is_deletable:
 * @record: the tva record.
 *
 * Returns: %TRUE if the tva record is deletable.
 *
 * A TVA record may be deleted while it is not validated.
 */
gboolean
ofo_tva_record_is_deletable( const ofoTVARecord *record )
{
	gboolean is_validated;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	is_validated = ofo_tva_record_get_is_validated( record );

	return( !is_validated );
}

/**
 * ofo_tva_record_is_valid_data:
 * @mnemo: an identifier mnemonic.
 * @begin: the begin date of the declaration.
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
ofo_tva_record_is_valid_data( const gchar *mnemo, const GDate *begin, const GDate *end, gchar **msgerr )
{
	gint cmp;

	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( mnemo)){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty mnemonic identifier" ));
		}
		return( FALSE );
	}
	if( !my_date_is_valid( end )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Invalid ending date" ));
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
 * ofo_tva_record_is_validable_by_record:
 * @record:
 *
 * Returns: %TRUE if this TVA record may be validated.
 *
 * This is the case if both date are set (while only end date is needed
 * to record the data).
 */
gboolean
ofo_tva_record_is_validable_by_record( const ofoTVARecord *record )
{
	const GDate *cbegin, *cend;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, FALSE );

	cstr = ofo_tva_record_get_mnemo( record );
	cbegin = ofo_tva_record_get_begin( record );
	cend = ofo_tva_record_get_end( record );

	if( !ofo_tva_record_is_valid_data( cstr, cbegin, cend, NULL )){
		return( FALSE );
	}

	return( my_date_is_valid( cbegin ) &&
			my_date_compare( cbegin, cend ) <= 0 );
}

/**
 * ofo_tva_record_is_validable_by_data:
 * @mnemo: an identifier mnemonic.
 * @begin: the begin date of the declaration.
 * @end: the end date of the declaration.
 *
 * Returns: %TRUE if the provided datas make the TVA record validable.
 *
 * This is the case if both date are set (while only end date is needed
 * to record the data).
 */
gboolean
ofo_tva_record_is_validable_by_data( const gchar *mnemo, const GDate *begin, const GDate *end )
{
	if( !ofo_tva_record_is_valid_data( mnemo, begin, end, NULL )){
		return( FALSE );
	}
	return( my_date_is_valid( begin ) &&
			my_date_compare( begin, end ) <= 0 );
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

/**
 * ofo_tva_record_set_mnemo:
 */
void
ofo_tva_record_set_mnemo( ofoTVARecord *record, const gchar *mnemo )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_MNEMO, mnemo );
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
 * ofo_tva_record_set_has_correspondence:
 */
void
ofo_tva_record_set_has_correspondence( ofoTVARecord *record, gboolean has_correspondence )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_HAS_CORRESPONDENCE, has_correspondence ? "Y":"N" );
}

/**
 * ofo_tva_record_set_notes:
 */
void
ofo_tva_record_set_notes( ofoTVARecord *record, const gchar *notes )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_NOTES, notes );
}

/**
 * ofo_tva_record_set_is_validated:
 */
void
ofo_tva_record_set_is_validated( ofoTVARecord *record, gboolean is_validated )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_VALIDATED, is_validated ? "Y":"N" );
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
 * ofo_tva_record_set_end:
 */
void
ofo_tva_record_set_end( ofoTVARecord *record, const GDate *date )
{
	ofo_base_setter( TVA_RECORD, record, date, TFO_END, date );
}

/*
 * ofo_tva_record_set_upd_user:
 */
static void
tva_record_set_upd_user( ofoTVARecord *record, const gchar *upd_user )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_UPD_USER, upd_user );
}

/*
 * ofo_tva_record_set_upd_stamp:
 */
static void
tva_record_set_upd_stamp( ofoTVARecord *record, const GTimeVal *upd_stamp )
{
	ofo_base_setter( TVA_RECORD, record, string, TFO_UPD_STAMP, upd_stamp );
}

void
ofo_tva_record_detail_add( ofoTVARecord *record,
							guint level,
							const gchar *code, const gchar *label,
							gboolean has_base, const gchar *base_rule, ofxAmount base,
							gboolean has_amount, const gchar *amount_rule, ofxAmount amount )
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
	ofa_box_set_int( fields, TFO_DET_LEVEL, level );
	ofa_box_set_string( fields, TFO_DET_CODE, code );
	ofa_box_set_string( fields, TFO_DET_LABEL, label );
	ofa_box_set_string( fields, TFO_DET_HAS_BASE, has_base ? "Y":"N" );
	ofa_box_set_string( fields, TFO_DET_BASE_RULE, base_rule );
	ofa_box_set_amount( fields, TFO_DET_BASE, base );
	ofa_box_set_string( fields, TFO_DET_HAS_AMOUNT, has_amount ? "Y":"N" );
	ofa_box_set_string( fields, TFO_DET_AMOUNT_RULE, amount_rule );
	ofa_box_set_amount( fields, TFO_DET_AMOUNT, amount );

	priv->details = g_list_append( priv->details, fields );
}

/**
 * ofo_tva_record_detail_free_all:
 */
void
ofo_tva_record_detail_free_all( ofoTVARecord *record )
{
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	details_list_free( record );
}

/**
 * ofo_tva_record_detail_get_count:
 */
guint
ofo_tva_record_detail_get_count( const ofoTVARecord *record )
{
	ofoTVARecordPrivate *priv;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	return( g_list_length( priv->details ));
}

/**
 * ofo_tva_record_detail_get_level:
 * @idx is the index in the details list, starting with zero
 */
guint
ofo_tva_record_detail_get_level( const ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	guint value;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), 0 );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, 0 );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	value = nth ? ofa_box_get_int( nth->data, TFO_DET_LEVEL ) : 0;

	return( value );
}

/**
 * ofo_tva_record_detail_get_code:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_code( const ofoTVARecord *record, guint idx )
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
ofo_tva_record_detail_get_label( const ofoTVARecord *record, guint idx )
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
 * ofo_tva_record_detail_get_has_base:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_record_detail_get_has_base( const ofoTVARecord *record, guint idx )
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
 * ofo_tva_record_detail_get_base_rule:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_base_rule( const ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_BASE_RULE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_base:
 * @idx is the index in the details list, starting with zero
 */
ofxAmount
ofo_tva_record_detail_get_base( const ofoTVARecord *record, guint idx )
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
ofo_tva_record_detail_get_has_amount( const ofoTVARecord *record, guint idx )
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
 * ofo_tva_record_detail_get_amount_rule:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_record_detail_get_amount_rule( const ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_AMOUNT_RULE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_detail_get_amount:
 * @idx is the index in the details list, starting with zero
 */
ofxAmount
ofo_tva_record_detail_get_amount( const ofoTVARecord *record, guint idx )
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

/**
 * ofo_tva_record_boolean_add:
 * @record:
 * @label:
 * @is_true:
 */
void
ofo_tva_record_boolean_add( ofoTVARecord *record, const gchar *label, gboolean is_true )
{
	ofoTVARecordPrivate *priv;
	GList *fields;

	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
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
 * ofo_tva_record_boolean_free_all:
 */
void
ofo_tva_record_boolean_free_all( ofoTVARecord *record )
{
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));
	g_return_if_fail( !OFO_BASE( record )->prot->dispose_has_run );

	bools_list_free( record );
}

/**
 * ofo_tva_record_boolean_get_count:
 */
guint
ofo_tva_record_boolean_get_count( const ofoTVARecord *record )
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
ofo_tva_record_boolean_get_label( const ofoTVARecord *record, guint idx )
{
	ofoTVARecordPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( record && OFO_IS_TVA_RECORD( record ), NULL );
	g_return_val_if_fail( !OFO_BASE( record )->prot->dispose_has_run, NULL );

	priv = ofo_tva_record_get_instance_private( record );

	nth = g_list_nth( priv->bools, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_BOOL_LABEL ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_record_boolean_get_is_true:
 * @idx is the index in the booleans list, starting with zero
 */
gboolean
ofo_tva_record_boolean_get_is_true( const ofoTVARecord *record, guint idx )
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
 * ofo_tva_record_insert:
 */
gboolean
ofo_tva_record_insert( ofoTVARecord *tva_record, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_tva_record_insert";
	gboolean ok;

	g_debug( "%s: record=%p, hub=%p",
			thisfn, ( void * ) tva_record, ( void * ) hub );

	g_return_val_if_fail( tva_record && OFO_IS_TVA_RECORD( tva_record ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_record )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( record_do_insert( tva_record, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( tva_record ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( tva_record ), ( GCompareFunc ) tva_record_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, tva_record );
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
	gchar *label, *notes, *sbegin, *send, *userid;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_tva_record_get_label( record ));
	notes = my_utils_quote_single( ofo_tva_record_get_notes( record ));
	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), MY_DATE_SQL );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO TVA_T_RECORDS" );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_LABEL,TFO_HAS_CORRESPONDENCE,"
			"	 TFO_NOTES,TFO_VALIDATED,TFO_BEGIN,TFO_END,"
			"	 TFO_UPD_USER, TFO_UPD_STAMP) VALUES ('%s'",
			ofo_tva_record_get_mnemo( record ));

	if( my_strlen( label )){
		g_string_append_printf( query, ",'%s'", label );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	g_string_append_printf( query, ",'%s'", ofo_tva_record_get_has_correspondence( record ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, ",'%s'", notes );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	g_string_append_printf( query, ",'%s'", ofo_tva_record_get_is_validated( record ) ? "Y":"N" );

	if( my_strlen( sbegin )){
		g_string_append_printf( query, ",'%s'", sbegin );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( send )){
		g_string_append_printf( query, ",'%s'", send );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	g_string_append_printf( query,
			",'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	tva_record_set_upd_user( record, userid );
	tva_record_set_upd_stamp( record, &stamp );

	g_free( send );
	g_free( sbegin );
	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

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
record_insert_details_ex( ofoTVARecord *record, const ofaIDBConnect *connect )
{
	ofoTVARecordPrivate *priv;
	gboolean ok;
	GList *idet;
	guint rang;

	priv = ofo_tva_record_get_instance_private( record );

	ok = FALSE;

	if( record_delete_details( record, connect ) && record_delete_bools( record, connect )){
		ok = TRUE;
		for( idet=priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !record_insert_details( record, connect, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
		for( idet=priv->bools, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !record_insert_bools( record, connect, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
record_insert_details( ofoTVARecord *record, const ofaIDBConnect *connect, guint rang, GList *details )
{
	GString *query;
	gboolean ok;
	gchar *send, *code, *label, *base, *amount;
	const gchar *cstr;

	query = g_string_new( "INSERT INTO TVA_T_RECORDS_DET " );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_END,TFO_DET_ROW,"
			"	 TFO_DET_LEVEL,TFO_DET_CODE,TFO_DET_LABEL,"
			"	 TFO_DET_HAS_BASE,TFO_DET_BASE_RULE,TFO_DET_BASE,"
			"	 TFO_DET_HAS_AMOUNT,TFO_DET_AMOUNT_RULE,TFO_DET_AMOUNT) "
			"	VALUES('%s','%s',%d",
			ofo_tva_record_get_mnemo( record ), send, rang );

	g_string_append_printf( query, ",%u", ofa_box_get_int( details, TFO_DET_LEVEL ));

	code = my_utils_quote_single( ofa_box_get_string( details, TFO_DET_CODE ));
	if( my_strlen( code )){
		g_string_append_printf( query, ",'%s'", code );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( code );

	label = my_utils_quote_single( ofa_box_get_string( details, TFO_DET_LABEL ));
	if( my_strlen( label )){
		g_string_append_printf( query, ",'%s'", label );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( label );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_BASE );
	g_string_append_printf( query, ",'%s'", cstr );

	base = my_utils_quote_single( ofa_box_get_string( details, TFO_DET_BASE_RULE ));
	if( my_strlen( base )){
		g_string_append_printf( query, ",'%s'", base );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( base );

	base = my_double_to_sql( ofa_box_get_amount( details, TFO_DET_BASE ));
	g_string_append_printf( query, ",%s", base );
	g_free( base );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_AMOUNT );
	g_string_append_printf( query, ",'%s'", cstr );

	amount = my_utils_quote_single( ofa_box_get_string( details, TFO_DET_AMOUNT_RULE ));
	if( my_strlen( amount )){
		g_string_append_printf( query, ",'%s'", amount );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( amount );

	amount = my_double_to_sql( ofa_box_get_amount( details, TFO_DET_AMOUNT ));
	g_string_append_printf( query, ",%s", amount );
	g_free( amount );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( send );

	return( ok );
}

static gboolean
record_insert_bools( ofoTVARecord *record, const ofaIDBConnect *connect, guint rang, GList *fields )
{
	GString *query;
	gboolean ok;
	gchar *send, *label;
	const gchar *cstr;

	query = g_string_new( "INSERT INTO TVA_T_RECORDS_BOOL " );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_END,TFO_BOOL_ROW,"
			"	 TFO_BOOL_LABEL,TFO_BOOL_TRUE) "
			"	VALUES('%s','%s',%d",
			ofo_tva_record_get_mnemo( record ), send, rang );

	label = my_utils_quote_single( ofa_box_get_string( fields, TFO_BOOL_LABEL ));
	g_string_append_printf( query, ",'%s'", label );
	g_free( label );

	cstr = ofa_box_get_string( fields, TFO_BOOL_TRUE );
	g_string_append_printf( query, ",'%s'", cstr );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( send );

	return( ok );
}

/**
 * ofo_tva_record_update:
 * @record:
 *
 * ofaTVARecordProperties dialog refuses to modify mnemonic and end
 * date: they are set once and never modified.
 */
gboolean
ofo_tva_record_update( ofoTVARecord *tva_record )
{
	static const gchar *thisfn = "ofo_tva_record_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) tva_record );

	g_return_val_if_fail( tva_record && OFO_IS_TVA_RECORD( tva_record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( tva_record ));

	if( record_do_update( tva_record, ofa_hub_get_connect( hub ))){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_ACCOUNT, ( GCompareFunc ) tva_record_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, tva_record, NULL );
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
	gchar *label, *notes, *sbegin, *send, *userid;
	const gchar *mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_tva_record_get_label( record ));
	notes = my_utils_quote_single( ofo_tva_record_get_notes( record ));
	mnemo = ofo_tva_record_get_mnemo( record );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), MY_DATE_SQL );
	send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

	query = g_string_new( "UPDATE TVA_T_RECORDS SET " );

	if( my_strlen( label )){
		g_string_append_printf( query, "TFO_LABEL='%s'", label );
	} else {
		query = g_string_append( query, "TFO_LABEL=NULL" );
	}

	g_string_append_printf(
			query, ",TFO_HAS_CORRESPONDENCE='%s'",
			ofo_tva_record_get_has_correspondence( record ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, ",TFO_NOTES='%s'", notes );
	} else {
		query = g_string_append( query, ",TFO_NOTES=NULL" );
	}

	g_string_append_printf(
			query, ",TFO_VALIDATED='%s'",
			ofo_tva_record_get_is_validated( record ) ? "Y":"N" );

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

	tva_record_set_upd_user( record, userid );
	tva_record_set_upd_stamp( record, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );
	g_free( sbegin );
	g_free( send );
	g_free( userid );

	return( ok );
}

/**
 * ofo_tva_record_delete:
 */
gboolean
ofo_tva_record_delete( ofoTVARecord *tva_record )
{
	static const gchar *thisfn = "ofo_tva_record_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: record=%p", thisfn, ( void * ) tva_record );

	g_return_val_if_fail( tva_record && OFO_IS_TVA_RECORD( tva_record ), FALSE );
	g_return_val_if_fail( ofo_tva_record_is_deletable( tva_record ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_record )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( tva_record ));

	if( record_do_delete( tva_record, ofa_hub_get_connect( hub ))){
		g_object_ref( tva_record );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( tva_record ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, tva_record );
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
record_cmp_by_compare( const ofoTVARecord *a, const sCompare *cmp )
{
	return( record_cmp_by_mnemo_end( a, cmp->mnemo, &cmp->date ));
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

	cmp = g_utf8_collate( akey, bkey );

	g_free( bkey );
	g_free( bend );
	g_free( akey );
	g_free( aend );

	return( cmp );
}

static gint
tva_record_cmp_by_ptr( const ofoTVARecord *a, const ofoTVARecord *b )
{
	return( record_cmp_by_mnemo_end( a, ofo_tva_record_get_mnemo( b ), ofo_tva_record_get_end( b )));
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_record_icollectionable_iface_init";

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
	static const gchar *thisfn = "ofo_tva_record_load_dataset";
	ofoTVARecordPrivate *priv;
	GList *dataset, *it, *ir;
	ofoTVARecord *record;
	gchar *from, *send;
	const ofaIDBConnect *connect;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"TVA_T_RECORDS ORDER BY TFO_MNEMO ASC,TFO_END DESC",
					OFO_TYPE_TVA_RECORD,
					hub );

	connect = ofa_hub_get_connect( hub );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		priv = ofo_tva_record_get_instance_private( record );

		send = my_date_to_str( ofo_tva_record_get_end( record ), MY_DATE_SQL );

		from = g_strdup_printf(
				"TVA_T_RECORDS_DET WHERE TFO_MNEMO='%s' AND TFO_END='%s' ORDER BY TFO_DET_ROW ASC",
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
				"TVA_T_RECORDS_BOOL WHERE TFO_MNEMO='%s' AND TFO_END='%s' ORDER BY TFO_BOOL_ROW ASC",
				ofo_tva_record_get_mnemo( record ), send );
		priv->bools = ofo_base_load_rows( st_bools_defs, connect, from );
		g_free( from );

		g_free( send );
	}

	return( dataset );
}
