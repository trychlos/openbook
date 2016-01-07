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
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-idataset.h"
#include "api/ofa-idbconnect.h"
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

static const ofsBoxDef st_details_defs[] = {
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

struct _ofoOpeTemplatePrivate {

	/* the details of the operation template as a GList of GList fields
	 */
	GList     *details;
};

/* mnemonic max length */
#define MNEMO_LENGTH                    6

static ofoBaseClass *ofo_ope_template_parent_class = NULL;

static void            on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data );
static gboolean        on_update_ledger_mnemo( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id );
static gboolean        on_update_rate_mnemo( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id );
static GList          *ope_template_load_dataset( ofoDossier *dossier );
static ofoOpeTemplate *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gint            model_count_for_ledger( const ofaIDBConnect *cnx, const gchar *ledger );
static gint            model_count_for_rate( const ofaIDBConnect *cnx, const gchar *mnemo );
static void            ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user );
static void            ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp );
static gboolean        model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user );
static gboolean        model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user );
static gboolean        model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *cnx );
static gboolean        model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *cnx );
static gboolean        model_insert_details( ofoOpeTemplate *model, const ofaIDBConnect *cnx, gint rang, GList *details );
static gboolean        model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo );
static gboolean        model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo );
static gboolean        model_do_delete( ofoOpeTemplate *model, const ofaIDBConnect *cnx );
static gint            model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo );
static gint            ope_template_cmp_by_ptr( const ofoOpeTemplate *a, const ofoOpeTemplate *b );
static void            iexportable_iface_init( ofaIExportableInterface *iface );
static guint           iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean        iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static gchar          *update_decimal_sep( const ofsBoxDef *def, eBoxType type, const gchar *text, const ofaFileFormat *settings );
static void            iimportable_iface_init( ofaIImportableInterface *iface );
static guint           iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean        iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofoDossier *dossier );
static ofoOpeTemplate *model_import_csv_model( ofaIImportable *importable, GSList *fields, guint count, guint *errors );
static GList          *model_import_csv_detail( ofaIImportable *importable, GSList *fields, guint count, guint *errors, gchar **mnemo );
static gboolean        model_do_drop_content( const ofaIDBConnect *cnx );

OFA_IDATASET_LOAD( OPE_TEMPLATE, ope_template );

static void
details_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
details_list_free( ofoOpeTemplate *model )
{
	g_list_free_full( model->priv->details, ( GDestroyNotify ) details_list_free_detail );
	model->priv->details = NULL;
}

static void
ope_template_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ope_template_finalize";
	ofoOpeTemplatePrivate *priv;

	priv = OFO_OPE_TEMPLATE( instance )->priv;

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplatePrivate );
}

static void
ofo_ope_template_class_init( ofoOpeTemplateClass *klass )
{
	static const gchar *thisfn = "ofo_ope_template_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_ope_template_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_finalize;

	g_type_class_add_private( klass, sizeof( ofoOpeTemplatePrivate ));
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_ope_template_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoOpeTemplateClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ofo_ope_template_class_init,
		NULL,
		NULL,
		sizeof( ofoOpeTemplate ),
		0,
		( GInstanceInitFunc ) ofo_ope_template_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoOpeTemplate", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );

	return( type );
}

GType
ofo_ope_template_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
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
			G_OBJECT( hub ), SIGNAL_HUB_UPDATED, G_CALLBACK( on_updated_object ), NULL );
}

