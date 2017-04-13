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

#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-ope-template.h"

#include "ofo-rec-period.h"
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
	REC_DEF_AMOUNT1,
	REC_DEF_AMOUNT2,
	REC_DEF_AMOUNT3,
	REC_ENABLED,
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
				OFA_TYPE_COUNTER,
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
		{ OFA_BOX_CSV( REC_DEF_AMOUNT1 ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_DEF_AMOUNT2 ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_DEF_AMOUNT3 ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_ENABLED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( REC_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define MODEL_TABLES_COUNT              2
#define MODEL_EXPORT_VERSION            1

typedef struct {
	GList *docs;
}
	ofoRecurrentModelPrivate;

static ofoRecurrentModel *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gchar             *get_mnemo_new_from( const ofoRecurrentModel *model );
static void               recurrent_model_set_upd_user( ofoRecurrentModel *model, const gchar *upd_user );
static void               recurrent_model_set_upd_stamp( ofoRecurrentModel *model, const GTimeVal *upd_stamp );
static GList             *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean           model_do_insert( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_insert_main( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_do_update( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_update_main( ofoRecurrentModel *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean           model_do_delete( ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gint               model_cmp_by_mnemo( const ofoRecurrentModel *a, const gchar *mnemo );
static void               icollectionable_iface_init( myICollectionableInterface *iface );
static guint              icollectionable_get_interface_version( void );
static GList             *icollectionable_load_collection( void *user_data );
static void               idoc_iface_init( ofaIDocInterface *iface );
static guint              idoc_get_interface_version( void );
static void               iexportable_iface_init( ofaIExportableInterface *iface );
static guint              iexportable_get_interface_version( void );
static gchar             *iexportable_get_label( const ofaIExportable *instance );
static gboolean           iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean           iexportable_export_default( ofaIExportable *exportable );
static void               iimportable_iface_init( ofaIImportableInterface *iface );
static guint              iimportable_get_interface_version( void );
static gchar             *iimportable_get_label( const ofaIImportable *instance );
static guint              iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList             *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void               iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean           model_get_exists( const ofoRecurrentModel *model, const ofaIDBConnect *connect );
static gboolean           model_drop_content( const ofaIDBConnect *connect );
static void               isignalable_iface_init( ofaISignalableInterface *iface );
static void               isignalable_connect_to( ofaISignaler *signaler );
static gboolean           signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean           signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template );
static void               signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean           signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id );
static void               signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentModel, ofo_recurrent_model, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentModel )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoRecurrentModelPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_recurrent_model_get_instance_private( self );

	priv->docs = NULL;
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
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoRecurrentModel dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_recurrent_model_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_RECURRENT_MODEL, getter ));
}

/**
 * ofo_recurrent_model_get_by_mnemo:
 * @getter: a #ofaIGetter instance.
 * @mnemo:
 *
 * Returns: the searched recurrent model, or %NULL.
 *
 * The returned object is owned by the #ofoRecurrentModel class, and should
 * not be unreffed by the caller.
 */
ofoRecurrentModel *
ofo_recurrent_model_get_by_mnemo( ofaIGetter *getter, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_recurrent_model_get_dataset( getter );

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
 * @getter: a #ofaIGetter instance.
 * @ope_template: the #ofoOpeTemplate mnemonic.
 *
 * Returns: %TRUE if any #ofoRecurrentModel use this @ope_template
 *  operation template.
 */
gboolean
ofo_recurrent_model_use_ope_template( ofaIGetter *getter, const gchar *ope_template )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( my_strlen( ope_template ), FALSE );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM REC_T_MODELS WHERE REC_OPE_TEMPLATE='%s'",
			ope_template );

	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count > 0 );
}

/**
 * ofo_recurrent_model_new:
 * @getter: a #ofaIGetter instance.
 */
ofoRecurrentModel *
ofo_recurrent_model_new( ofaIGetter *getter )
{
	ofoRecurrentModel *model;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	model = g_object_new( OFO_TYPE_RECURRENT_MODEL, "ofo-base-getter", getter, NULL );
	OFO_BASE( model )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( model );
}

/**
 * ofo_recurrent_model_new_from_model:
 * @model: the source #oforecurrent_model_ to be copied from.
 *
 * Allocates a new #ofoRecurrentModel object, initializing it with data
 * copied from @model #ofoRecurrentModel source.
 *
 * Update the mnemo to make it unique.
 * Update the label.
 */
