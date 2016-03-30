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

#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* priv instance data
 */
enum {
	OTE_MNEMO = 1,
	OTE_LABEL,
	OTE_LED_MNEMO,
	OTE_LED_LOCKED,
	OTE_REF,
	OTE_REF_LOCKED,
	OTE_NOTES,
	OTE_UPD_USER,
	OTE_UPD_STAMP,
	OTE_DET_ROW,
	OTE_DET_COMMENT,
	OTE_DET_ACCOUNT,
	OTE_DET_ACCOUNT_LOCKED,
	OTE_DET_LABEL,
	OTE_DET_LABEL_LOCKED,
	OTE_DET_DEBIT,
	OTE_DET_DEBIT_LOCKED,
	OTE_DET_CREDIT,
	OTE_DET_CREDIT_LOCKED
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( OTE_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( OTE_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_LED_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_LED_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_REF ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_REF_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( OTE_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_detail_defs[] = {
		{ OFA_BOX_CSV( OTE_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( OTE_DET_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_COMMENT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_ACCOUNT_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_LABEL_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_DEBIT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_DEBIT_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_CREDIT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( OTE_DET_CREDIT_LOCKED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

typedef struct {

	/* the details of the operation template as a GList of GList fields
	 */
	GList     *details;
}
	ofoOpeTemplatePrivate;

static void            on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean        on_update_ledger_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );
static gboolean        on_update_rate_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );
static ofoOpeTemplate *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gint            model_count_for_ledger( const ofaIDBConnect *connect, const gchar *ledger );
static gint            model_count_for_rate( const ofaIDBConnect *connect, const gchar *mnemo );
static void            ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user );
static void            ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp );
static gboolean        model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_insert_details( ofoOpeTemplate *model, const ofaIDBConnect *connect, gint rang, GList *details );
static gboolean        model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_do_delete( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gint            model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo );
static gint            ope_template_cmp_by_ptr( const ofoOpeTemplate *a, const ofoOpeTemplate *b );
static void            icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint           icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList          *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void            iexportable_iface_init( ofaIExportableInterface *iface );
static guint           iexportable_get_interface_version( const ofaIExportable *instance );
static gchar          *iexportable_get_label( const ofaIExportable *instance );
static gboolean        iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static gchar          *update_decimal_sep( const ofsBoxData *box_data, const ofaFileFormat *format, const gchar *text, void *empty );
static void            iimportable_iface_init( ofaIImportableInterface *iface );
static guint           iimportable_get_interface_version( const ofaIImportable *instance );
static gchar          *iimportable_get_label( const ofaIImportable *instance );
static gboolean        iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub );
static ofoOpeTemplate *model_import_csv_model( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint count, guint *errors );
static GList          *model_import_csv_detail( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint count, guint *errors, gchar **mnemo );
static gboolean        model_do_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoOpeTemplate, ofo_ope_template, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoOpeTemplate )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

static void
details_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
details_list_free( ofoOpeTemplate *model )
{
	ofoOpeTemplatePrivate *priv;

	priv = ofo_ope_template_get_instance_private( model );

	g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
	priv->details = NULL;
}

static void
ope_template_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ope_template_finalize";
	ofoOpeTemplatePrivate *priv;

	priv = ofo_ope_template_get_instance_private( OFO_OPE_TEMPLATE( instance ));

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, OTE_MNEMO ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, OTE_LABEL ));

	g_return_if_fail( instance && OFO_IS_OPE_TEMPLATE( instance ));

	/* free data members here */

	if( priv->details ){
		details_list_free( OFO_OPE_TEMPLATE( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ope_template_parent_class )->finalize( instance );
}

static void
ope_template_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_OPE_TEMPLATE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ope_template_parent_class )->dispose( instance );
}

