/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
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
	REC_HAVE_DETAIL,
	REC_ADD_TYPE,
	REC_ADD_COUNT,
	REC_NOTES,
	REC_UPD_USER,
	REC_UPD_STAMP,
	REC_DET_ID,
	REC_DET_ORDER,
	REC_DET_LABEL,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ REC_ID,
				"REC_PER_ID",
				"REC_ID",
				OFA_TYPE_COUNTER,
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
		{ REC_HAVE_DETAIL,
				"REC_PER_HAVE_DETAIL",
				"REC_HAVE_DETAIL",
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ REC_ADD_TYPE,
				"REC_PER_ADD_TYPE",
				"REC_ADD_TYPE",
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ REC_ADD_COUNT,
				"REC_PER_ADD_COUNT",
				"REC_ADD_COUNT",
				OFA_TYPE_INTEGER,
				FALSE,
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
				OFA_TYPE_COUNTER,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
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

typedef struct {
	/* the details of the periodicity as a GList of GList fields
	 */
	GList     *details;
}
	ofoRecPeriodPrivate;

static ofoRecPeriod *period_find_by_id( GList *set, ofxCounter id );
static void          rec_period_set_id( ofoRecPeriod *period, ofxCounter id );
static void          rec_period_set_upd_user( ofoRecPeriod *period, const gchar *upd_user );
static void          rec_period_set_upd_stamp( ofoRecPeriod *period, const GTimeVal *upd_stamp );
static void          rec_period_detail_set_id( ofoRecPeriod *period, gint i, ofxCounter id );
static gboolean      rec_period_do_insert( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_insert_main( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_do_update( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_update_main( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_delete_details( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_insert_details_ex( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_insert_details( ofoRecPeriod *period, ofaHub *hub, guint i );
static gboolean      rec_period_do_delete( ofoRecPeriod *period, ofaHub *hub );
static gboolean      rec_period_delete_main( ofoRecPeriod *period, ofaHub *hub );
static gint          period_cmp_by_id( ofoRecPeriod *a, void *pid );
static void          icollectionable_iface_init( myICollectionableInterface *iface );
static guint         icollectionable_get_interface_version( void );
static GList        *icollectionable_load_collection( void *user_data );
static void          iexportable_iface_init( ofaIExportableInterface *iface );
static guint         iexportable_get_interface_version( void );
static gchar        *iexportable_get_label( const ofaIExportable *instance );
static gboolean      iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub );
static void          iimportable_iface_init( ofaIImportableInterface *iface );
static guint         iimportable_get_interface_version( void );
static gchar        *iimportable_get_label( const ofaIImportable *instance );
static guint         iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList        *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoRecPeriod *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static GList        *iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, ofxCounter *id );
static void          iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean      period_drop_content( const ofaIDBConnect *connect );
static void          isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void          isignal_hub_connect( ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoRecPeriod, ofo_rec_period, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecPeriod )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
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
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoRecPeriod dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_rec_period_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_REC_PERIOD, hub ));
}

/**
 * ofo_rec_period_get_by_id:
 * @hub: the current #ofaHub object.
 * @code: the identifier of the period.
 *
 * Returns: the found periodicity, if exists, or %NULL.
 *
 * The returned object is owned by the @hub collector, and should not
 * be released by the caller.
 */
ofoRecPeriod *
ofo_rec_period_get_by_id( ofaHub *hub, ofxCounter id )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_rec_period_get_dataset( hub );

	return( period_find_by_id( dataset, id ));
}

static ofoRecPeriod *
period_find_by_id( GList *set, ofxCounter id )
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
 */
ofoRecPeriod *
ofo_rec_period_new( void )
{
	ofoRecPeriod *period;

	period = g_object_new( OFO_TYPE_REC_PERIOD, NULL );
	OFO_BASE( period )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( period );
}

/**
 * ofo_rec_period_get_id:
 * @period: this #ofoRecPeriod object.
 */
ofxCounter
ofo_rec_period_get_id( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, counter, 0, REC_ID );
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
 * ofo_rec_period_get_have_details:
 * @period: this #ofoRecPeriod object.
 */
gboolean
ofo_rec_period_get_have_details( ofoRecPeriod *period )
{
	const gchar *cstr;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( period )->prot->fields, REC_HAVE_DETAIL );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_rec_period_get_add_type:
 * @period: this #ofoRecPeriod object.
 */
const gchar *
ofo_rec_period_get_add_type( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, string, NULL, REC_ADD_TYPE );
}