ofoRecurrentModel *
ofo_recurrent_model_new_from_model( const ofoRecurrentModel *model )
{
	ofaIGetter *getter;
	ofoRecurrentModel *dest;
	gchar *new_mnemo, *new_label;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	getter = ofo_base_get_getter( OFO_BASE( model ));

	dest = ofo_recurrent_model_new( getter );

	new_mnemo = get_mnemo_new_from( model );
	ofo_recurrent_model_set_mnemo( dest, new_mnemo );
	g_free( new_mnemo );

	new_label = g_strdup_printf( "%s (%s)", ofo_recurrent_model_get_label( model ), _( "Duplicate" ));
	ofo_recurrent_model_set_label( dest, new_label );
	g_free( new_label );

	ofo_recurrent_model_set_ope_template( dest, ofo_recurrent_model_get_ope_template( model ));
	ofo_recurrent_model_set_periodicity( dest, ofo_recurrent_model_get_periodicity( model ));
	ofo_recurrent_model_set_periodicity_detail( dest, ofo_recurrent_model_get_periodicity_detail( model ));
	ofo_recurrent_model_set_notes( dest, ofo_recurrent_model_get_notes( model ));
	ofo_recurrent_model_set_def_amount1( dest, ofo_recurrent_model_get_def_amount1( model ));
	ofo_recurrent_model_set_def_amount2( dest, ofo_recurrent_model_get_def_amount2( model ));
	ofo_recurrent_model_set_def_amount3( dest, ofo_recurrent_model_get_def_amount3( model ));

	return( dest );
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

/*
 * Returns a new mnemo derived from the given one, as a newly allocated
 * string that the caller should g_free().
 */
static gchar *
get_mnemo_new_from( const ofoRecurrentModel *model )
{
	ofaIGetter *getter;
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	str = NULL;
	getter = ofo_base_get_getter( OFO_BASE( model ));
	mnemo = ofo_recurrent_model_get_mnemo( model );
	len_mnemo = my_strlen( mnemo );

	for( i=2 ; ; ++i ){
		/* if we are greater than 9999, there is a problem... */
		maxlen = ( i < 10 ? RECM_MNEMO_MAX_LENGTH-1 :
					( i < 100 ? RECM_MNEMO_MAX_LENGTH-2 :
					( i < 1000 ? RECM_MNEMO_MAX_LENGTH-3 : RECM_MNEMO_MAX_LENGTH-4 )));
		if( maxlen < len_mnemo ){
			str = g_strdup_printf( "%*.*s%d", maxlen, maxlen, mnemo, i );
		} else {
			str = g_strdup_printf( "%s%d", mnemo, i );
		}
		if( !ofo_recurrent_model_get_by_mnemo( getter, str )){
			break;
		}
		g_free( str );
	}

	return( str );
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
 * @model:
 */
const gchar *
ofo_recurrent_model_get_periodicity( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_PERIOD );
}

/**
 * ofo_recurrent_model_get_periodicity_detail:
 * @model:
 */
ofxCounter
ofo_recurrent_model_get_periodicity_detail( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, counter, 0, REC_PERIOD_DETAIL );
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
 * ofo_recurrent_model_get_def_amount1:
 */
const gchar *
ofo_recurrent_model_get_def_amount1( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_DEF_AMOUNT1 );
}

/**
 * ofo_recurrent_model_get_def_amount2:
 */
const gchar *
ofo_recurrent_model_get_def_amount2( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_DEF_AMOUNT2 );
}

/**
 * ofo_recurrent_model_get_def_amount3:
 */
const gchar *
ofo_recurrent_model_get_def_amount3( const ofoRecurrentModel *model )
{
	ofo_base_getter( RECURRENT_MODEL, model, string, NULL, REC_DEF_AMOUNT3 );
}

/**
 * ofo_recurrent_model_get_is_enabled:
 */
gboolean
ofo_recurrent_model_get_is_enabled( const ofoRecurrentModel *model )
{
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( model )->prot->fields, REC_ENABLED );
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );

	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( model ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, model, &deletable );
	}

	return( deletable );
}

/**
 * ofo_recurrent_model_is_valid_data:
 * @mnemo:
 * @label:
 * @ope_template:
 * @period:
 * @detail:
 * @msgerr: [allow-none][out]:
 *
 * Returns: %TRUE if provided datas are enough to make the future
 * #ofoRecurrentModel valid, %FALSE else.
 */