static void
ofo_ope_template_init( ofoOpeTemplate *self )
{
	static const gchar *thisfn = "ofo_ope_template_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_ope_template_class_init( ofoOpeTemplateClass *klass )
{
	static const gchar *thisfn = "ofo_ope_template_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_finalize;
}

/**
 * ofo_ope_template_connect_to_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_ope_template_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_ope_template_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_signal_connect(
			G_OBJECT( hub ), SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), NULL );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_ope_template_on_hub_updated_object";
	const gchar *mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_update_ledger_mnemo( hub, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_RATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_update_rate_mnemo( hub, mnemo, prev_id );
			}
		}
	}
}

static gboolean
on_update_ledger_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_ledger_mnemo";
	gchar *query;
	gboolean ok;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

	query = g_strdup_printf(
					"UPDATE OFA_T_OPE_TEMPLATES "
					"	SET OTE_LED_MNEMO='%s' WHERE OTE_LED_MNEMO='%s'",
								mnemo, prev_id );

	ok = ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_OPE_TEMPLATE );

	g_signal_emit_by_name( hub, SIGNAL_HUB_RELOAD, OFO_TYPE_OPE_TEMPLATE );

	return( ok );
}

static gboolean
on_update_rate_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_rate_mnemo";
	gchar *query;
	const ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_debit, *det_credit;
	gint det_row;
	gboolean ok;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"SELECT OTE_MNEMO,OTE_DET_ROW,OTE_DET_DEBIT,OTE_DET_CREDIT "
					"	FROM OFA_T_OPE_TEMPLATES_DET "
					"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
							prev_id, prev_id );

	ok = ofa_idbconnect_query_ex( connect, query, &result, TRUE );
	g_free( query );

	if( ok ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			etp_mnemo = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			det_row = atoi(( gchar * ) icol->data );
			icol = icol->next;
			det_debit = my_utils_str_replace(( gchar * ) icol->data, prev_id, mnemo );
			icol = icol->next;
			det_credit = my_utils_str_replace(( gchar * ) icol->data, prev_id, mnemo );

			query = g_strdup_printf(
							"UPDATE OFA_T_OPE_TEMPLATES_DET "
							"	SET OTE_DET_DEBIT='%s',OTE_DET_CREDIT='%s' "
							"	WHERE OTE_MNEMO='%s' AND OTE_DET_ROW=%d",
									det_debit, det_credit,
									etp_mnemo, det_row );

			ofa_idbconnect_query( connect, query, TRUE );

			g_free( query );
			g_free( det_credit );
			g_free( det_debit );
			g_free( etp_mnemo );
		}

		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_OPE_TEMPLATE );

		g_signal_emit_by_name( hub, SIGNAL_HUB_RELOAD, OFO_TYPE_OPE_TEMPLATE );
	}

	return( ok );
}

/**
 * ofo_ope_template_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoOpeTemplate dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_ope_template_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_OPE_TEMPLATE ));
}

/**
 * ofo_ope_template_get_by_mnemo:
 *
 * Returns: the searched model, or %NULL.
 *
 * The returned object is owned by the #ofoOpeTemplate class, and should
 * not be unreffed by the caller.
 */
ofoOpeTemplate *
ofo_ope_template_get_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_ope_template_get_dataset( hub );

	return( model_find_by_mnemo( dataset, mnemo ));
}

static ofoOpeTemplate *
model_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) model_cmp_by_mnemo );
	if( found ){
		return( OFO_OPE_TEMPLATE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_use_ledger:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_ope_template_use_ledger( ofaHub *hub, const gchar *ledger )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( my_strlen( ledger ), FALSE );

	/* make sure dataset is loaded */
	ofo_ope_template_get_dataset( hub );

	return( model_count_for_ledger( ofa_hub_get_connect( hub ), ledger ) > 0 );
}

static gint
model_count_for_ledger( const ofaIDBConnect *connect, const gchar *ledger )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES WHERE OTE_LED_MNEMO='%s'", ledger );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_ope_template_use_rate:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified rate.
 */
gboolean
ofo_ope_template_use_rate( ofaHub *hub, const gchar *mnemo )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	/* make sure dataset is loaded */
	ofo_ope_template_get_dataset( hub );

	return( model_count_for_rate( ofa_hub_get_connect( hub ), mnemo ) > 0 );
}

static gint
model_count_for_rate( const ofaIDBConnect *connect, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES_DET "
				"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
					mnemo, mnemo );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_ope_template_new:
 */
ofoOpeTemplate *
ofo_ope_template_new( void )
{
	ofoOpeTemplate *model;

	model = g_object_new( OFO_TYPE_OPE_TEMPLATE, NULL );
	OFO_BASE( model )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( model );
}

/**
 * ofo_ope_template_new_from_template:
 * @model: the source #ofoOpeTemplate to be copied from.
 *
 * Allocates a new #ofoOpeTemplates object, initializing it with data
 * copied from @model #ofoOpeTemplate source.
 */
