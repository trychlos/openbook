/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "api/ofo-counters.h"
#include "api/ofo-tiers.h"

/* priv instance data
 */
enum {
	TRS_ID = 1,
	TRS_CRE_USER,
	TRS_CRE_STAMP,
	TRS_LABEL,
	TRS_NOTES,
	TRS_UPD_USER,
	TRS_UPD_STAMP,
	TRS_DOC_ID,
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
		{ OFA_BOX_CSV( TRS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TRS_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TRS_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOX_CSV( TRS_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TRS_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TRS_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TRS_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( TRS_ID ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TRS_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define TIERS_TABLES_COUNT            2
#define TIERS_EXPORT_VERSION          1

typedef struct {
	GList *docs;
}
	ofoTiersPrivate;

static ofoTiers   *tiers_find_by_id( GList *set, ofxCounter id );
static void        tiers_set_id( ofoTiers *tiers, ofxCounter id );
static void        tiers_set_cre_user( ofoTiers *tiers, const gchar *user );
static void        tiers_set_cre_stamp( ofoTiers *tiers, const GTimeVal *stamp );
static void        tiers_set_upd_user( ofoTiers *tiers, const gchar *user );
static void        tiers_set_upd_stamp( ofoTiers *tiers, const GTimeVal *stamp );
static GList      *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean    tiers_do_insert( ofoTiers *tiers, const ofaIDBConnect *connect );
static gboolean    tiers_insert_main( ofoTiers *tiers, const ofaIDBConnect *connect );
static gboolean    tiers_do_update( ofoTiers *tiers, const ofaIDBConnect *connect );
static gboolean    tiers_update_main( ofoTiers *tiers, const ofaIDBConnect *connect );
static gboolean    tiers_do_delete( ofoTiers *tiers, const ofaIDBConnect *connect );
static gint        tiers_cmp_by_id( const ofoTiers *a, ofxCounter *code );
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
static ofoTiers   *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    tiers_get_exists( const ofoTiers *tiers, const ofaIDBConnect *connect );
static gboolean    tiers_drop_content( const ofaIDBConnect *connect );
static void        isignalable_iface_init( ofaISignalableInterface *iface );
static void        isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoTiers, ofo_tiers, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoTiers )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
tiers_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_tiers_finalize";

	g_debug( "%s: instance=%p (%s): %lu - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_counter( OFO_BASE( instance )->prot->fields, TRS_ID ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, TRS_LABEL ));

	g_return_if_fail( instance && OFO_IS_TIERS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tiers_parent_class )->finalize( instance );
}

static void
tiers_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_TIERS( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tiers_parent_class )->dispose( instance );
}

static void
ofo_tiers_init( ofoTiers *self )
{
	static const gchar *thisfn = "ofo_tiers_init";
	ofoTiersPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_tiers_get_instance_private( self );

	priv->docs = NULL;
}

static void
ofo_tiers_class_init( ofoTiersClass *klass )
{
	static const gchar *thisfn = "ofo_tiers_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tiers_dispose;
	G_OBJECT_CLASS( klass )->finalize = tiers_finalize;
}

/**
 * ofo_tiers_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoTiers dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_tiers_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_TIERS, getter ));
}

/**
 * ofo_tiers_get_by_id:
 * @getter: a #ofaIGetter instance.
 * @id: the searched identifier.
 *
 * Returns: the searched tiers, or %NULL.
 *
 * The returned object is owned by the #ofoTiers class, and should
 * not be unreffed by the caller.
 */
ofoTiers *
ofo_tiers_get_by_id( ofaIGetter *getter, ofxCounter id )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dataset = ofo_tiers_get_dataset( getter );

	return( tiers_find_by_id( dataset, id ));
}

static ofoTiers *
tiers_find_by_id( GList *set, ofxCounter id )
{
	GList *found;

	found = g_list_find_custom(
				set, &id, ( GCompareFunc ) tiers_cmp_by_id );
	if( found ){
		return( OFO_TIERS( found->data ));
	}

	return( NULL );
}

/**
 * ofo_tiers_new:
 * @getter: a #ofaIGetter instance.
 */
ofoTiers *
ofo_tiers_new( ofaIGetter *getter )
{
	ofoTiers *tiers;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	tiers = g_object_new( OFO_TYPE_TIERS, "ofo-base-getter", getter, NULL );
	OFO_BASE( tiers )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( tiers );
}

/**
 * ofo_tiers_get_id:
 */
ofxCounter
ofo_tiers_get_id( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, counter, 0, TRS_ID );
}

/**
 * ofo_tiers_get_cre_user:
 */
const gchar *
ofo_tiers_get_cre_user( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, string, NULL, TRS_CRE_USER );
}

/**
 * ofo_tiers_get_cre_stamp:
 */
const GTimeVal *
ofo_tiers_get_cre_stamp( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, timestamp, NULL, TRS_CRE_STAMP );
}

/**
 * ofo_tiers_get_label:
 */