gboolean
ofo_recurrent_model_is_valid_data( const gchar *mnemo, const gchar *label,
		const gchar *ope_template, ofoRecPeriod *period, ofxCounter detail_id, gchar **msgerr )
{
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
	if( !my_strlen( ope_template )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Operation template is empty" ));
		}
		return( FALSE );
	}
	if( !period ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Periodicity is not set" ));
		}
		return( FALSE );
	}
	if( ofo_rec_period_detail_get_count( period ) > 0 && detail_id <= 0 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Periodicity expects details, but no detail is set" ));
		}
		return( FALSE );
	}
	if( ofo_rec_period_detail_get_count( period ) == 0 && detail_id > 0 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Periodicity does not expect detail, but a detail is set" ));
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
ofo_recurrent_model_set_periodicity_detail( ofoRecurrentModel *model, ofxCounter detail )
{
	ofo_base_setter( RECURRENT_MODEL, model, counter, REC_PERIOD_DETAIL, detail );
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
 * ofo_recurrent_model_set_def_amount1:
 */
void
ofo_recurrent_model_set_def_amount1( ofoRecurrentModel *model, const gchar *def_amount )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_DEF_AMOUNT1, def_amount );
}

/**
 * ofo_recurrent_model_set_def_amount2:
 */
void
ofo_recurrent_model_set_def_amount2( ofoRecurrentModel *model, const gchar *def_amount )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_DEF_AMOUNT2, def_amount );
}

/**
 * ofo_recurrent_model_set_def_amount3:
 */
void
ofo_recurrent_model_set_def_amount3( ofoRecurrentModel *model, const gchar *def_amount )
{
	ofo_base_setter( RECURRENT_MODEL, model, string, REC_DEF_AMOUNT3, def_amount );
}

/**
 * ofo_recurrent_model_set_is_enabled:
 */
void
ofo_recurrent_model_set_is_enabled( ofoRecurrentModel *model, gboolean is_enabled )
{
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
	g_return_if_fail( !OFO_BASE( model )->prot->dispose_has_run );

	ofa_box_set_string( OFO_BASE( model )->prot->fields, REC_ENABLED, is_enabled ? "Y":"N" );
}

/**
 * ofo_recurrent_model_doc_get_count:
 * @model: this #ofoRecurrentModel object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_recurrent_model_doc_get_count( ofoRecurrentModel *model )
{
	ofoRecurrentModelPrivate *priv;

	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), 0 );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, 0 );

	priv = ofo_recurrent_model_get_instance_private( model );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_recurrent_model_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown recurrent_model mnemos in REC_T_MODELS_DOC child table.
 *
 * The returned list should be #ofo_recurrent_model_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_recurrent_model_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "REC_T_MODELS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(REC_MNEMO) FROM %s "
			"	WHERE REC_MNEMO NOT IN (SELECT REC_MNEMO FROM REC_T_MODELS)", table );

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
 * ofo_recurrent_model_insert:
 */
