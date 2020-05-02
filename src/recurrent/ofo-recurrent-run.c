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

#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-recurrent-gen.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* priv instance data
 */
enum {
	REC_NUMSEQ = 1,
	REC_MNEMO,
	REC_DATE,
	REC_LABEL,
	REC_OPE_TEMPLATE,
	REC_PERIOD_ID,
	REC_PERIOD_N,
	REC_PERIOD_DET,
	REC_END,
	REC_CRE_USER,
	REC_CRE_STAMP,
	REC_STATUS,
	REC_STA_USER,
	REC_STA_STAMP,
	REC_AMOUNT1,
	REC_AMOUNT2,
	REC_AMOUNT3,
	REC_EDI_USER,
	REC_EDI_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( REC_NUMSEQ ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( REC_DATE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_OPE_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_PERIOD_ID ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_PERIOD_N ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_PERIOD_DET ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( REC_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( REC_STATUS ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_STA_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( REC_STA_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( REC_AMOUNT1 ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_AMOUNT2 ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_AMOUNT3 ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_EDI_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( REC_EDI_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

typedef struct {
	myPeriod *period;
}
	ofoRecurrentRunPrivate;

/* manage the status
 * - the identifier is from a public enum (easier for the code)
 * - a non-localized char stored in dbms
 * - a localized char (short string for treeviews)
 * - a localized label
 */
typedef struct {
	ofeRecurrentStatus id;
	const gchar       *dbms;
	const gchar       *abr;
	const gchar       *label;
}
	sStatus;

static sStatus st_status[] = {
		{ REC_STATUS_CANCELLED, "C", N_( "C" ), N_( "Cancelled" ) },
		{ REC_STATUS_WAITING,   "W", N_( "W" ), N_( "Waiting" ) },
		{ REC_STATUS_VALIDATED, "V", N_( "A" ), N_( "Accounted" ) },
		{ 0 },
};

static void       recurrent_run_set_numseq( ofoRecurrentRun *model, ofxCounter numseq );
static void       recurrent_run_set_period( ofoRecurrentRun *run, myPeriod *period );
static void       recurrent_run_set_cre_user( ofoRecurrentRun *model, const gchar *user );
static void       recurrent_run_set_cre_stamp( ofoRecurrentRun *model, const myStampVal *stamp );
static void       recurrent_run_set_sta_user( ofoRecurrentRun *model, const gchar *user );
static void       recurrent_run_set_sta_stamp( ofoRecurrentRun *model, const myStampVal *stamp );
static void       recurrent_run_set_edi_user( ofoRecurrentRun *model, const gchar *user );
static void       recurrent_run_set_edi_stamp( ofoRecurrentRun *model, const myStampVal *stamp );
static GList     *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean   recurrent_run_do_insert( ofoRecurrentRun *model, ofaIGetter *getter );
static gboolean   recurrent_run_insert_main( ofoRecurrentRun *model, ofaIGetter *getter );
static gboolean   recurrent_run_do_update_status( ofoRecurrentRun *model, ofaIGetter *getter );
static gboolean   recurrent_run_do_update_amounts( ofoRecurrentRun *model, ofaIGetter *getter );
static gint       recurrent_run_cmp_by_mnemo_date( const ofoRecurrentRun *a, const gchar *mnemo, const GDate *date, ofeRecurrentStatus status );
static gint       recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );
static GList     *icollectionable_load_collection( void *user_data );
static void       idoc_iface_init( ofaIDocInterface *iface );
static guint      idoc_get_interface_version( void );
static void       isignalable_iface_init( ofaISignalableInterface *iface );
static void       isignalable_connect_to( ofaISignaler *signaler );
static gboolean   signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean   signaler_is_deletable_recurrent_model( ofaISignaler *signaler, ofoRecurrentModel *model );
static void       signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean   signaler_on_updated_rec_model_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentRun, ofo_recurrent_run, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentRun )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
recurrent_run_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_recurrent_run_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, REC_MNEMO ));

	g_return_if_fail( instance && OFO_IS_RECURRENT_RUN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_run_parent_class )->finalize( instance );
}

static void
recurrent_run_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RECURRENT_RUN( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_run_parent_class )->dispose( instance );
}

static void
ofo_recurrent_run_init( ofoRecurrentRun *self )
{
	static const gchar *thisfn = "ofo_recurrent_run_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_recurrent_run_class_init( ofoRecurrentRunClass *klass )
{
	static const gchar *thisfn = "ofo_recurrent_run_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_finalize;
}

/**
 * ofo_recurrent_run_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoRecurrentRun dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_recurrent_run_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_RECURRENT_RUN, getter ));
}

/**
 * ofo_recurrent_run_get_by_id:
 * @getter: a #ofaIGetter instance.
 * @mnemo: the mnemonic identifier of the model.
 * @date: the date of the recurrent operation.
 *
 * Returns: the found operation, if exists, or %NULL.
 *
 * We are searching here for an operation with same mnemo+date, which
 * would have a 'Waiting' or 'Validated' status.
 * Cancelled operations are silently ignored.
 *
 * The returned object is owned by the @hub collector, and should not
 * be released by the caller.
 */
ofoRecurrentRun *
ofo_recurrent_run_get_by_id( ofaIGetter *getter, const gchar *mnemo, const GDate *date )
{
	GList *dataset, *it;
	ofoRecurrentRun *ope;
	const gchar *mnemo_ope;
	const GDate *date_ope;
	ofeRecurrentStatus status_ope;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !my_strlen( mnemo ) || !my_date_is_valid( date )){
		return( NULL );
	}

	dataset = ofo_recurrent_run_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		ope = OFO_RECURRENT_RUN( it->data );

		status_ope = ofo_recurrent_run_get_status( ope );
		if( status_ope == REC_STATUS_CANCELLED ){
			continue;
		}
		g_return_val_if_fail(
				status_ope == REC_STATUS_WAITING || status_ope == REC_STATUS_VALIDATED,
				NULL );

		mnemo_ope = ofo_recurrent_run_get_mnemo( ope );
		g_return_val_if_fail( my_strlen( mnemo_ope ), NULL );

		date_ope = ofo_recurrent_run_get_date( ope );
		g_return_val_if_fail( my_date_is_valid( date_ope ), NULL );

		if( !my_collate( mnemo_ope, mnemo ) && !my_date_compare( date_ope, date )){
			return( ope );
		}
	}

	return( NULL );
}

/**
 * ofo_recurrent_run_new:
 * @model: the #ofoRecurrentModel object which serves as a template.
 */
ofoRecurrentRun *
ofo_recurrent_run_new( ofoRecurrentModel *model )
{
	ofaIGetter *getter;
	ofoRecurrentRun *run;
	myPeriod *period;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), NULL );

	getter = ofo_base_get_getter( OFO_BASE( model ));
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	run = g_object_new( OFO_TYPE_RECURRENT_RUN, "ofo-base-getter", getter, NULL );
	OFO_BASE( run )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	ofa_box_set_string( OFO_BASE( run )->prot->fields, REC_MNEMO, ofo_recurrent_model_get_mnemo( model ));
	ofa_box_set_string( OFO_BASE( run )->prot->fields, REC_LABEL, ofo_recurrent_model_get_label( model ));
	ofa_box_set_string( OFO_BASE( run )->prot->fields, REC_OPE_TEMPLATE, ofo_recurrent_model_get_ope_template( model ));
	ofa_box_set_date( OFO_BASE( run )->prot->fields, REC_END, ofo_recurrent_model_get_end( model ));

	period = ofo_recurrent_model_get_period( model );
	recurrent_run_set_period( run, period );

	ofo_recurrent_run_set_status( run, REC_STATUS_WAITING );

	return( run );
}