/**
 * ofo_ope_template_connect_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_ope_template_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ope_template_connect_handlers";

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data )
{
	static const gchar *thisfn = "ofo_ope_template_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_update_ledger_mnemo( dossier, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_RATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				on_update_rate_mnemo( dossier, mnemo, prev_id );
			}
		}
	}
}

static gboolean
on_update_ledger_mnemo( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_ledger_mnemo";
	gchar *query;
	gboolean ok;

	g_debug( "%s: dossier=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) dossier, mnemo, prev_id );

	query = g_strdup_printf(
					"UPDATE OFA_T_OPE_TEMPLATES "
					"	SET OTE_LED_MNEMO='%s' WHERE OTE_LED_MNEMO='%s'",
								mnemo, prev_id );

	ok = ofa_idbconnect_query( ofo_dossier_get_connect( dossier ), query, TRUE );

	g_free( query );

	ofa_idataset_free_dataset( dossier, OFO_TYPE_OPE_TEMPLATE );

	g_signal_emit_by_name(
			G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );

	return( ok );
}

static gboolean
on_update_rate_mnemo( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_rate_mnemo";
	gchar *query;
	const ofaIDBConnect *cnx;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_debit, *det_credit;
	gint det_row;
	gboolean ok;

	g_debug( "%s: dossier=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) dossier, mnemo, prev_id );

	cnx = ofo_dossier_get_connect( dossier );

	query = g_strdup_printf(
					"SELECT OTE_MNEMO,OTE_DET_ROW,OTE_DET_DEBIT,OTE_DET_CREDIT "
					"	FROM OFA_T_OPE_TEMPLATES_DET "
					"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
							prev_id, prev_id );

	ok = ofa_idbconnect_query_ex( cnx, query, &result, TRUE );
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

			ofa_idbconnect_query( cnx, query, TRUE );

			g_free( query );
			g_free( det_credit );
			g_free( det_debit );
			g_free( etp_mnemo );
		}

		ofa_idataset_free_dataset( dossier, OFO_TYPE_OPE_TEMPLATE );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );
	}

	return( ok );
}

static GList *
ope_template_load_dataset( ofoDossier *dossier )
{
	GList *dataset, *it;
	ofoOpeTemplate *template;
	gchar *from;

	dataset = ofo_base_load_dataset_from_dossier(
					st_boxed_defs,
					ofo_dossier_get_connect( dossier ),
					"OFA_T_OPE_TEMPLATES ORDER BY OTE_MNEMO ASC",
					OFO_TYPE_OPE_TEMPLATE );

	for( it=dataset ; it ; it=it->next ){
		template = OFO_OPE_TEMPLATE( it->data );
		from = g_strdup_printf(
				"OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s' ORDER BY OTE_DET_ROW ASC",
				ofo_ope_template_get_mnemo( template ));
		template->priv->details =
				ofo_base_load_rows( st_details_defs, ofo_dossier_get_connect( dossier ), from );
		g_free( from );
	}

	return( dataset );
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
ofo_ope_template_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	/*static const gchar *thisfn = "ofo_ope_template_get_by_mnemo";*/

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	OFA_IDATASET_GET( dossier, OPE_TEMPLATE, ope_template );

	return( model_find_by_mnemo( ope_template_dataset, mnemo ));
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
ofo_ope_template_use_ledger( ofoDossier *dossier, const gchar *ledger )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( my_strlen( ledger ), FALSE );

	OFA_IDATASET_GET( dossier, OPE_TEMPLATE, ope_template );

	return( model_count_for_ledger( ofo_dossier_get_connect( dossier ), ledger ) > 0 );
}

