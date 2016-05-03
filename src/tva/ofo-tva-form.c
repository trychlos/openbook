/*
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
#include "api/ofa-stream-format.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-ope-template.h"

#include "tva/ofo-tva-form.h"

/* priv instance data
 */
enum {
	TFO_MNEMO = 1,
	TFO_LABEL,
	TFO_HAS_CORRESPONDENCE,
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
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
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

typedef struct {

	/* the details of the form as a GList of GList fields
	 */
	GList     *bools;
	GList     *details;
}
	ofoTVAFormPrivate;

static ofoTVAForm *form_find_by_mnemo( GList *set, const gchar *mnemo );
static void        tva_form_set_upd_user( ofoTVAForm *form, const gchar *upd_user );
static void        tva_form_set_upd_stamp( ofoTVAForm *form, const GTimeVal *upd_stamp );
static GList      *form_detail_new( ofoTVAForm *form, guint level, const gchar *code, const gchar *label, gboolean has_base, const gchar *base, gboolean has_amount, const gchar *amount, gboolean has_template, const gchar *template );
static void        form_detail_add( ofoTVAForm *form, GList *fields );
static GList      *form_boolean_new( ofoTVAForm *form, const gchar *label );
static void        form_boolean_add( ofoTVAForm *form, GList *fields );
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
static gint        tva_form_cmp_by_ptr( const ofoTVAForm *a, const ofoTVAForm *b );
static void        icollectionable_iface_init( myICollectionableInterface *iface );
static guint       icollectionable_get_interface_version( void );
static GList      *icollectionable_load_collection( void *user_data );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( void );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub );
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
static void        isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void        isignal_hub_connect( ofaHub *hub );
static gboolean    hub_on_deletable_object( ofaHub *hub, ofoBase *object, void *empty );
static gboolean    hub_is_deletable_account( ofaHub *hub, ofoAccount *account );
static gboolean    hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template );
static void        hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static gboolean    hub_on_updated_account_id( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );
static gboolean    hub_on_updated_ope_template_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id );

G_DEFINE_TYPE_EXTENDED( ofoTVAForm, ofo_tva_form, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoTVAForm )
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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
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
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoTVAForm dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_tva_form_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_TVA_FORM, hub ));
}

/**
 * ofo_tva_form_get_by_mnemo:
 * @hub:
 * @mnemo:
 *
 * Returns: the searched tva form, or %NULL.
 *
 * The returned object is owned by the #ofoTVAForm class, and should
 * not be unreffed by the caller.
 */
ofoTVAForm *
ofo_tva_form_get_by_mnemo( ofaHub *hub, const gchar *mnemo )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	dataset = ofo_tva_form_get_dataset( hub );

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
 * ofo_tva_form_new:
 */