/**
 * ofo_recurrent_run_get_numseq:
 */
ofxCounter
ofo_recurrent_run_get_numseq( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, counter, 0, REC_NUMSEQ );
}

/**
 * ofo_recurrent_run_get_mnemo:
 * @model:
 */
const gchar *
ofo_recurrent_run_get_mnemo( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_MNEMO );
}

/**
 * ofo_recurrent_run_get_date:
 */
const GDate *
ofo_recurrent_run_get_date( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, date, NULL, REC_DATE );
}

/**
 * ofo_recurrent_run_get_label:
 * @model:
 */
const gchar *
ofo_recurrent_run_get_label( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_LABEL );
}

/**
 * ofo_recurrent_run_get_ope_template:
 * @model:
 */
const gchar *
ofo_recurrent_run_get_ope_template( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_OPE_TEMPLATE );
}

/**
 * ofo_recurrent_run_get_period:
 * @run:
 */
myPeriod *
ofo_recurrent_run_get_period( ofoRecurrentRun *run )
{
	ofoRecurrentRunPrivate *priv;

	g_return_val_if_fail( run && OFO_IS_RECURRENT_RUN( run ), NULL );
	g_return_val_if_fail( !OFO_BASE( run )->prot->dispose_has_run, NULL );

	priv = ofo_recurrent_run_get_instance_private( run );

	return( priv->period );
}