static gint
model_count_for_ledger( const ofaIDBConnect *cnx, const gchar *ledger )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES WHERE OTE_LED_MNEMO='%s'", ledger );

	ofa_idbconnect_query_int( cnx, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_ope_template_use_rate:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified rate.
 */
gboolean
ofo_ope_template_use_rate( ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	OFA_IDATASET_GET( dossier, OPE_TEMPLATE, ope_template );

	return( model_count_for_rate( ofo_dossier_get_connect( dossier ), mnemo ) > 0 );
}

static gint
model_count_for_rate( const ofaIDBConnect *cnx, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES_DET "
				"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
					mnemo, mnemo );

	ofa_idbconnect_query_int( cnx, query, &count, TRUE );

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

	dest = NULL;

	if( !OFO_BASE( model )->prot->dispose_has_run ){

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
ofo_ope_template_get_mnemo_new_from( const ofoOpeTemplate *model, ofoDossier *dossier )
{
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	str = NULL;

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		mnemo = ofo_ope_template_get_mnemo( model );
		len_mnemo = my_strlen( mnemo );
		for( i=2 ; ; ++i ){
			/* if we are greater than 9999, there is a problem... */
			maxlen = ( i < 10 ? MNEMO_LENGTH-1 :
						( i < 100 ? MNEMO_LENGTH-2 :
						( i < 1000 ? MNEMO_LENGTH-3 : MNEMO_LENGTH-4 )));
			if( maxlen < len_mnemo ){
				str = g_strdup_printf( "%*.*s%d", maxlen, maxlen, mnemo, i );
			} else {
				str = g_strdup_printf( "%s%d", mnemo, i );
			}
			if( !ofo_ope_template_get_by_mnemo( dossier, str )){
				break;
			}
			g_free( str );
		}
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

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		cstr = ofa_box_get_string( OFO_BASE( model )->prot->fields, OTE_LED_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	g_return_val_if_reached( FALSE );
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

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		cstr = ofa_box_get_string( OFO_BASE( model )->prot->fields, OTE_REF_LOCKED );
		return( !my_collate( cstr, "Y" ));
	}

	g_return_val_if_reached( FALSE );
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
 * @dossier: the dossier.
 *
 * Returns: %TRUE if the operation template is deletable.
 */
gboolean
ofo_ope_template_is_deletable( const ofoOpeTemplate *model, ofoDossier *dossier )
{
	const gchar *mnemo;
	gboolean is_current;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		mnemo = ofo_ope_template_get_mnemo( model );
		is_current = ofo_dossier_is_current( dossier );

		return( is_current &&
				!ofo_entry_use_ope_template( dossier, mnemo ) &&
				!ofo_dossier_use_ope_template( dossier, mnemo ));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_is_valid:
 */
gboolean
ofo_ope_template_is_valid( ofoDossier *dossier, const gchar *mnemo, const gchar *label, const gchar *ledger )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( my_strlen( mnemo ) &&
			my_strlen( label ) &&
			my_strlen( ledger ));
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
ofo_ope_template_add_detail( ofoOpeTemplate *model, const gchar *comment,
							const gchar *account, gboolean account_locked,
							const gchar *label, gboolean label_locked,
							const gchar *debit, gboolean debit_locked,
							const gchar *credit, gboolean credit_locked )
{
	GList *fields;

	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		fields = ofa_box_init_fields_list( st_details_defs );
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

		model->priv->details = g_list_append( model->priv->details, fields );
	}
}

/**
 * ofo_ope_template_free_detail_all:
 */
void
ofo_ope_template_free_detail_all( ofoOpeTemplate *model )
{
	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		details_list_free( model );
	}
}

/**
 * ofo_ope_template_get_detail_count:
 */
gint
ofo_ope_template_get_detail_count( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), 0 );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( g_list_length( model->priv->details ));
	}

	return( 0 );
}

