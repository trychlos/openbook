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
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofa-periodicity.h"
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
		{ OFA_BOX_CSV( REC_PERIOD_DETAIL ),
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

typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofoRecurrentModelPrivate;

static ofoRecurrentModel *model_find_by_mnemo( GList *set, const gchar *mnemo );
static void               recurrent_model_set_upd_user( ofoRecurrentModel *model, const gchar *upd_user );
static void               recurrent_model_set_upd_stamp( ofoRecurrentModel *model, const GTimeVal *upd_stamp );
static gboolean           model_do_insert( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_insert_main( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_do_update( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_update_main( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_do_delete( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gint               model_cmp_by_mnemo( const ofoRecurrentModel *a, const gchar *mnemo );
static gint               recurrent_model_cmp_by_ptr( const ofoRecurrentModel *a, const ofoRecurrentModel *b );
static void               icollectionable_iface_init( myICollectionableInterface *iface );
static guint              icollectionable_get_interface_version( void );
static GList             *icollectionable_load_collection( void *user_data );
static void               iexportable_iface_init( ofaIExportableInterface *iface );
static guint              iexportable_get_interface_version( void );
static gchar             *iexportable_get_label( const ofaIExportable *instance );
static gboolean           iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub );
static void               iimportable_iface_init( ofaIImportableInterface *iface );
static guint              iimportable_get_interface_version( void );
static gchar             *iimportable_get_label( const ofaIImportable *instance );
static guint              iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList             *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void               iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean           model_get_exists( const ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_drop_content( const ofaIDBConnect *connect );
static void               isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void               isignal_hub_connect( ofaHub *hub );
static gboolean           hub_on_deletable_object( ofaHub *hub, ofoBase *object, void *empty );
static gboolean           hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template );
static void               hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean           hub_on_updated_ope_template_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentModel, ofo_recurrent_model, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentModel )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

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

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_RECURRENT_MODEL, hub ));
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
 * @mnemo:
 *
 * Returns: %TRUE if any #ofoRecurrentModel use this @mnemo operation
 * template.
 */
gboolean
ofo_recurrent_model_use_ope_template( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset, *it;
	ofoRecurrentModel *model;
	const gchar *model_template;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( my_strlen( mnemo ), FALSE );

	dataset = ofo_recurrent_model_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		model_template = ofo_recurrent_model_get_ope_template( model );
		if( !my_collate( model_template, mnemo )){
			return( TRUE );
		}
	}

	return( FALSE );
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
 * A recurrent model may be deleted while it is not used by any run
 * object.
 */
gboolean
ofo_recurrent_model_is_deletable( const ofoRecurrentModel *model )
{
	gboolean deletable;
	ofaHub *hub;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );

	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	hub = ofo_base_get_hub( OFO_BASE( model ));

	if( hub ){
		g_signal_emit_by_name( hub, SIGNAL_HUB_DELETABLE, model, &deletable );
	}

	return( deletable );
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
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ),
				MY_ICOLLECTIONABLE( recurrent_model ), ( GCompareFunc ) recurrent_model_cmp_by_ptr, hub );
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
	label = my_utils_quote_sql( ofo_recurrent_model_get_label( model ));
	template = my_utils_quote_sql( ofo_recurrent_model_get_ope_template( model ));
	notes = my_utils_quote_sql( ofo_recurrent_model_get_notes( model ));
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
		my_icollector_collection_sort(
				ofa_hub_get_collector( hub ),
				OFO_TYPE_RECURRENT_MODEL, ( GCompareFunc ) recurrent_model_cmp_by_ptr );
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
	label = my_utils_quote_sql( ofo_recurrent_model_get_label( model ));
	notes = my_utils_quote_sql( ofo_recurrent_model_get_notes( model ));
	new_mnemo = ofo_recurrent_model_get_mnemo( model );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE REC_T_MODELS SET " );

	if( my_collate( new_mnemo, prev_mnemo )){
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
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( recurrent_model ));
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
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_model_icollectionable_iface_init";

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
					"REC_T_MODELS ORDER BY REC_MNEMO ASC",
					OFO_TYPE_RECURRENT_MODEL,
					OFA_HUB( user_data ));

	return( dataset );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_model_iexportable_iface_init";

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
	return( g_strdup( _( "_Recurrent operation definitions" )));
}