const gchar *
ofo_tiers_get_label( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, string, NULL, TRS_LABEL );
}

/**
 * ofo_tiers_get_notes:
 */
const gchar *
ofo_tiers_get_notes( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, string, NULL, TRS_NOTES );
}

/**
 * ofo_tiers_get_upd_user:
 */
const gchar *
ofo_tiers_get_upd_user( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, string, NULL, TRS_UPD_USER );
}

/**
 * ofo_tiers_get_upd_stamp:
 */
const GTimeVal *
ofo_tiers_get_upd_stamp( const ofoTiers *tiers )
{
	ofo_base_getter( TIERS, tiers, timestamp, NULL, TRS_UPD_STAMP );
}

/**
 * ofo_tiers_is_deletable:
 * @tiers: the tiers
 *
 * There is no hard reference set to this #ofoTiers class.
 * Entries and ope.templates which reference one of these means of
 * paiement will continue to just work, just losing the benefit of
 * account pre-setting.
 *
 * Returns: %TRUE if the tiers is deletable.
 */
gboolean
ofo_tiers_is_deletable( const ofoTiers *tiers )
{
	return( TRUE );
}

/**
 * ofo_tiers_is_valid_data:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate code or so.
 */
gboolean
ofo_tiers_is_valid_data( const gchar *label, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Label is empty" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/*
 * tiers_set_id:
 */
static void
tiers_set_id( ofoTiers *tiers, ofxCounter id )
{
	ofo_base_setter( TIERS, tiers, counter, TRS_ID, id );
}

/*
 * tiers_set_cre_user:
 */
static void
tiers_set_cre_user( ofoTiers *tiers, const gchar *user )
{
	ofo_base_setter( TIERS, tiers, string, TRS_CRE_USER, user );
}

/*
 * tiers_set_cre_stamp:
 */
static void
tiers_set_cre_stamp( ofoTiers *tiers, const GTimeVal *stamp )
{
	ofo_base_setter( TIERS, tiers, timestamp, TRS_CRE_STAMP, stamp );
}

/**
 * ofo_tiers_set_label:
 */
void
ofo_tiers_set_label( ofoTiers *tiers, const gchar *label )
{
	ofo_base_setter( TIERS, tiers, string, TRS_LABEL, label );
}

/**
 * ofo_tiers_set_notes:
 */
void
ofo_tiers_set_notes( ofoTiers *tiers, const gchar *notes )
{
	ofo_base_setter( TIERS, tiers, string, TRS_NOTES, notes );
}

/*
 * tiers_set_upd_user:
 */
static void
tiers_set_upd_user( ofoTiers *tiers, const gchar *user )
{
	ofo_base_setter( TIERS, tiers, string, TRS_UPD_USER, user );
}

/*
 * tiers_set_upd_stamp:
 */
static void
tiers_set_upd_stamp( ofoTiers *tiers, const GTimeVal *stamp )
{
	ofo_base_setter( TIERS, tiers, timestamp, TRS_UPD_STAMP, stamp );
}

/**
 * ofo_tiers_doc_get_count:
 * @tiers: this #ofoTiers object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_tiers_doc_get_count( ofoTiers *tiers )
{
	ofoTiersPrivate *priv;

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), 0 );
	g_return_val_if_fail( !OFO_BASE( tiers )->prot->dispose_has_run, 0 );

	priv = ofo_tiers_get_instance_private( tiers );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_tiers_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown means of paiement identifiers in
 * OFA_T_TIERS_DOC child table.
 *
 * The returned list should be ofo_tiers_free_orphans() by the caller.
 */
GList *
ofo_tiers_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_TIERS_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(TRS_ID) FROM %s "
			"	WHERE TRS_ID NOT IN (SELECT TRS_ID FROM OFA_T_TIERS)", table );

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
 * ofo_tiers_insert:
 *
 * First creation of a new #ofoTiers.
 */
