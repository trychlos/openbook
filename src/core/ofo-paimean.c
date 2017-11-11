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
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-paimean.h"

/* priv instance data
 */
enum {
	PAM_CODE = 1,
	PAM_CRE_USER,
	PAM_CRE_STAMP,
	PAM_LABEL,
	PAM_ACCOUNT,
	PAM_NOTES,
	PAM_UPD_USER,
	PAM_UPD_STAMP,
	PAM_DOC_ID,
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
		{ OFA_BOX_CSV( PAM_CODE ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( PAM_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( PAM_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( PAM_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( PAM_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( PAM_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( PAM_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( PAM_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( PAM_CODE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( PAM_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define PAIMEAN_TABLES_COUNT            2
#define PAIMEAN_EXPORT_VERSION          2

typedef struct {
	GList *docs;
}
	ofoPaimeanPrivate;

static ofoPaimean *paimean_find_by_code( GList *set, const gchar *code );
static void        paimean_set_cre_user( ofoPaimean *paimean, const gchar *user );
static void        paimean_set_cre_stamp( ofoPaimean *paimean, const GTimeVal *stamp );
static void        paimean_set_upd_user( ofoPaimean *paimean, const gchar *user );
static void        paimean_set_upd_stamp( ofoPaimean *paimean, const GTimeVal *stamp );
static GList      *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean    paimean_do_insert( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_insert_main( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_do_update( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean    paimean_update_main( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean    paimean_do_delete( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gint        paimean_cmp_by_code( const ofoPaimean *a, const gchar *code );
static void        icollectionable_iface_init( myICollectionableInterface *iface );
static guint       icollectionable_get_interface_version( void );
static GList      *icollectionable_load_collection( void *user_data );
static void        idoc_iface_init( ofaIDocInterface *iface );
static guint       idoc_get_interface_version( void );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( void );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_get_published( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean    iexportable_export_default( ofaIExportable *exportable );
static void        iimportable_iface_init( ofaIImportableInterface *iface );
static guint       iimportable_get_interface_version( void );
static gchar      *iimportable_get_label( const ofaIImportable *instance );
static guint       iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList      *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoPaimean *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    paimean_get_exists( const ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_drop_content( const ofaIDBConnect *connect );
static void        isignalable_iface_init( ofaISignalableInterface *iface );
static void        isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoPaimean, ofo_paimean, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoPaimean )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
paimean_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_paimean_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, PAM_CODE ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, PAM_LABEL ));

	g_return_if_fail( instance && OFO_IS_PAIMEAN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_paimean_parent_class )->finalize( instance );
}

static void
paimean_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_PAIMEAN( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_paimean_parent_class )->dispose( instance );
}

static void
ofo_paimean_init( ofoPaimean *self )
{
	static const gchar *thisfn = "ofo_paimean_init";
	ofoPaimeanPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_paimean_get_instance_private( self );

	priv->docs = NULL;
}

static void
ofo_paimean_class_init( ofoPaimeanClass *klass )
{
	static const gchar *thisfn = "ofo_paimean_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_finalize;
}

/**
 * ofo_paimean_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoPaimean dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_paimean_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_PAIMEAN, getter ));
}

/**
 * ofo_paimean_get_by_code:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched paimean, or %NULL.
 *
 * The returned object is owned by the #ofoPaimean class, and should
 * not be unreffed by the caller.
 */
ofoPaimean *
ofo_paimean_get_by_code( ofaIGetter *getter, const gchar *code )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( code ), NULL );

	dataset = ofo_paimean_get_dataset( getter );

	return( paimean_find_by_code( dataset, code ));
}

static ofoPaimean *
paimean_find_by_code( GList *set, const gchar *code )
{
	GList *found;

	found = g_list_find_custom(
				set, code, ( GCompareFunc ) paimean_cmp_by_code );
	if( found ){
		return( OFO_PAIMEAN( found->data ));
	}

	return( NULL );
}

/**
 * ofo_paimean_new:
 * @getter: a #ofaIGetter instance.
 */
ofoPaimean *
ofo_paimean_new( ofaIGetter *getter )
{
	ofoPaimean *paimean;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	paimean = g_object_new( OFO_TYPE_PAIMEAN, "ofo-base-getter", getter, NULL );
	OFO_BASE( paimean )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( paimean );
}

/**
 * ofo_paimean_get_code:
 */
const gchar *
ofo_paimean_get_code( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_CODE );
}

/**
 * ofo_paimean_get_cre_user:
 */
const gchar *
ofo_paimean_get_cre_user( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_CRE_USER );
}

/**
 * ofo_paimean_get_cre_stamp:
 */
const GTimeVal *
ofo_paimean_get_cre_stamp( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, timestamp, NULL, PAM_CRE_STAMP );
}

/**
 * ofo_paimean_get_label:
 */
const gchar *
ofo_paimean_get_label( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_LABEL );
}

/**
 * ofo_paimean_get_account:
 */
const gchar *
ofo_paimean_get_account( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_ACCOUNT );
}

/**
 * ofo_paimean_get_notes:
 */
const gchar *
ofo_paimean_get_notes( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_NOTES );
}

/**
 * ofo_paimean_get_upd_user:
 */
const gchar *
ofo_paimean_get_upd_user( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, string, NULL, PAM_UPD_USER );
}

/**
 * ofo_paimean_get_upd_stamp:
 */
const GTimeVal *
ofo_paimean_get_upd_stamp( const ofoPaimean *paimean )
{
	ofo_base_getter( PAIMEAN, paimean, timestamp, NULL, PAM_UPD_STAMP );
}

/**
 * ofo_paimean_is_deletable:
 * @paimean: the paimean
 *
 * There is no hard reference set to this #ofoPaimean class.
 * Entries and ope.templates which reference one of these means of
 * paiement will continue to just work, just losing the benefit of
 * account pre-setting.
 *
 * Returns: %TRUE if the paimean is deletable.
 */
gboolean
ofo_paimean_is_deletable( const ofoPaimean *paimean )
{
	return( TRUE );
}

/**
 * ofo_paimean_is_valid_data:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate code or so.
 */
gboolean
ofo_paimean_is_valid_data( const gchar *code, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( code )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Identifier is empty" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/**
 * ofo_paimean_set_code:
 */
void
ofo_paimean_set_code( ofoPaimean *paimean, const gchar *code )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_CODE, code );
}

/*
 * paimean_set_cre_user:
 */
static void
paimean_set_cre_user( ofoPaimean *paimean, const gchar *user )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_CRE_USER, user );
}

/*
 * paimean_set_cre_stamp:
 */
static void
paimean_set_cre_stamp( ofoPaimean *paimean, const GTimeVal *stamp )
{
	ofo_base_setter( PAIMEAN, paimean, timestamp, PAM_CRE_STAMP, stamp );
}

/**
 * ofo_paimean_set_label:
 */
void
ofo_paimean_set_label( ofoPaimean *paimean, const gchar *label )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_LABEL, label );
}