/**
 * ofo_rec_period_get_add_count:
 * @period: this #ofoRecPeriod object.
 */
guint
ofo_rec_period_get_add_count( ofoRecPeriod *period )
{
	ofo_base_getter( REC_PERIOD, period, int, 0, REC_ADD_COUNT );
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
 * ofo_rec_period_is_add_type_valid:
 * @add_type:
 */
gboolean
ofo_rec_period_is_add_type_valid( const gchar *add_type )
{
	return( !my_collate( add_type, REC_PERIOD_DAY ) ||
			!my_collate( add_type, REC_PERIOD_WEEK ) ||
			!my_collate( add_type, REC_PERIOD_MONTH ));
}

/**
 * ofo_rec_period_is_valid_data:
 * @label:
 * @have_details:
 * @add_type:
 * @add_count:
 * @msgerr: [out][allow-none]: error message placeholder
 *
 * Wants a label at least.
 */
gboolean
ofo_rec_period_is_valid_data( const gchar *label, gboolean have_details, const gchar *add_type, guint add_count, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
		}
		return( FALSE );
	}
	if( !my_strlen( add_type )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty add type" ));
		}
		return( FALSE );
	}
	if( !ofo_rec_period_is_add_type_valid( add_type )){
		if( msgerr ){
			*msgerr = g_strdup_printf( _( "Invalid add type: %s" ), add_type );
		}
		return( FALSE );
	}
	if( !add_count ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Add count must be greater than zero" ));
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
	ofaHub *hub;
	gboolean deletable;

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	hub = ofo_base_get_hub( OFO_BASE( period ));

	if( hub && deletable ){
		g_signal_emit_by_name( hub, SIGNAL_HUB_DELETABLE, period, &deletable );
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
#if 0
	GDate date;
	GDateDay date_day;
	GDateWeekday date_week;
	gint detail_day, detail_week;

	g_return_if_fail( period && OFO_IS_REC_PERIOD( period ));
	g_return_if_fail( !OFO_BASE( period )->prot->dispose_has_run);
	g_return_if_fail( my_date_is_valid( begin ));
	g_return_if_fail( my_date_is_valid( end ));

	my_date_set_from_date( &date, begin );
	detail_day = my_collate( periodicity, PER_MONTHLY ) ? G_DATE_BAD_DAY : atoi( detail );
	detail_week = my_collate( periodicity, PER_WEEKLY ) ? G_DATE_BAD_WEEKDAY : get_weekday( detail );

	while( my_date_compare( &date, end ) <= 0 ){
		if( !my_collate( periodicity, PER_MONTHLY )){
			date_day = g_date_get_day( &date );
			if( date_day == detail_day ){
				( *cb )( &date, user_data );
			}
		}
		if( !my_collate( periodicity, PER_WEEKLY )){
			date_week = g_date_get_weekday( &date );
			if( date_week == detail_week ){
				( *cb )( &date, user_data );
			}
		}
		g_date_add_days( &date, 1 );
	}
}

static gint
get_weekday( const gchar *detail )
{
	gint i;

	for( i=0 ; st_weekly_days[i].code ; ++i ){
		if( !my_collate( st_weekly_days[i].code, detail )){
			return( st_weekly_days[i].weekday );
		}
	}

	return( G_DATE_BAD_WEEKDAY );
#endif
}

/*
 * rec_period_set_per_id:
 * @period: this #ofoRecPeriod object.
 * @order:
 */
static void
rec_period_set_id( ofoRecPeriod *period, ofxCounter id )
{
	ofo_base_setter( REC_PERIOD, period, counter, REC_ID, id );
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
 * ofo_rec_period_set_have_details:
 * @period: this #ofoRecPeriod object.
 * @have_detail:
 */
void
ofo_rec_period_set_have_details( ofoRecPeriod *period, gboolean have_detail )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_HAVE_DETAIL, have_detail ? "Y":"N" );
}