/**
 * ofo_recurrent_run_get_end:
 * @model:
 */
const GDate *
ofo_recurrent_run_get_end( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, date, NULL, REC_DATE );
}

/**
 * ofo_recurrent_run_get_cre_user:
 */
const gchar *
ofo_recurrent_run_get_cre_user( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_CRE_USER );
}

/**
 * ofo_recurrent_run_get_cre_stamp:
 */
const myStampVal *
ofo_recurrent_run_get_cre_stamp( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, timestamp, NULL, REC_CRE_STAMP );
}

/**
 * ofo_recurrent_run_get_status:
 * @recrun: this #ofoRecurrentRun object.
 *
 * Returns: the #ofeRecurrentStatus status.
 */
ofeRecurrentStatus
ofo_recurrent_run_get_status( const ofoRecurrentRun *recrun )
{
	static const gchar *thisfn = "ofo_recurrent_run_get_status";
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( recrun && OFO_IS_RECURRENT_RUN( recrun ), 0 );
	g_return_val_if_fail( !OFO_BASE( recrun )->prot->dispose_has_run, 0 );

	cstr = ofa_box_get_string( OFO_BASE( recrun )->prot->fields, REC_STATUS );

	for( i=0 ; st_status[i].id ; ++i ){
		if( !my_collate( st_status[i].dbms, cstr )){
			return( st_status[i].id );
		}
	}

	g_warning( "%s: unknown or invalid dbms status: %s", thisfn, cstr );

	return( 0 );
}

/**
 * ofo_recurrent_run_status_get_dbms:
 * @status: a #ofeRecurrentStatus status.
 *
 * Returns: the dbms string corresponding to the @status.
 */
const gchar *
ofo_recurrent_run_status_get_dbms( ofeRecurrentStatus status )
{
	static const gchar *thisfn = "ofo_recurrent_run_status_get_dbms";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( st_status[i].dbms );
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_recurrent_run_status_get_abr:
 * @status: a #ofeRecurrentStatus status.
 *
 * Returns: an short localized string corresponding to the @status.
 */
const gchar *
ofo_recurrent_run_status_get_abr( ofeRecurrentStatus status )
{
	static const gchar *thisfn = "ofo_recurrent_run_status_get_abr";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( st_status[i].abr );
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_recurrent_run_status_get_label:
 * @status: a #ofeRecurrentStatus status.
 *
 * Returns: a localized label string corresponding to the @status.
 */
const gchar *
ofo_recurrent_run_status_get_label( ofeRecurrentStatus status )
{
	static const gchar *thisfn = "ofo_recurrent_run_status_get_label";
	gint i;

	for( i=0 ; st_status[i].id ; ++i ){
		if( st_status[i].id == status ){
			return( st_status[i].label );
		}
	}

	g_warning( "%s: unknown or invalid status identifier: %u", thisfn, status );

	return( "" );
}

/**
 * ofo_recurrent_run_get_sta_user:
 */
const gchar *
ofo_recurrent_run_get_sta_user( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_STA_USER );
}

/**
 * ofo_recurrent_run_get_sta_stamp:
 */
const myStampVal *
ofo_recurrent_run_get_sta_stamp( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, timestamp, NULL, REC_STA_STAMP );
}

/**
 * ofo_recurrent_run_get_amount1:
 * @recope: this #ofoRecurrentRun object.
 *
 * Returns: the #1 @amount.
 */
ofxAmount
ofo_recurrent_run_get_amount1( const ofoRecurrentRun *recope )
{
	ofo_base_getter( RECURRENT_RUN, recope, amount, 0, REC_AMOUNT1 );
}

/**
 * ofo_recurrent_run_get_amount2:
 * @recope: this #ofoRecurrentRun object.
 *
 * Returns: the #2 @amount.
 */