ofoTVAForm *
ofo_tva_form_new( void )
{
	ofoTVAForm *form;

	form = g_object_new( OFO_TYPE_TVA_FORM, NULL );
	OFO_BASE( form )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

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
ofo_tva_form_new_from_form( const ofoTVAForm *form )
{
	ofoTVAForm *dest;
	gint count, i;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	dest = NULL;

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, NULL );

	dest = ofo_tva_form_new();

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
	ofaHub *hub;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	g_return_val_if_fail( !OFO_BASE( form )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	hub = ofo_base_get_hub( OFO_BASE( form ));

	if( hub ){
		g_signal_emit_by_name( hub, SIGNAL_HUB_DELETABLE, form, &deletable );
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
			*msgerr = g_strdup( _( "Empty mnemonic" ));
		}
	}
	if( !my_strlen( label )){
		ok = FALSE;
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty label" ));
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
ofo_tva_form_detail_get_count( const ofoTVAForm *form )
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
ofo_tva_form_detail_get_level( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_code( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_label( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_has_base( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_base( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_has_amount( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_amount( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_has_template( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_detail_get_template( const ofoTVAForm *form, guint idx )
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
ofo_tva_form_boolean_get_count( const ofoTVAForm *form )
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
ofo_tva_form_boolean_get_label( const ofoTVAForm *form, guint idx )
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
 * ofo_tva_form_insert:
 */
gboolean
ofo_tva_form_insert( ofoTVAForm *tva_form, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_tva_form_insert";
	gboolean ok;

	g_debug( "%s: form=%p, hub=%p",
			thisfn, ( void * ) tva_form, ( void * ) hub );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( form_do_insert( tva_form, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( tva_form ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ),
				MY_ICOLLECTIONABLE( tva_form ), ( GCompareFunc ) tva_form_cmp_by_ptr, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, tva_form );
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
	gchar *label, *notes, *userid;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_form_get_label( form ));
	notes = my_utils_quote_sql( ofo_tva_form_get_notes( form ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO TVA_T_FORMS" );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_LABEL,TFO_HAS_CORRESPONDENCE,"
			"	 TFO_NOTES,TFO_UPD_USER, TFO_UPD_STAMP) VALUES ('%s',",
			ofo_tva_form_get_mnemo( form ));

	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_tva_form_get_has_correspondence( form ) ? "Y":"N" );

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
	g_free( userid );

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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: form=%p, prev_mnemo=%s",
			thisfn, ( void * ) tva_form, prev_mnemo );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( tva_form ));

	if( form_do_update( tva_form, ofa_hub_get_connect( hub ), prev_mnemo )){
		my_icollector_collection_sort(
				ofa_hub_get_collector( hub ),
				OFO_TYPE_TVA_FORM, ( GCompareFunc ) tva_form_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, tva_form, prev_mnemo );
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
	gchar *label, *notes, *userid;
	const gchar *new_mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tva_form_get_label( form ));
	notes = my_utils_quote_sql( ofo_tva_form_get_notes( form ));
	new_mnemo = ofo_tva_form_get_mnemo( form );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

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
	g_free( userid );

	return( ok );
}

/**
 * ofo_tva_form_delete:
 */
gboolean
ofo_tva_form_delete( ofoTVAForm *tva_form )
{
	static const gchar *thisfn = "ofo_tva_form_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: form=%p", thisfn, ( void * ) tva_form );

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( ofo_tva_form_is_deletable( tva_form ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tva_form )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( tva_form ));

	if( form_do_delete( tva_form, ofa_hub_get_connect( hub ))){
		g_object_ref( tva_form );
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( tva_form ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, tva_form );
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
	return( g_utf8_collate( ofo_tva_form_get_mnemo( a ), mnemo ));
}

static gint
tva_form_cmp_by_ptr( const ofoTVAForm *a, const ofoTVAForm *b )
{
	return( form_cmp_by_mnemo( a, ofo_tva_form_get_mnemo( b )));
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
	const ofaIDBConnect *connect;

	g_return_val_if_fail( user_data && OFA_IS_HUB( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"TVA_T_FORMS ORDER BY TFO_MNEMO ASC",
					OFO_TYPE_TVA_FORM,
					OFA_HUB( user_data ));

	connect = ofa_hub_get_connect( OFA_HUB( user_data ));

	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );
		priv = ofo_tva_form_get_instance_private( form );

		from = g_strdup_printf(
				"TVA_T_FORMS_DET WHERE TFO_MNEMO='%s' ORDER BY TFO_DET_ROW ASC",
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
				"TVA_T_FORMS_BOOL WHERE TFO_MNEMO='%s' ORDER BY TFO_BOOL_ROW ASC",
				ofo_tva_form_get_mnemo( form ));
		priv->bools = ofo_base_load_rows( st_boolean_defs, connect, from );
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
 *
 * Exports the VAT forms line by line.
 * 1: the main record
 * 2: the booleans
 * 3: the details
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaStreamFormat *settings, ofaHub *hub )
{
	ofoTVAFormPrivate *priv;
	GList *dataset, *it, *det;
	ofoTVAForm *form;
	gchar *str, *str2;
	gboolean ok, with_headers;
	gulong count;
	gchar field_sep;

	dataset = ofo_tva_form_get_dataset( hub );

	with_headers = ofa_stream_format_get_with_headers( settings );
	field_sep = ofa_stream_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 3;
	}
	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );
		count += ofo_tva_form_boolean_get_count( form );
		count += ofo_tva_form_detail_get_count( form );
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

		str = ofa_box_csv_get_header( st_boolean_defs, settings );
		str2 = g_strdup_printf( "2%c%s", field_sep, str );
		ok = ofa_iexportable_set_line( exportable, str2 );
		g_free( str2 );
		g_free( str );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_detail_defs, settings );
		str2 = g_strdup_printf( "3%c%s", field_sep, str );
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

		form = OFO_TVA_FORM( it->data );
		priv = ofo_tva_form_get_instance_private( form );

		for( det=priv->bools ; det ; det=det->next ){
			str = ofa_box_csv_get_line( det->data, settings );
			str2 = g_strdup_printf( "2%c%s", field_sep, str );
			ok = ofa_iexportable_set_line( exportable, str2 );
			g_free( str2 );
			g_free( str );
			if( !ok ){
				return( FALSE );
			}
		}

		for( det=priv->details ; det ; det=det->next ){
			str = ofa_box_csv_get_line( det->data, settings );
			str2 = g_strdup_printf( "3%c%s", field_sep, str );
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
	GList *dataset;
	gchar *bck_table, *bck_det_table, *bck_bool_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "TVA_T_FORMS" );
		bck_det_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "TVA_T_FORMS_DET" );
		bck_bool_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "TVA_T_FORMS_BOOL" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_hub_get_collector( parms->hub ), OFO_TYPE_TVA_FORM );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_TVA_FORM );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "TVA_T_FORMS" );
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_det_table, "TVA_T_FORMS_DET" );
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_bool_table, "TVA_T_FORMS_BOOL" );
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

	form = ofo_tva_form_new();

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

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_tva_form_set_notes( form, splitted );
	g_free( splitted );

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
	const ofaIDBConnect *connect;
	const gchar *mnemo;
	ofoTVAForm *form;
	gboolean insert;
	guint total;
	gchar *str;

	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
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
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}
		}
		if( str ){
			ofa_iimporter_progress_text( importer, parms, str );
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
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_tva_form_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_tva_form_isignal_hub_connect";

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
	static const gchar *thisfn = "ofo_tva_form_hub_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = hub_is_deletable_account( hub, OFO_ACCOUNT( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = hub_is_deletable_ope_template( hub, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
hub_is_deletable_account( ofaHub *hub, ofoAccount *account )
{
	gchar *query;
	gint count;

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
hub_is_deletable_ope_template( ofaHub *hub, ofoOpeTemplate *template )
{
	gchar *query;
	gint count;

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM TVA_T_FORMS_DET WHERE TFO_DET_TEMPLATE='%s'",
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
	static const gchar *thisfn = "ofo_tva_form_hub_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( my_collate( mnemo, prev_id )){
				hub_on_updated_account_id( hub, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( my_collate( mnemo, prev_id )){
				hub_on_updated_ope_template_mnemo( hub, mnemo, prev_id );
			}
		}
	}
}

static gboolean
hub_on_updated_account_id( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_hub_on_updated_account_id";
	gchar *query;
	const ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_base, *det_amount;
	gint det_row;
	gboolean ok;
	const gchar *prev_base, *prev_amount;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

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
			det_base = my_utils_str_replace( prev_base, prev_id, mnemo );
			icol = icol->next;
			prev_amount = ( const gchar * ) icol->data;
			det_amount = my_utils_str_replace( prev_amount, prev_id, mnemo );

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

		my_icollector_collection_free( ofa_hub_get_collector( hub ), OFO_TYPE_TVA_FORM );
	}

	return( ok );
}

static gboolean
hub_on_updated_ope_template_mnemo( ofaHub *hub, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_hub_on_updated_ope_template_mnemo";
	gchar *query;
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: hub=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) hub, mnemo, prev_id );

	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"UPDATE TVA_T_FORMS_DET "
					"	SET TFO_DET_TEMPLATE='%s'"
					"	WHERE TFO_DET_TEMPLATE='%s'", mnemo, prev_id );

	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	my_icollector_collection_free( ofa_hub_get_collector( hub ), OFO_TYPE_TVA_FORM );

	return( ok );
}
