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

#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-ope-template.h"

#include "ofo-recurrent-model.h"

/* priv instance data
 */
enum {
	REC_MNEMO = 1,
	REC_LABEL,
	REC_OPE_TEMPLATE,
	REC_PERIOD,
	REC_PERIOD_DETAIL,
	REC_NOTES,
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
		{ OFA_BOX_CSV( REC_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_OPE_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_PERIOD ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_NOTES ),
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

struct _ofoRecurrentModelPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

static void               hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean           hub_update_ope_template_identifier( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );
static ofoRecurrentModel *model_find_by_mnemo( GList *set, const gchar *mnemo );
static guint              model_count_for_ope_template( const ofaIDBConnect *connect, const gchar *template );
static void               recurrent_model_set_upd_user( ofoRecurrentModel *model, const gchar *upd_user );
static void               recurrent_model_set_upd_stamp( ofoRecurrentModel *model, const GTimeVal *upd_stamp );
static gboolean           model_do_insert( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_insert_main( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_do_update( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_update_main( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_do_delete( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gint               model_cmp_by_mnemo( const ofoRecurrentModel *a, const gchar *mnemo );
static gint               recurrent_model_cmp_by_ptr( const ofoRecurrentModel *a, const ofoRecurrentModel *b );
static void               icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint              icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList             *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentModel, ofo_recurrent_model, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentModel )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init ))

static void
recurrent_model_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_recurrent_model_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, REC_MNEMO ));

	g_return_if_fail( instance && OFO_IS_RECURRENT_MODEL( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_model_parent_class )->finalize( instance );
}

static void
recurrent_model_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RECURRENT_MODEL( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_model_parent_class )->dispose( instance );
}

static void
ofo_recurrent_model_init( ofoRecurrentModel *self )
{
	static const gchar *thisfn = "ofo_recurrent_model_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_recurrent_model_class_init( ofoRecurrentModelClass *klass )
{
	static const gchar *thisfn = "ofo_recurrent_model_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_finalize;
}

/**
 * ofo_recurrent_model_connect_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_recurrent_model_connect_handlers( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_model_connect_handlers";

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
	static const gchar *thisfn = "ofo_recurrent_model_hub_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				hub_update_ope_template_identifier( hub, mnemo, prev_id );
			}
		}
	}
}

static gboolean
hub_update_ope_template_identifier( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_recurrent_model_hub_update_ope_template_identifier";
	gchar *query;
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE REC_T_MODELS "
					"	SET REC_OPE_TEMPLATE='%s' "
					"	WHERE REC_OPE_TEMPLATE='%s'",
							mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_RECURRENT_MODEL );
	g_signal_emit_by_name( hub, SIGNAL_HUB_RELOAD, OFO_TYPE_RECURRENT_MODEL );

	return( ok );
}

/**
 * ofo_recurrent_model_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoRecurrentModel dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_recurrent_model_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_RECURRENT_MODEL ));
}

/**
 * ofo_recurrent_model_get_by_mnemo:
 * @hub:
 * @mnemo:
 *
 * Returns: the searched recurrent model, or %NULL.
 *
 * The returned object is owned by the #ofoRecurrentModel class, and should
 * not be unreffed by the caller.
 */
ofoRecurrentModel *
ofo_recurrent_model_get_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_recurrent_model_get_dataset( hub );

	return( model_find_by_mnemo( dataset, mnemo ));
}

static ofoRecurrentModel *
model_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) model_cmp_by_mnemo );
	if( found ){
		return( OFO_RECURRENT_MODEL( found->data ));
	}

	return( NULL );
}

/**
 * ofo_recurrent_model_use_ope_template:
 * @hub:
 * @temmplate:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified
 * operation template.
 */
gboolean
ofo_recurrent_model_use_ope_template( ofaHub *hub, const gchar *template )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( my_strlen( template ), FALSE );

	return( model_count_for_ope_template( ofa_hub_get_connect( hub ), template ) > 0 );
}

static guint
model_count_for_ope_template( const ofaIDBConnect *connect, const gchar *template )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM REC_T_MODELS WHERE REC_OPE_TEMPLATE='%s'", template );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( abs( count ));
}

/**
 * ofo_recurrent_model_new:
 */
ofoRecurrentModel *
ofo_recurrent_model_new( void )
{
	ofoRecurrentModel *model;

	model = g_object_new( OFO_TYPE_RECURRENT_MODEL, NULL );
	OFO_BASE( model )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( model );
}

/**
 * ofo_recurrent_model_get_mnemo:
 * @model:
 */
const gchar *
ofo_recurrent_model_get_mnemo( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_MNEMO );
}

/**
 * ofo_recurrent_model_get_label:
 */
const gchar *
ofo_recurrent_model_get_label( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_LABEL );
}

/**
 * ofo_recurrent_model_get_ope_template:
 */
const gchar *
ofo_recurrent_model_get_ope_template( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_OPE_TEMPLATE );
}

/**
 * ofo_recurrent_model_get_periodicity:
 */