ofxAmount
ofo_recurrent_run_get_amount2( const ofoRecurrentRun *recope )
{
	ofo_base_getter( RECURRENT_RUN, recope, amount, 0, REC_AMOUNT2 );
}

/**
 * ofo_recurrent_run_get_amount3:
 * @recope: this #ofoRecurrentRun object.
 *
 * Returns: the #2 @amount.
 */
ofxAmount
ofo_recurrent_run_get_amount3( const ofoRecurrentRun *recope )
{
	ofo_base_getter( RECURRENT_RUN, recope, amount, 0, REC_AMOUNT3 );
}

/**
 * ofo_recurrent_run_get_edi_user:
 */
const gchar *
ofo_recurrent_run_get_edi_user( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_EDI_USER );
}

/**
 * ofo_recurrent_run_get_edi_stamp:
 */
const myStampVal *
ofo_recurrent_run_get_edi_stamp( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, timestamp, NULL, REC_EDI_STAMP );
}

/**
 * ofo_recurrent_run_compare:
 * @a: a #ofoRecurrentRun object.
 * @b: another #ofoRecurrentRun object.
 *
 * Returns: -1, 0 or 1 depending of whether @a < @b, @a == @b or @a > @b.
 */
gint
ofo_recurrent_run_compare( const ofoRecurrentRun *a, const ofoRecurrentRun *b )
{
	g_return_val_if_fail( a && OFO_IS_RECURRENT_RUN( a ), 0 );
	g_return_val_if_fail( b && OFO_IS_RECURRENT_RUN( b ), 0 );

	return( recurrent_run_cmp_by_ptr( a, b ));
}

/**
 * ofo_recurrent_run_get_last:
 * @getter: a #ofaIGetter of the application.
 * @dlast: [out]: a placeholder for the output date.
 * @mnemo: the mnemonic identifier of a recurrent model.
 * @status: [allow-none]: the OR-ed result of desired ofeRecurrentStatus.
 *
 * Returns: the @dlast date of last recurrent operation which satisfies
 * the desired @status.
 */
const GDate *
ofo_recurrent_run_get_last( ofaIGetter *getter, GDate *dlast, const gchar *mnemo, guint status )
{
	GString *query;
	guint count;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GSList *result, *irow, *icol;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( dlast, NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	g_date_clear( dlast, 1 );

	query = g_string_new( "SELECT MAX(REC_DATE) FROM REC_T_RUN WHERE " );
	g_string_append_printf( query, "REC_MNEMO='%s'", mnemo );
	count = 0;
	if( status & REC_STATUS_CANCELLED ){
		if( count == 0 ){
			count += 1;
			query = g_string_append( query, " AND (" );
		}
		g_string_append_printf( query, "REC_STATUS='%s'", ofo_recurrent_run_status_get_dbms( REC_STATUS_CANCELLED ));
	}
	if( status & REC_STATUS_WAITING ){
		if( count == 0 ){
			count += 1;
			query = g_string_append( query, " AND (" );
		} else if( count == 1 ){
			count += 1;
			query = g_string_append( query, " OR " );
		}
		g_string_append_printf( query, "REC_STATUS='%s'", ofo_recurrent_run_status_get_dbms( REC_STATUS_WAITING ));
	}
	if( status & REC_STATUS_VALIDATED ){
		if( count == 0 ){
			count += 1;
			query = g_string_append( query, " AND (" );
		} else if( count == 1 ){
			count += 1;
			query = g_string_append( query, " OR " );
		}
		g_string_append_printf( query, "REC_STATUS='%s'", ofo_recurrent_run_status_get_dbms( REC_STATUS_VALIDATED ));
	}
	if( count > 0 ){
		query = g_string_append( query, ")" );
	}

	hub = ofa_igetter_get_hub( getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	if( ofa_idbconnect_query_ex( connect, query->str, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			my_date_set_from_str( dlast, ( const gchar * ) icol->data, MY_DATE_SQL );
		}
		ofa_idbconnect_free_results( result );
	}

	g_string_free( query, TRUE );

	return( dlast );
}

/*
 * ofo_recurrent_run_set_numseq:
 */
static void
recurrent_run_set_numseq( ofoRecurrentRun *model, ofxCounter numseq )
{
	ofo_base_setter( RECURRENT_RUN, model, counter, REC_NUMSEQ, numseq );
}

/**
 * ofo_recurrent_run_set_mnemo:
 */
void
ofo_recurrent_run_set_mnemo( ofoRecurrentRun *model, const gchar *mnemo )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_MNEMO, mnemo );
}