/*
 * iexportable_export:
 *
 * Exports the models.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub )
{
	gchar *str;
	GList *dataset, *it;
	gboolean with_headers, ok;
	gulong count;

	dataset = ofo_recurrent_model_get_dataset( hub );
	with_headers = ofa_stream_format_get_with_headers( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		ok = ofa_iexportable_set_line( exportable, str );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
		ok = ofa_iexportable_set_line( exportable, str );
		g_free( str );
		if( !ok ){
			return( FALSE );
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
	static const gchar *thisfn = "ofo_account_iimportable_iface_init";

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
 * ofo_recurrent_model_iimportable_import:
 * @importer: the #ofaIImporter instance.
 * @parms: the #ofsImporterParms arguments.
 * @lines: the list of fields-splitted lines.
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - mnemo id
 * - label
 * - ope template
 * - periodicity
 * - periodicity detail
 * - notes (opt)
 *
 * Returns: the total count of errors.
 */
static guint
iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	gchar *bck_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "REC_T_MODELS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_hub_get_collector( parms->hub ), OFO_TYPE_RECURRENT_MODEL );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_RECURRENT_MODEL );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "REC_T_MODELS" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
	}

	return( parms->parse_errs+parms->insert_errs );
}

static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	guint numline, total;
	const gchar *cstr, *clabel, *cperiod;
	GSList *itl, *fields, *itf;
	ofoRecurrentModel *model;
	gchar *str, *splitted;

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
		model = ofo_recurrent_model_new();

		/* mnemo */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty model mnemonic" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_mnemo( model, cstr );

		/* label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty model label" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_label( model, cstr );

		/* ope template */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty target operation template" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_ope_template( model, cstr );

		/* periodicity */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty model periodicity" ));
			parms->parse_errs += 1;
			continue;
		}
		clabel = ofa_periodicity_get_label( cstr );
		if( !my_strlen( clabel )){
			str = g_strdup_printf( _( "unknown periodicity code: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_periodicity( model, cstr );
		cperiod = cstr;

		/* periodicity detail */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty model detail periodicity" ));
			parms->parse_errs += 1;
			continue;
		}
		clabel = ofa_periodicity_get_detail_label( cperiod, cstr );
		if( !my_strlen( clabel )){
			str = g_strdup_printf( _( "unknown detail periodicity code: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_periodicity_detail( model, cstr );

		/* notes
		 * we are tolerant on the last field... */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_recurrent_model_set_notes( model, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, model );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( dataset );
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	const gchar *model_id;
	gboolean insert;
	guint total;
	ofoRecurrentModel *model;
	gchar *str;

	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		model_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		model = OFO_RECURRENT_MODEL( it->data );

		if( model_get_exists( model, connect )){
			parms->duplicate_count += 1;
			model_id = ofo_recurrent_model_get_mnemo( model );

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate model, replacing previous one" ), model_id );
					model_do_delete( model, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate model, ignored (skipped)" ), model_id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate model" ), model_id );
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}

			ofa_iimporter_progress_text( importer, parms, str );
			g_free( str );
		}

		if( insert ){
			if( model_do_insert( model, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
model_get_exists( const ofoRecurrentModel *model, const ofaIDBConnect *connect )
{
	const gchar *model_id;
	gint count;
	gchar *str;

	count = 0;
	model_id = ofo_recurrent_model_get_mnemo( model );
	str = g_strdup_printf( "SELECT COUNT(*) FROM REC_T_MODELS WHERE REC_MNEMO='%s'", model_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
model_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM REC_T_MODELS", TRUE ));
}

/*
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_model_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_model_isignal_hub_connect";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_signal_connect( hub, SIGNAL_HUB_DELETABLE, G_CALLBACK( hub_on_deletable_object ), NULL );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), NULL );
}

/*
 * SIGNAL_HUB_DELETABLE signal handler
 */
static gboolean
hub_on_deletable_object( ofaHub *hub, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_model_hub_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = hub_is_deletable_ope_template( hub, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM REC_T_MODELS WHERE REC_OPE_TEMPLATE='%s'",
			ofo_ope_template_get_mnemo( template ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
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
			if( my_collate( mnemo, prev_id )){
				hub_on_updated_ope_template_mnemo( hub, mnemo, prev_id );
			}
		}
	}
}

static gboolean
hub_on_updated_ope_template_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_recurrent_model_hub_on_updated_ope_template_mnemo";
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

	return( ok );
}
