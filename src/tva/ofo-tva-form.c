/*
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
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "tva/ofo-tva-form.h"

/* priv instance data
 */
enum {
	TFO_MNEMO = 1,
	TFO_LABEL,
	TFO_HAS_CORRESPONDENCE,
	TFO_ENABLED,
	TFO_NOTES,
	TFO_UPD_USER,
	TFO_UPD_STAMP,
	TFO_BOOL_ROW,
	TFO_BOOL_LABEL,
	TFO_DET_ROW,
	TFO_DET_LEVEL,
	TFO_DET_CODE,
	TFO_DET_LABEL,
	TFO_DET_HAS_BASE,
	TFO_DET_BASE,
	TFO_DET_HAS_AMOUNT,
	TFO_DET_AMOUNT,
	TFO_DET_HAS_TEMPLATE,
	TFO_DET_TEMPLATE,
	TFO_DOC_ID,
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
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_HAS_CORRESPONDENCE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( TFO_ENABLED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_boolean_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_BOOL_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BOOL_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_detail_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_DET_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LEVEL ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_CODE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_AMOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_AMOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_TEMPLATE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define FORM_TABLES_COUNT               4
#define FORM_EXPORT_VERSION             1

typedef struct {
	GList *bools;						/* the details of the form as a GList of GList fields */
	GList *details;
	GList *docs;
}
	ofoTVAFormPrivate;

static ofoTVAForm *form_find_by_mnemo( GList *set, const gchar *mnemo );
static void        tva_form_set_upd_user( ofoTVAForm *form, const gchar *upd_user );
static void        tva_form_set_upd_stamp( ofoTVAForm *form, const GTimeVal *upd_stamp );
static GList      *form_detail_new( ofoTVAForm *form, guint level, const gchar *code, const gchar *label, gboolean has_base, const gchar *base, gboolean has_amount, const gchar *amount, gboolean has_template, const gchar *template );
static void        form_detail_add( ofoTVAForm *form, GList *fields );
static GList      *form_boolean_new( ofoTVAForm *form, const gchar *label );
static void        form_boolean_add( ofoTVAForm *form, GList *fields );
static GList      *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean    form_do_insert( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_insert_main( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_insert_details_ex( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_insert_details( ofoTVAForm *form, const ofaIDBConnect *connect, guint rang, GList *details );
static gboolean    form_insert_bools( ofoTVAForm *form, const ofaIDBConnect *connect, guint rang, GList *details );
static gboolean    form_do_update( ofoTVAForm *form, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean    form_update_main( ofoTVAForm *form, const ofaIDBConnect *connect, const gchar *prev_mnemo );
static gboolean    form_do_delete( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_do_delete_by_mnemo( const ofaIDBConnect *connect, const gchar *mnemo, const gchar *table );
static gboolean    form_delete_details( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_delete_bools( ofoTVAForm *form, const ofaIDBConnect *connect );
static gint        form_cmp_by_mnemo( const ofoTVAForm *a, const gchar *mnemo );
static void        icollectionable_iface_init( myICollectionableInterface *iface );
static guint       icollectionable_get_interface_version( void );
static GList      *icollectionable_load_collection( void *user_data );
static void        idoc_iface_init( ofaIDocInterface *iface );
static guint       idoc_get_interface_version( void );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( void );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean    iexportable_export_default( ofaIExportable *exportable );
static void        iimportable_iface_init( ofaIImportableInterface *iface );
static guint       iimportable_get_interface_version( void );
static gchar      *iimportable_get_label( const ofaIImportable *instance );
static guint       iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList      *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoTVAForm *iimportable_import_parse_form( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static GList      *iimportable_import_parse_bool( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo );
static GList      *iimportable_import_parse_rule( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    form_get_exists( ofoTVAForm *form, const ofaIDBConnect *connect );
static gboolean    form_drop_content( const ofaIDBConnect *connect );
static void        isignalable_iface_init( ofaISignalableInterface *iface );
static void        isignalable_connect_to( ofaISignaler *signaler );
static gboolean    signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean    signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account );
static gboolean    signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template );
static void        signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean    signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id );
static gboolean    signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id );
static gboolean    signaler_on_updated_rate_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id );
static gboolean    do_update_formulas( ofaIGetter *getter, const gchar *new_id, const gchar *prev_id );
static void        signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty );

G_DEFINE_TYPE_EXTENDED( ofoTVAForm, ofo_tva_form, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoTVAForm )
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
details_list_free( ofoTVAForm *form )
{
	ofoTVAFormPrivate *priv;

	priv = ofo_tva_form_get_instance_private( form );

	g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
	priv->details = NULL;
}

static void
bools_list_free_bool( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
bools_list_free( ofoTVAForm *form )
{
	ofoTVAFormPrivate *priv;

	priv = ofo_tva_form_get_instance_private( form );

	g_list_free_full( priv->bools, ( GDestroyNotify ) bools_list_free_bool );
	priv->bools = NULL;
}

static void
tva_form_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_tva_form_finalize";
	ofoTVAFormPrivate *priv;

	priv = ofo_tva_form_get_instance_private( OFO_TVA_FORM( instance ));

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, TFO_MNEMO ));

	g_return_if_fail( instance && OFO_IS_TVA_FORM( instance ));

	/* free data members here */
	if( priv->bools ){
		bools_list_free( OFO_TVA_FORM( instance ));
	}
	if( priv->details ){
		details_list_free( OFO_TVA_FORM( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_form_parent_class )->finalize( instance );
}

static void
tva_form_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_TVA_FORM( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_form_parent_class )->dispose( instance );
}

static void
ofo_tva_form_init( ofoTVAForm *self )
{
	static const gchar *thisfn = "ofo_tva_form_init";
	ofoTVAFormPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_tva_form_get_instance_private( self );

	priv->bools = NULL;
	priv->details = NULL;
	priv->docs = NULL;
}

static void
ofo_tva_form_class_init( ofoTVAFormClass *klass )
{
	static const gchar *thisfn = "ofo_tva_form_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_finalize;
}

/**
 * ofo_tva_form_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoTVAForm dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_tva_form_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_TVA_FORM, getter ));
}

/**
 * ofo_tva_form_get_by_mnemo:
 * @getter: a #ofaIGetter instance.
 * @mnemo:
 *
 * Returns: the searched tva form, or %NULL.
 *
 * The returned object is owned by the #ofoTVAForm class, and should
 * not be unreffed by the caller.
 */
ofoTVAForm *
ofo_tva_form_get_by_mnemo( ofaIGetter *getter, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_tva_form_get_dataset( getter );

	return( form_find_by_mnemo( dataset, mnemo ));
}

static ofoTVAForm *
form_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) form_cmp_by_mnemo );
	if( found ){
		return( OFO_TVA_FORM( found->data ));
	}