/**
 * ofo_recurrent_run_set_date:
 */
void
ofo_recurrent_run_set_date( ofoRecurrentRun *model, const GDate *date )
{
	ofo_base_setter( RECURRENT_RUN, model, date, REC_DATE, date );
}

/*
 * recurrent_run_set_period:
 * @model: this #ofoRecurrentModel object.
 * @period: a #myPeriod object.
 *
 * Set the @period periodicity.
 *
 * Please note that this method takes its own reference on the @period
 * object. The provided @period instance may then be released by the
 * caller.
 */
static void
recurrent_run_set_period( ofoRecurrentRun *run, myPeriod *period )
{
	ofoRecurrentRunPrivate *priv;

	g_return_if_fail( run && OFO_IS_RECURRENT_RUN( run ));
	g_return_if_fail( !OFO_BASE( run )->prot->dispose_has_run );

	priv = ofo_recurrent_run_get_instance_private( run );

	g_clear_object( &priv->period );
	priv->period = g_object_ref( period );
}

/*
 * recurrent_run_set_cre_user:
 */
static void
recurrent_run_set_cre_user( ofoRecurrentRun *model, const gchar *user )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_CRE_USER, user );
}

/*
 * recurrent_run_set_cre_stamp:
 */
static void
recurrent_run_set_cre_stamp( ofoRecurrentRun *model, const myStampVal *stamp )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_CRE_STAMP, stamp );
}

/**
 * ofo_recurrent_run_set_status:
 * @recrun: this #ofoRecurrentRun object.
 * @status. a #ofeRecurrentStatus status.
 *
 * Set @status.
 */
void
ofo_recurrent_run_set_status( ofoRecurrentRun *recrun, ofeRecurrentStatus status )
{
	const gchar *cstr;

	g_return_if_fail( recrun && OFO_IS_RECURRENT_RUN( recrun ));
	g_return_if_fail( !OFO_BASE( recrun )->prot->dispose_has_run );

	cstr = ofo_recurrent_run_status_get_dbms( status );
	ofa_box_set_string( OFO_BASE( recrun )->prot->fields, REC_STATUS, cstr );
}

/*
 * recurrent_run_set_sta_user:
 */
static void
recurrent_run_set_sta_user( ofoRecurrentRun *model, const gchar *user )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_STA_USER, user );
}

/*
 * recurrent_run_set_sta_stamp:
 */
static void
recurrent_run_set_sta_stamp( ofoRecurrentRun *model, const myStampVal *stamp )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_STA_STAMP, stamp );
}

/**
 * ofo_recurrent_run_set_amount1:
 * @recope: this #ofoRecurrentRun object.
 * @amount: the amount to be set.
 *
 * Set the #1 @amount.
 */
void
ofo_recurrent_run_set_amount1( ofoRecurrentRun *recope, ofxAmount amount )
{
	ofo_base_setter( RECURRENT_RUN, recope, amount, REC_AMOUNT1, amount );
}

/**
 * ofo_recurrent_run_set_amount2:
 * @recope: this #ofoRecurrentRun object.
 * @amount: the amount to be set.
 *
 * Set the #2 @amount.
 */
void
ofo_recurrent_run_set_amount2( ofoRecurrentRun *recope, ofxAmount amount )
{
	ofo_base_setter( RECURRENT_RUN, recope, amount, REC_AMOUNT2, amount );
}

/**
 * ofo_recurrent_run_set_amount3:
 * @recope: this #ofoRecurrentRun object.
 * @amount: the amount to be set.
 *
 * Set the #3 @amount.
 */
void
ofo_recurrent_run_set_amount3( ofoRecurrentRun *recope, ofxAmount amount )
{
	ofo_base_setter( RECURRENT_RUN, recope, amount, REC_AMOUNT3, amount );
}

/*
 * recurrent_run_set_edi_user:
 */
static void
recurrent_run_set_edi_user( ofoRecurrentRun *model, const gchar *user )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_EDI_USER, user );
}

/*
 * recurrent_run_set_edi_stamp:
 */