/**
 * ofo_paimean_set_account:
 */
void
ofo_paimean_set_account( ofoPaimean *paimean, const gchar *account )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_ACCOUNT, account );
}

/**
 * ofo_paimean_set_notes:
 */
void
ofo_paimean_set_notes( ofoPaimean *paimean, const gchar *notes )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_NOTES, notes );
}

/*
 * paimean_set_upd_user:
 */
static void
paimean_set_upd_user( ofoPaimean *paimean, const gchar *user )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_UPD_USER, user );
}

/*
 * paimean_set_upd_stamp:
 */
static void
paimean_set_upd_stamp( ofoPaimean *paimean, const GTimeVal *stamp )
{
	ofo_base_setter( PAIMEAN, paimean, timestamp, PAM_UPD_STAMP, stamp );
}

/**
 * ofo_paimean_doc_get_count:
 * @paimean: this #ofoPaimean object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_paimean_doc_get_count( ofoPaimean *paimean )
{
	ofoPaimeanPrivate *priv;

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), 0 );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, 0 );

	priv = ofo_paimean_get_instance_private( paimean );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_paimean_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown means of paiement identifiers in
 * OFA_T_PAIMEANS_DOC child table.
 *
 * The returned list should be ofo_paimean_free_orphans() by the caller.
 */
GList *
ofo_paimean_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_PAIMEANS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(PAM_CODE) FROM %s "
			"	WHERE PAM_CODE NOT IN (SELECT PAM_CODE FROM OFA_T_PAIMEANS)", table );

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
 * ofo_paimean_insert:
 *
 * First creation of a new #ofoPaimean.
 */
