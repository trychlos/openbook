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
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-rec-period.h"
#include "ofo-recurrent-gen.h"

/* priv instance data
 */
enum {
	REC_ID = 1,
	REC_ORDER,
	REC_LABEL,
	REC_NOTES,
	REC_UPD_USER,
	REC_UPD_STAMP,
	REC_DET_ID,
	REC_DET_ORDER,
	REC_DET_NUMBER,
	REC_DET_VALUE,
	REC_DET_LABEL,
	REC_DOC_ID,
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
		{ REC_ID,
				"REC_PER_ID",
				"REC_ID",
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ REC_ORDER,
				"REC_PER_ORDER",
				"REC_ORDER",
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ REC_LABEL,
				"REC_PER_LABEL",
				"REC_LABEL",
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ REC_NOTES,
				"REC_PER_NOTES",
				"REC_NOTES",
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ REC_UPD_USER,
				"REC_PER_UPD_USER",
				"REC_UPD_USER",
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ REC_UPD_STAMP,
				"REC_PER_UPD_STAMP",
				"REC_UPD_STAMP",
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_detail_defs[] = {
		{ REC_ID,
				"REC_PER_ID",
				"REC_ID",
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ REC_DET_NUMBER,
				"REC_PER_DET_NUMBER",
				"REC_DET_NUMBER",
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ REC_DET_VALUE,
				"REC_PER_DET_VALUE",
				"REC_DET_VALUE",
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ REC_DET_ID,
				"REC_PER_DET_ID",
				"REC_DET_ID",
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ REC_DET_ORDER,
				"REC_PER_DET_ORDER",
				"REC_DET_ORDER",
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ REC_DET_LABEL,
				"REC_PER_DET_LABEL",
				"REC_DET_LABEL",
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ REC_ID,
				"REC_PER_ID",
				"REC_ID",
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define TABLES_COUNT                    3

typedef struct {
	GList *details;						/* the details of the periodicity as a GList of GList fields */
	GList *docs;
}
	ofoRecPeriodPrivate;

static ofoRecPeriod *period_find_by_id( GList *set, const gchar *id );
static void          rec_period_set_upd_user( ofoRecPeriod *period, const gchar *upd_user );
static void          rec_period_set_upd_stamp( ofoRecPeriod *period, const GTimeVal *upd_stamp );
static void          rec_period_detail_set_id( ofoRecPeriod *period, gint i, ofxCounter id );
static GList        *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean      rec_period_do_insert( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_insert_main( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_do_update( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_update_main( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_delete_details( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_insert_details_ex( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_insert_details( ofoRecPeriod *period, ofaIGetter *getter, guint i );
static gboolean      rec_period_do_delete( ofoRecPeriod *period, ofaIGetter *getter );
static gboolean      rec_period_delete_main( ofoRecPeriod *period, ofaIGetter *getter );
static gint          period_cmp_by_id( ofoRecPeriod *a, const gchar *id );
static void          icollectionable_iface_init( myICollectionableInterface *iface );
static guint         icollectionable_get_interface_version( void );
static GList        *icollectionable_load_collection( void *user_data );
static void          idoc_iface_init( ofaIDocInterface *iface );
static guint         idoc_get_interface_version( void );
static void          iexportable_iface_init( ofaIExportableInterface *iface );
static guint         iexportable_get_interface_version( void );
static gchar        *iexportable_get_label( const ofaIExportable *instance );
static gboolean      iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean      iexportable_export_default( ofaIExportable *exportable );
static void          iimportable_iface_init( ofaIImportableInterface *iface );
static guint         iimportable_get_interface_version( void );
static gchar        *iimportable_get_label( const ofaIImportable *instance );
static guint         iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList        *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoRecPeriod *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static GList        *iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **id );
static void          iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean      period_drop_content( const ofaIDBConnect *connect );
static void          isignalable_iface_init( ofaISignalableInterface *iface );
static void          isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoRecPeriod, ofo_rec_period, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecPeriod )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
details_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
details_list_free( ofoRecPeriod *period )
{
	ofoRecPeriodPrivate *priv;

	priv = ofo_rec_period_get_instance_private( period );

	g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
	priv->details = NULL;
}

static void
rec_period_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_rec_period_finalize";
	ofoRecPeriodPrivate *priv;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, REC_LABEL ));

	g_return_if_fail( instance && OFO_IS_REC_PERIOD( instance ));

	/* free data members here */
	priv = ofo_rec_period_get_instance_private( OFO_REC_PERIOD( instance ));

	if( priv->details ){
		details_list_free( OFO_REC_PERIOD( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rec_period_parent_class )->finalize( instance );
}

static void
rec_period_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_REC_PERIOD( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rec_period_parent_class )->dispose( instance );
}

static void
ofo_rec_period_init( ofoRecPeriod *self )
{
	static const gchar *thisfn = "ofo_rec_period_init";
	ofoRecPeriodPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_rec_period_get_instance_private( self );

	priv->details = NULL;
	priv->docs = NULL;
}

static void
ofo_rec_period_class_init( ofoRecPeriodClass *klass )
{
	static const gchar *thisfn = "ofo_rec_period_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_finalize;
}

/**
 * ofo_rec_period_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoRecPeriod dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_rec_period_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_REC_PERIOD, getter ));
}

/**
 * ofo_rec_period_get_by_id:
 * @getter: a #ofaIGetter instance.
 * @code: the identifier of the period.
 *
 * Returns: the found periodicity, if exists, or %NULL.
 *
 * The returned object is owned by the @hub collector, and should not
 * be released by the caller.
 */
ofoRecPeriod *
ofo_rec_period_get_by_id( ofaIGetter *getter, const gchar *id )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dataset = ofo_rec_period_get_dataset( getter );

	return( period_find_by_id( dataset, id ));
}

static ofoRecPeriod *
period_find_by_id( GList *set, const gchar *id )
{
	GList *found;

	found = g_list_find_custom( set, GINT_TO_POINTER( id ), ( GCompareFunc ) period_cmp_by_id );
	if( found ){
		return( OFO_REC_PERIOD( found->data ));
	}

	return( NULL );
}

/**
 * ofo_rec_period_new:
 * @getter: a #ofaIGetter instance.
 */
ofoRecPeriod *
ofo_rec_period_new( ofaIGetter *getter )
{
	ofoRecPeriod *period;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	period = g_object_new( OFO_TYPE_REC_PERIOD, "ofo-base-getter", getter, NULL );
	OFO_BASE( period )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( period );
}

/**
 * ofo_rec_period_get_id:
 * @period: this #ofoRecPeriod object.
 */
const gchar *
ofo_rec_period_get_id( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, string, NULL, REC_ID );
}

/**
 * ofo_rec_period_get_order:
 * @period: this #ofoRecPeriod object.
 */
guint
ofo_rec_period_get_order( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, int, 0, REC_ORDER );
}

/**
 * ofo_rec_period_get_label:
 * @period: this #ofoRecPeriod object.
 */
const gchar *
ofo_rec_period_get_label( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, string, NULL, REC_LABEL );
}

/**
 * ofo_rec_period_get_notes:
 * @period: this #ofoRecPeriod object.
 */
const gchar *
ofo_rec_period_get_notes( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, string, NULL, REC_NOTES );
}

/**
 * ofo_rec_period_get_upd_user:
 */
const gchar *
ofo_rec_period_get_upd_user( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, string, NULL, REC_UPD_USER );
}