gboolean
ofo_tiers_insert( ofoTiers *tiers )
{
	static const gchar *thisfn = "ofo_tiers_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: tiers=%p", thisfn, ( void * ) tiers );

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tiers )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tiers ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_tiers_get_dataset( getter );

	if( tiers_do_insert( tiers, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( tiers ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, tiers );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
tiers_do_insert( ofoTiers *tiers, const ofaIDBConnect *connect )
{
	return( tiers_insert_main( tiers, connect ));
}

static gboolean
tiers_insert_main( ofoTiers *tiers, const ofaIDBConnect *connect )
{
	ofaIGetter *getter;
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;
	ofxCounter id;

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tiers_get_label( tiers ));
	notes = my_utils_quote_sql( ofo_tiers_get_notes( tiers ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	getter = ofo_base_get_getter( OFO_BASE( tiers ));
	id = ofo_counters_get_next_tiers_id( getter );

	query = g_string_new( "INSERT INTO OFA_T_TIERS " );

	g_string_append_printf( query,
			"	(TRS_ID,TRS_CRE_USER, TRS_CRE_STAMP,TRS_LABEL,TRS_NOTES)"
			"	VALUES (%lu,'%s','%s','%s',",
			id,
			userid,
			stamp_str,
			label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		tiers_set_id( tiers, id );
		tiers_set_cre_user( tiers, userid );
		tiers_set_cre_stamp( tiers, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_tiers_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_tiers_update( ofoTiers *tiers )
{
	static const gchar *thisfn = "ofo_tiers_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: tiers=%p", thisfn, ( void * ) tiers );

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tiers )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tiers ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( tiers_do_update( tiers, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, tiers, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
tiers_do_update( ofoTiers *tiers, const ofaIDBConnect *connect )
{
	return( tiers_update_main( tiers, connect ));
}

static gboolean
tiers_update_main( ofoTiers *tiers, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_tiers_get_label( tiers ));
	notes = my_utils_quote_sql( ofo_tiers_get_notes( tiers ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_TIERS SET " );

	g_string_append_printf( query, "TRS_LABEL='%s',", label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "TRS_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TRS_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	TRS_UPD_USER='%s',TRS_UPD_STAMP='%s'"
			"	WHERE TRS_ID=%lu",
					userid,
					stamp_str,
					ofo_tiers_get_id( tiers ));

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		tiers_set_upd_user( tiers, userid );
		tiers_set_upd_stamp( tiers, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_tiers_delete:
 */
gboolean
ofo_tiers_delete( ofoTiers *tiers )
{
	static const gchar *thisfn = "ofo_tiers_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: tiers=%p", thisfn, ( void * ) tiers );

	g_return_val_if_fail( tiers && OFO_IS_TIERS( tiers ), FALSE );
	g_return_val_if_fail( !OFO_BASE( tiers )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( tiers ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( tiers_do_delete( tiers, ofa_hub_get_connect( hub ))){
		g_object_ref( tiers );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( tiers ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, tiers );
		g_object_unref( tiers );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
tiers_do_delete( ofoTiers *tiers, const ofaIDBConnect *connect )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TIERS WHERE TRS_ID=%lu",
					ofo_tiers_get_id( tiers ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
tiers_cmp_by_id( const ofoTiers *a, ofxCounter *id )
{
	ofxCounter ca, cb;

	ca = ofo_tiers_get_id( a );
	cb = *id;

	return( ca < cb ? -1 : ( ca > cb ? 1 : 0 ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_tiers_icollectionable_iface_init";

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
					"OFA_T_TIERS",
					OFO_TYPE_TIERS,
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
	static const gchar *thisfn = "ofo_tiers_iexportable_iface_init";

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
	static const gchar *thisfn = "ofo_tiers_iexportable_export";

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
	ofoTiers *tiers;
	ofoTiersPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_tiers_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += TIERS_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		tiers = OFO_TIERS( it->data );
		count += ofo_tiers_doc_get_count( tiers );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, TIERS_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					TIERS_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		tiers = OFO_TIERS( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( tiers )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_tiers_get_instance_private( tiers );

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
	static const gchar *thisfn = "ofo_tiers_iimportable_iface_init";

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
 * ofo_tiers_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - tiers id (not imported)
 * - label
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_TIERS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_TIERS );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_TIERS );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_TIERS" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_tiers_free_dataset( dataset );
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
	ofoTiers *tiers;

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
		tiers = iimportable_import_parse_main( importer, parms, numline, fields );

		if( tiers ){
			dataset = g_list_prepend( dataset, tiers );
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

static ofoTiers *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoTiers *tiers;
	GTimeVal stamp;

	tiers = ofo_tiers_new( parms->getter );

	/* tiers id (ignored) */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty identifier" ));
		parms->parse_errs += 1;
		g_object_unref( tiers );
		return( NULL );
	}

	/* creation user */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		tiers_set_cre_user( tiers, cstr );
	}

	/* creation timestamp */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		tiers_set_cre_stamp( tiers, my_stamp_set_from_sql( &stamp, cstr ));
	}

	/* tiers label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_tiers_set_label( tiers, cstr );
	}

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_tiers_set_notes( tiers, splitted );
	g_free( splitted );

	return( tiers );
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
	ofxCounter id;
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoTiers *tiers;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		tiers_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		tiers = OFO_TIERS( it->data );

		if( tiers_get_exists( tiers, connect )){
			parms->duplicate_count += 1;
			id = ofo_tiers_get_id( tiers );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%lu: duplicate tiers, replacing previous one" ), id );
					tiers_do_delete( tiers, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%lu: duplicate tiers, ignored (skipped)" ), id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%lu: erroneous duplicate tiers" ), id );
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
			if( tiers_do_insert( tiers, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
tiers_get_exists( const ofoTiers *tiers, const ofaIDBConnect *connect )
{
	return( FALSE );
}

static gboolean
tiers_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_TIERS", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_tiers_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_tiers_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