ofoOpeTemplate *
ofo_ope_template_new_from_template( const ofoOpeTemplate *model )
{
	ofoOpeTemplate *dest;
	gint count, i;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	dest = ofo_ope_template_new();

	ofo_ope_template_set_mnemo( dest, ofo_ope_template_get_mnemo( model ));
	ofo_ope_template_set_label( dest, ofo_ope_template_get_label( model ));
	ofo_ope_template_set_ledger( dest, ofo_ope_template_get_ledger( model ));
	ofo_ope_template_set_ledger_locked( dest, ofo_ope_template_get_ledger_locked( model ));
	ofo_ope_template_set_ref( dest, ofo_ope_template_get_ref( model ));
	ofo_ope_template_set_ref_locked( dest, ofo_ope_template_get_ref_locked( model ));
	ofo_ope_template_set_notes( dest, ofo_ope_template_get_notes( model ));

	count = ofo_ope_template_get_detail_count( model );
	for( i=0 ; i<count ; ++i ){
		ofo_ope_template_add_detail( dest,
				ofo_ope_template_get_detail_comment( model, i ),
				ofo_ope_template_get_detail_account( model, i ),
				ofo_ope_template_get_detail_account_locked( model, i ),
				ofo_ope_template_get_detail_label( model, i ),
				ofo_ope_template_get_detail_label_locked( model, i ),
				ofo_ope_template_get_detail_debit( model, i ),
				ofo_ope_template_get_detail_debit_locked( model, i ),
				ofo_ope_template_get_detail_credit( model, i ),
				ofo_ope_template_get_detail_credit_locked( model, i ));
	}

	return( dest );
}

/**
 * ofo_ope_template_get_mnemo:
 */
const gchar *
ofo_ope_template_get_mnemo( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_MNEMO );
}

/**
 * ofo_ope_template_get_mnemo_new_from:
 *
 * Returns a new mnemo derived from the given one, as a newly allocated
 * string that the caller should g_free().
 */
gchar *
ofo_ope_template_get_mnemo_new_from( const ofoOpeTemplate *model )
{
	ofaHub *hub;
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	str = NULL;
	hub = ofo_base_get_hub( OFO_BASE( model ));
	mnemo = ofo_ope_template_get_mnemo( model );
	len_mnemo = my_strlen( mnemo );
	for( i=2 ; ; ++i ){
		/* if we are greater than 9999, there is a problem... */
		maxlen = ( i < 10 ? OTE_MNEMO_MAX_LENGTH-1 :
					( i < 100 ? OTE_MNEMO_MAX_LENGTH-2 :
					( i < 1000 ? OTE_MNEMO_MAX_LENGTH-3 : OTE_MNEMO_MAX_LENGTH-4 )));
		if( maxlen < len_mnemo ){
			str = g_strdup_printf( "%*.*s%d", maxlen, maxlen, mnemo, i );
		} else {
			str = g_strdup_printf( "%s%d", mnemo, i );
		}
		if( !ofo_ope_template_get_by_mnemo( hub, str )){
			break;
		}
		g_free( str );
	}

	return( str );
}

/**
 * ofo_ope_template_get_label:
 */
const gchar *
ofo_ope_template_get_label( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_LABEL );
}

/**
 * ofo_ope_template_get_ledger:
 */
const gchar *
ofo_ope_template_get_ledger( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_LED_MNEMO );
}

/**
 * ofo_ope_template_get_ledger_locked:
 */
gboolean
ofo_ope_template_get_ledger_locked( const ofoOpeTemplate *model )
{
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( model )->prot->fields, OTE_LED_LOCKED );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_ope_template_get_ref:
 */
const gchar *
ofo_ope_template_get_ref( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_REF );
}

/**
 * ofo_ope_template_get_ref_locked:
 */
gboolean
ofo_ope_template_get_ref_locked( const ofoOpeTemplate *model )
{
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( model )->prot->fields, OTE_REF_LOCKED );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_ope_template_get_notes:
 */
const gchar *
ofo_ope_template_get_notes( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_NOTES );
}

/**
 * ofo_ope_template_get_upd_user:
 */
const gchar *
ofo_ope_template_get_upd_user( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, string, NULL, OTE_UPD_USER );
}

/**
 * ofo_ope_template_get_upd_stamp:
 */
const GTimeVal *
ofo_ope_template_get_upd_stamp( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, timestamp, NULL, OTE_UPD_STAMP );
}