	return( NULL );
}

/**
 * ofo_tva_form_use_ope_template:
 * @getter: a #ofaIGetter instance.
 * @ope_template: the #ofoOpeTemplate mnemonic.
 *
 * Returns: %TRUE if any #ofoTVAForm use this @ope_template operation
 * template.
 */
gboolean
ofo_tva_form_use_ope_template( ofaIGetter *getter, const gchar *ope_template )
{
	gchar *query;
	gint count;
	ofaHub *hub;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( my_strlen( ope_template ), FALSE );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM TVA_T_FORMS_DET WHERE TFO_DET_TEMPLATE='%s'",
			ope_template );

	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count > 0 );
}

/**
 * ofo_tva_form_new:
 * @getter: a #ofaIGetter instance.
 */
ofoTVAForm *
ofo_tva_form_new( ofaIGetter *getter )
{
	ofoTVAForm *form;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	form = g_object_new( OFO_TYPE_TVA_FORM, "ofo-base-getter", getter, NULL );
	OFO_BASE( form )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	ofo_tva_form_set_is_enabled( form, FALSE );

	return( form );
}

/**
 * ofo_tva_form_new_from_form:
 * @form: the source #ofoTVAForm to be copied from.
 *
 * Allocates a new #ofoTVAForms object, initializing it with data
 * copied from @form #ofoTVAForm source.
 */
ofoTVAForm *
ofo_tva_form_new_from_form( ofoTVAForm *form )
{
	ofoTVAForm *dest;
	ofaIGetter *getter;
	gint count, i;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	dest = NULL;
	getter = ofo_base_get_getter( OFO_BASE( form ));

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	dest = ofo_tva_form_new( getter );

	ofo_tva_form_set_mnemo( dest, ofo_tva_form_get_mnemo( form ));
	ofo_tva_form_set_label( dest, ofo_tva_form_get_label( form ));
	ofo_tva_form_set_has_correspondence( dest, ofo_tva_form_get_has_correspondence( form ));
	ofo_tva_form_set_notes( dest, ofo_tva_form_get_notes( form ));

	count = ofo_tva_form_detail_get_count( form );
	for( i=0 ; i<count ; ++i ){
		ofo_tva_form_detail_add( dest,
				ofo_tva_form_detail_get_level( form, i ),
				ofo_tva_form_detail_get_code( form, i ),
				ofo_tva_form_detail_get_label( form, i ),
				ofo_tva_form_detail_get_has_base( form, i ),
				ofo_tva_form_detail_get_base( form, i ),
				ofo_tva_form_detail_get_has_amount( form, i ),
				ofo_tva_form_detail_get_amount( form, i ),
				ofo_tva_form_detail_get_has_template( form, i ),
				ofo_tva_form_detail_get_template( form, i ));
	}

	count = ofo_tva_form_boolean_get_count( form );
	for( i=0 ; i<count ; ++i ){
		ofo_tva_form_boolean_add( dest,
				ofo_tva_form_boolean_get_label( form, i ));
	}

	return( dest );
}

/**
 * ofo_tva_form_get_mnemo:
 * @form:
 */
const gchar *
ofo_tva_form_get_mnemo( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_MNEMO );
}

/**
 * ofo_tva_form_get_label:
 */
const gchar *
ofo_tva_form_get_label( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_LABEL );
}

/**
 * ofo_tva_form_get_has_correspondence:
 */
gboolean
ofo_tva_form_get_has_correspondence( const ofoTVAForm *form )
{
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( form )->prot->fields, TFO_HAS_CORRESPONDENCE );

	return( my_utils_boolean_from_str( cstr ));
}

/**
 * ofo_tva_form_get_is_enabled:
 */
gboolean
ofo_tva_form_get_is_enabled( const ofoTVAForm *form )
{
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( form )->prot->fields, TFO_ENABLED );

	return( my_utils_boolean_from_str( cstr ));
}

/**
 * ofo_tva_form_get_notes:
 */
const gchar *
ofo_tva_form_get_notes( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_NOTES );
}

/**
 * ofo_tva_form_get_upd_user:
 */
const gchar *
ofo_tva_form_get_upd_user( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_UPD_USER );
}

/**
 * ofo_tva_form_get_upd_stamp:
 */
const GTimeVal *
ofo_tva_form_get_upd_stamp( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, timestamp, NULL, TFO_UPD_STAMP );
}

/**
 * ofo_tva_form_is_deletable:
 * @form: the tva formular.
 *
 * Returns: %TRUE if the TVA form is deletable.
 *
 * A TVA form is deletable while no record has been created from it.
 */