gboolean
ofo_recurrent_model_insert( ofoRecurrentModel *recurrent_model )
{
	static const gchar *thisfn = "ofo_recurrent_model_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: model=%p", thisfn, ( void * ) recurrent_model );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_model ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_recurrent_model_get_dataset( getter );

	if( model_do_insert( recurrent_model, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( recurrent_model ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, recurrent_model );
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
	const gchar *period, *def_amount1, *def_amount2, *def_amount3, *userid;
	gchar *label, *template, *notes, *stamp_str;
	GTimeVal stamp;
	ofxCounter detail;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_recurrent_model_get_label( model ));
	template = my_utils_quote_sql( ofo_recurrent_model_get_ope_template( model ));
	def_amount1 = ofo_recurrent_model_get_def_amount1( model );
	def_amount2 = ofo_recurrent_model_get_def_amount2( model );
	def_amount3 = ofo_recurrent_model_get_def_amount3( model );
	notes = my_utils_quote_sql( ofo_recurrent_model_get_notes( model ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_MODELS " );

	g_string_append_printf( query,
			"	(REC_MNEMO,REC_LABEL,REC_OPE_TEMPLATE,"
			"	 REC_PERIOD,REC_PERIOD_DETAIL,"
			"	 REC_DEF_AMOUNT1,REC_DEF_AMOUNT2,REC_DEF_AMOUNT3,"
			"	 REC_ENABLED,"
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
	if( my_strlen( period ) > 0 ){
		detail = ofo_recurrent_model_get_periodicity_detail( model );
		if( detail > 0 ){
			g_string_append_printf( query, "'%s',%lu,", period, detail );
		} else {
			g_string_append_printf( query, "'%s',NULL,", period );
		}
	} else {
		query = g_string_append( query, "NULL,NULL," );
	}

	if( my_strlen( def_amount1 )){
		g_string_append_printf( query, "'%s',", def_amount1 );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( def_amount2 )){
		g_string_append_printf( query, "'%s',", def_amount2 );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( def_amount3 )){
		g_string_append_printf( query, "'%s',", def_amount3 );
	} else {
		query = g_string_append( query, "NULL," );
	}

	query = g_string_append( query, ofo_recurrent_model_get_is_enabled( model ) ? "'Y',":"'N'," );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: model=%p, prev_mnemo=%s",
			thisfn, ( void * ) recurrent_model, prev_mnemo );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_model ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( model_do_update( recurrent_model, ofa_hub_get_connect( hub ), prev_mnemo )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, recurrent_model, prev_mnemo );
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
	gchar *label, *notes;
	const gchar *new_mnemo, *template, *period, *def_amount, *userid;
	gchar *stamp_str;
	GTimeVal stamp;
	ofxCounter detail;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_recurrent_model_get_label( model ));
	notes = my_utils_quote_sql( ofo_recurrent_model_get_notes( model ));
	new_mnemo = ofo_recurrent_model_get_mnemo( model );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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
	if( my_strlen( period ) > 0 ){
		detail = ofo_recurrent_model_get_periodicity_detail( model );
		if( detail > 0 ){
			g_string_append_printf( query, "REC_PERIOD='%s',", period );
			g_string_append_printf( query, "REC_PERIOD_DETAIL=%lu,", detail );
		} else {
			g_string_append_printf( query, "REC_PERIOD='%s',", period );
			query = g_string_append( query, "REC_PERIOD_DETAIL=NULL," );
		}
	} else {
		query = g_string_append( query, "REC_PERIOD=NULL," );
		query = g_string_append( query, "REC_PERIOD_DETAIL=NULL," );
	}

	def_amount = ofo_recurrent_model_get_def_amount1( model );
	if( my_strlen( def_amount )){
		g_string_append_printf( query, "REC_DEF_AMOUNT1='%s',", def_amount );
	} else {
		query = g_string_append( query, "REC_DEF_AMOUNT1=NULL," );
	}

	def_amount = ofo_recurrent_model_get_def_amount2( model );
	if( my_strlen( def_amount )){
		g_string_append_printf( query, "REC_DEF_AMOUNT2='%s',", def_amount );
	} else {
		query = g_string_append( query, "REC_DEF_AMOUNT2=NULL," );
	}

	def_amount = ofo_recurrent_model_get_def_amount3( model );
	if( my_strlen( def_amount )){
		g_string_append_printf( query, "REC_DEF_AMOUNT3='%s',", def_amount );
	} else {
		query = g_string_append( query, "REC_DEF_AMOUNT3=NULL," );
	}

	query = g_string_append( query, "REC_ENABLED=" );
	query = g_string_append( query, ofo_recurrent_model_get_is_enabled( model ) ? "'Y',":"'N'," );

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

	return( ok );
}

/**
 * ofo_recurrent_model_delete:
 */
gboolean
ofo_recurrent_model_delete( ofoRecurrentModel *recurrent_model )
{
	static const gchar *thisfn = "ofo_recurrent_model_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: model=%p", thisfn, ( void * ) recurrent_model );

	g_return_val_if_fail( recurrent_model && OFO_IS_RECURRENT_MODEL( recurrent_model ), FALSE );
	g_return_val_if_fail( ofo_recurrent_model_is_deletable( recurrent_model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_model )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( recurrent_model ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( model_do_delete( recurrent_model, ofa_hub_get_connect( hub ))){
		g_object_ref( recurrent_model );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( recurrent_model ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, recurrent_model );
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
	return( my_collate( ofo_recurrent_model_get_mnemo( a ), mnemo ));
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

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_MODELS",
					OFO_TYPE_RECURRENT_MODEL,
					OFA_IGETTER( user_data ));

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_model_idoc_iface_init";

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
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the models.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_recurrent_model_iexportable_export";

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
	gchar *str1, *str2;
	GList *dataset, *it, *itd;
	gboolean ok;
	gulong count;
	ofoRecurrentModel *model;
	ofoRecurrentModelPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_recurrent_model_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += MODEL_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		count += ofo_recurrent_model_doc_get_count( model );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, MODEL_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					MODEL_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( model )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_recurrent_model_get_instance_private( model );

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
			str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
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
 * - periodicity identifier
 * - periodicity detail identifier
 * - notes (opt)
 * - last update user (placeholder)
 * - last update timestamp (placeholder)
 * - def_amount1
 * - def_amount2
 * - def_amount3
 *
 * Returns: the total count of errors.
 */
static guint
iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	ofaISignaler *signaler;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *dataset;
	gchar *bck_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "REC_T_MODELS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_RECURRENT_MODEL );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_RECURRENT_MODEL );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "REC_T_MODELS" );
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
	const gchar *cstr, *perid;
	GSList *itl, *fields, *itf;
	ofoRecurrentModel *model;
	gchar *str, *splitted;
	ofoRecPeriod *period;
	ofxCounter perdetid;
	gint idx;

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
		model = ofo_recurrent_model_new( parms->getter );

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

		/* periodicity id. */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty periodicity" ));
			parms->parse_errs += 1;
			continue;
		}
		perid = cstr;
		period = ofo_rec_period_get_by_id( parms->getter, perid );
		if( !period ){
			str = g_strdup_printf( _( "unknown periodicity identifier: %s" ), perid );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_recurrent_model_set_periodicity( model, perid );

		/* periodicity detail id. */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( period ){
				if( ofo_rec_period_detail_get_count( period ) > 0 ){
					perdetid = atol( cstr );
					idx = ofo_rec_period_detail_get_by_id( period, perdetid );
					if( idx >= 0 ){
						ofo_recurrent_model_set_periodicity_detail( model, perdetid );
					} else {
						str = g_strdup_printf( _( "unknown periodicity detail identifier: %lu" ), perdetid );
						ofa_iimporter_progress_num_text( importer, parms, numline, str );
						g_free( str );
						parms->parse_errs += 1;
						continue;
					}
				} else {
					str = g_strdup_printf( _( "periodicity does not accept details, %s identifier found" ), cstr );
					ofa_iimporter_progress_num_text( importer, parms, numline, str );
					g_free( str );
					parms->parse_errs += 1;
					continue;
				}
			} else {
				ofa_iimporter_progress_num_text( importer, parms, numline,
						_( "found periodictiy detail, but periodicity is unknown" ));
				parms->parse_errs += 1;
				continue;
			}
		} else if( period ){
			if( ofo_rec_period_detail_get_count( period ) > 0 ){
				ofa_iimporter_progress_num_text( importer, parms, numline,
						_( "periodicity expects unspecified details" ));
				parms->parse_errs += 1;
				continue;
			}
		}

		/* notes */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			splitted = my_utils_import_multi_lines( cstr );
			ofo_recurrent_model_set_notes( model, splitted );
			g_free( splitted );
		}

		/* last update user (placeholder) */
		itf = itf ? itf->next : NULL;

		/* last update timestammp (placeholder) */
		itf = itf ? itf->next : NULL;

		/* def_amount1 */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			ofo_recurrent_model_set_def_amount1( model, cstr );
		}

		/* def_amount2 */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			ofo_recurrent_model_set_def_amount2( model, cstr );
		}

		/* def_amount3 */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			ofo_recurrent_model_set_def_amount3( model, cstr );
		}

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
	ofaHub *hub;
	const ofaIDBConnect *connect;
	const gchar *model_id;
	gboolean insert;
	guint total, type;
	ofoRecurrentModel *model;
	gchar *str;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
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
			type = MY_PROGRESS_NORMAL;

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
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_recurrent_model_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_recurrent_model_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_model_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = signaler_is_deletable_ope_template( signaler, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template )
{
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	return( !ofo_recurrent_model_use_ope_template( getter, ofo_ope_template_get_mnemo( template )));
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_model_signaler_on_updated_base";
	const gchar *mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_ope_template_mnemo( signaler, object, mnemo, prev_id );
			}
		}
	}
}

static gboolean
signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, ofoBase *object, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_recurrent_model_signaler_on_updated_ope_template_mnemo";
	gchar *query;
	ofaIGetter *getter;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gboolean ok;
	myICollector *collector;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE REC_T_MODELS "
					"	SET REC_OPE_TEMPLATE='%s' "
					"	WHERE REC_OPE_TEMPLATE='%s'",
							mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	collector = ofa_igetter_get_collector( getter );
	my_icollector_collection_free( collector, OFO_TYPE_RECURRENT_MODEL );

	return( ok );
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_recurrent_model_signaler_on_deleted_base";
	ofoOpeTemplate *template_obj;
	ofaIGetter *getter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	getter = ofa_isignaler_get_getter( signaler );

	if( OFO_IS_RECURRENT_MODEL( object )){
		template_obj = ofo_ope_template_get_by_mnemo( getter, ofo_recurrent_model_get_ope_template( OFO_RECURRENT_MODEL( object )));
		if( template_obj ){
			g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, template_obj, NULL );
		}
	}
}