/**
 * ofo_ope_template_is_deletable:
 * @model: the operation template
 *
 * Returns: %TRUE if the operation template is deletable.
 */
gboolean
ofo_ope_template_is_deletable( const ofoOpeTemplate *model )
{
	ofaHub *hub;
	const gchar *mnemo;
	ofoDossier *dossier;
	gboolean deletable;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( model ));
	dossier = ofa_hub_get_dossier( hub );
	mnemo = ofo_ope_template_get_mnemo( model );

	deletable = !ofo_entry_use_ope_template( hub, mnemo ) &&
				!ofo_dossier_use_ope_template( dossier, mnemo );

	deletable &= ofa_idbmodel_get_is_deletable( hub, OFO_BASE( model ));

	return( deletable );
}

/**
 * ofo_ope_template_is_valid_data:
 * @mnemo: operation template mnemomnic.
 * @label: operation template label.
 * @ledger: attached ledger mnemonic.
 * @msgerr: [allow-none]: error message placeholder.
 *
 * Returns: %TRUE if the arguments are valid and let us save the
 * operation template, %FALSE else. In this later case, an error
 * message is set in @msgerr, and should be #g_free() by the caller.
 */
gboolean
ofo_ope_template_is_valid_data( const gchar *mnemo, const gchar *label, const gchar *ledger, gchar **msgerr )
{
	gboolean ok;

	ok = FALSE;
	if( msgerr ){
		*msgerr = NULL;
	}

	if( !my_strlen( mnemo )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty mnemonic" ));
		}
	} else if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
		}
	} else if( !my_strlen( ledger )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty ledger" ));
		}
	} else {
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_ope_template_set_mnemo:
 */
void
ofo_ope_template_set_mnemo( ofoOpeTemplate *model, const gchar *mnemo )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_MNEMO, mnemo );
}

/**
 * ofo_ope_template_set_label:
 */
void
ofo_ope_template_set_label( ofoOpeTemplate *model, const gchar *label )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_LABEL, label );
}

/**
 * ofo_ope_template_set_ledger:
 */
void
ofo_ope_template_set_ledger( ofoOpeTemplate *model, const gchar *ledger )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_LED_MNEMO, ledger );
}

/**
 * ofo_ope_template_set_ledger_locked:
 */
void
ofo_ope_template_set_ledger_locked( ofoOpeTemplate *model, gboolean ledger_locked )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_LED_LOCKED, ledger_locked ? "Y":"N" );
}

/**
 * ofo_ope_template_set_ref:
 */
void
ofo_ope_template_set_ref( ofoOpeTemplate *model, const gchar *ref )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_REF, ref );
}

/**
 * ofo_ope_template_set_ref:
 */
void
ofo_ope_template_set_ref_locked( ofoOpeTemplate *model, gboolean ref_locked )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_REF_LOCKED, ref_locked ? "Y":"N" );
}

/**
 * ofo_ope_template_set_notes:
 */
void
ofo_ope_template_set_notes( ofoOpeTemplate *model, const gchar *notes )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_NOTES, notes );
}

/*
 * ofo_ope_template_set_upd_user:
 */
static void
ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_UPD_USER, upd_user );
}

/*
 * ofo_ope_template_set_upd_stamp:
 */
static void
ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp )
{
	ofo_base_setter( OPE_TEMPLATE, model, string, OTE_UPD_STAMP, upd_stamp );
}

void
ofo_ope_template_add_detail( ofoOpeTemplate *model,
								const gchar *comment,
								const gchar *account, gboolean account_locked,
								const gchar *label, gboolean label_locked,
								const gchar *debit, gboolean debit_locked,
								const gchar *credit, gboolean credit_locked )
{
	ofoOpeTemplatePrivate *priv;
	GList *fields;

	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));
	g_return_if_fail( !OFO_BASE( model )->prot->dispose_has_run );

	priv = ofo_ope_template_get_instance_private( model );

	fields = ofa_box_init_fields_list( st_detail_defs );
	ofa_box_set_string( fields, OTE_MNEMO, ofo_ope_template_get_mnemo( model ));
	ofa_box_set_int( fields, OTE_DET_ROW, 1+ofo_ope_template_get_detail_count( model ));
	ofa_box_set_string( fields, OTE_DET_COMMENT, comment );
	ofa_box_set_string( fields, OTE_DET_ACCOUNT, account );
	ofa_box_set_string( fields, OTE_DET_ACCOUNT_LOCKED, account_locked ? "Y":"N" );
	ofa_box_set_string( fields, OTE_DET_LABEL, label );
	ofa_box_set_string( fields, OTE_DET_LABEL_LOCKED, label_locked ? "Y":"N" );
	ofa_box_set_string( fields, OTE_DET_DEBIT, debit );
	ofa_box_set_string( fields, OTE_DET_DEBIT_LOCKED, debit_locked ? "Y":"N" );
	ofa_box_set_string( fields, OTE_DET_CREDIT, credit );
	ofa_box_set_string( fields, OTE_DET_CREDIT_LOCKED, credit_locked ? "Y":"N" );

	priv->details = g_list_append( priv->details, fields );
}