gboolean
ofo_tva_form_is_deletable( const ofoTVAForm *form )
{
	gboolean deletable;
	ofaIGetter *getter;
	ofaISignaler *signaler;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( form ));
	signaler = ofa_igetter_get_signaler( getter );

	if( deletable ){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, form, &deletable );
	}

	return( deletable );
}

/**
 * ofo_tva_form_is_valid_data:
 * @mnemo:
 * @label:
 * @msgerr: [allow-none][out]:
 *
 * Returns: %TRUE if provided datas are enough to make the future
 * #ofoTVAForm valid, %FALSE else.
 */
gboolean
ofo_tva_form_is_valid_data( const gchar *mnemo, const gchar *label, gchar **msgerr )
{
	gboolean ok;

	ok = TRUE;
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( mnemo )){
		ok = FALSE;
		if( msgerr ){
			*msgerr = g_strdup( _( "Mnemonic is empty" ));
		}
	}
	if( !my_strlen( label )){
		ok = FALSE;
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
	}

	return( ok );
}

/**
 * ofo_tva_form_compare_id:
 * @a:
 * @b:
 *
 * Returns: -1 if @a'ids are lesser than @b'ids, 0 if they are equal,
 * +1 if they are greater.
 */
gint
ofo_tva_form_compare_id( const ofoTVAForm *a, const ofoTVAForm *b )
{
	const gchar *ca, *cb;
	gint cmp;

	g_return_val_if_fail( a && OFO_IS_TVA_FORM( a ), 0 );
	g_return_val_if_fail( b && OFO_IS_TVA_FORM( b ), 0 );
	g_return_val_if_fail( !OFO_BASE( a )->prot->dispose_has_run && !OFO_BASE( b )->prot->dispose_has_run, 0 );

	ca = ofo_tva_form_get_mnemo( a );
	cb = ofo_tva_form_get_mnemo( b );
	cmp = g_utf8_collate( ca, cb );

	return( cmp );
}

/**
 * ofo_tva_form_set_mnemo:
 */
void
ofo_tva_form_set_mnemo( ofoTVAForm *form, const gchar *mnemo )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_MNEMO, mnemo );
}

/**
 * ofo_tva_form_set_label:
 */
void
ofo_tva_form_set_label( ofoTVAForm *form, const gchar *label )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_LABEL, label );
}

/**
 * ofo_tva_form_set_has_correspondence:
 */
void
ofo_tva_form_set_has_correspondence( ofoTVAForm *form, gboolean has_correspondence )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_HAS_CORRESPONDENCE, has_correspondence ? "Y":"N" );
}

/**
 * ofo_tva_form_set_is_enabled:
 */
void
ofo_tva_form_set_is_enabled( ofoTVAForm *form, gboolean enabled )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_ENABLED, enabled ? "Y":"N" );
}

/**
 * ofo_tva_form_set_notes:
 */
void
ofo_tva_form_set_notes( ofoTVAForm *form, const gchar *notes )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_NOTES, notes );
}

/*
 * ofo_tva_form_set_upd_user:
 */
static void
tva_form_set_upd_user( ofoTVAForm *form, const gchar *upd_user )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_UPD_USER, upd_user );
}

/*
 * ofo_tva_form_set_upd_stamp:
 */
static void
tva_form_set_upd_stamp( ofoTVAForm *form, const GTimeVal *upd_stamp )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_UPD_STAMP, upd_stamp );
}

/**
 * ofo_tva_form_detail_add:
 */
void
ofo_tva_form_detail_add( ofoTVAForm *form,
							guint level,
							const gchar *code, const gchar *label,
							gboolean has_base, const gchar *base,
							gboolean has_amount, const gchar *amount,
							gboolean has_template, const gchar *template )
{
	GList *fields;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
	g_return_if_fail( !OFO_BASE( form )->prot->dispose_has_run );

	fields = form_detail_new( form, level, code, label, has_base, base, has_amount, amount, has_template, template );
	form_detail_add( form, fields );
}

static GList *
form_detail_new( ofoTVAForm *form,
							guint level,
							const gchar *code, const gchar *label,
							gboolean has_base, const gchar *base,
							gboolean has_amount, const gchar *amount,
							gboolean has_template, const gchar *template )
{
	GList *fields;

	fields = ofa_box_init_fields_list( st_detail_defs );
	ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_form_get_mnemo( form ));
	ofa_box_set_int( fields, TFO_DET_ROW, 1+ofo_tva_form_detail_get_count( form ));
	ofa_box_set_int( fields, TFO_DET_LEVEL, level );
	ofa_box_set_string( fields, TFO_DET_CODE, code );
	ofa_box_set_string( fields, TFO_DET_LABEL, label );
	ofa_box_set_string( fields, TFO_DET_HAS_BASE, has_base ? "Y":"N" );
	ofa_box_set_string( fields, TFO_DET_BASE, base );
	ofa_box_set_string( fields, TFO_DET_HAS_AMOUNT, has_amount ? "Y":"N" );
	ofa_box_set_string( fields, TFO_DET_AMOUNT, amount );
	ofa_box_set_string( fields, TFO_DET_HAS_TEMPLATE, has_template ? "Y":"N" );
	ofa_box_set_string( fields, TFO_DET_TEMPLATE, template );

	return( fields );
}

static void
form_detail_add( ofoTVAForm *form, GList *fields )
{
	ofoTVAFormPrivate *priv;

	priv = ofo_tva_form_get_instance_private( form );

	priv->details = g_list_append( priv->details, fields );
}

/**
 * ofo_tva_form_detail_free_all:
 */
void
ofo_tva_form_detail_free_all( ofoTVAForm *form )
{
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	g_return_if_fail( !OFO_BASE( form )->prot->dispose_has_run );

	details_list_free( form );
}

/**
 * ofo_tva_form_detail_get_count:
 */