/**
 * ofo_rec_period_get_upd_stamp:
 */
const GTimeVal *
ofo_rec_period_get_upd_stamp( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, timestamp, NULL, REC_UPD_STAMP );
}

/**
 * ofo_rec_period_detail_get_count:
 * @period: this #ofoRecPeriod object.
 *
 * Returns: the count of detail periodicities.
 */
guint
ofo_rec_period_detail_get_count( ofoRecPeriod *period )
{
	ofoRecPeriodPrivate *priv;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	return( g_list_length( priv->details ));
}

/**
 * ofo_rec_period_detail_get_by_id:
 * @period: this #ofoRecPeriod object.
 * @det_id: the identifier of the searched detail.
 *
 * Returns: the index from zero of the @det_id in the list of details,
 * or -1 if not found.
 */
gint
ofo_rec_period_detail_get_by_id( ofoRecPeriod *period, ofxCounter det_id )
{
	ofoRecPeriodPrivate *priv;
	GList *it;
	gint i;
	ofxCounter it_id;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	for( i=0, it=priv->details ; it ; ++i, it=it->next ){
		it_id = ofo_rec_period_detail_get_id( period, i );
		if( it_id == det_id ){
			return( i );
		}
	}

	return( -1 );
}

/**
 * ofo_rec_period_detail_get_id:
 * @period: this #ofoRecPeriod object.
 * @idx: the index of the searched detail, counted from zero.
 */