gboolean
ofo_paimean_insert( ofoPaimean *paimean )
{
	static const gchar *thisfn = "ofo_paimean_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: paimean=%p", thisfn, ( void * ) paimean );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_paimean_get_dataset( getter );

	if( paimean_do_insert( paimean, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( paimean ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, paimean );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimean_do_insert( ofoPaimean *paimean, const ofaIDBConnect *connect )
{
	return( paimean_insert_main( paimean, connect ));
}

static gboolean
paimean_insert_main( ofoPaimean *paimean, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_paimean_get_label( paimean ));
	notes = my_utils_quote_sql( ofo_paimean_get_notes( paimean ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_PAIMEANS" );

	g_string_append_printf( query,
			"	(PAM_CODE,PAM_CRE_USER, PAM_CRE_STAMP,PAM_LABEL,PAM_ACCOUNT,PAM_NOTES)"
			"	VALUES ('%s','%s','%s','%s','%s',",
			ofo_paimean_get_code( paimean ),
			userid,
			stamp_str,
			label,
			ofo_paimean_get_account( paimean ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		paimean_set_cre_user( paimean, userid );
		paimean_set_cre_stamp( paimean, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_paimean_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_paimean_update( ofoPaimean *paimean, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_paimean_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimean=%p, prev_code=%s",
			thisfn, ( void * ) paimean, prev_code );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( my_strlen( prev_code ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( paimean_do_update( paimean, prev_code, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, paimean, prev_code );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimean_do_update( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect )
{
	return( paimean_update_main( paimean, prev_code, connect ));
}

static gboolean
paimean_update_main( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_paimean_get_label( paimean ));
	notes = my_utils_quote_sql( ofo_paimean_get_notes( paimean ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_PAIMEANS SET " );

	g_string_append_printf( query, "PAM_CODE='%s',", ofo_paimean_get_code( paimean ));
	g_string_append_printf( query, "PAM_LABEL='%s',", label );
	g_string_append_printf( query, "PAM_ACCOUNT='%s',", ofo_paimean_get_account( paimean ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "PAM_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "PAM_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	PAM_UPD_USER='%s',PAM_UPD_STAMP='%s'"
			"	WHERE PAM_CODE='%s'",
					userid, stamp_str, prev_code );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		paimean_set_upd_user( paimean, userid );
		paimean_set_upd_stamp( paimean, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_paimean_delete:
 */
gboolean
ofo_paimean_delete( ofoPaimean *paimean )
{
	static const gchar *thisfn = "ofo_paimean_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimean=%p", thisfn, ( void * ) paimean );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( paimean_do_delete( paimean, ofa_hub_get_connect( hub ))){
		g_object_ref( paimean );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( paimean ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, paimean );
		g_object_unref( paimean );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimean_do_delete( ofoPaimean *paimean, const ofaIDBConnect *connect )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_PAIMEANS WHERE PAM_CODE='%s'",
					ofo_paimean_get_code( paimean ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
paimean_cmp_by_code( const ofoPaimean *a, const gchar *code )
{
	return( my_collate( ofo_paimean_get_code( a ), code ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_paimean_icollectionable_iface_init";

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
					"OFA_T_PAIMEANS",
					OFO_TYPE_PAIMEAN,
					OFA_IGETTER( user_data ));

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
	static const gchar *thisfn = "ofo_paimean_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
	iface->get_published = iexportable_get_published;
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
	return( g_strdup( _( "Reference : _means of paiement" )));
}

static gboolean
iexportable_get_published( const ofaIExportable *instance )
{
	return( TRUE );
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports all the paiment means.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_paimean_iexportable_export";

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
	GList *dataset, *it, *itd;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	ofoPaimean *paimean;
	ofoPaimeanPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_paimean_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += PAIMEAN_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		paimean = OFO_PAIMEAN( it->data );
		count += ofo_paimean_doc_get_count( paimean );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, PAIMEAN_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					PAIMEAN_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		paimean = OFO_PAIMEAN( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( paimean )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_paimean_get_instance_private( paimean );

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
	static const gchar *thisfn = "ofo_paimean_iimportable_iface_init";

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
 * ofo_paimean_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - paimean code
 * - label
 * - account (opt)
 * - notes (opt)
 *
 * It is not required that the input csv files be sorted by code.
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
	gchar *bck_table;

	dataset = iimportable_import_parse( importer, parms, lines );

	signaler = ofa_igetter_get_signaler( parms->getter );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_PAIMEANS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_PAIMEAN );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_PAIMEAN );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_PAIMEANS" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_paimean_free_dataset( dataset );
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
	GSList *itl, *fields;
	guint numline, total;
	gchar *str;
	ofoPaimean *paimean;

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
		paimean = iimportable_import_parse_main( importer, parms, numline, fields );

		if( paimean ){
			dataset = g_list_prepend( dataset, paimean );
			parms->parsed_count += 1;
			ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
		} else {
			str = g_strdup_printf( _( "unable to import line %d" ), numline );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
		}
	}

	return( dataset );
}

static ofoPaimean *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoPaimean *paimean;
	GTimeVal stamp;

	paimean = ofo_paimean_new( parms->getter );

	/* paimean code */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty mean of paiement identifier" ));
		parms->parse_errs += 1;
		g_object_unref( paimean );
		return( NULL );
	}
	ofo_paimean_set_code( paimean, cstr );

	/* creation user */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		paimean_set_cre_user( paimean, cstr );
	}

	/* creation timestamp */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		paimean_set_cre_stamp( paimean, my_stamp_set_from_sql( &stamp, cstr ));
	}

	/* paimean label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_paimean_set_label( paimean, cstr );
	}

	/* paimean account */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_paimean_set_account( paimean, cstr );
	}

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_paimean_set_notes( paimean, splitted );
	g_free( splitted );

	return( paimean );
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
	const gchar *code;
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoPaimean *paimean;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		paimean_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		paimean = OFO_PAIMEAN( it->data );

		if( paimean_get_exists( paimean, connect )){
			parms->duplicate_count += 1;
			code = ofo_paimean_get_code( paimean );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate mean of paiement, replacing previous one" ), code );
					paimean_do_delete( paimean, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate mean of paiement, ignored (skipped)" ), code );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate mean of paiement" ), code );
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
			if( paimean_do_insert( paimean, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
paimean_get_exists( const ofoPaimean *paimean, const ofaIDBConnect *connect )
{
	const gchar *paimean_id;
	gint count;
	gchar *str;

	count = 0;
	paimean_id = ofo_paimean_get_code( paimean );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_PAIMEANS WHERE PAM_COUNT='%s'", paimean_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
paimean_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_PAIMEANS", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_paimean_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_paimean_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