guint
ofo_tva_form_detail_get_count( ofoTVAForm *form )
{
	ofoTVAFormPrivate *priv;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, 0 );

	priv = ofo_tva_form_get_instance_private( form );

	return( g_list_length( priv->details ));
}

/**
 * ofo_tva_form_detail_get_level:
 * @idx is the index in the details list, starting with zero
 */
guint
ofo_tva_form_detail_get_level( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	guint value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, 0 );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	value = nth ? ofa_box_get_int( nth->data, TFO_DET_LEVEL ) : 0;

	return( value );
}

/**
 * ofo_tva_form_detail_get_code:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_code( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_CODE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_detail_get_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_label( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_LABEL ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_detail_get_has_base:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_form_detail_get_has_base( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_BASE ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_form_detail_get_base:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_base( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_BASE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_detail_get_has_amount:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_form_detail_get_has_amount( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_AMOUNT ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_form_detail_get_amount:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_amount( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_AMOUNT ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_detail_get_has_template:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_form_detail_get_has_template( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_TEMPLATE ) : NULL;
	value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	return( value );
}

/**
 * ofo_tva_form_detail_get_template:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_template( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->details, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_TEMPLATE ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_update_ope_template:
 * @form: this #ofoTVAForm instance.
 * @prev_id: the previous ope template identifier.
 * @new_id: the new ope template identifier.
 *
 * Update the operation template identifier.
 */
void
ofo_tva_form_update_ope_template( ofoTVAForm *form, const gchar *prev_id, const gchar *new_id )
{
	ofoTVAFormPrivate *priv;
	GList *it;
	const gchar *det_model;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
	g_return_if_fail( !OFO_BASE( form )->prot->dispose_has_run );
	g_return_if_fail( my_strlen( prev_id ) > 0 );
	g_return_if_fail( my_strlen( new_id ) > 0 );

	priv = ofo_tva_form_get_instance_private( form );

	for( it=priv->details ; it ; it=it->next ){
		det_model = ofa_box_get_string(( GList * ) it->data, TFO_DET_TEMPLATE );
		if( my_strlen( det_model ) && !my_collate( det_model, prev_id )){
			ofa_box_set_string(( GList * ) it->data, TFO_DET_TEMPLATE, new_id );
		}
	}
}

/**
 * ofo_tva_form_boolean_add:
 * @form:
 * @label:
 */
void
ofo_tva_form_boolean_add( ofoTVAForm *form, const gchar *label )
{
	GList *fields;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
	g_return_if_fail( !OFO_BASE( form )->prot->dispose_has_run );

	fields = form_boolean_new( form, label );
	form_boolean_add( form, fields );
}

static GList *
form_boolean_new( ofoTVAForm *form, const gchar *label )
{
	GList *fields;

	fields = ofa_box_init_fields_list( st_boolean_defs );
	ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_form_get_mnemo( form ));
	ofa_box_set_int( fields, TFO_BOOL_ROW, 1+ofo_tva_form_boolean_get_count( form ));
	ofa_box_set_string( fields, TFO_BOOL_LABEL, label );

	return( fields );
}

static void
form_boolean_add( ofoTVAForm *form, GList *fields )
{
	ofoTVAFormPrivate *priv;

	priv = ofo_tva_form_get_instance_private( form );

	priv->bools = g_list_append( priv->bools, fields );
}

/**
 * ofo_tva_form_boolean_free_all:
 */
void
ofo_tva_form_boolean_free_all( ofoTVAForm *form )
{
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
	g_return_if_fail( !OFO_BASE( form )->prot->dispose_has_run );

	bools_list_free( form );
}

/**
 * ofo_tva_form_boolean_get_count:
 */
guint
ofo_tva_form_boolean_get_count( ofoTVAForm *form )
{
	ofoTVAFormPrivate *priv;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, 0 );

	priv = ofo_tva_form_get_instance_private( form );

	return( g_list_length( priv->bools ));
}

/**
 * ofo_tva_form_boolean_get_label:
 * @idx is the index in the booleans list, starting with zero
 */
const gchar *
ofo_tva_form_boolean_get_label( ofoTVAForm *form, guint idx )
{
	ofoTVAFormPrivate *priv;
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	priv = ofo_tva_form_get_instance_private( form );

	nth = g_list_nth( priv->bools, idx );
	cstr = nth ? ofa_box_get_string( nth->data, TFO_BOOL_LABEL ) : NULL;

	return( cstr );
}

/**
 * ofo_tva_form_doc_get_count:
 */
guint
ofo_tva_form_doc_get_count( ofoTVAForm *form )
{
	ofoTVAFormPrivate *priv;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );
	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, 0 );

	priv = ofo_tva_form_get_instance_private( form );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_tva_form_get_bool_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_FORMS_BOOL
 * child table.
 *
 * The returned list should be #ofo_tva_form_free_bool_orphans() by the
 * caller.
 */
GList *
ofo_tva_form_get_bool_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_FORMS_BOOL" ));
}

/**
 * ofo_tva_form_get_det_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_FORMS_DET
 * child table.
 *
 * The returned list should be #ofo_tva_form_free_det_orphans() by the
 * caller.
 */
GList *
ofo_tva_form_get_det_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_FORMS_DET" ));
}

/**
 * ofo_tva_form_get_doc_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown period mnemos in TVA_T_FORMS_DOC
 * child table.
 *
 * The returned list should be #ofo_tva_form_free_doc_orphans() by the
 * caller.
 */
GList *
ofo_tva_form_get_doc_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "TVA_T_FORMS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(TFO_MNEMO) FROM %s "
			"	WHERE TFO_MNEMO NOT IN (SELECT TFO_MNEMO FROM TVA_T_FORMS)", table );

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
 * ofo_tva_form_insert:
 */