/**
 * ofo_ope_template_free_detail_all:
 */
void
ofo_ope_template_free_detail_all( ofoOpeTemplate *model )
{
	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));
	g_return_if_fail( !OFO_BASE( model )->prot->dispose_has_run );

	details_list_free( model );
}

/**
 * ofo_ope_template_get_detail_count:
 */
gint
ofo_ope_template_get_detail_count( const ofoOpeTemplate *model )
{
	ofoOpeTemplatePrivate *priv;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), 0 );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, 0 );

	priv = ofo_ope_template_get_instance_private( model );

	return( g_list_length( priv->details ));
}

/**
 * ofo_ope_template_get_detail_comment:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_comment( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, OTE_DET_COMMENT ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_account:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_account( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, OTE_DET_ACCOUNT ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_account_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_account_locked( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		cstr = ofa_box_get_string( nth->data, OTE_DET_ACCOUNT_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_label( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, OTE_DET_LABEL ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_label_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_label_locked( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		cstr = ofa_box_get_string( nth->data, OTE_DET_LABEL_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_debit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_debit( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, OTE_DET_DEBIT ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_debit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_debit_locked( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		cstr = ofa_box_get_string( nth->data, OTE_DET_DEBIT_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_credit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_credit( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		return( ofa_box_get_string( nth->data, OTE_DET_CREDIT ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_credit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_credit_locked( const ofoOpeTemplate *model, gint idx )
{
	ofoOpeTemplatePrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	priv = ofo_ope_template_get_instance_private( model );

	nth = g_list_nth( priv->details, idx );
	if( nth ){
		cstr = ofa_box_get_string( nth->data, OTE_DET_CREDIT_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_insert:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_ope_template_insert( ofoOpeTemplate *ope_template, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_ope_template_insert";
	gboolean ok;

	g_debug( "%s: ope_template=%p, hub=%p",
			thisfn, ( void * ) ope_template, ( void * ) hub );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( model_do_insert( ope_template, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( ope_template ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( ope_template ), ( GCompareFunc ) ope_template_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, ope_template );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	return( model_insert_main( model, connect ) &&
			model_insert_details_ex( model, connect ));
}

static gboolean
model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *ref, *userid;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_ope_template_get_label( model ));
	ref = my_utils_quote_single( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote_single( ofo_ope_template_get_notes( model ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_OPE_TEMPLATES" );

	g_string_append_printf( query,
			"	(OTE_MNEMO,OTE_LABEL,OTE_LED_MNEMO,OTE_LED_LOCKED,"
			"	OTE_REF,OTE_REF_LOCKED,OTE_NOTES,"
			"	OTE_UPD_USER, OTE_UPD_STAMP) VALUES ('%s','%s','%s','%s',",
			ofo_ope_template_get_mnemo( model ),
			label,
			ofo_ope_template_get_ledger( model ),
			ofo_ope_template_get_ledger_locked( model ) ? "Y":"N" );

	if( my_strlen( ref )){
		g_string_append_printf( query, "'%s',", ref );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_ope_template_get_ref_locked( model ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	ope_template_set_upd_user( model, userid );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

static gboolean
model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s'",
			ofo_ope_template_get_mnemo( model ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	ofoOpeTemplatePrivate *priv;
	gboolean ok;
	GList *idet;
	gint rang;

	ok = FALSE;
	priv = ofo_ope_template_get_instance_private( model );

	if( model_delete_details( model, connect )){
		ok = TRUE;
		for( idet=priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !model_insert_details( model, connect, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
model_insert_details( ofoOpeTemplate *model, const ofaIDBConnect *connect, gint rang, GList *details )
{
	GString *query;
	gboolean ok;
	gchar *label, *comment, *account;
	const gchar *cstr;

	query = g_string_new( "INSERT INTO OFA_T_OPE_TEMPLATES_DET " );

	g_string_append_printf( query,
			"	(OTE_MNEMO,OTE_DET_ROW,OTE_DET_COMMENT,"
			"	OTE_DET_ACCOUNT,OTE_DET_ACCOUNT_LOCKED,"
			"	OTE_DET_LABEL,OTE_DET_LABEL_LOCKED,"
			"	OTE_DET_DEBIT,OTE_DET_DEBIT_LOCKED,"
			"	OTE_DET_CREDIT,OTE_DET_CREDIT_LOCKED) "
			"	VALUES('%s',%d,",
			ofo_ope_template_get_mnemo( model ), rang );

	comment = my_utils_quote_single( ofa_box_get_string( details, OTE_DET_COMMENT ));
	if( my_strlen( comment )){
		g_string_append_printf( query, "'%s',", comment );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( comment );

	account = my_utils_quote_single( ofa_box_get_string( details, OTE_DET_ACCOUNT ));
	if( my_strlen( account )){
		g_string_append_printf( query, "'%s',", account );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( account );

	g_string_append_printf( query, "'%s',", ofa_box_get_string( details, OTE_DET_ACCOUNT_LOCKED ));

	label = my_utils_quote_single( ofa_box_get_string( details, OTE_DET_LABEL ));
	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( label );

	g_string_append_printf( query, "'%s',", ofa_box_get_string( details, OTE_DET_LABEL_LOCKED ));

	cstr = ofa_box_get_string( details, OTE_DET_DEBIT );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "'%s',", cstr );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofa_box_get_string( details, OTE_DET_DEBIT_LOCKED ));

	cstr = ofa_box_get_string( details, OTE_DET_CREDIT );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "'%s',", cstr );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s'", ofa_box_get_string( details, OTE_DET_CREDIT_LOCKED ));

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_ope_template_update:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_ope_template_update( ofoOpeTemplate *ope_template, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ope_template_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ope_template=%p, prev_mnemo=%s",
			thisfn, ( void * ) ope_template, prev_mnemo );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ope_template ));

	if( model_do_update( ope_template, ofa_hub_get_connect( hub ), prev_mnemo )){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_OPE_TEMPLATE, ( GCompareFunc ) ope_template_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, ope_template, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	return( model_update_main( model, connect, prev_mnemo ) &&
			model_insert_details_ex( model, connect ));
}

static gboolean
model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *ref, *notes, *userid;
	const gchar *new_mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_ope_template_get_label( model ));
	ref = my_utils_quote_single( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote_single( ofo_ope_template_get_notes( model ));
	new_mnemo = ofo_ope_template_get_mnemo( model );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_OPE_TEMPLATES SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "OTE_MNEMO='%s',", new_mnemo );
	}

	g_string_append_printf( query, "OTE_LABEL='%s',", label );
	g_string_append_printf( query, "OTE_LED_MNEMO='%s',", ofo_ope_template_get_ledger( model ));
	g_string_append_printf( query, "OTE_LED_LOCKED='%s',", ofo_ope_template_get_ledger_locked( model ) ? "Y":"N" );

	if( my_strlen( ref )){
		g_string_append_printf( query, "OTE_REF='%s',", ref );
	} else {
		query = g_string_append( query, "OTE_REF=NULL," );
	}

	g_string_append_printf( query, "OTE_REF_LOCKED='%s',", ofo_ope_template_get_ref_locked( model ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "OTE_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "OTE_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	OTE_UPD_USER='%s',OTE_UPD_STAMP='%s'"
			"	WHERE OTE_MNEMO='%s'",
					userid,
					stamp_str,
					prev_mnemo );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	ope_template_set_upd_user( model, userid );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );
	g_free( userid );

	return( ok );
}

/**
 * ofo_ope_template_delete:
 */
