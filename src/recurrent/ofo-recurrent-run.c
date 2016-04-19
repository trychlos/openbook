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

#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* priv instance data
 */
enum {
	REC_MNEMO = 1,
	REC_DATE,
	REC_STATUS,
	REC_UPD_USER,
	REC_UPD_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( REC_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( REC_DATE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_STATUS ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( REC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofoRecurrentRunPrivate;

/* Status labels
 */
typedef struct {
	const gchar *code;
	const gchar *label;
}
	sLabels;

static const sLabels st_labels[] = {
		{ REC_STATUS_CANCELLED, N_( "Cancelled" )},
		{ REC_STATUS_WAITING,   N_( "Waiting" )},
		{ REC_STATUS_VALIDATED, N_( "Validated" )},
		{ 0 }
};

static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean hub_update_recurrent_model_identifier( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );
static void     recurrent_run_set_upd_user( ofoRecurrentRun *model, const gchar *upd_user );
static void     recurrent_run_set_upd_stamp( ofoRecurrentRun *model, const GTimeVal *upd_stamp );
static gboolean model_do_insert( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gboolean model_insert_main( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gboolean model_do_update( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gboolean model_update_main( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gint     model_cmp_by_mnemo_date( const ofoRecurrentRun *a, const gchar *mnemo, const GDate *date );
static gint     recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b );
static void     icollectionable_iface_init( myICollectionableInterface *iface );
static guint    icollectionable_get_interface_version( void );
static GList   *icollectionable_load_collection( void *user_data );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentRun, ofo_recurrent_run, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentRun )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init ))

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
 * ofo_recurrent_run_connect_to_hub_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_recurrent_run_connect_to_hub_handlers( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_run_connect_to_hub_handlers";

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_signal_connect( G_OBJECT( hub ),
				SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), NULL );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_run_hub_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_RECURRENT_MODEL( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				hub_update_recurrent_model_identifier( hub, mnemo, prev_id );
			}
		}
	}
}

static gboolean
hub_update_recurrent_model_identifier( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_recurrent_run_hub_update_recurrent_model_identifier";
	gchar *query;
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE REC_T_RUN "
					"	SET REC_MNEMO='%s' "
					"	WHERE REC_MNEMO='%s'",
							mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	my_icollector_collection_free( ofa_hub_get_collector( hub ), OFO_TYPE_RECURRENT_RUN );
	g_signal_emit_by_name( hub, SIGNAL_HUB_RELOAD, OFO_TYPE_RECURRENT_RUN );

	return( ok );
}

/**
 * ofo_recurrent_run_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoRecurrentRun dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_recurrent_run_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_RECURRENT_RUN, hub ));
}

/**
 * ofo_recurrent_run_get_by_id:
 * @hub: the current #ofaHub object.
 * @mnemo: the mnemonic identifier of the model.
 * @date: the date of the recurrent operation.
 *
 * Returns: the found operation, if exists, or %NULL.
 *
 * The search is made whatever be the status of the operation.
 * All operations are candidate to be retrieved.
 *
 * The returned object is owned by the @hub collector, and should not
 * be released by the caller.
 */
ofoRecurrentRun *
ofo_recurrent_run_get_by_id( ofaHub *hub, const gchar *mnemo, const GDate *date )
{
	GList *dataset, *it;
	ofoRecurrentRun *ope;
	const gchar *mnemo_ope;
	const GDate *date_ope;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( !my_strlen( mnemo ) || !my_date_is_valid( date )){
		return( NULL );
	}

	dataset = ofo_recurrent_run_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		ope = OFO_RECURRENT_RUN( it->data );

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
 * ofo_recurrent_run_get_is_deletable:
 * @hub: the current #ofaHub object of the application.
 * @object: the object to be tested.
 *
 * Returns: %TRUE if the @object is not used by ofoTVAForm, thus may be
 * deleted.
 */
gboolean
ofo_recurrent_run_get_is_deletable( const ofaHub *hub, const ofoBase *object )
{
	return( TRUE );
}

/**
 * ofo_recurrent_run_new:
 */
ofoRecurrentRun *
ofo_recurrent_run_new( void )
{
	ofoRecurrentRun *model;

	model = g_object_new( OFO_TYPE_RECURRENT_RUN, NULL );
	OFO_BASE( model )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	ofo_recurrent_run_set_status( model, REC_STATUS_WAITING );

	return( model );
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
 * ofo_recurrent_run_get_status:
 */
const gchar *
ofo_recurrent_run_get_status( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_STATUS );
}

/**
 * ofo_recurrent_run_get_status_label:
 *
 * Returns: the label of the status, as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofo_recurrent_run_get_status_label( const gchar *status )
{
	gint i;
	gchar *str;

	for( i=0 ; st_labels[i].code ; ++i ){
		if( !my_collate( st_labels[i].code, status )){
			return( g_strdup( st_labels[i].label ));
		}
	}

	str = g_strdup_printf( _( "Unknown status: %s" ), status );
	return( str );
}

/**
 * ofo_recurrent_run_get_upd_user:
 */
const gchar *
ofo_recurrent_run_get_upd_user( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_UPD_USER );
}

/**
 * ofo_recurrent_run_get_upd_stamp:
 */
const GTimeVal *
ofo_recurrent_run_get_upd_stamp( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, timestamp, NULL, REC_UPD_STAMP );
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

/**
 * ofo_recurrent_run_set_status:
 */
void
ofo_recurrent_run_set_status( ofoRecurrentRun *model, const gchar *status )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_STATUS, status );
}