gboolean
ofo_tva_form_insert( ofoTVAForm *tva_form )
{
	static const gchar *thisfn = "ofo_tva_form_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: form=%p", thisfn, ( void * ) tva_form );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tva_form ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_tva_form_get_dataset( getter );

	if( form_do_insert( tva_form, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( tva_form ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, tva_form );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
form_do_insert( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	return( form_insert_main( form, connect ) &&
			form_insert_details_ex( form, connect ));
}

static gboolean
form_insert_main( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	const gchar *userid;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_form_get_label( form ));
	notes = my_utils_quote_sql( ofo_tva_form_get_notes( form ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO TVA_T_FORMS" );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_LABEL,TFO_HAS_CORRESPONDENCE,TFO_ENABLED,"
			"	 TFO_NOTES,TFO_UPD_USER, TFO_UPD_STAMP) VALUES ('%s',",
			ofo_tva_form_get_mnemo( form ));

	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_tva_form_get_has_correspondence( form ) ? "Y":"N" );

	g_string_append_printf( query, "'%s',", ofo_tva_form_get_is_enabled( form ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	tva_form_set_upd_user( form, userid );
	tva_form_set_upd_stamp( form, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
form_insert_details_ex( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	ofoTVAFormPrivate *priv;
	gboolean ok;
	GList *idet;
	guint rang;

	priv = ofo_tva_form_get_instance_private( form );

	ok = FALSE;

	if( form_delete_details( form, connect ) && form_delete_bools( form, connect )){
		ok = TRUE;
		for( idet=priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !form_insert_details( form, connect, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
		for( idet=priv->bools, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !form_insert_bools( form, connect, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
form_insert_details( ofoTVAForm *form, const ofaIDBConnect *connect, guint rang, GList *details )
{
	GString *query;
	gboolean ok;
	gchar *code, *label, *base, *amount;
	const gchar *cstr;

	query = g_string_new( "INSERT INTO TVA_T_FORMS_DET " );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_DET_ROW,"
			"	 TFO_DET_LEVEL,TFO_DET_CODE,TFO_DET_LABEL,"
			"	 TFO_DET_HAS_BASE,TFO_DET_BASE,"
			"	 TFO_DET_HAS_AMOUNT,TFO_DET_AMOUNT,"
			"	 TFO_DET_HAS_TEMPLATE,TFO_DET_TEMPLATE) "
			"	VALUES('%s',%d",
			ofo_tva_form_get_mnemo( form ), rang );

	g_string_append_printf( query, ",%u", ofa_box_get_int( details, TFO_DET_LEVEL ));

	code = my_utils_quote_sql( ofa_box_get_string( details, TFO_DET_CODE ));
	if( my_strlen( code )){
		g_string_append_printf( query, ",'%s'", code );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( code );

	label = my_utils_quote_sql( ofa_box_get_string( details, TFO_DET_LABEL ));
	if( my_strlen( label )){
		g_string_append_printf( query, ",'%s'", label );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( label );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_BASE );
	g_string_append_printf( query, ",'%s'", cstr );

	base = my_utils_quote_sql( ofa_box_get_string( details, TFO_DET_BASE ));
	if( my_strlen( base )){
		g_string_append_printf( query, ",'%s'", base );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( base );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_AMOUNT );
	g_string_append_printf( query, ",'%s'", cstr );

	amount = my_utils_quote_sql( ofa_box_get_string( details, TFO_DET_AMOUNT ));
	if( my_strlen( amount )){
		g_string_append_printf( query, ",'%s'", amount );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( amount );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_TEMPLATE );
	g_string_append_printf( query, ",'%s'", cstr );

	code = my_utils_quote_sql( ofa_box_get_string( details, TFO_DET_TEMPLATE ));
	if( my_strlen( code )){
		g_string_append_printf( query, ",'%s'", code );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( code );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
form_insert_bools( ofoTVAForm *form, const ofaIDBConnect *connect, guint rang, GList *fields )
{
	GString *query;
	gboolean ok;
	gchar *label;

	query = g_string_new( "INSERT INTO TVA_T_FORMS_BOOL " );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_BOOL_ROW,"
			"	 TFO_BOOL_LABEL) "
			"	VALUES('%s',%d",
			ofo_tva_form_get_mnemo( form ), rang );

	label = my_utils_quote_sql( ofa_box_get_string( fields, TFO_BOOL_LABEL ));
	g_string_append_printf( query, ",'%s'", label );
	g_free( label );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_tva_form_update:
 * @form:
 * @prev_mnemo:
 */
gboolean
ofo_tva_form_update( ofoTVAForm *tva_form, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_tva_form_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: form=%p, prev_mnemo=%s",
			thisfn, ( void * ) tva_form, prev_mnemo );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tva_form ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( form_do_update( tva_form, ofa_hub_get_connect( hub ), prev_mnemo )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, tva_form, prev_mnemo );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
form_do_update( ofoTVAForm *form, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	return( form_update_main( form, connect, prev_mnemo ) &&
			form_insert_details_ex( form, connect ));
}

static gboolean
form_update_main( ofoTVAForm *form, const ofaIDBConnect *connect, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes, *stamp_str;
	const gchar *new_mnemo, *userid;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_form_get_label( form ));
	notes = my_utils_quote_sql( ofo_tva_form_get_notes( form ));
	new_mnemo = ofo_tva_form_get_mnemo( form );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE TVA_T_FORMS SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "TFO_MNEMO='%s',", new_mnemo );
	}

	if( my_strlen( label )){
		g_string_append_printf( query, "TFO_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "TFO_LABEL=NULL," );
	}

	g_string_append_printf(
			query, "TFO_HAS_CORRESPONDENCE='%s',",
			ofo_tva_form_get_has_correspondence( form ) ? "Y":"N" );

	g_string_append_printf(
			query, "TFO_ENABLED='%s',",
			ofo_tva_form_get_is_enabled( form ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "TFO_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TFO_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	TFO_UPD_USER='%s',TFO_UPD_STAMP='%s'"
			"	WHERE TFO_MNEMO='%s'",
					userid,
					stamp_str,
					prev_mnemo );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	tva_form_set_upd_user( form, userid );
	tva_form_set_upd_stamp( form, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );

	return( ok );
}

/**
 * ofo_tva_form_delete:
 */
gboolean
ofo_tva_form_delete( ofoTVAForm *tva_form )
{
	static const gchar *thisfn = "ofo_tva_form_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: form=%p", thisfn, ( void * ) tva_form );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( ofo_tva_form_is_deletable( tva_form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tva_form ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( form_do_delete( tva_form, ofa_hub_get_connect( hub ))){
		g_object_ref( tva_form );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( tva_form ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, tva_form );
		g_object_unref( tva_form );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
form_do_delete( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	gboolean ok;
	const gchar *mnemo;

	mnemo = ofo_tva_form_get_mnemo( form );

	ok = form_do_delete_by_mnemo( connect, mnemo, "TVA_T_FORMS" ) &&
			form_do_delete_by_mnemo( connect, mnemo, "TVA_T_FORMS_DET" ) &&
			form_do_delete_by_mnemo( connect, mnemo, "TVA_T_FORMS_BOOL" );

	return( ok );
}

static gboolean
form_do_delete_by_mnemo( const ofaIDBConnect *connect, const gchar *mnemo, const gchar *table )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf( "DELETE FROM %s WHERE TFO_MNEMO='%s'", table, mnemo );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
form_delete_details( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	return( form_do_delete_by_mnemo( connect, ofo_tva_form_get_mnemo( form ), "TVA_T_FORMS_DET" ));
}

static gboolean
form_delete_bools( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	return( form_do_delete_by_mnemo( connect, ofo_tva_form_get_mnemo( form ), "TVA_T_FORMS_BOOL" ));
}

static gint
form_cmp_by_mnemo( const ofoTVAForm *a, const gchar *mnemo )
{
	return( my_collate( ofo_tva_form_get_mnemo( a ), mnemo ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_form_icollectionable_iface_init";

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
	static const gchar *thisfn = "ofo_tva_form_load_dataset";
	ofoTVAFormPrivate *priv;
	GList *dataset, *it, *ir;
	ofoTVAForm *form;
	gchar *from;
	ofaHub *hub;
	const ofaIDBConnect *connect;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"TVA_T_FORMS",
					OFO_TYPE_TVA_FORM,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));
	connect = ofa_hub_get_connect( hub );

	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );
		priv = ofo_tva_form_get_instance_private( form );

		from = g_strdup_printf(
				"TVA_T_FORMS_DET WHERE TFO_MNEMO='%s'",
				ofo_tva_form_get_mnemo( form ));
		priv->details = ofo_base_load_rows( st_detail_defs, connect, from );
		g_free( from );

		/* dump the detail rows */
		if( 0 ){
			for( ir=priv->details ; ir ; ir=ir->next ){
				ofa_box_dump_fields_list( thisfn, ir->data );
			}
		}

		from = g_strdup_printf(
				"TVA_T_FORMS_BOOL WHERE TFO_MNEMO='%s'",
				ofo_tva_form_get_mnemo( form ));
		priv->bools = ofo_base_load_rows( st_boolean_defs, connect, from );
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
	static const gchar *thisfn = "ofo_tva_form_iexportable_iface_init";

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
	return( g_strdup( _( "_VAT forms" )));
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the defined VAT forms.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_tva_form_iexportable_export";

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
	ofoTVAFormPrivate *priv;
	GList *dataset, *it, *det, *itd;
	ofoTVAForm *form;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_tva_form_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += FORM_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );
		count += ofo_tva_form_boolean_get_count( form );
		count += ofo_tva_form_detail_get_count( form );
		count += ofo_tva_form_doc_get_count( form );
	}
	ofa_iexportable_set_count( exportable, count );

	/* add a version line at the very beginning of the file */
	str1 = g_strdup_printf( "0%cVersion%c%u", field_sep, field_sep, FORM_EXPORT_VERSION );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );

	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					FORM_TABLES_COUNT, st_boxed_defs, st_boolean_defs, st_detail_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		form = OFO_TVA_FORM( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( form )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c%s", field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_tva_form_get_instance_private( form );

		for( det=priv->bools ; det && ok ; det=det->next ){
			str1 = ofa_box_csv_get_line( det->data, stformat, NULL );
			str2 = g_strdup_printf( "2%c%s", field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( det=priv->details ; det && ok ; det=det->next ){
			str1 = ofa_box_csv_get_line( det->data, stformat, NULL );
			str2 = g_strdup_printf( "3%c%s", field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
			str2 = g_strdup_printf( "2%c%s", field_sep, str1 );
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
	static const gchar *thisfn = "ofo_class_iimportable_iface_init";

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
 * ofo_tva_form_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - form mnemo
 * - label
 * - has correspondence
 * - notes (opt)
 *
 * - 2:
 * - form mnemo
 * - row number (placeholder, not imported, but recomputed)
 * - bool label
 *
 * - 3:
 * - form mnemo
 * - row number (placeholder, not imported, but recomputed)
 * - level
 * - code
 * - label
 * - has base
 * - base rule
 * - has amount
 * - amount rule
 * - has ope template
 * - template id
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'main' records, then all 'boolean' and 'detail' records
 * (but main record must be defined before any boolean or detail one
 *  for a given mnemo).
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
	gchar *bck_table, *bck_det_table, *bck_bool_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "TVA_T_FORMS" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "TVA_T_FORMS_DET" );
		bck_bool_table = ofa_idbconnect_table_backup( connect, "TVA_T_FORMS_BOOL" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_TVA_FORM );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_TVA_FORM );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "TVA_T_FORMS" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "TVA_T_FORMS_DET" );
			ofa_idbconnect_table_restore( connect, bck_bool_table, "TVA_T_FORMS_BOOL" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
		g_free( bck_bool_table );
	}

	if( dataset ){
		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
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
	ofoTVAForm *form;
	gchar *str, *mnemo;
	gint type;
	GList *bool, *rule;

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
				form = iimportable_import_parse_form( importer, parms, numline, itf );
				if( form ){
					parms->parsed_count += 1;
					dataset = g_list_prepend( dataset, form );
					ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
				}
				break;
			case 2:
				mnemo = NULL;
				bool = iimportable_import_parse_bool( importer, parms, numline, itf, &mnemo );
				if( bool ){
					form = form_find_by_mnemo( dataset, mnemo );
					if( form ){
						parms->parsed_count += 1;
						ofa_box_set_int( bool, TFO_BOOL_ROW, 1+ofo_tva_form_boolean_get_count( form ));
						form_boolean_add( form, bool );
						ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
					} else {
						str = g_strdup_printf( _( "invalid mnemo: %s" ), mnemo );
						ofa_iimporter_progress_num_text( importer, parms, numline, str );
						parms->parse_errs += 1;
						g_free( str );
						ofa_box_free_fields_list( bool );
						return( NULL );
					}
					g_free( mnemo );
				}
				break;
			case 3:
				mnemo = NULL;
				rule = iimportable_import_parse_rule( importer, parms, numline, itf, &mnemo );
				if( rule ){
					form = form_find_by_mnemo( dataset, mnemo );
					if( form ){
						parms->parsed_count += 1;
						ofa_box_set_int( rule, TFO_DET_ROW, 1+ofo_tva_form_detail_get_count( form ));
						form_detail_add( form, rule );
						ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
					} else {
						str = g_strdup_printf( _( "invalid mnemo: %s" ), mnemo );
						ofa_iimporter_progress_num_text( importer, parms, numline, str );
						parms->parse_errs += 1;
						g_free( str );
						ofa_box_free_fields_list( rule );
						return( NULL );
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

static ofoTVAForm *
iimportable_import_parse_form( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	ofoTVAForm *form;
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;

	form = ofo_tva_form_new( parms->getter );

	/* mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty form mnemonic" ));
		parms->parse_errs += 1;
		g_object_unref( form );
		return( NULL );
	}
	ofo_tva_form_set_mnemo( form, cstr );

	/* label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty form label" ));
		parms->parse_errs += 1;
		g_object_unref( form );
		return( NULL );
	}
	ofo_tva_form_set_label( form, cstr );

	/* has correspondence */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofo_tva_form_set_has_correspondence( form, my_utils_boolean_from_str( cstr ));

	/* notes */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_tva_form_set_notes( form, splitted );
	g_free( splitted );

	/* update user - not imported */
	itf = itf ? itf->next : NULL;

	/* update stamp - not imported */
	itf = itf ? itf->next : NULL;

	/* enabled
	 * default is TRUE */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_tva_form_set_is_enabled( form, my_utils_boolean_from_str( cstr ));
	} else {
		ofo_tva_form_set_is_enabled( form, TRUE );
	}

	return( form );
}

static GList *
iimportable_import_parse_bool( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo )
{
	GList *boolean;
	const gchar *cstr;
	GSList *itf;

	boolean = ofa_box_init_fields_list( st_boolean_defs );

	/* mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty form mnemonic" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( boolean );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );
	ofa_box_set_string( boolean, TFO_MNEMO, cstr );

	/* row number (placeholder) */
	itf = itf ? itf->next : NULL;

	/* label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( boolean, TFO_BOOL_LABEL, cstr );

	return( boolean );
}

static GList *
iimportable_import_parse_rule( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gchar **mnemo )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;
	gint level;
	gchar *str;

	detail = ofa_box_init_fields_list( st_detail_defs );

	/* mnemo */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty form mnemonic" ));
		parms->parse_errs += 1;
		ofa_box_free_fields_list( detail );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );
	ofa_box_set_string( detail, TFO_MNEMO, cstr );

	/* row number (placeholder) */
	itf = itf ? itf->next : NULL;

	/* level */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	level = cstr ? atoi( cstr ) : 0;
	if( level <= 0 ){
		str = g_strdup_printf( _( "invalid level: %s, should be greater than zero" ), cstr );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		parms->parse_errs += 1;
		g_free( str );
		ofa_box_free_fields_list( detail );
		return( NULL );
	} else {
		ofa_box_set_int( detail, TFO_DET_LEVEL, level );
	}

	/* code */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, TFO_DET_CODE, cstr );

	/* label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, TFO_DET_LABEL, cstr );

	/* has base */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
		str = g_strdup_printf( _( "invalid HasBase indicator: %s, should be 'Y' or 'N'" ), cstr );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		parms->parse_errs += 1;
		g_free( str );
		ofa_box_free_fields_list( detail );
		return( NULL );
	} else {
		ofa_box_set_string( detail, TFO_DET_HAS_BASE, cstr );
	}

	/* base */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, TFO_DET_BASE, cstr );

	/* has amount */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
		str = g_strdup_printf( _( "invalid HasAmount indicator: %s, should be 'Y' or 'N'" ), cstr );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		parms->parse_errs += 1;
		g_free( str );
		ofa_box_free_fields_list( detail );
		return( NULL );
	} else {
		ofa_box_set_string( detail, TFO_DET_HAS_AMOUNT, cstr );
	}

	/* amount */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, TFO_DET_AMOUNT, cstr );

	/* has ope template */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( cstr && my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
		str = g_strdup_printf( _( "invalid HasOpeTemplate indicator: %s, should be 'Y' or 'N'" ), cstr );
		ofa_iimporter_progress_num_text( importer, parms, numline, str );
		parms->parse_errs += 1;
		g_free( str );
		ofa_box_free_fields_list( detail );
		return( NULL );
	} else {
		ofa_box_set_string( detail, TFO_DET_HAS_TEMPLATE, cstr ? cstr : "N" );
	}

	/* template id */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofa_box_set_string( detail, TFO_DET_TEMPLATE, cstr );

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
	ofoTVAForm *form;
	gboolean insert;
	guint total, type;
	gchar *str;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		form_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		form = OFO_TVA_FORM( it->data );

		if( form_get_exists( form, connect )){
			parms->duplicate_count += 1;
			mnemo = ofo_tva_form_get_mnemo( form );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate VAT form, replacing previous one" ), mnemo );
					form_do_delete( form, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate VAT form, ignored (skipped)" ), mnemo );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate VAT form" ), mnemo );
					type = MY_PROGRESS_ERROR;
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}
		}
		if( str ){
			ofa_iimporter_progress_text( importer, parms, type, str );
			g_free( str );
		}
		if( insert ){
			if( form_do_insert( form, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
form_get_exists( ofoTVAForm *form, const ofaIDBConnect *connect )
{
	const gchar *form_id;
	gint count;
	gchar *str;

	count = 0;
	form_id = ofo_tva_form_get_mnemo( form );
	str = g_strdup_printf( "SELECT COUNT(*) FROM TVA_T_FORMS WHERE TFO_MNEMO='%s'", form_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
form_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM TVA_T_FORMS", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM TVA_T_FORMS_DET", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM TVA_T_FORMS_BOOL", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_form_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_tva_form_isignalable_connect_to";

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
	static const gchar *thisfn = "ofo_tva_form_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = signaler_is_deletable_account( signaler, OFO_ACCOUNT( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = signaler_is_deletable_ope_template( signaler, OFO_OPE_TEMPLATE( object ));
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
			"SELECT COUNT(*) FROM TVA_T_FORMS_DET "
			"	WHERE TFO_DET_BASE LIKE '%%%s%%' OR TFO_DET_AMOUNT LIKE '%%%s%%'",
			ofo_account_get_number( account ),
			ofo_account_get_number( account ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template )
{
	ofaIGetter *getter;

	getter = ofa_isignaler_get_getter( signaler );

	return( !ofo_tva_form_use_ope_template( getter, ofo_ope_template_get_mnemo( template )));
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_tva_form_signaler_on_updated_base";
	const gchar *mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_account_id( signaler, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( my_collate( mnemo, prev_id )){
				signaler_on_updated_ope_template_mnemo( signaler, mnemo, prev_id );
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
signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_signaler_on_updated_account_id";
	ofaIGetter *getter;
	gboolean ok;

	g_debug( "%s: signaler=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) signaler, mnemo, prev_id );

	getter = ofa_isignaler_get_getter( signaler );

	ok = do_update_formulas( getter, mnemo, prev_id );

	return( ok );
}

static gboolean
signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_signaler_on_updated_ope_template_mnemo";
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
					"UPDATE TVA_T_FORMS_DET "
					"	SET TFO_DET_TEMPLATE='%s'"
					"	WHERE TFO_DET_TEMPLATE='%s'", mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gboolean
signaler_on_updated_rate_mnemo( ofaISignaler *signaler, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_signaler_on_updated_rate_mnemo";
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
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *query;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_base, *det_amount;
	gint det_row;
	gboolean ok;
	const gchar *prev_base, *prev_amount;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"SELECT TFO_MNEMO,TFO_DET_ROW,TFO_DET_BASE,TFO_DET_AMOUNT "
					"	FROM TVA_T_FORMS_DET "
					"	WHERE TFO_DET_BASE LIKE '%%%s%%' OR TFO_DET_AMOUNT LIKE '%%%s%%'",
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
			prev_base = ( const gchar * ) icol->data;
			det_base = my_utils_str_replace( prev_base, prev_id, new_id );
			icol = icol->next;
			prev_amount = ( const gchar * ) icol->data;
			det_amount = my_utils_str_replace( prev_amount, prev_id, new_id );

			if( my_collate( prev_base, det_base ) || my_collate( prev_amount, det_amount )){
				query = g_strdup_printf(
								"UPDATE TVA_T_FORMS_DET "
								"	SET TFO_DET_BASE='%s',TFO_DET_AMOUNT='%s' "
								"	WHERE TFO_MNEMO='%s' AND TFO_DET_ROW=%d",
										det_base, det_amount, etp_mnemo, det_row );

				ofa_idbconnect_query( connect, query, TRUE );

				g_free( query );
			}

			g_free( det_base );
			g_free( det_amount );
			g_free( etp_mnemo );
		}
	}

	return( ok );
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_tva_form_signaler_on_deleted_base";
	ofoOpeTemplate *template_obj;
	guint count, i;
	const gchar *cstr;
	ofaIGetter *getter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_TVA_FORM( object )){
		count = ofo_tva_form_detail_get_count( OFO_TVA_FORM( object ));
		for( i=0 ; i<count ; ++i ){
			cstr = ofo_tva_form_detail_get_has_template( OFO_TVA_FORM( object ), i ) ?
					ofo_tva_form_detail_get_template( OFO_TVA_FORM( object ), i ) : NULL;
			if( my_strlen( cstr )){
				getter = ofo_base_get_getter( object );
				template_obj = ofo_ope_template_get_by_mnemo( getter , cstr );
				if( template_obj ){
					g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, template_obj, NULL );
				}
			}
		}
	}
}