/**
 * ofo_ope_template_get_detail_comment:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_comment( const ofoOpeTemplate *model, gint idx )
{
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			return( ofa_box_get_string( nth->data, OTE_DET_COMMENT ));
		}
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
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			return( ofa_box_get_string( nth->data, OTE_DET_ACCOUNT ));
		}
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
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			cstr = ofa_box_get_string( nth->data, OTE_DET_ACCOUNT_LOCKED );
			return( !my_collate( cstr, "Y" ));
		}
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
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			return( ofa_box_get_string( nth->data, OTE_DET_LABEL ));
		}
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
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			cstr = ofa_box_get_string( nth->data, OTE_DET_LABEL_LOCKED );
			return( !my_collate( cstr, "Y" ));
		}
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
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			return( ofa_box_get_string( nth->data, OTE_DET_DEBIT ));
		}
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
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			cstr = ofa_box_get_string( nth->data, OTE_DET_DEBIT_LOCKED );
			return( !my_collate( cstr, "Y" ));
		}
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
	GList *nth;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( idx >= 0, NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			return( ofa_box_get_string( nth->data, OTE_DET_CREDIT ));
		}
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
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( idx >= 0, FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		nth = g_list_nth( model->priv->details, idx );
		if( nth ){
			cstr = ofa_box_get_string( nth->data, OTE_DET_CREDIT_LOCKED );
			return( !my_collate( cstr, "Y" ));
		}
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
ofo_ope_template_insert( ofoOpeTemplate *ope_template, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ope_template_insert";

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( ope_template )->prot->dispose_has_run ){

		g_debug( "%s: ope_template=%p, dossier=%p",
				thisfn, ( void * ) ope_template, ( void * ) dossier );

		if( model_do_insert(
					ope_template,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, OPE_TEMPLATE, ope_template );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user )
{
	return( model_insert_main( model, cnx, user ) &&
			model_insert_details_ex( model, cnx ));
}

static gboolean
model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *ref;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_ope_template_get_label( model ));
	ref = my_utils_quote( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote( ofo_ope_template_get_notes( model ));
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
			"'%s','%s')", user, stamp_str );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	ope_template_set_upd_user( model, user );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s'",
			ofo_ope_template_get_mnemo( model ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *cnx )
{
	gboolean ok;
	GList *idet;
	gint rang;

	ok = FALSE;

	if( model_delete_details( model, cnx )){
		ok = TRUE;
		for( idet=model->priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !model_insert_details( model, cnx, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
model_insert_details( ofoOpeTemplate *model, const ofaIDBConnect *cnx, gint rang, GList *details )
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

	comment = my_utils_quote( ofa_box_get_string( details, OTE_DET_COMMENT ));
	if( my_strlen( comment )){
		g_string_append_printf( query, "'%s',", comment );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( comment );

	account = my_utils_quote( ofa_box_get_string( details, OTE_DET_ACCOUNT ));
	if( my_strlen( account )){
		g_string_append_printf( query, "'%s',", account );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( account );

	g_string_append_printf( query, "'%s',", ofa_box_get_string( details, OTE_DET_ACCOUNT_LOCKED ));

	label = my_utils_quote( ofa_box_get_string( details, OTE_DET_LABEL ));
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

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

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
ofo_ope_template_update( ofoOpeTemplate *ope_template, ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ope_template_update";

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );

	if( !OFO_BASE( ope_template )->prot->dispose_has_run ){

		g_debug( "%s: ope_template=%p, dossier=%p, prev_mnemo=%s",
				thisfn, ( void * ) ope_template, ( void * ) dossier, prev_mnemo );

		if( model_do_update(
					ope_template,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ),
					prev_mnemo )){

			OFA_IDATASET_UPDATE( dossier, OPE_TEMPLATE, ope_template, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo )
{
	return( model_update_main( model, cnx, user, prev_mnemo ) &&
			model_insert_details_ex( model, cnx ));
}

static gboolean
model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *ref, *notes;
	const gchar *new_mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_ope_template_get_label( model ));
	ref = my_utils_quote( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote( ofo_ope_template_get_notes( model ));
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
					user,
					stamp_str,
					prev_mnemo );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	ope_template_set_upd_user( model, user );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );

	return( ok );
}

/**
 * ofo_ope_template_delete:
 */
