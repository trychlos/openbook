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
#include "api/ofa-idbmodel.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-account.h"
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
	OTE_PAM_ROW,
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
	OTE_DET_CREDIT_LOCKED,
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
		{ OFA_BOX_CSV( OTE_PAM_ROW ),
				OFA_TYPE_INTEGER,
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

static ofoOpeTemplate *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gchar          *get_mnemo_new_from( const ofoOpeTemplate *model );
static void            ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user );
static void            ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp );
static GList          *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean        model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_insert_details( ofoOpeTemplate *model, const ofaIDBConnect *connect, gint rang, GList *details );
static gboolean        model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean        model_do_delete( ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gint            model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo );
static void            icollectionable_iface_init( myICollectionableInterface *iface );
static guint           icollectionable_get_interface_version( void );
static GList          *icollectionable_load_collection( void *user_data );
static void            idoc_iface_init( ofaIDocInterface *iface );
static guint           idoc_get_interface_version( void );
static void            iexportable_iface_init( ofaIExportableInterface *iface );
static guint           iexportable_get_interface_version( void );
static gchar          *iexportable_get_label( const ofaIExportable *instance );
static gboolean        iexportable_export( ofaIExportable *exportable, const gchar *format_id, ofaStreamFormat *settings, ofaIGetter *getter );
static gchar          *update_decimal_sep( const ofsBoxData *box_data, ofaStreamFormat *format, const gchar *text, void *empty );
static void            iimportable_iface_init( ofaIImportableInterface *iface );
static guint           iimportable_get_interface_version( void );
static gchar          *iimportable_get_label( const ofaIImportable *instance );
static guint           iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList          *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoOpeTemplate *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static GList          *iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo );
static void            iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean        model_get_exists( const ofoOpeTemplate *model, const ofaIDBConnect *connect );
static gboolean        model_drop_content( const ofaIDBConnect *connect );
static void            isignalable_iface_init( ofaISignalableInterface *iface );
static void            isignalable_connect_to( ofaISignaler *signaler );
static gboolean        signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean        signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account );
static gboolean        signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger );
static gboolean        signaler_is_deletable_rate( ofaISignaler *signaler, ofoRate *rate );
static void            signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean        signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *new_id, const gchar *prev_id );
static gboolean        signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id );
static gboolean        signaler_on_updated_rate_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id );
static gboolean        do_update_formulas( ofaIGetter *getter, const gchar *new_id, const gchar *prev_id );

G_DEFINE_TYPE_EXTENDED( ofoOpeTemplate, ofo_ope_template, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoOpeTemplate )
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
 * ofo_ope_template_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoOpeTemplate dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_ope_template_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_OPE_TEMPLATE, getter ));
}

/**
 * ofo_ope_template_get_by_mnemo:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched model, or %NULL.
 *
 * The returned object is owned by the #ofoOpeTemplate class, and should
 * not be unreffed by the caller.
 */