static void
recurrent_run_set_edi_stamp( ofoRecurrentRun *model, const myStampVal *stamp )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_EDI_STAMP, stamp );
}

/**
 * ofo_recurrent_run_get_doc_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown recurrent_run identifiers in
 * REC_T_RUN_DOC child table.
 *
 * The returned list should be #ofo_recurrent_run_free_doc_orphans() by the
 * caller.
 */
GList *
ofo_recurrent_run_get_doc_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "REC_T_RUN_DOC" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	ofxCounter numseq;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(REC_NUMSEQ) FROM %s "
			"	WHERE REC_NUMSEQ NOT IN (SELECT REC_NUMSEQ FROM REC_T_RUN)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			numseq = atol(( const gchar * ) icol->data );
			orphans = g_list_prepend( orphans, ( gpointer ) numseq );
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_recurrent_run_insert:
 */
gboolean
ofo_recurrent_run_insert( ofoRecurrentRun *recurrent_run )
{
	static const gchar *thisfn = "ofo_recurrent_run_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: model=%p", thisfn, ( void * ) recurrent_run );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_run ));
	signaler = ofa_igetter_get_signaler( getter );

	/* rationale: see ofo-account.c */
	ofo_recurrent_run_get_dataset( getter );

	if( recurrent_run_do_insert( recurrent_run, getter )){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( recurrent_run ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, recurrent_run );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
recurrent_run_do_insert( ofoRecurrentRun *recrun, ofaIGetter *getter )
{
	return( recurrent_run_insert_main( recrun, getter ));
}

static gboolean
recurrent_run_insert_main( ofoRecurrentRun *recrun, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	const GDate *date;
	const gchar *mnemo, *userid, *cdbms;
	gchar *sdate, *stamp_str, *samount, *label, *template, *period_str;
	myStampVal *stamp;
	ofoRecurrentModel *model;
	ofxCounter numseq;
	ofeRecurrentStatus status;
	myPeriod *period;
	myPeriodKey perkey;
	ofxAmount amount;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	mnemo = ofo_recurrent_run_get_mnemo( recrun );
	model = ofo_recurrent_model_get_by_mnemo( getter, mnemo );
	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );

	numseq = ofo_recurrent_gen_get_next_numseq( getter );
	recurrent_run_set_numseq( recrun, numseq );

	query = g_string_new( "INSERT INTO REC_T_RUN " );

	g_string_append_printf( query,
			"	(REC_NUMSEQ,REC_MNEMO,REC_DATE,"
			"	 REC_LABEL,REC_OPE_TEMPLATE,"
			"	 REC_PERIOD_ID,REC_PERIOD_N,REC_PERIOD_DET,"
			"	 REC_END,REC_CRE_USER,REC_CRE_STAMP,"
			"	 REC_STATUS,REC_STA_USER,REC_STA_STAMP,"
			"	 REC_AMOUNT1,REC_AMOUNT2,REC_AMOUNT3,"
			"	 REC_EDI_USER, REC_EDI_STAMP) VALUES (%ld,'%s',",
			numseq, mnemo );

	date = ofo_recurrent_run_get_date( recrun );
	g_return_val_if_fail( my_date_is_valid( date ), FALSE );
	sdate = my_date_to_str( date, MY_DATE_SQL );
	g_string_append_printf( query, "'%s',", sdate );
	g_free( sdate );

	label = my_utils_quote_sql( ofo_recurrent_run_get_label( recrun ));
	g_string_append_printf( query, "'%s',", label );
	g_free( label );

	template = my_utils_quote_sql( ofo_recurrent_run_get_ope_template( recrun ));
	g_string_append_printf( query, "'%s',", template );
	g_free( template );

	period_str = NULL;
	period = ofo_recurrent_run_get_period( recrun );
	if( period ){
		perkey = my_period_get_key( period );
		g_string_append_printf( query, "'%s',%d,",
				my_period_key_get_dbms( perkey ),
				my_period_get_every( period ));

		period_str = my_period_get_details_str_i( period );
		if( my_strlen( period_str )){
			g_string_append_printf( query, "'%s',", period_str );
		} else {
			query = g_string_append( query, "NULL," );
		}
	} else {
		query = g_string_append( query, "NULL,NULL,NULL," );
	}

	date = ofo_recurrent_run_get_end( recrun );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "NULL," );
	}

	/* creation user and timestamp */
	g_string_append_printf( query, "'%s',", userid );
	g_string_append_printf( query, "'%s',", stamp_str );

	recurrent_run_set_cre_user( recrun, userid );
	recurrent_run_set_cre_stamp( recrun, stamp );

	/* status */
	status = ofo_recurrent_run_get_status( recrun );
	cdbms = ofo_recurrent_run_status_get_dbms( status );
	if( my_strlen( cdbms )){
		g_string_append_printf( query, "'%s',", cdbms );
	} else {
		query = g_string_append( query, "NULL," );
	}

	/* status user and timestamp */
	g_string_append_printf( query, "'%s',", userid );
	g_string_append_printf( query, "'%s',", stamp_str );

	recurrent_run_set_sta_user( recrun, userid );
	recurrent_run_set_sta_stamp( recrun, stamp );

	amount = ofo_recurrent_run_get_amount1( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "NULL," );
	}

	amount = ofo_recurrent_run_get_amount2( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "NULL," );
	}

	amount = ofo_recurrent_run_get_amount3( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "NULL," );
	}

	/* amounts edition user and timestamp */
	g_string_append_printf( query, "'%s',", userid );
	g_string_append_printf( query, "'%s')", stamp_str );

	recurrent_run_set_edi_user( recrun, userid );
	recurrent_run_set_edi_stamp( recrun, stamp );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_recurrent_run_update_status:
 * @model:
 *
 * Update the status.
 */