/*
 * ofo_recurrent_run_set_upd_user:
 */
static void
recurrent_run_set_upd_user( ofoRecurrentRun *model, const gchar *upd_user )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_UPD_USER, upd_user );
}

/*
 * ofo_recurrent_run_set_upd_stamp:
 */
static void
recurrent_run_set_upd_stamp( ofoRecurrentRun *model, const GTimeVal *upd_stamp )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_UPD_STAMP, upd_stamp );
}

/**
 * ofo_recurrent_run_insert:
 */
gboolean
ofo_recurrent_run_insert( ofoRecurrentRun *recurrent_run, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_run_insert";
	gboolean ok;

	g_debug( "%s: model=%p, hub=%p",
			thisfn, ( void * ) recurrent_run, ( void * ) hub );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( model_do_insert( recurrent_run, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( recurrent_run ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ),
				MY_ICOLLECTIONABLE( recurrent_run ), ( GCompareFunc ) recurrent_run_cmp_by_ptr, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, recurrent_run );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_insert( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	return( model_insert_main( model, connect ));
}

static gboolean
model_insert_main( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	const GDate *date;
	const gchar *status;
	gchar *sdate, *userid, *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_RUN " );

	g_string_append_printf( query,
			"	(REC_MNEMO,REC_DATE,REC_STATUS,"
			"	 REC_UPD_USER, REC_UPD_STAMP) VALUES ('%s',",
			ofo_recurrent_run_get_mnemo( model ));

	date = ofo_recurrent_run_get_date( model );
	g_return_val_if_fail( my_date_is_valid( date ), FALSE );
	sdate = my_date_to_str( date, MY_DATE_SQL );
	g_string_append_printf( query, "'%s',", sdate );
	g_free( sdate );

	status = ofo_recurrent_run_get_status( model );
	if( my_strlen( status )){
		g_string_append_printf( query, "'%s',", status );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_run_set_upd_user( model, userid );
	recurrent_run_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_recurrent_run_update:
 * @model:
 * @prev_mnemo:
 *
 * Update the status.
 */
gboolean
ofo_recurrent_run_update( ofoRecurrentRun *recurrent_run )
{
	static const gchar *thisfn = "ofo_recurrent_run_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: model=%p",
			thisfn, ( void * ) recurrent_run );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( recurrent_run ));

	if( model_do_update( recurrent_run, ofa_hub_get_connect( hub ))){
		my_icollector_collection_sort(
				ofa_hub_get_collector( hub ),
				OFO_TYPE_RECURRENT_MODEL, ( GCompareFunc ) recurrent_run_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, recurrent_run, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_update( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	return( model_update_main( model, connect ));
}

static gboolean
model_update_main( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *userid, *sdate;
	const gchar *status, *mnemo;
	const GDate *date;
	gchar *stamp_str;
	GTimeVal stamp;

	mnemo = ofo_recurrent_run_get_mnemo( model );
	userid = ofa_idbconnect_get_account( connect );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	date = ofo_recurrent_run_get_date( model );
	sdate = my_date_to_str( date, MY_DATE_SQL );

	query = g_string_new( "UPDATE REC_T_RUN SET " );

	status = ofo_recurrent_run_get_status( model );
	g_string_append_printf( query, "REC_STATUS='%s',", status );

	g_string_append_printf( query,
			"	REC_UPD_USER='%s',REC_UPD_STAMP='%s'"
			"	WHERE REC_MNEMO='%s' AND REC_DATE='%s'",
					userid,
					stamp_str,
					mnemo, sdate );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_run_set_upd_user( model, userid );
	recurrent_run_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( sdate );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

static gint
model_cmp_by_mnemo_date( const ofoRecurrentRun *a, const gchar *mnemo, const GDate *date )
{
	gint cmp;

	cmp = g_utf8_collate( ofo_recurrent_run_get_mnemo( a ), mnemo );
	if( cmp == 0 ){
		cmp = my_date_compare( ofo_recurrent_run_get_date( a ), date );
	}
	return( cmp );
}

static gint
recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b )
{
	return( model_cmp_by_mnemo_date( a, ofo_recurrent_run_get_mnemo( b ), ofo_recurrent_run_get_date( b )));
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
	GList *dataset;

	g_return_val_if_fail( user_data && OFA_IS_HUB( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_RUN ORDER BY REC_MNEMO ASC, REC_DATE ASC",
					OFO_TYPE_RECURRENT_RUN,
					OFA_HUB( user_data ));

	return( dataset );
}