ofxCounter
ofo_rec_period_detail_get_id( ofoRecPeriod *period, guint idx )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( idx >= 0, 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_counter( nth->data, REC_DET_ID ));
	}

	return( 0 );
}

/**
 * ofo_rec_period_detail_get_order:
 * @period: this #ofoRecPeriod object.
 * @idx: the index of the searched detail, counted from zero.
 */
guint
ofo_rec_period_detail_get_order( ofoRecPeriod *period, guint idx )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( idx >= 0, 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_int( nth->data, REC_DET_ORDER ));
	}

	return( 0 );
}

/**
 * ofo_rec_period_detail_get_number:
 * @period: this #ofoRecPeriod object.
 * @idx: the index of the searched detail, counted from zero.
 */
guint
ofo_rec_period_detail_get_number( ofoRecPeriod *period, guint idx )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( idx >= 0, 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_int( nth->data, REC_DET_NUMBER ));
	}

	return( 0 );
}

/**
 * ofo_rec_period_detail_get_value:
 * @period: this #ofoRecPeriod object.
 * @idx: the index of the searched detail, counted from zero.
 */
guint
ofo_rec_period_detail_get_value( ofoRecPeriod *period, guint idx )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), 0 );
	g_return_val_if_fail( idx >= 0, 0 );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, 0 );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_int( nth->data, REC_DET_VALUE ));
	}

	return( 0 );
}

/**
 * ofo_rec_period_detail_get_label:
 * @period: this #ofoRecPeriod object.
 * @idx: the index of the searched detail, counted from zero.
 */
const gchar *
ofo_rec_period_detail_get_label( ofoRecPeriod *period, guint idx )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, NULL );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, REC_DET_LABEL ));
	}

	return( NULL );
}

/**
 * ofo_rec_period_is_valid_data:
 * @label:
 * @msgerr: [out][allow-none]: error message placeholder
 *
 * Wants a label at least.
 */
gboolean
ofo_rec_period_is_valid_data( const gchar *label, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/**
 * ofo_rec_period_is_deletable:
 *
 * A periodicity may be deleted when it is not referenced anywhere.
 */
gboolean
ofo_rec_period_is_deletable( ofoRecPeriod *period )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( period ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, period, &deletable );
	}

	return( deletable );
}

/**
 * ofo_rec_period_enum_between:
 * @period: this #ofoRecPeriod instance.
 * @detail: the detail identifier.
 * @begin: the beginning date.
 * @end: the ending date.
 * @cb: the user callback.
 * @user_data: a user data provided pointer.
 *
 * Enumerates all valid dates between @begin and @end included dates.
 */
void
ofo_rec_period_enum_between( ofoRecPeriod *period, ofxCounter detail_id,
										const GDate *begin, const GDate *end, RecPeriodEnumBetweenCb cb, void *user_data )
{
	GDate date;
	GDateDay date_month;
	GDateWeekday date_week;
	const gchar *period_id;
	gint idx;
	guint detail_month, detail_week;

	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( !OFO_BASE( period )->prot->dispose_has_run);
	g_return_if_fail( my_date_is_valid( begin ));
	g_return_if_fail( my_date_is_valid( end ));

	my_date_set_from_date( &date, begin );
	period_id = ofo_rec_period_get_id( period );
	idx = ofo_rec_period_detail_get_by_id( period, detail_id );

	detail_week = G_DATE_BAD_WEEKDAY;
	if( idx >= 0 && !my_collate( period_id, REC_PERIOD_WEEKLY )){
		detail_week = ofo_rec_period_detail_get_value( period, idx );
	}

	detail_month = G_DATE_BAD_DAY;
	if( idx >= 0 && !my_collate( period_id, REC_PERIOD_MONTHLY )){
		detail_month = ofo_rec_period_detail_get_value( period, idx );
	}

	while( my_date_compare( &date, end ) <= 0 ){
		if( !my_collate( period_id, REC_PERIOD_WEEKLY )){
			date_week = g_date_get_weekday( &date );
			if(( guint ) date_week == detail_week ){
				( *cb )( &date, user_data );
			}
		}
		if( !my_collate( period_id, REC_PERIOD_MONTHLY )){
			date_month = g_date_get_day( &date );
			if(( guint ) date_month == detail_month ){
				( *cb )( &date, user_data );
			}
		}
		g_date_add_days( &date, 1 );
	}
}