gboolean
ofo_recurrent_run_update_status( ofoRecurrentRun *recurrent_run )
{
	static const gchar *thisfn = "ofo_recurrent_run_update_status";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: model=%p",
			thisfn, ( void * ) recurrent_run );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_run ));
	signaler = ofa_igetter_get_signaler( getter );

	if( recurrent_run_do_update_status( recurrent_run, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, recurrent_run, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
recurrent_run_do_update_status( ofoRecurrentRun *recrun, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofo_recurrent_run_do_update_status";
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	const gchar *userid, *cdbms;
	gchar *stamp_str;
	myStampVal *stamp;
	ofxCounter numseq;
	ofeRecurrentStatus status;

	if( 0 ){
		g_debug( "%s: recrun=%p (%s)", thisfn,
				( void * ) recrun, G_OBJECT_TYPE_NAME( recrun ));
	}

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_RUN SET " );

	status = ofo_recurrent_run_get_status( recrun );
	cdbms = ofo_recurrent_run_status_get_dbms( status );
	if( my_strlen( cdbms )){
		g_string_append_printf( query, "REC_STATUS='%s',", cdbms );
	} else {
		query = g_string_append( query, "REC_STATUS=NULL," );
	}

	numseq = ofo_recurrent_run_get_numseq( recrun );

	g_string_append_printf( query,
			"	REC_STA_USER='%s',REC_STA_STAMP='%s'"
			"	WHERE REC_NUMSEQ=%ld",
					userid,
					stamp_str,
					numseq );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_run_set_sta_user( recrun, userid );
	recurrent_run_set_sta_stamp( recrun, stamp );

	g_string_free( query, TRUE );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_recurrent_run_update_amounts:
 * @model:
 *
 * Update the amounts.
 */
gboolean
ofo_recurrent_run_update_amounts( ofoRecurrentRun *recurrent_run )
{
	static const gchar *thisfn = "ofo_recurrent_run_update_amounts";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: model=%p",
			thisfn, ( void * ) recurrent_run );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_run ));
	signaler = ofa_igetter_get_signaler( getter );

	if( recurrent_run_do_update_amounts( recurrent_run, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, recurrent_run, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
recurrent_run_do_update_amounts( ofoRecurrentRun *recrun, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofo_recurrent_run_do_update_amounts";
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *samount;
	const gchar *userid, *mnemo;
	gchar *stamp_str;
	myStampVal *stamp;
	ofxCounter numseq;
	ofoRecurrentModel *model;
	ofxAmount amount;

	mnemo = ofo_recurrent_run_get_mnemo( recrun );
	model = ofo_recurrent_model_get_by_mnemo( getter, mnemo );

	if( 0 ){
		g_debug( "%s: recrun=%p (%s), model=%p (%s)", thisfn,
				( void * ) recrun, G_OBJECT_TYPE_NAME( recrun ),
				( void * ) model, G_OBJECT_TYPE_NAME( model ));
	}

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	userid = ofa_idbconnect_get_account( connect );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_RUN SET " );

	amount = ofo_recurrent_run_get_amount1( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "REC_AMOUNT1=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "REC_AMOUNT1=NULL," );
	}

	amount = ofo_recurrent_run_get_amount2( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "REC_AMOUNT2=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "REC_AMOUNT2=NULL," );
	}

	amount = ofo_recurrent_run_get_amount3( recrun );
	if( amount > 0 ){
		samount = ofa_amount_to_sql( amount, NULL );
		g_string_append_printf( query, "REC_AMOUNT3=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "REC_AMOUNT3=NULL," );
	}

	numseq = ofo_recurrent_run_get_numseq( recrun );

	g_string_append_printf( query,
			"	REC_EDI_USER='%s',REC_EDI_STAMP='%s'"
			"	WHERE REC_NUMSEQ=%ld",
					userid,
					stamp_str,
					numseq );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_run_set_edi_user( recrun, userid );
	recurrent_run_set_edi_stamp( recrun, stamp );

	g_string_free( query, TRUE );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

static gint
recurrent_run_cmp_by_mnemo_date( const ofoRecurrentRun *a, const gchar *mnemo, const GDate *date, ofeRecurrentStatus status )
{
	gint cmp;
	ofeRecurrentStatus status_a;

	cmp = g_utf8_collate( ofo_recurrent_run_get_mnemo( a ), mnemo );
	if( cmp == 0 ){
		cmp = my_date_compare( ofo_recurrent_run_get_date( a ), date );
		if( cmp == 0 ){
			status_a = ofo_recurrent_run_get_status( a );
			cmp = status_a < status ? -1 : ( status_a > status ? 1 : 0 );
		}
	}

	return( cmp );
}

static gint
recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b )
{
	if( 0 ){
		g_debug( "a=%p (%s), b=%p (%s)", a, G_OBJECT_TYPE_NAME( a ), b, G_OBJECT_TYPE_NAME( b ));
	}

	return( recurrent_run_cmp_by_mnemo_date( a,
			ofo_recurrent_run_get_mnemo( b ), ofo_recurrent_run_get_date( b ), ofo_recurrent_run_get_status( b )));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_run_icollectionable_iface_init";

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
	ofoRecurrentRun *run;
	const gchar *ckey, *clist;
	guint cn;
	myPeriod *period;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_RUN",
					OFO_TYPE_RECURRENT_RUN,
					OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		run = OFO_RECURRENT_RUN( it->data );
		ckey = ofa_box_get_string( OFO_BASE( run )->prot->fields, REC_PERIOD_ID );
		cn = ofa_box_get_int( OFO_BASE( run )->prot->fields, REC_PERIOD_N );
		clist = ofa_box_get_string( OFO_BASE( run )->prot->fields, REC_PERIOD_DET );
		period = my_period_new_with_data( ckey, cn, clist );
		recurrent_run_set_period( run, period );
		g_object_unref( period );
	}

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_run_idoc_iface_init";

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
	static const gchar *thisfn = "ofo_recurrent_run_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_recurrent_run_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_run_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_RECURRENT_MODEL( object )){
		deletable = signaler_is_deletable_recurrent_model( signaler, OFO_RECURRENT_MODEL( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_recurrent_model( ofaISignaler *signaler, ofoRecurrentModel *model )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM REC_T_RUN WHERE REC_MNEMO='%s'",
			ofo_recurrent_model_get_mnemo( model ));

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
	static const gchar *thisfn = "ofo_recurrent_run_signaler_on_updated_base";
	const gchar *mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_RECURRENT_MODEL( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_rec_model_mnemo( signaler, object, mnemo, prev_id );
			}
		}
	}
}

static gboolean
signaler_on_updated_rec_model_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_recurrent_run_signaler_on_updated_rec_model_mnemo";
	ofaIGetter *getter;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *query;
	gboolean ok;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE REC_T_RUN "
					"	SET REC_MNEMO='%s' "
					"	WHERE REC_MNEMO='%s'",
							mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	my_icollector_collection_free( ofa_igetter_get_collector( getter ), OFO_TYPE_RECURRENT_RUN );

	return( ok );
}