gboolean
ofo_ope_template_delete( ofoOpeTemplate *ope_template )
{
	static const gchar *thisfn = "ofo_ope_template_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ope_template=%p", thisfn, ( void * ) ope_template );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( ofo_ope_template_is_deletable( ope_template ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( ope_template ));

	if( model_do_delete( ope_template, ofa_hub_get_connect( hub ))){
		g_object_ref( ope_template );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( ope_template ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, ope_template );
		g_object_unref( ope_template );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_delete( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES"
			"	WHERE OTE_MNEMO='%s'",
					ofo_ope_template_get_mnemo( model ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	ok &= model_delete_details( model, connect );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_ope_template_get_mnemo( a ), mnemo ));
}

static gint
ope_template_cmp_by_ptr( const ofoOpeTemplate *a, const ofoOpeTemplate *b )
{
	return( model_cmp_by_mnemo( a, ofo_ope_template_get_mnemo( b )));
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
	ofoOpeTemplatePrivate *priv;
	GList *dataset, *it;
	ofoOpeTemplate *template;
	gchar *from;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_OPE_TEMPLATES ORDER BY OTE_MNEMO ASC",
					OFO_TYPE_OPE_TEMPLATE,
					hub );

	for( it=dataset ; it ; it=it->next ){
		template = OFO_OPE_TEMPLATE( it->data );
		priv = ofo_ope_template_get_instance_private( template );
		from = g_strdup_printf(
				"OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s' ORDER BY OTE_DET_ROW ASC",
				ofo_ope_template_get_mnemo( template ));
		priv->details =
				ofo_base_load_rows( st_detail_defs, ofa_hub_get_connect( hub ), from );
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
	static const gchar *thisfn = "ofo_ope_template_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "Reference : operation _templates" )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	ofoOpeTemplatePrivate *priv;
	GList *dataset, *it, *det;
	gchar *str, *str2;
	ofoOpeTemplate *model;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;

	dataset = ofo_ope_template_get_dataset( hub );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=dataset ; it ; it=it->next ){
		model = OFO_OPE_TEMPLATE( it->data );
		count += ofo_ope_template_get_detail_count( model );
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

		model = OFO_OPE_TEMPLATE( it->data );
		priv = ofo_ope_template_get_instance_private( model );

		for( det=priv->details ; det ; det=det->next ){
			str = ofa_box_csv_get_line_ex( det->data, settings, ( CSVExportFunc ) update_decimal_sep, NULL );
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
 * operation template debit and credit are managed as strings, and are
 * thus not remediated for decimal separator -
 * but they may contain amounts and so should.
 * => this user-remediation function
 */
static gchar *
update_decimal_sep( const ofsBoxData *box_data, const ofaFileFormat *settings, const gchar *text, void *empty )
{
	const ofsBoxDef *box_def;
	gchar *str;
	gchar decimal_sep;

	str = g_strdup( text );
	box_def = ofa_box_data_get_def( box_data );

	if( box_def->id == OTE_DET_DEBIT || box_def->id == OTE_DET_CREDIT ){
		decimal_sep = ofa_file_format_get_decimal_sep( settings );
		if( decimal_sep != '.' ){
			g_free( str );
			str = my_utils_char_replace( text, '.', decimal_sep );
		}
	}

	return( str );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_ope_template_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->get_label = iimportable_get_label;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

static gchar *
iimportable_get_label( const ofaIImportable *instance )
{
	return( iexportable_get_label( OFA_IEXPORTABLE( instance )));
}

/*
 * ofo_ope_template_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - model mnemo
 * - label
 * - ledger
 * - ledger locked
 * - ref
 * - ref locked
 * - notes (opt)
 *
 * - 2:
 * - model mnemo
 * - comment
 * - account
 * - account locked
 * - label
 * - label locked
 * - debit
 * - debit locked
 * - credit
 * - credit locked
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'model' records, then all 'validity' records...
 *
 * Replace the whole table with the provided datas.
 *
 * Returns: 0 if no error has occurred, >0 if an error has been detected
 * during import phase (input file read), <0 if an error has occured
 * during insert phase.
 *
 * As the table is dropped between import phase and insert phase, if an
 * error occurs during insert phase, then the table is changed and only
 * contains the successfully inserted records.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoOpeTemplate *model;
	ofoOpeTemplatePrivate *priv;
	GList *dataset, *it;
	guint errors, line;
	gchar *msg, *mnemo;
	gint type;
	GList *detail;
	const ofaIDBConnect *connect;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = atoi( cstr );
		switch( type ){
			case 1:
				model = model_import_csv_model( importable, fields, settings, line, &errors );
				if( model ){
					dataset = g_list_prepend( dataset, model );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = model_import_csv_detail( importable, fields, settings, line, &errors, &mnemo );
				if( detail ){
					model = model_find_by_mnemo( dataset, mnemo );
					if( model ){
						priv = ofo_ope_template_get_instance_private( model );
						ofa_box_set_int( detail, OTE_DET_ROW, 1+ofo_ope_template_get_detail_count( model ));
						priv->details = g_list_append( priv->details, detail );
					}
					g_free( mnemo );
				}
				break;
			default:
				msg = g_strdup_printf( _( "invalid ope template line type: %s" ), cstr );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				errors += 1;
				continue;
		}
	}

	if( !errors ){
		connect = ofa_hub_get_connect( hub );
		model_do_drop_content( connect );

		for( it=dataset ; it ; it=it->next ){
			model = OFO_OPE_TEMPLATE( it->data );
			if( !model_do_insert( model, connect )){
				errors -= 1;
			}
			ofa_iimportable_increment_progress(
					importable, IMPORTABLE_PHASE_INSERT, 1+ofo_ope_template_get_detail_count( model ));
		}

		ofo_ope_template_free_dataset( dataset );
		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_OPE_TEMPLATE );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_OPE_TEMPLATE );
	}

	return( errors );
}

static ofoOpeTemplate *
model_import_csv_model( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint line, guint *errors )
{
	ofoOpeTemplate *model;
	gchar *str;
	GSList *itf;
	gboolean locked;
	gchar *splitted;

	model = ofo_ope_template_new();
	itf = fields ? fields->next : NULL;

	/* model mnemo */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template mnemonic" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_mnemo( model, str );
	g_free( str );

	/* model label */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template label" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_label( model, str );
	g_free( str );

	/* model ledger */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template ledger" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_ledger( model, str );
	g_free( str );

	/* model ledger locked
	 * default to false if not set, but must be valid if set */
	str = ofa_iimportable_get_string( &itf, settings );
	locked = my_utils_boolean_from_str( str );
	ofo_ope_template_set_ledger_locked( model, locked );
	g_free( str );

	/* model ref */
	str = ofa_iimportable_get_string( &itf, settings );
	if( my_strlen( str )){
		ofo_ope_template_set_ref( model, str );
	}
	g_free( str );

	/* model ref locked
	 * default to false if not set, but must be valid if set */
	str = ofa_iimportable_get_string( &itf, settings );
	locked = my_utils_boolean_from_str( str );
	ofo_ope_template_set_ref_locked( model, locked );
	g_free( str );

	/* notes
	 * we are tolerant on the last field... */
	str = ofa_iimportable_get_string( &itf, settings );
	splitted = my_utils_import_multi_lines( str );
	ofo_ope_template_set_notes( model, splitted );
	g_free( splitted );
	g_free( str );

	return( model );
}

static GList *
model_import_csv_detail( ofaIImportable *importable, GSList *fields, const ofaFileFormat *settings, guint line, guint *errors, gchar **mnemo )
{
	GList *detail;
	gchar *str;
	GSList *itf;

	detail = ofa_box_init_fields_list( st_detail_defs );
	itf = fields ? fields->next : NULL;

	/* model mnemo */
	str = ofa_iimportable_get_string( &itf, settings );
	if( !my_strlen( str )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template mnemonic" ));
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( str );
	ofa_box_set_string( detail, OTE_MNEMO, str );
	g_free( str );

	/* detail comment */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_COMMENT, str );
	g_free( str );

	/* detail account */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_ACCOUNT, str );
	g_free( str );

	/* detail account locked */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_ACCOUNT_LOCKED, str );
	g_free( str );

	/* detail label */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_LABEL, str );
	g_free( str );

	/* detail label locked */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_LABEL_LOCKED, str );
	g_free( str );

	/* detail debit */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_DEBIT, str );
	g_free( str );

	/* detail debit locked */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_DEBIT_LOCKED, str );
	g_free( str );

	/* detail credit */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_CREDIT, str );
	g_free( str );

	/* detail credit locked */
	str = ofa_iimportable_get_string( &itf, settings );
	ofa_box_set_string( detail, OTE_DET_CREDIT_LOCKED, str );
	g_free( str );

	return( detail );
}

static gboolean
model_do_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_OPE_TEMPLATES", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_OPE_TEMPLATES_DET", TRUE ));
}
