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
#include "api/ofa-iexportable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-paimean.h"

/* priv instance data
 */
enum {
	PAM_CODE = 1,
	PAM_LABEL,
	PAM_ACCOUNT,
	PAM_NOTES,
	PAM_UPD_USER,
	PAM_UPD_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( PAM_CODE ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
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

typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofoPaimeanPrivate;

static ofoPaimean *paimean_find_by_code( GList *set, const gchar *code );
static void        paimean_set_upd_user( ofoPaimean *paimean, const gchar *user );
static void        paimean_set_upd_stamp( ofoPaimean *paimean, const GTimeVal *stamp );
static gboolean    paimean_do_insert( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_insert_main( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_do_update( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean    paimean_update_main( ofoPaimean *paimean, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean    paimean_do_delete( ofoPaimean *paimean, const ofaIDBConnect *connect );
static gint        paimean_cmp_by_code( const ofoPaimean *a, const gchar *code );
static void        icollectionable_iface_init( myICollectionableInterface *iface );
static guint       icollectionable_get_interface_version( void );
static GList      *icollectionable_load_collection( void *user_data );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( void );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaIGetter *getter );
static void        iimportable_iface_init( ofaIImportableInterface *iface );
static guint       iimportable_get_interface_version( void );
static gchar      *iimportable_get_label( const ofaIImportable *instance );
static guint       iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList      *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoPaimean *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    paimean_get_exists( const ofoPaimean *paimean, const ofaIDBConnect *connect );
static gboolean    paimean_drop_content( const ofaIDBConnect *connect );
static void        isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void        isignal_hub_connect( ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoPaimean, ofo_paimean, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoPaimean )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
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
 * ofo_paimean_set_upd_user:
 */
static void
paimean_set_upd_user( ofoPaimean *paimean, const gchar *upd_user )
{
	ofo_base_setter( PAIMEAN, paimean, string, PAM_UPD_USER, upd_user );
}

/*
 * ofo_paimean_set_upd_stamp:
 */
static void
paimean_set_upd_stamp( ofoPaimean *paimean, const GTimeVal *upd_stamp )
{
	ofo_base_setter( PAIMEAN, paimean, timestamp, PAM_UPD_STAMP, upd_stamp );
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
	ofaHub *hub;

	g_debug( "%s: paimean=%p", thisfn, ( void * ) paimean );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	hub = ofa_igetter_get_hub( getter );

	if( paimean_do_insert( paimean, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( paimean ), NULL, getter );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, paimean );
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
			"	(PAM_CODE,PAM_LABEL,PAM_ACCOUNT,"
			"	PAM_NOTES,PAM_UPD_USER, PAM_UPD_STAMP)"
			"	VALUES ('%s','%s','%s',",
			ofo_paimean_get_code( paimean ),
			label,
			ofo_paimean_get_account( paimean ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

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
 * ofo_paimean_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_paimean_update( ofoPaimean *paimean, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_paimean_update";
	ofaIGetter *getter;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimean=%p, prev_code=%s",
			thisfn, ( void * ) paimean, prev_code );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( my_strlen( prev_code ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	hub = ofa_igetter_get_hub( getter );

	if( paimean_do_update( paimean, prev_code, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, paimean, prev_code );
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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimean=%p", thisfn, ( void * ) paimean );

	g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimean )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( paimean ));
	hub = ofa_igetter_get_hub( getter );

	if( paimean_do_delete( paimean, ofa_hub_get_connect( hub ))){
		g_object_ref( paimean );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( paimean ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, paimean );
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
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_paimean_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _means of paiement" )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaIGetter *getter )
{
	GList *dataset, *it;
	gchar *str;
	gboolean ok, with_headers;
	gulong count;

	dataset = ofo_paimean_get_dataset( getter );
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
	GList *dataset;
	gchar *bck_table;
	ofaHub *hub;

	dataset = iimportable_import_parse( importer, parms, lines );
	hub = ofa_igetter_get_hub( parms->getter );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( hub ), "OFA_T_PAIMEANS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_PAIMEAN );
			g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_PAIMEAN );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( hub ), bck_table, "OFA_T_PAIMEANS" );
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
	guint total;
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
					insert = FALSE;
					total -= 1;
					parms->insert_errs += 1;
					break;
			}

			ofa_iimporter_progress_text( importer, parms, str );
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
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_paimean_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_paimean_isignal_hub_connect";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}