/**
 * ofo_rec_period_set_per_id:
 * @period: this #ofoRecPeriod object.
 * @id:
 */
void
ofo_rec_period_set_id( ofoRecPeriod *period, const gchar *id )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_ID, id );
}

/**
 * ofo_rec_period_set_order:
 * @period: this #ofoRecPeriod object.
 * @order:
 */
void
ofo_rec_period_set_order( ofoRecPeriod *period, guint order )
{
	ofo_base_setter( REC_PERIOD, period, int, REC_ORDER, order );
}

/**
 * ofo_rec_period_set_label:
 * @period: this #ofoRecPeriod object.
 * @label:
 */
void
ofo_rec_period_set_label( ofoRecPeriod *period, const gchar *label )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_LABEL, label );
}

/**
 * ofo_rec_period_set_notes:
 * @period: this #ofoRecPeriod object.
 * @notes:
 */
void
ofo_rec_period_set_notes( ofoRecPeriod *period, const gchar *notes )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_NOTES, notes );
}

/*
 * ofo_rec_period_set_upd_user:
 */
static void
rec_period_set_upd_user( ofoRecPeriod *period, const gchar *upd_user )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_UPD_USER, upd_user );
}

/*
 * ofo_rec_period_set_upd_stamp:
 */
static void
rec_period_set_upd_stamp( ofoRecPeriod *period, const GTimeVal *upd_stamp )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_UPD_STAMP, upd_stamp );
}

/**
 * ofo_rec_period_free_detail_all:
 * @period:
 */
void
ofo_rec_period_free_detail_all( ofoRecPeriod *period )
{
	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( !OFO_BASE( period )->prot->dispose_has_run );

	details_list_free( period );
}

/**
 * ofo_rec_period_add_detail:
 * @period:
 * @order:
 * @label:
 * @number:
 * @value:
 */
void
ofo_rec_period_add_detail( ofoRecPeriod *period, guint order, const gchar *label, guint number, guint value )
{
	ofoRecPeriodPrivate *priv;
	GList *fields;

	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( !OFO_BASE( period )->prot->dispose_has_run );

	priv = ofo_rec_period_get_instance_private( period );

	fields = ofa_box_init_fields_list( st_detail_defs );
	ofa_box_set_counter( fields, REC_ID, ofo_rec_period_get_id( period ));
	ofa_box_set_int( fields, REC_DET_ORDER, order );
	ofa_box_set_string( fields, REC_DET_LABEL, label );
	ofa_box_set_int( fields, REC_DET_NUMBER, number );
	ofa_box_set_int( fields, REC_DET_VALUE, value );

	priv->details = g_list_append( priv->details, fields );
}

/*
 * rec_period_detail_set_id:
 * @period:
 * @idx:
 * @id:
 */
static void
rec_period_detail_set_id( ofoRecPeriod *period, gint idx, ofxCounter id )
{
	ofoRecPeriodPrivate *priv;
	GList *nth;

	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( !OFO_BASE( period )->prot->dispose_has_run );

	priv = ofo_rec_period_get_instance_private( period );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		ofa_box_set_int( nth->data, REC_DET_ID, id );
	}
}

/**
 * ofo_rec_period_get_det_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in REC_T_PERIODS_DET
 * child table.
 *
 * The returned list should be #ofo_rec_period_free_det_orphans() by the
 * caller.
 */