const gchar *
ofo_recurrent_model_get_periodicity( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_PERIOD );
}

/**
 * ofo_recurrent_model_get_periodicity_detail:
 */
const gchar *
ofo_recurrent_model_get_periodicity_detail( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_PERIOD_DETAIL );
}

/**
 * ofo_recurrent_model_get_notes:
 */
const gchar *
ofo_recurrent_model_get_notes( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_NOTES );
}

/**
 * ofo_recurrent_model_get_upd_user:
 */
const gchar *
ofo_recurrent_model_get_upd_user( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_UPD_USER );
}

/**
 * ofo_recurrent_model_get_upd_stamp:
 */
const GTimeVal *
ofo_recurrent_model_get_upd_stamp( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, timestamp, NULL, REC_UPD_STAMP );
}

/**
 * ofo_recurrent_model_is_deletable:
 * @model: the recurrent model.
 *
 * Returns: %TRUE if the recurrent model is deletable.
 *
 * A recurrent model may be deleted if it is not used nor archived.
 */
gboolean
ofo_recurrent_model_is_deletable( const ofoRecurrentModel *model )
{
	const gchar *model_id;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gint run_count, arch_count;
	gchar *query;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );

	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	model_id = ofo_recurrent_model_get_mnemo( model );
	hub = ofo_base_get_hub( OFO_BASE( model ));
	connect = ofa_hub_get_connect( hub );
	arch_count = 0;

	query = g_strdup_printf( "SELECT COUNT(*) FROM REC_T_RUN WHERE REC_MNEMO='%s'", model_id );
	ofa_idbconnect_query_int( connect, query, &run_count, TRUE );
	g_free( query );

	if( ofa_idbconnect_has_table( connect, "ARCHREC_T_RUN" )){
		query = g_strdup_printf( "SELECT COUNT(*) FROM ARCHREC_T_RUN WHERE REC_MNEMO='%s'", model_id );
		ofa_idbconnect_query_int( connect, query, &arch_count, TRUE );
		g_free( query );
	}

	return( run_count+arch_count == 0 );
}

/**
 * ofo_recurrent_model_is_valid_data:
 * @mnemo:
 * @label:
 * @ope_template:
 * @period:
 * @msgerr: [allow-none][out]:
 *
 * Returns: %TRUE if provided datas are enough to make the future
 * #ofoRecurrentModel valid, %FALSE else.
 */
gboolean
ofo_recurrent_model_is_valid_data( const gchar *mnemo, const gchar *label, const gchar *ope_template, const gchar *period, gchar **msgerr )
{
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
	if( !my_strlen( ope_template )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty operation template" ));
		}
		return( FALSE );
	}
	if( !my_strlen( period )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty periodicity" ));
		}
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_recurrent_model_set_mnemo:
 */
void
ofo_recurrent_model_set_mnemo( ofoRecurrentModel *model, const gchar *mnemo )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_MNEMO, mnemo );
}

/**
 * ofo_recurrent_model_set_label:
 */
void
ofo_recurrent_model_set_label( ofoRecurrentModel *model, const gchar *label )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_LABEL, label );
}

/**
 * ofo_recurrent_model_set_ope_template:
 */
void
ofo_recurrent_model_set_ope_template( ofoRecurrentModel *model, const gchar *template )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_OPE_TEMPLATE, template );
}

/**
 * ofo_recurrent_model_set_periodicity:
 */
void
ofo_recurrent_model_set_periodicity( ofoRecurrentModel *model, const gchar *period )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_PERIOD, period );
}

/**
 * ofo_recurrent_model_set_periodicity_detail:
 */
void
ofo_recurrent_model_set_periodicity_detail( ofoRecurrentModel *model, const gchar *detail )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_PERIOD_DETAIL, detail );
}

/**
 * ofo_recurrent_model_set_notes:
 */
void
ofo_recurrent_model_set_notes( ofoRecurrentModel *model, const gchar *notes )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_NOTES, notes );
}

/*
 * ofo_recurrent_model_set_upd_user:
 */
static void
recurrent_model_set_upd_user( ofoRecurrentModel *model, const gchar *upd_user )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_UPD_USER, upd_user );
}

/*
 * ofo_recurrent_model_set_upd_stamp:
 */
static void
recurrent_model_set_upd_stamp( ofoRecurrentModel *model, const GTimeVal *upd_stamp )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_UPD_STAMP, upd_stamp );
}

/**
 * ofo_recurrent_model_insert:
 */