gboolean
ofo_ope_template_delete( ofoOpeTemplate *ope_template, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ope_template_delete";

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_ope_template_is_deletable( ope_template, dossier ), FALSE );

	if( !OFO_BASE( ope_template )->prot->dispose_has_run ){

		g_debug( "%s: ope_template=%p, dossier=%p",
				thisfn, ( void * ) ope_template, ( void * ) dossier );

		if( model_do_delete(
					ope_template,
					ofo_dossier_get_connect( dossier ))){

			OFA_IDATASET_REMOVE( dossier, OPE_TEMPLATE, ope_template );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_delete( ofoOpeTemplate *model, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES"
			"	WHERE OTE_MNEMO='%s'",
					ofo_ope_template_get_mnemo( model ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	ok &= model_delete_details( model, cnx );

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
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_ope_template_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export_from_dossier = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GList *it, *det;
	GSList *lines;
	gchar *str;
	ofoOpeTemplate *model;
	gboolean ok, with_headers;
	gchar field_sep, decimal_sep;
	gulong count;

	OFA_IDATASET_GET( dossier, OPE_TEMPLATE, ope_template );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );
	decimal_sep = ofa_file_format_get_decimal_sep( settings );

	count = ( gulong ) g_list_length( ope_template_dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=ope_template_dataset ; it ; it=it->next ){
		model = OFO_OPE_TEMPLATE( it->data );
		count += g_list_length( model->priv->details );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_get_csv_header( st_details_defs, field_sep );
		lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=ope_template_dataset ; it ; it=it->next ){
		str = ofa_box_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, decimal_sep );
		lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		model = OFO_OPE_TEMPLATE( it->data );
		for( det=model->priv->details ; det ; det=det->next ){
			str = ofa_box_get_csv_line_ex(
					det->data, field_sep, decimal_sep, ( CSVExportFunc ) update_decimal_sep, ( void * ) settings );
			lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
			g_free( str );
			ok = ofa_iexportable_export_lines( exportable, lines );
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
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
update_decimal_sep( const ofsBoxDef *def, eBoxType type, const gchar *text, const ofaFileFormat *settings )
{
	gchar *str;
	gchar decimal_sep;

	str = g_strdup( text );

	if( def->id == OTE_DET_DEBIT || def->id == OTE_DET_CREDIT ){
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
	static const gchar *thisfn = "ofo_class_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import_to_dossier = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
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
iimportable_import( ofaIImportable *importable, GSList *lines, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoOpeTemplate *model;
	GList *dataset, *it;
	guint errors, line;
	gchar *msg, *mnemo;
	gint type;
	GList *detail;

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
				model = model_import_csv_model( importable, fields, line, &errors );
				if( model ){
					dataset = g_list_prepend( dataset, model );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = model_import_csv_detail( importable, fields, line, &errors, &mnemo );
				if( detail ){
					model = model_find_by_mnemo( dataset, mnemo );
					if( model ){
						ofa_box_set_int( detail, OTE_DET_ROW, 1+ofo_ope_template_get_detail_count( model ));
						model->priv->details = g_list_append( model->priv->details, detail );
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
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_OPE_TEMPLATE, FALSE );

		model_do_drop_content( ofo_dossier_get_connect( dossier ));

		for( it=dataset ; it ; it=it->next ){
			model = OFO_OPE_TEMPLATE( it->data );
			if( !model_do_insert(
					model,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ))){
				errors -= 1;
			}

			ofa_iimportable_increment_progress(
					importable, IMPORTABLE_PHASE_INSERT, 1+ofo_ope_template_get_detail_count( model ));
		}

		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
		ofa_idataset_free_dataset( dossier, OFO_TYPE_OPE_TEMPLATE );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_OPE_TEMPLATE, TRUE );
	}

	return( errors );
}

static ofoOpeTemplate *
model_import_csv_model( ofaIImportable *importable, GSList *fields, guint line, guint *errors )
{
	ofoOpeTemplate *model;
	const gchar *cstr;
	GSList *itf;
	gboolean locked;
	gchar *splitted;

	model = ofo_ope_template_new();
	itf = fields;

	/* model mnemo */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template mnemonic" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_mnemo( model, cstr );

	/* model label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template label" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_label( model, cstr );

	/* model ledger */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template ledger" ));
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_ledger( model, cstr );

	/* model ledger locked
	 * default to false if not set, but must be valid if set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	locked = my_utils_boolean_from_str( cstr );
	ofo_ope_template_set_ledger_locked( model, locked );

	/* model ref */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_ope_template_set_ref( model, cstr );
	}

	/* model ref locked
	 * default to false if not set, but must be valid if set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	locked = my_utils_boolean_from_str( cstr );
	ofo_ope_template_set_ref_locked( model, locked );

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_ope_template_set_notes( model, splitted );
	g_free( splitted );

	return( model );
}

static GList *
model_import_csv_detail( ofaIImportable *importable, GSList *fields, guint line, guint *errors, gchar **mnemo )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;

	detail = ofa_box_init_fields_list( st_details_defs );
	itf = fields;

	/* model mnemo */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty operation template mnemonic" ));
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );
	ofa_box_set_string( detail, OTE_MNEMO, cstr );

	/* detail comment */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_COMMENT, cstr );

	/* detail account */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_ACCOUNT, cstr );

	/* detail account locked */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_ACCOUNT_LOCKED, cstr );

	/* detail label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_LABEL, cstr );

	/* detail label locked */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_LABEL_LOCKED, cstr );

	/* detail debit */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_DEBIT, cstr );

	/* detail debit locked */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_DEBIT_LOCKED, cstr );

	/* detail credit */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_CREDIT, cstr );

	/* detail credit locked */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, OTE_DET_CREDIT_LOCKED, cstr );

	return( detail );
}

static gboolean
model_do_drop_content( const ofaIDBConnect *cnx )
{
	return( ofa_idbconnect_query( cnx, "DELETE FROM OFA_T_OPE_TEMPLATES", TRUE ) &&
			ofa_idbconnect_query( cnx, "DELETE FROM OFA_T_OPE_TEMPLATES_DET", TRUE ));
}