ofoOpeTemplate *
ofo_ope_template_get_by_mnemo( ofaIGetter *getter, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_ope_template_get_dataset( getter );

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
 * ofo_ope_template_new:
 * @getter: a #ofaIGetter instance.
 */
ofoOpeTemplate *
ofo_ope_template_new( ofaIGetter *getter )
{
	ofoOpeTemplate *model;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	model = g_object_new( OFO_TYPE_OPE_TEMPLATE, "ofo-base-getter", getter, NULL );
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
ofo_ope_template_new_from_template( ofoOpeTemplate *model )
{
	ofoOpeTemplate *dest;
	ofaIGetter *getter;
	gint count, i;
	gchar *new_mnemo, *new_label;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	getter = ofo_base_get_getter( OFO_BASE( model ));
	dest = ofo_ope_template_new( getter );

	new_mnemo = get_mnemo_new_from( model );
	ofo_ope_template_set_mnemo( dest, new_mnemo );
	g_free( new_mnemo );

	new_label = g_strdup_printf( "%s (%s)", ofo_ope_template_get_label( model ), _( "Duplicate" ));
	ofo_ope_template_set_label( dest, new_label );
	g_free( new_label );

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

/*
 * Returns a new mnemo derived from the given one, as a newly allocated
 * string that the caller should g_free().
 */
static gchar *
get_mnemo_new_from( const ofoOpeTemplate *model )
{
	ofaIGetter *getter;
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, NULL );

	str = NULL;
	getter = ofo_base_get_getter( OFO_BASE( model ));
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
		if( !ofo_ope_template_get_by_mnemo( getter, str )){
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
 * ofo_ope_template_get_pam_row:
 *
 * Returns: the row index, starting with zero, of the account which is
 * driven by the mean of paiement.
 */
gint
ofo_ope_template_get_pam_row( const ofoOpeTemplate *model )
{
	ofo_base_getter( OPE_TEMPLATE, model, int, -1, OTE_PAM_ROW );
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
	static const gchar *thisfn = "ofo_ope_template_is_deletable";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( !OFO_BASE( model )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( model ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, model, &deletable );
		if( 0 ){
			g_debug( "%s: mnemo=%s, deletable=%s",
					thisfn, ofo_ope_template_get_mnemo( model ), deletable ? "True":"False" );
		}
	}

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
			*msgerr = g_strdup( _( "Mnemonic is empty" ));
		}
	} else if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
	} else if( !my_strlen( ledger )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Ledger is empty" ));
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
 * ofo_ope_template_set_pam_row:
 */
void
ofo_ope_template_set_pam_row( ofoOpeTemplate *model, gint row )
{
	ofo_base_setter( OPE_TEMPLATE, model, int, OTE_PAM_ROW, row );
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

/**
 * ofo_ope_template_add_detail:
 * @model:
 */
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
ofo_ope_template_get_detail_count( ofoOpeTemplate *model )
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
ofo_ope_template_get_detail_comment( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_account( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_account_locked( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_label( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_label_locked( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_debit( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_debit_locked( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_credit( ofoOpeTemplate *model, gint idx )
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
ofo_ope_template_get_detail_credit_locked( ofoOpeTemplate *model, gint idx )
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
 * ofo_ope_template_update_account:
 * @model: this #ofoOpeTemplate instance.
 * @prev_id: the previous account identifier.
 * @new_id: the new account identifier.
 *
 * Update the account identifier.
 */
void
ofo_ope_template_update_account( ofoOpeTemplate *model, const gchar *prev_id, const gchar *new_id )
{
	ofoOpeTemplatePrivate *priv;
	GList *it;
	const gchar *det_account;

	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));
	g_return_if_fail( !OFO_BASE( model )->prot->dispose_has_run );
	g_return_if_fail( my_strlen( prev_id ) > 0 );
	g_return_if_fail( my_strlen( new_id ) > 0 );

	priv = ofo_ope_template_get_instance_private( model );

	for( it=priv->details ; it ; it=it->next ){
		det_account = ofa_box_get_string(( GList * ) it->data, OTE_DET_ACCOUNT );
		if( my_strlen( det_account ) && !my_collate( det_account, prev_id )){
			ofa_box_set_string(( GList * ) it->data, OTE_DET_ACCOUNT, new_id );
		}
	}
}

/**
 * ofo_ope_template_get_det_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ope_template mnemos in
 * OFA_T_OPE_TEMPLATES_DET child table.
 *
 * The returned list should be #ofo_ope_template_free_det_orphans() by
 * the caller.
 */
GList *
ofo_ope_template_get_det_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_OPE_TEMPLATES_DET" ));
}

/**
 * ofo_ope_template_get_doc_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ope_template mnemos in
 * OFA_T_OPE_TEMPLATES_DOC child table.
 *
 * The returned list should be #ofo_ope_template_free_doc_orphans() by
 * the caller.
 */
GList *
ofo_ope_template_get_doc_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_OPE_TEMPLATES_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(OTE_MNEMO) FROM %s "
			"	WHERE OTE_MNEMO NOT IN (SELECT OTE_MNEMO FROM OFA_T_OPE_TEMPLATES)", table );

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
 * ofo_ope_template_insert:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_ope_template_insert( ofoOpeTemplate *ope_template )
{
	static const gchar *thisfn = "ofo_ope_template_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: ope_template=%p", thisfn, ( void * ) ope_template );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ope_template ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_ope_template_get_dataset( getter );

	if( model_do_insert( ope_template, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( ope_template ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, ope_template );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_insert( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	return( model_insert_main( model, connect ) &&
			model_insert_details_ex( model, connect, NULL ));
}

static gboolean
model_insert_main( ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *ref, *stamp_str;
	GTimeVal stamp;
	gint row;
	const gchar *userid;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ope_template_get_label( model ));
	ref = my_utils_quote_sql( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote_sql( ofo_ope_template_get_notes( model ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_OPE_TEMPLATES" );

	g_string_append_printf( query,
			"	(OTE_MNEMO,OTE_LABEL,OTE_LED_MNEMO,OTE_LED_LOCKED,"
			"	OTE_REF,OTE_REF_LOCKED,OTE_PAM_ROW,OTE_NOTES,"
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

	row = ofo_ope_template_get_pam_row( model );
	if( row >= 0 ){
		g_string_append_printf( query, "%d,", row );
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

	ope_template_set_upd_user( model, userid );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
model_delete_details( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s'",
			my_strlen( prev_mnemo ) ? prev_mnemo : ofo_ope_template_get_mnemo( model ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
model_insert_details_ex( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	ofoOpeTemplatePrivate *priv;
	gboolean ok;
	GList *idet;
	gint rang;

	ok = FALSE;
	priv = ofo_ope_template_get_instance_private( model );

	if( model_delete_details( model, connect, prev_mnemo )){
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

	comment = my_utils_quote_sql( ofa_box_get_string( details, OTE_DET_COMMENT ));
	if( my_strlen( comment )){
		g_string_append_printf( query, "'%s',", comment );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( comment );

	account = my_utils_quote_sql( ofa_box_get_string( details, OTE_DET_ACCOUNT ));
	if( my_strlen( account )){
		g_string_append_printf( query, "'%s',", account );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( account );

	g_string_append_printf( query, "'%s',", ofa_box_get_string( details, OTE_DET_ACCOUNT_LOCKED ));

	label = my_utils_quote_sql( ofa_box_get_string( details, OTE_DET_LABEL ));
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ope_template=%p, prev_mnemo=%s",
			thisfn, ( void * ) ope_template, prev_mnemo );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ope_template ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( model_do_update( ope_template, ofa_hub_get_connect( hub ), prev_mnemo )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, ope_template, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_update( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	return( model_update_main( model, connect, prev_mnemo ) &&
			model_insert_details_ex( model, connect, prev_mnemo ));
}

static gboolean
model_update_main( ofoOpeTemplate *model, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *ref, *notes, *stamp_str;
	const gchar *new_mnemo, *userid;
	GTimeVal stamp;
	gint row;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_ope_template_get_label( model ));
	ref = my_utils_quote_sql( ofo_ope_template_get_ref( model ));
	notes = my_utils_quote_sql( ofo_ope_template_get_notes( model ));
	new_mnemo = ofo_ope_template_get_mnemo( model );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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

	row = ofo_ope_template_get_pam_row( model );
	if( row >= 0 ){
		g_string_append_printf( query, "OTE_PAM_ROW=%d,", row );
	} else {
		query = g_string_append( query, "OTE_PAM_ROW=NULL," );
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

	return( ok );
}

/**
 * ofo_ope_template_delete:
 */
gboolean
ofo_ope_template_delete( ofoOpeTemplate *ope_template )
{
	static const gchar *thisfn = "ofo_ope_template_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: ope_template=%p", thisfn, ( void * ) ope_template );

	g_return_val_if_fail( ope_template && OFO_IS_OPE_TEMPLATE( ope_template ), FALSE );
	g_return_val_if_fail( ofo_ope_template_is_deletable( ope_template ), FALSE );
	g_return_val_if_fail( !OFO_BASE( ope_template )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( ope_template ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( model_do_delete( ope_template, ofa_hub_get_connect( hub ))){
		g_object_ref( ope_template );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( ope_template ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, ope_template );
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

	ok &= model_delete_details( model, connect, NULL );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo )
{
	return( my_collate( ofo_ope_template_get_mnemo( a ), mnemo ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_ope_template_icollectionable_iface_init";

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
	ofoOpeTemplatePrivate *priv;
	GList *dataset, *it;
	ofoOpeTemplate *template;
	gchar *from;
	ofaHub *hub;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_OPE_TEMPLATES",
					OFO_TYPE_OPE_TEMPLATE,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		template = OFO_OPE_TEMPLATE( it->data );
		priv = ofo_ope_template_get_instance_private( template );
		from = g_strdup_printf(
				"OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s'",
				ofo_ope_template_get_mnemo( template ));
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
	static const gchar *thisfn = "ofo_ope_template_idoc_iface_init";

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
	static const gchar *thisfn = "ofo_ope_template_iexportable_iface_init";

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
iexportable_export( ofaIExportable *exportable, const gchar *format_id, ofaStreamFormat *settings, ofaIGetter *getter )
{
	ofoOpeTemplatePrivate *priv;
	GList *dataset, *it, *det;
	gchar *str, *str2;
	ofoOpeTemplate *model;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;

	dataset = ofo_ope_template_get_dataset( getter );

	with_headers = ofa_stream_format_get_with_headers( settings );
	field_sep = ofa_stream_format_get_field_sep( settings );

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
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_detail_defs, settings );
		str2 = g_strdup_printf( "2%c%s", field_sep, str );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings, NULL );
		str2 = g_strdup_printf( "1%c%s", field_sep, str );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		model = OFO_OPE_TEMPLATE( it->data );
		priv = ofo_ope_template_get_instance_private( model );

		for( det=priv->details ; det ; det=det->next ){
			str = ofa_box_csv_get_line_ex( det->data, settings, NULL, ( CSVExportFunc ) update_decimal_sep, NULL );
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
 * operation template debit and credit are managed as strings, and are
 * thus not remediated for decimal separator -
 * but they may contain amounts and so should.
 * => this user-remediation function
 */
static gchar *
update_decimal_sep( const ofsBoxData *box_data, ofaStreamFormat *settings, const gchar *text, void *empty )
{
	const ofsBoxDef *box_def;
	gchar *str;
	gchar decimal_sep;

	str = g_strdup( text );
	box_def = ofa_box_data_get_def( box_data );

	if( box_def->id == OTE_DET_DEBIT || box_def->id == OTE_DET_CREDIT ){
		decimal_sep = ofa_stream_format_get_decimal_sep( settings );
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_OPE_TEMPLATES" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "OFA_T_OPE_TEMPLATES_DET" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_OPE_TEMPLATE );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_OPE_TEMPLATE );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_OPE_TEMPLATES" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "OFA_T_OPE_TEMPLATES_DET" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
	}

	if( dataset ){
		ofo_ope_template_free_dataset( dataset );
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
	gchar *str, *mnemo;
	gint type;
	GList *detail;
	ofoOpeTemplate *model;
	ofoOpeTemplatePrivate *priv;

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
				model = iimportable_import_parse_main( importer, parms, numline, itf );
				if( model ){
					dataset = g_list_prepend( dataset, model );
					parms->parsed_count += 1;
					ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = iimportable_import_parse_detail( importer, parms, numline, itf, &mnemo );
				if( detail ){
					model = model_find_by_mnemo( dataset, mnemo );
					if( model ){
						priv = ofo_ope_template_get_instance_private( model );
						ofa_box_set_int( detail, OTE_DET_ROW, 1+ofo_ope_template_get_detail_count( model ));
						priv->details = g_list_append( priv->details, detail );
						total -= 1;
						ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
					}
					g_free( mnemo );
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

static ofoOpeTemplate *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoOpeTemplate *model;
	gboolean locked;

	model = ofo_ope_template_new( parms->getter );

	/* model mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty operation template mnemonic" ));
		parms->parse_errs += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_mnemo( model, cstr );

	/* model label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty operation template label" ));
		parms->parse_errs += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_label( model, cstr );

	/* model ledger */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty operation template ledger" ));
		parms->parse_errs += 1;
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

	/* model ref (optional) */
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
iimportable_import_parse_detail( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;

	detail = ofa_box_init_fields_list( st_detail_defs );

	/* model mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty operation template mnemonic" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );
	ofa_box_set_string( detail, OTE_MNEMO, cstr );

	/* row number (placeholder) */
	itf = itf ? itf->next : NULL;

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

/*
 * insert records
 */
static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	const gchar *mnemo;
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoOpeTemplate *model;

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
		model = OFO_OPE_TEMPLATE( it->data );

		if( model_get_exists( model, connect )){
			parms->duplicate_count += 1;
			mnemo = ofo_ope_template_get_mnemo( model );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate operation template, replacing previous one" ), mnemo );
					model_do_delete( model, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate operation template, ignored (skipped)" ), mnemo );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate operation template" ), mnemo );
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
model_get_exists( const ofoOpeTemplate *model, const ofaIDBConnect *connect )
{
	const gchar *model_id;
	gint count;
	gchar *str;

	count = 0;
	model_id = ofo_ope_template_get_mnemo( model );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES WHERE OTE_MNEMO='%s'", model_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
model_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_OPE_TEMPLATES", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_OPE_TEMPLATES_DET", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_ope_template_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_ope_template_isignalable_connect_to";

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
	static const gchar *thisfn = "ofo_ope_template_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = signaler_is_deletable_account( signaler, OFO_ACCOUNT( object ));

	} else if( OFO_IS_LEDGER( object )){
		deletable = signaler_is_deletable_ledger( signaler, OFO_LEDGER( object ));

	} else if( OFO_IS_RATE( object )){
		deletable = signaler_is_deletable_rate( signaler, OFO_RATE( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES_DET WHERE OTE_DET_ACCOUNT LIKE '%%%s%%'",
			ofo_account_get_number( account ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES WHERE OTE_LED_MNEMO='%s'",
			ofo_ledger_get_mnemo( ledger ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_rate( ofaISignaler *signaler, ofoRate *rate )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES_DET "
			"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
			ofo_rate_get_mnemo( rate ),
			ofo_rate_get_mnemo( rate ));

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
	static const gchar *thisfn = "ofo_ope_template_signaler_on_updated_base";
	const gchar *mnemo, *new_id;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			new_id = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( my_collate( new_id, prev_id )){
				signaler_on_updated_account_id( signaler, new_id, prev_id );
			}
		}

	} else if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_ledger_mnemo( signaler, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_RATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_rate_mnemo( signaler, mnemo, prev_id );
			}
		}
	}
}

static gboolean
signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *new_id, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_signaler_on_updated_account_id";
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gboolean ok;

	g_debug( "%s: signaler=%p, new_id=%s, prev_id=%s",
			thisfn, ( void * ) signaler, new_id, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
					"UPDATE OFA_T_OPE_TEMPLATES_DET SET "
					"	OTE_DET_ACCOUNT='%s' WHERE OTE_DET_ACCOUNT='%s'",
								new_id, prev_id );

	ok = ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	if( ok ){
		ok = do_update_formulas( getter, new_id, prev_id );
	}

	return( ok );
}

static gboolean
signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_signaler_on_updated_ledger_mnemo";
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gboolean ok;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
					"UPDATE OFA_T_OPE_TEMPLATES "
					"	SET OTE_LED_MNEMO='%s' WHERE OTE_LED_MNEMO='%s'",
								mnemo, prev_id );

	ok = ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
signaler_on_updated_rate_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_signaler_on_updated_rate_mnemo";
	ofaIGetter *getter;
	gboolean ok;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );

	ok = do_update_formulas( getter, mnemo, prev_id );

	return( ok );
}

static gboolean
do_update_formulas( ofaIGetter *getter, const gchar *new_id, const gchar *prev_id )
{
	gchar *query;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_label, *det_debit, *det_credit;
	gint det_row;
	gboolean ok;
	const gchar *prev_label, *prev_debit, *prev_credit;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"SELECT OTE_MNEMO,OTE_DET_ROW,OTE_DET_LABEL,OTE_DET_DEBIT,OTE_DET_CREDIT "
					"	FROM OFA_T_OPE_TEMPLATES_DET "
					"	WHERE OTE_DET_LABEL LIKE '%%%s%%' OR OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
							prev_id, prev_id, prev_id );

	ok = ofa_idbconnect_query_ex( connect, query, &result, TRUE );
	g_free( query );

	if( ok ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			etp_mnemo = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			det_row = atoi(( gchar * ) icol->data );
			icol = icol->next;
			prev_label = ( const gchar * ) icol->data;
			det_label = my_utils_str_replace( prev_label, prev_id, new_id );
			icol = icol->next;
			prev_debit = ( const gchar * ) icol->data;
			det_debit = my_utils_str_replace( prev_debit, prev_id, new_id );
			icol = icol->next;
			prev_credit = ( const gchar * ) icol->data;
			det_credit = my_utils_str_replace( prev_credit, prev_id, new_id );

			if( my_collate( prev_label, det_label ) ||
					my_collate( prev_debit, det_debit ) || my_collate( prev_credit, det_credit )){
				query = g_strdup_printf(
								"UPDATE OFA_T_OPE_TEMPLATES_DET "
								"	SET OTE_DET_LABEL='%s',OTE_DET_DEBIT='%s',OTE_DET_CREDIT='%s' "
								"	WHERE OTE_MNEMO='%s' AND OTE_DET_ROW=%d",
										det_label, det_debit, det_credit,
										etp_mnemo, det_row );

				ofa_idbconnect_query( connect, query, TRUE );

				g_free( query );
			}

			g_free( det_credit );
			g_free( det_debit );
			g_free( det_label );
			g_free( etp_mnemo );
		}
		ofa_idbconnect_free_results( result );
	}

	return( ok );
}