gboolean
ofo_recurrent_model_insert( ofoRecurrentModel *recurrent_model, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_model_insert";
	gboolean ok;

	g_debug( "%s: model=%p, hub=%p",
			thisfn, ( void * ) recurrent_model, ( void * ) hub );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( model_do_insert( recurrent_model, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( recurrent_model ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( recurrent_model ), ( GCompareFunc ) recurrent_model_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, recurrent_model );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_insert( ofoRecurrentModel *model, const ofaIDBConnect *connect )
{
	return( model_insert_main( model, connect ));
}

static gboolean
model_insert_main( ofoRecurrentModel *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	const gchar *period, *detail;
	gchar *label, *template, *notes, *userid, *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_recurrent_model_get_label( model ));
	template = my_utils_quote_single( ofo_recurrent_model_get_ope_template( model ));
	notes = my_utils_quote_single( ofo_recurrent_model_get_notes( model ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_MODELS " );

	g_string_append_printf( query,
			"	(REC_MNEMO,REC_LABEL,REC_OPE_TEMPLATE,"
			"	 REC_PERIOD,REC_PERIOD_DETAIL,"
			"	 REC_NOTES,REC_UPD_USER, REC_UPD_STAMP) VALUES ('%s',",
			ofo_recurrent_model_get_mnemo( model ));

	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( template )){
		g_string_append_printf( query, "'%s',", template );
	} else {
		query = g_string_append( query, "NULL," );
	}

	period = ofo_recurrent_model_get_periodicity( model );
	if( my_strlen( period )){
		g_string_append_printf( query, "'%s',", period );
	} else {
		query = g_string_append( query, "NULL," );
	}

	detail = ofo_recurrent_model_get_periodicity_detail( model );
	if( my_strlen( detail )){
		g_string_append_printf( query, "'%s',", detail );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_model_set_upd_user( model, userid );
	recurrent_model_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( template );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_recurrent_model_update:
 * @model:
 * @prev_mnemo:
 */
gboolean
ofo_recurrent_model_update( ofoRecurrentModel *recurrent_model, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_recurrent_model_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: model=%p, prev_mnemo=%s",
			thisfn, ( void * ) recurrent_model, prev_mnemo );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( recurrent_model ));

	if( model_do_update( recurrent_model, ofa_hub_get_connect( hub ), prev_mnemo )){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_RECURRENT_MODEL, ( GCompareFunc ) recurrent_model_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, recurrent_model, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_update( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	return( model_update_main( model, connect, prev_mnemo ));
}

static gboolean
model_update_main( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *userid;
	const gchar *new_mnemo, *template, *period, *detail;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_recurrent_model_get_label( model ));
	notes = my_utils_quote_single( ofo_recurrent_model_get_notes( model ));
	new_mnemo = ofo_recurrent_model_get_mnemo( model );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_MOMDELS SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "REC_MNEMO='%s',", new_mnemo );
	}

	if( my_strlen( label )){
		g_string_append_printf( query, "REC_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "REC_LABEL=NULL," );
	}

	template = ofo_recurrent_model_get_ope_template( model );
	if( my_strlen( template )){
		g_string_append_printf( query, "REC_OPE_TEMPLATE='%s',", template );
	} else {
		query = g_string_append( query, "REC_OPE_TEMPLATE=NULL," );
	}

	period = ofo_recurrent_model_get_periodicity( model );
	if( my_strlen( period )){
		g_string_append_printf( query, "REC_PERIOD='%s',", period );
	} else {
		query = g_string_append( query, "REC_PERIOD=NULL," );
	}

	detail = ofo_recurrent_model_get_periodicity_detail( model );
	if( my_strlen( detail )){
		g_string_append_printf( query, "REC_PERIOD_DETAIL='%s',", detail );
	} else {
		query = g_string_append( query, "REC_PERIOD_DETAIL=NULL," );
	}

	if( my_strlen( notes )){
		g_string_append_printf( query, "REC_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "REC_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	REC_UPD_USER='%s',REC_UPD_STAMP='%s'"
			"	WHERE REC_MNEMO='%s'",
					userid,
					stamp_str,
					prev_mnemo );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_model_set_upd_user( model, userid );
	recurrent_model_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );
	g_free( userid );

	return( ok );
}

/**
 * ofo_recurrent_model_delete:
 */
gboolean
ofo_recurrent_model_delete( ofoRecurrentModel *recurrent_model )
{
	static const gchar *thisfn = "ofo_recurrent_model_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: model=%p", thisfn, ( void * ) recurrent_model );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( ofo_recurrent_model_is_deletable( recurrent_model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( recurrent_model ));

	if( model_do_delete( recurrent_model, ofa_hub_get_connect( hub ))){
		g_object_ref( recurrent_model );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( recurrent_model ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, recurrent_model );
		g_object_unref( recurrent_model );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_delete( ofoRecurrentModel *model, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM REC_T_MODELS"
			"	WHERE REC_MNEMO='%s'",
					ofo_recurrent_model_get_mnemo( model ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoRecurrentModel *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_recurrent_model_get_mnemo( a ), mnemo ));
}

static gint
recurrent_model_cmp_by_ptr( const ofoRecurrentModel *a, const ofoRecurrentModel *b )
{
	return( model_cmp_by_mnemo( a, ofo_recurrent_model_get_mnemo( b )));
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_icollectionable_iface_init";

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
	GList *dataset;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_MODELS ORDER BY REC_MNEMO ASC",
					OFO_TYPE_RECURRENT_MODEL,
					hub );

	return( dataset );
}