GList *
ofo_rec_period_get_det_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "REC_T_PERIODS_DET" ));
}

/**
 * ofo_rec_period_get_doc_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in REC_T_PERIODS_DOC
 * child table.
 *
 * The returned list should be #ofo_rec_period_free_doc_orphans() by the
 * caller.
 */
GList *
ofo_rec_period_get_doc_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "REC_T_PERIODS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(REC_PER_ID) FROM %s "
			"	WHERE REC_PER_ID NOT IN (SELECT REC_PER_ID FROM REC_T_PERIODS)", table );

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
 * ofo_rec_period_insert:
 * @period:
 */
gboolean
ofo_rec_period_insert( ofoRecPeriod *period )
{
	static const gchar *thisfn = "ofo_rec_period_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;

	g_debug( "%s: period=%p", thisfn, ( void * ) period );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( period ));
	signaler = ofa_igetter_get_signaler( getter );

	/* rationale: see ofo-account.c */
	ofo_rec_period_get_dataset( getter );

	if( rec_period_do_insert( period, getter )){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( period ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, period );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_insert( ofoRecPeriod *period, ofaIGetter *getter )
{
	return( rec_period_insert_main( period, getter ) &&
			rec_period_delete_details( period, getter ) &&
			rec_period_insert_details_ex( period, getter ));
}

static gboolean
rec_period_insert_main( ofoRecPeriod *period, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *sstamp, *notes;
	GTimeVal stamp;
	const gchar *userid;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	my_stamp_set_now( &stamp );
	sstamp = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_PERIODS " );

	g_string_append_printf( query,
			"	(REC_PER_ID,REC_PER_ORDER,REC_PER_LABEL,REC_PER_DETAILS_COUNT,"
			"	 REC_PER_NOTES,REC_PER_UPD_USER, REC_PER_UPD_STAMP) "
			"	VALUES ('%s',%u,'%s',%u,",
			ofo_rec_period_get_id( period ),
			ofo_rec_period_get_order( period ),
			ofo_rec_period_get_label( period ),
			ofo_rec_period_detail_get_count( period ));

	notes = my_utils_quote_sql( ofo_rec_period_get_notes( period ));
	g_string_append_printf( query,
			"'%s','%s','%s')", notes, userid, sstamp );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	rec_period_set_upd_user( period, userid );
	rec_period_set_upd_stamp( period, &stamp );

	g_string_free( query, TRUE );
	g_free( sstamp );
	g_free( notes );

	return( ok );
}

static gboolean
rec_period_insert_details_ex( ofoRecPeriod *period, ofaIGetter *getter )
{
	gboolean ok;
	guint count, i;

	ok = TRUE;
	count = ofo_rec_period_detail_get_count( period );

	for( i=0 ; i<count ; ++i ){
		if( !rec_period_insert_details( period, getter, i )){
			ok = FALSE;
			break;
		}
	}

	return( ok );
}

static gboolean
rec_period_insert_details( ofoRecPeriod *period, ofaIGetter *getter, guint i )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	ofxCounter det_id;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	det_id = ofo_rec_period_detail_get_id( period, i );
	if( !det_id ){
		det_id = ofo_recurrent_gen_get_next_per_det_id( getter );
		rec_period_detail_set_id( period, i, det_id );
	}

	query = g_string_new( "INSERT INTO REC_T_PERIODS_DET " );

	g_string_append_printf( query,
			"	(REC_PER_ID,REC_PER_DET_NUMBER,REC_PER_DET_VALUE,"
			"	 REC_PER_DET_ID,REC_PER_DET_ORDER,REC_PER_DET_LABEL "
			"	VALUES ('%s',%u,%u,%lu,%u,'%s')",
			ofo_rec_period_get_id( period ),
			ofo_rec_period_detail_get_number( period, i ),
			ofo_rec_period_detail_get_value( period, i ),
			det_id,
			ofo_rec_period_detail_get_order( period, i ),
			ofo_rec_period_detail_get_label( period, i ));

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_rec_period_update:
 * @period:
 *
 * Update the @period object.
 */
gboolean
ofo_rec_period_update( ofoRecPeriod *period )
{
	static const gchar *thisfn = "ofo_rec_period_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: period=%p", thisfn, ( void * ) period );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( period ));
	signaler = ofa_igetter_get_signaler( getter );

	if( rec_period_do_update( period, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, period, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_update( ofoRecPeriod *period, ofaIGetter *getter )
{
	return( rec_period_update_main( period, getter ) &&
			rec_period_delete_details( period, getter ) &&
			rec_period_insert_details_ex( period, getter ));
}

static gboolean
rec_period_update_main( ofoRecPeriod *period, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *sstamp, *notes;
	GTimeVal stamp;
	const gchar *userid;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	my_stamp_set_now( &stamp );
	sstamp = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_PERIODS SET " );

	g_string_append_printf( query, "REC_PER_ORDER=%u,", ofo_rec_period_get_order( period ));
	g_string_append_printf( query, "REC_PER_LABEL='%s',", ofo_rec_period_get_label( period ));
	g_string_append_printf( query, "REC_PER_DETAILS_COUNT=%u,", ofo_rec_period_detail_get_count( period ));

	notes = my_utils_quote_sql( ofo_rec_period_get_notes( period ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "REC_PER_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "REC_PER_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	REC_PER_UPD_USER='%s',REC_PER_UPD_STAMP='%s'"
			"	WHERE REC_PER_ID='%s'",
					userid, sstamp, ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	rec_period_set_upd_user( period, userid );
	rec_period_set_upd_stamp( period, &stamp );

	g_string_free( query, TRUE );
	g_free( sstamp );
	g_free( notes );

	return( ok );
}

/**
 * ofo_rec_period_delete:
 * @period:
 *
 * Delete the @period object.
 */
gboolean
ofo_rec_period_delete( ofoRecPeriod *period )
{
	static const gchar *thisfn = "ofo_rec_period_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: period=%p",
			thisfn, ( void * ) period );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( period ));
	signaler = ofa_igetter_get_signaler( getter );

	if( rec_period_do_delete( period, getter )){
		g_object_ref( period );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( period ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, period );
		g_object_unref( period );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_delete( ofoRecPeriod *period, ofaIGetter *getter )
{
	return( rec_period_delete_main( period, getter ) &&
			rec_period_delete_details( period, getter ));
}

static gboolean
rec_period_delete_main( ofoRecPeriod *period, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
			"DELETE FROM REC_T_PERIODS WHERE REC_PER_ID='%s'",
			ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
rec_period_delete_details( ofoRecPeriod *period, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
			"DELETE FROM REC_T_PERIODS_DET WHERE REC_PER_ID='%s'",
			ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
period_cmp_by_id( ofoRecPeriod *a, const gchar *id )
{
	gint cmp;

	cmp = my_collate( ofo_rec_period_get_id( a ), id );

	return( cmp );
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_rec_period_icollectionable_iface_init";

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
	GList *dataset, *it;
	ofoRecPeriod *period;
	ofoRecPeriodPrivate *priv;
	gchar *from;
	ofaHub *hub;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_PERIODS",
					OFO_TYPE_REC_PERIOD,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		period = OFO_REC_PERIOD( it->data );
		priv = ofo_rec_period_get_instance_private( period );
		from = g_strdup_printf(
				"REC_T_PERIODS_DET WHERE REC_PER_ID='%s'",
				ofo_rec_period_get_id( period ));
		priv->details =
				ofo_base_load_rows( st_detail_defs, ofa_hub_get_connect( hub ), from );
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
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_rec_period_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
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
	return( g_strdup( _( "Recurrent _periodicities" )));
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the periodicities.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_rec_period_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTABLE_DEFAULT_FORMAT_ID )){
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
	ofoRecPeriodPrivate *priv;
	GList *dataset, *it, *det;
	gchar *str, *str2;
	ofoRecPeriod *period;
	gboolean ok;
	gchar field_sep;
	gulong count;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_rec_period_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		period = OFO_REC_PERIOD( it->data );
		count += ofo_rec_period_detail_get_count( period );
	}
	ofa_iexportable_set_count( exportable, count );

	/* add new ofsBoxDef array at the end of the list */
	ok = ofa_iexportable_append_headers( exportable,
				TABLES_COUNT, st_boxed_defs, st_detail_defs, st_doc_defs );

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		period = OFO_REC_PERIOD( it->data );
		priv = ofo_rec_period_get_instance_private( period );

		for( det=priv->details ; det ; det=det->next ){
			str = ofa_box_csv_get_line( det->data, stformat, NULL );
			str2 = g_strdup_printf( "2%c%s", field_sep, str );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str );
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
	static const gchar *thisfn = "ofo_rec_period_iimportable_iface_init";

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
 * ofo_rec_period_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - periodicity id (ignored, but for ensure the link with the details)
 * - periodicity display order
 * - label
 * - details count
 * - notes (opt)
 *
 * - 2:
 * - periodicity id (ignored, but for ensure the link with the periodicity)
 * - detail type number
 * - detail value
 * - detail id (ignored)
 * - detail display order
 * - detail label
 *
 * It is not required that the input csv files be sorted by code. We
 * may have all 'period' records, then all 'details' records...
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
		bck_table = ofa_idbconnect_table_backup( connect, "REC_T_PERIODS" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "REC_T_PERIODS_DET" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_REC_PERIOD );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_REC_PERIOD );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "REC_T_PERIODS" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "REC_T_PERIODS_DET" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
	}

	if( dataset ){
		ofo_rec_period_free_dataset( dataset );
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
	guint numline, total;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	gchar *str;
	gint type;
	GList *detail;
	ofoRecPeriod *period;
	ofoRecPeriodPrivate *priv;
	gchar *per_id;

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
				period = iimportable_import_parse_main( importer, parms, numline, itf );
				if( period ){
					dataset = g_list_prepend( dataset, period );
					parms->parsed_count += 1;
					ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
				}
				break;
			case 2:
				per_id = NULL;
				detail = iimportable_import_parse_detail( importer, parms, numline, itf, &per_id );
				if( detail ){
					period = period_find_by_id( dataset, per_id );
					if( period ){
						priv = ofo_rec_period_get_instance_private( period );
						priv->details = g_list_prepend( priv->details, detail );
						total -= 1;
						ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
					}
				}
				g_free( per_id );
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

static ofoRecPeriod *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoRecPeriod *period;

	period = ofo_rec_period_new( parms->getter );

	/* period id (ignored) */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity identifier" ));
		parms->parse_errs += 1;
		g_object_unref( period );
		return( NULL );
	}
	ofo_rec_period_set_id( period, cstr );

	/* period display order */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity order" ));
	} else {
		ofo_rec_period_set_order( period, atoi( cstr ));
	}

	/* period label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity label" ));
		parms->parse_errs += 1;
		g_object_unref( period );
		return( NULL );
	}
	ofo_rec_period_set_label( period, cstr );

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_rec_period_set_notes( period, splitted );
	g_free( splitted );

	return( period );
}

static GList *
iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **per_id )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;

	detail = ofa_box_init_fields_list( st_detail_defs );

	/* period identifier */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity identifier" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	*per_id = g_strdup( cstr );
	ofa_box_set_string( detail, REC_ID, *per_id );

	/* detail type number */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty detail type number" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	ofa_box_set_int( detail, REC_DET_NUMBER, atoi( cstr ));

	/* detail value */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty detail value" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	ofa_box_set_int( detail, REC_DET_VALUE, atoi( cstr ));

	/* detail identifier */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty detail identifier" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	ofa_box_set_counter( detail, REC_DET_ID, atol( cstr ));

	/* detail display order */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty detail order" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	ofa_box_set_int( detail, REC_DET_ORDER, atoi( cstr ));

	/* detail label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, REC_DET_LABEL, cstr );

	return( detail );
}

/*
 * insert records
 *
 * A new identifier is always attributed on insertion
 * no duplicate management here
 */
static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	guint total;
	ofoRecPeriod *period;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		period_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		period = OFO_REC_PERIOD( it->data );

		if( rec_period_do_insert( period, parms->getter )){
			parms->inserted_count += 1;
		} else {
			parms->insert_errs += 1;
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
period_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM REC_T_PERIODS", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM REC_T_PERIODS_DET", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_rec_period_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_rec_period_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