/**
 * ofo_rec_period_set_add_type:
 * @period: this #ofoRecPeriod object.
 * @add_type:
 */
void
ofo_rec_period_set_add_type( ofoRecPeriod *period, const gchar *add_type )
{
	ofo_base_setter( REC_PERIOD, period, string, REC_ADD_TYPE, add_type );
}

/**
 * ofo_rec_period_set_add_count:
 * @period: this #ofoRecPeriod object.
 * @add_count:
 */
void
ofo_rec_period_set_add_count( ofoRecPeriod *period, guint count )
{
	ofo_base_setter( REC_PERIOD, period, int, REC_ADD_COUNT, count );
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
 */
void
ofo_rec_period_add_detail( ofoRecPeriod *period, guint order, const gchar *label )
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
 * ofo_rec_period_insert:
 * @period:
 */
gboolean
ofo_rec_period_insert( ofoRecPeriod *period, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_rec_period_insert";
	gboolean ok;

	g_debug( "%s: period=%p, hub=%p",
			thisfn, ( void * ) period, ( void * ) hub );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( rec_period_do_insert( period, hub )){
		ofo_base_set_hub( OFO_BASE( period ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( period ), NULL, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, period );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_insert( ofoRecPeriod *period, ofaHub *hub )
{
	return( rec_period_insert_main( period, hub ) &&
			rec_period_delete_details( period, hub ) &&
			rec_period_insert_details_ex( period, hub ));
}

static gboolean
rec_period_insert_main( ofoRecPeriod *period, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	GString *query;
	const gchar *addtype;
	gchar *userid, *sstamp, *notes;
	GTimeVal stamp;
	ofxCounter id;

	connect = ofa_hub_get_connect( hub );

	id = ofo_recurrent_gen_get_next_per_id( hub );
	rec_period_set_id( period, id );

	userid = ofa_idbconnect_get_account( connect );
	my_utils_stamp_set_now( &stamp );
	sstamp = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_PERIODS " );

	g_string_append_printf( query,
			"	(REC_PER_ID,REC_PER_ORDER,REC_PER_LABEL,REC_PER_HAVE_DETAIL,"
			"	 REC_PER_ADD_TYPE,REC_PER_ADD_COUNT,"
			"	 REC_PER_NOTES,REC_PER_UPD_USER, REC_PER_UPD_STAMP) "
			"	VALUES (%lu,%u,'%s','%s',",
			id,
			ofo_rec_period_get_order( period ),
			ofo_rec_period_get_label( period ),
			ofo_rec_period_get_have_details( period ) ? "Y":"N" );

	addtype = ofo_rec_period_get_add_type( period );
	if( my_strlen( addtype )){
		g_string_append_printf( query, "'%s',%d,", addtype, ofo_rec_period_get_add_count( period ));
	} else {
		query = g_string_append( query, "NULL,NULL," );
	}

	notes = my_utils_quote_sql( ofo_rec_period_get_notes( period ));
	g_string_append_printf( query,
			"'%s','%s','%s')", notes, userid, sstamp );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	rec_period_set_upd_user( period, userid );
	rec_period_set_upd_stamp( period, &stamp );

	g_string_free( query, TRUE );
	g_free( sstamp );
	g_free( userid );
	g_free( notes );

	return( ok );
}

static gboolean
rec_period_insert_details_ex( ofoRecPeriod *period, ofaHub *hub )
{
	gboolean ok;
	guint count, i;

	ok = TRUE;
	count = ofo_rec_period_detail_get_count( period );

	for( i=0 ; i<count ; ++i ){
		if( !rec_period_insert_details( period, hub, i )){
			ok = FALSE;
			break;
		}
	}

	return( ok );
}

static gboolean
rec_period_insert_details( ofoRecPeriod *period, ofaHub *hub, guint i )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	GString *query;
	ofxCounter det_id;

	connect = ofa_hub_get_connect( hub );

	det_id = ofo_rec_period_detail_get_id( period, i );
	if( !det_id ){
		det_id = ofo_recurrent_gen_get_next_per_det_id( hub );
		rec_period_detail_set_id( period, i, det_id );
	}

	query = g_string_new( "INSERT INTO REC_T_PERIODS_DET " );

	g_string_append_printf( query,
			"	(REC_PER_ID,REC_PER_DET_ID,REC_PER_DET_ORDER,REC_PER_DET_LABEL "
			"	VALUES (%lu,%lu,%u,'%s')",
			ofo_rec_period_get_id( period ),
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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: period=%p", thisfn, ( void * ) period );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( period ));

	if( rec_period_do_update( period, hub )){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, period, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_update( ofoRecPeriod *period, ofaHub *hub )
{
	return( rec_period_update_main( period, hub ) &&
			rec_period_delete_details( period, hub ) &&
			rec_period_insert_details_ex( period, hub ));
}

static gboolean
rec_period_update_main( ofoRecPeriod *period, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *userid, *sstamp, *notes;
	const gchar *addtype;
	GTimeVal stamp;

	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	my_utils_stamp_set_now( &stamp );
	sstamp = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_PERIODS SET " );

	g_string_append_printf( query, "REC_PER_ORDER=%u,", ofo_rec_period_get_order( period ));
	g_string_append_printf( query, "REC_PER_LABEL='%s',", ofo_rec_period_get_label( period ));
	g_string_append_printf( query, "REC_PER_HAVE_DETAIL='%s',", ofo_rec_period_get_have_details( period ) ? "Y":"N" );

	addtype = ofo_rec_period_get_add_type( period );
	if( my_strlen( addtype )){
		g_string_append_printf( query, "REC_PER_ADD_TYPE='%s',", addtype );
		g_string_append_printf( query, "REC_PER_ADD_COUNT=%d,", ofo_rec_period_get_add_count( period ));
	} else {
		query = g_string_append( query, "REC_PER_ADD_TYPE=NULL," );
		query = g_string_append( query, "REC_PER_ADD_COUNT=NULL," );
	}

	notes = my_utils_quote_sql( ofo_rec_period_get_notes( period ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "REC_PER_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "REC_PER_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	REC_PER_UPD_USER='%s',REC_PER_UPD_STAMP='%s'"
			"	WHERE REC_PER_ID=%lu",
					userid, sstamp, ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	rec_period_set_upd_user( period, userid );
	rec_period_set_upd_stamp( period, &stamp );

	g_string_free( query, TRUE );
	g_free( sstamp );
	g_free( userid );
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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: period=%p",
			thisfn, ( void * ) period );

	g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), FALSE );
	g_return_val_if_fail( !OFO_BASE( period )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( period ));

	if( rec_period_do_delete( period, hub )){
		g_object_ref( period );
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( period ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, period );
		g_object_unref( period );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
rec_period_do_delete( ofoRecPeriod *period, ofaHub *hub )
{
	return( rec_period_delete_main( period, hub ) &&
			rec_period_delete_details( period, hub ));
}

static gboolean
rec_period_delete_main( ofoRecPeriod *period, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	gchar *query;

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
			"DELETE FROM REC_T_PERIODS WHERE REC_PER_ID=%lu",
			ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
rec_period_delete_details( ofoRecPeriod *period, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	gchar *query;

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
			"DELETE FROM REC_T_PERIODS_DET WHERE REC_PER_ID=%lu",
			ofo_rec_period_get_id( period ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
period_cmp_by_id( ofoRecPeriod *a, void *pid )
{
	ofxCounter id, aid;
	gint cmp;

	id = GPOINTER_TO_INT( pid );
	aid = ofo_rec_period_get_id( a );
	cmp = aid < id ? -1 : ( aid > id ? 1 : 0 );

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

	g_return_val_if_fail( user_data && OFA_IS_HUB( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_PERIODS",
					OFO_TYPE_REC_PERIOD,
					OFA_HUB( user_data ));

	for( it=dataset ; it ; it=it->next ){
		period = OFO_REC_PERIOD( it->data );
		priv = ofo_rec_period_get_instance_private( period );
		from = g_strdup_printf(
				"REC_T_PERIODS_DET WHERE REC_PER_ID=%lu",
				ofo_rec_period_get_id( period ));
		priv->details =
				ofo_base_load_rows( st_detail_defs, ofa_hub_get_connect( OFA_HUB( user_data )), from );
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
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub )
{
	ofoRecPeriodPrivate *priv;
	GList *dataset, *it, *det;
	gchar *str, *str2;
	ofoRecPeriod *period;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;

	dataset = ofo_rec_period_get_dataset( hub );

	with_headers = ofa_stream_format_get_with_headers( settings );
	field_sep = ofa_stream_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=dataset ; it ; it=it->next ){
		period = OFO_REC_PERIOD( it->data );
		count += ofo_rec_period_detail_get_count( period );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_detail_defs, settings );
		str2 = g_strdup_printf( "2%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		period = OFO_REC_PERIOD( it->data );
		priv = ofo_rec_period_get_instance_private( period );

		for( det=priv->details ; det ; det=det->next ){
			str = ofa_box_csv_get_line( det->data, settings );
			str2 = g_strdup_printf( "2%c%s", field_sep, str );
			ok = ofa_iexportable_set_line( exportable, str2 );
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
 * - periodicity order
 * - label
 * - have detail
 * - add type
 * - add count
 * - notes (opt)
 *
 * - 2:
 * - periodicity id (ignored, but for ensure the link with the periodicity)
 * - detail id (ignored)
 * - detail order
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
	GList *dataset;
	gchar *bck_table, *bck_det_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "REC_T_PERIODS" );
		bck_det_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "REC_T_PERIODS_DET" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_hub_get_collector( parms->hub ), OFO_TYPE_REC_PERIOD );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_REC_PERIOD );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "REC_T_PERIODS" );
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_det_table, "REC_T_PERIODS_DET" );
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
	ofxCounter per_id;

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
				per_id = 0;
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
	gchar *str, *splitted;
	ofoRecPeriod *period;
	gboolean have_add_type;
	guint count;

	period = ofo_rec_period_new();

	/* period id (ignored) */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity identifier" ));
		parms->parse_errs += 1;
		g_object_unref( period );
		return( NULL );
	}
	rec_period_set_id( period, atol( cstr ));

	/* period order */
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

	/* have detail */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty 'have detail' indicator" ));
		parms->parse_errs += 1;
		g_object_unref( period );
		return( NULL );
	}
	ofo_rec_period_set_have_details( period, my_utils_boolean_from_str( cstr ));

	/* add type */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	have_add_type = my_strlen( cstr ) > 0;
	if( have_add_type ){
		if( !ofo_rec_period_is_add_type_valid( cstr )){
			str = g_strdup_printf( _( "invalid add type: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			g_object_unref( period );
			return( NULL );
		}
		ofo_rec_period_set_add_type( period, cstr );
	}

	/* add count */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( have_add_type ){
		if( my_strlen( cstr )){
			count = atoi( cstr );
			if( count == 0 ){
				str = g_strdup_printf( _( "invalid add count: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				g_object_unref( period );
				return( NULL );
			}
			ofo_rec_period_set_add_count( period, count );
		} else {
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "add type was set, but add count is not"));
			parms->parse_errs += 1;
			g_object_unref( period );
			return( NULL );
		}
	} else if( my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "add type was not set, but add count is set: ignored" ));
	}

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
iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, ofxCounter *per_id )
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
	*per_id = atol( cstr );
	ofa_box_set_counter( detail, REC_ID, *per_id );

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

	/* detail order */
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
	const ofaIDBConnect *connect;
	guint total;
	ofoRecPeriod *period;

	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		period_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		period = OFO_REC_PERIOD( it->data );

		if( rec_period_do_insert( period, parms->hub )){
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
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_rec_period_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_rec_period_isignal_hub_connect";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}
