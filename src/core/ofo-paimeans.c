/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "my/my-utils.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignal-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-paimeans.h"

/* priv instance data
 */
enum {
	PAM_CODE = 1,
	PAM_LABEL,
	PAM_MUST_ALONE,
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
		{ OFA_BOX_CSV( PAM_MUST_ALONE ),
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
	ofoPaimeansPrivate;

static ofoPaimeans *paimeans_find_by_code( GList *set, const gchar *code );
static void         paimeans_set_upd_user( ofoPaimeans *paimeans, const gchar *user );
static void         paimeans_set_upd_stamp( ofoPaimeans *paimeans, const GTimeVal *stamp );
static gboolean     paimeans_do_insert( ofoPaimeans *paimeans, const ofaIDBConnect *connect );
static gboolean     paimeans_insert_main( ofoPaimeans *paimeans, const ofaIDBConnect *connect );
static gboolean     paimeans_do_update( ofoPaimeans *paimeans, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean     paimeans_update_main( ofoPaimeans *paimeans, const gchar *prev_code, const ofaIDBConnect *connect );
static gboolean     paimeans_do_delete( ofoPaimeans *paimeans, const ofaIDBConnect *connect );
static gint         paimeans_cmp_by_code( const ofoPaimeans *a, const gchar *code );
static void         icollectionable_iface_init( myICollectionableInterface *iface );
static guint        icollectionable_get_interface_version( void );
static GList       *icollectionable_load_collection( void *user_data );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( void );
static gchar       *iexportable_get_label( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( void );
static gchar       *iimportable_get_label( const ofaIImportable *instance );
static guint        iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList       *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoPaimeans *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static void         iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean     paimeans_get_exists( const ofoPaimeans *paimeans, const ofaIDBConnect *connect );
static gboolean     paimeans_drop_content( const ofaIDBConnect *connect );
static void         isignal_hub_iface_init( ofaISignalHubInterface *iface );
static void         isignal_hub_connect( ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoPaimeans, ofo_paimeans, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoPaimeans )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNAL_HUB, isignal_hub_iface_init ))

static void
paimeans_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_paimeans_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, PAM_CODE ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, PAM_LABEL ));

	g_return_if_fail( instance && OFO_IS_PAIMEANS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_paimeans_parent_class )->finalize( instance );
}

static void
paimeans_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_PAIMEANS( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_paimeans_parent_class )->dispose( instance );
}

static void
ofo_paimeans_init( ofoPaimeans *self )
{
	static const gchar *thisfn = "ofo_paimeans_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_paimeans_class_init( ofoPaimeansClass *klass )
{
	static const gchar *thisfn = "ofo_paimeans_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimeans_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimeans_finalize;
}

/**
 * ofo_paimeans_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoPaimeans dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_paimeans_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( my_icollector_collection_get( ofa_hub_get_collector( hub ), OFO_TYPE_PAIMEANS, hub ));
}

/**
 * ofo_paimeans_get_by_code:
 *
 * Returns: the searched paimeans, or %NULL.
 *
 * The returned object is owned by the #ofoPaimeans class, and should
 * not be unreffed by the caller.
 */
ofoPaimeans *
ofo_paimeans_get_by_code( ofaHub *hub, const gchar *code )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( code ), NULL );

	dataset = ofo_paimeans_get_dataset( hub );

	return( paimeans_find_by_code( dataset, code ));
}

static ofoPaimeans *
paimeans_find_by_code( GList *set, const gchar *code )
{
	GList *found;

	found = g_list_find_custom(
				set, code, ( GCompareFunc ) paimeans_cmp_by_code );
	if( found ){
		return( OFO_PAIMEANS( found->data ));
	}

	return( NULL );
}

/**
 * ofo_paimeans_new:
 */
ofoPaimeans *
ofo_paimeans_new( void )
{
	ofoPaimeans *paimeans;

	paimeans = g_object_new( OFO_TYPE_PAIMEANS, NULL );
	OFO_BASE( paimeans )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( paimeans );
}

/**
 * ofo_paimeans_get_code:
 */
const gchar *
ofo_paimeans_get_code( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, string, NULL, PAM_CODE );
}

/**
 * ofo_paimeans_get_label:
 */
const gchar *
ofo_paimeans_get_label( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, string, NULL, PAM_LABEL );
}

/**
 * ofo_paimeans_get_must_alone:
 */
gboolean
ofo_paimeans_get_must_alone( const ofoPaimeans *paimeans )
{
	const gchar *cstr;

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimeans )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( paimeans )->prot->fields, PAM_MUST_ALONE );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_paimeans_get_account:
 */
const gchar *
ofo_paimeans_get_account( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, string, NULL, PAM_ACCOUNT );
}

/**
 * ofo_paimeans_get_notes:
 */
const gchar *
ofo_paimeans_get_notes( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, string, NULL, PAM_NOTES );
}

/**
 * ofo_paimeans_get_upd_user:
 */
const gchar *
ofo_paimeans_get_upd_user( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, string, NULL, PAM_UPD_USER );
}

/**
 * ofo_paimeans_get_upd_stamp:
 */
const GTimeVal *
ofo_paimeans_get_upd_stamp( const ofoPaimeans *paimeans )
{
	ofo_base_getter( PAIMEANS, paimeans, timestamp, NULL, PAM_UPD_STAMP );
}

/**
 * ofo_paimeans_is_deletable:
 * @paimeans: the paimeans
 *
 * There is no hard reference set to this #ofoPaimeans class.
 * Entries and ope.templates which reference one of these means of
 * paiement will continue to just work, just losing the benefit of
 * account pre-setting.
 *
 * Returns: %TRUE if the paimeans is deletable.
 */
gboolean
ofo_paimeans_is_deletable( const ofoPaimeans *paimeans )
{
	return( TRUE );
}

/**
 * ofo_paimeans_is_valid_data:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate code or so.
 */
gboolean
ofo_paimeans_is_valid_data( const gchar *code, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !my_strlen( code )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty identifier" ));
		}
		return( FALSE );
	}
	return( TRUE );
}

/**
 * ofo_paimeans_set_code:
 */
void
ofo_paimeans_set_code( ofoPaimeans *paimeans, const gchar *code )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_CODE, code );
}

/**
 * ofo_paimeans_set_label:
 */
void
ofo_paimeans_set_label( ofoPaimeans *paimeans, const gchar *label )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_LABEL, label );
}

/**
 * ofo_paimeans_set_must_alone:
 */
void
ofo_paimeans_set_must_alone( ofoPaimeans *paimeans, gboolean alone )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_MUST_ALONE, alone ? "Y":"N" );
}

/**
 * ofo_paimeans_set_account:
 */
void
ofo_paimeans_set_account( ofoPaimeans *paimeans, const gchar *account )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_ACCOUNT, account );
}

/**
 * ofo_paimeans_set_notes:
 */
void
ofo_paimeans_set_notes( ofoPaimeans *paimeans, const gchar *notes )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_NOTES, notes );
}

/*
 * ofo_paimeans_set_upd_user:
 */
static void
paimeans_set_upd_user( ofoPaimeans *paimeans, const gchar *upd_user )
{
	ofo_base_setter( PAIMEANS, paimeans, string, PAM_UPD_USER, upd_user );
}

/*
 * ofo_paimeans_set_upd_stamp:
 */
static void
paimeans_set_upd_stamp( ofoPaimeans *paimeans, const GTimeVal *upd_stamp )
{
	ofo_base_setter( PAIMEANS, paimeans, timestamp, PAM_UPD_STAMP, upd_stamp );
}

/**
 * ofo_paimeans_insert:
 *
 * First creation of a new #ofoPaimeans.
 */
gboolean
ofo_paimeans_insert( ofoPaimeans *paimeans, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_paimeans_insert";
	gboolean ok;

	g_debug( "%s: paimeans=%p, hub=%p",
			thisfn, ( void * ) paimeans, ( void * ) hub );

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimeans )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( paimeans_do_insert( paimeans, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( paimeans ), hub );
		my_icollector_collection_add_object(
				ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( paimeans ), NULL, hub );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, paimeans );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimeans_do_insert( ofoPaimeans *paimeans, const ofaIDBConnect *connect )
{
	return( paimeans_insert_main( paimeans, connect ));
}

static gboolean
paimeans_insert_main( ofoPaimeans *paimeans, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_paimeans_get_label( paimeans ));
	notes = my_utils_quote_sql( ofo_paimeans_get_notes( paimeans ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_PAIMEANS" );

	g_string_append_printf( query,
			"	(PAM_CODE,PAM_LABEL,PAM_MUST_ALONE,PAM_ACCOUNT,"
			"	PAM_NOTES,PAM_UPD_USER, PAM_UPD_STAMP)"
			"	VALUES ('%s','%s','%s','%s',",
			ofo_paimeans_get_code( paimeans ),
			label,
			ofo_paimeans_get_must_alone( paimeans ) ? "Y":"N",
			ofo_paimeans_get_account( paimeans ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		paimeans_set_upd_user( paimeans, userid );
		paimeans_set_upd_stamp( paimeans, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_paimeans_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_paimeans_update( ofoPaimeans *paimeans, const gchar *prev_code )
{
	static const gchar *thisfn = "ofo_paimeans_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimeans=%p, prev_code=%s",
			thisfn, ( void * ) paimeans, prev_code );

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( my_strlen( prev_code ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimeans )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( paimeans ));
	ok = FALSE;

	if( paimeans_do_update( paimeans, prev_code, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, paimeans, prev_code );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimeans_do_update( ofoPaimeans *paimeans, const gchar *prev_code, const ofaIDBConnect *connect )
{
	return( paimeans_update_main( paimeans, prev_code, connect ));
}

static gboolean
paimeans_update_main( ofoPaimeans *paimeans, const gchar *prev_code, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_paimeans_get_label( paimeans ));
	notes = my_utils_quote_sql( ofo_paimeans_get_notes( paimeans ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_PAIMEANS SET " );

	g_string_append_printf( query, "PAM_CODE='%s',", ofo_paimeans_get_code( paimeans ));
	g_string_append_printf( query, "PAM_LABEL='%s',", label );
	g_string_append_printf( query, "PAM_MUST_ALONE='%s',", ofo_paimeans_get_must_alone( paimeans ) ? "Y":"N" );
	g_string_append_printf( query, "PAM_ACCOUNT='%s',", ofo_paimeans_get_account( paimeans ));

	if( my_strlen( notes )){
		g_string_append_printf( query, "PAM_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "PAM_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	PAM_UPD_USER='%s',PAM_UPD_STAMP='%s'"
			"	WHERE PAM_MNEMO='%s'",
					userid, stamp_str, prev_code );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){

		paimeans_set_upd_user( paimeans, userid );
		paimeans_set_upd_stamp( paimeans, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_paimeans_delete:
 */
gboolean
ofo_paimeans_delete( ofoPaimeans *paimeans )
{
	static const gchar *thisfn = "ofo_paimeans_delete";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: paimeans=%p", thisfn, ( void * ) paimeans );

	g_return_val_if_fail( paimeans && OFO_IS_PAIMEANS( paimeans ), FALSE );
	g_return_val_if_fail( !OFO_BASE( paimeans )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( paimeans ));
	ok = FALSE;

	if( paimeans_do_delete( paimeans, ofa_hub_get_connect( hub ))){
		g_object_ref( paimeans );
		my_icollector_collection_remove_object( ofa_hub_get_collector( hub ), MY_ICOLLECTIONABLE( paimeans ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, paimeans );
		g_object_unref( paimeans );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
paimeans_do_delete( ofoPaimeans *paimeans, const ofaIDBConnect *connect )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_PAIMEANS WHERE PAM_CODE='%s'",
					ofo_paimeans_get_code( paimeans ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
paimeans_cmp_by_code( const ofoPaimeans *a, const gchar *code )
{
	return( my_collate( ofo_paimeans_get_code( a ), code ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_paimeans_icollectionable_iface_init";

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
					"OFA_T_PAIMEANS",
					OFO_TYPE_PAIMEANS,
					OFA_HUB( user_data ));

	return( dataset );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_paimeans_iexportable_iface_init";

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
iexportable_export( ofaIExportable *exportable, ofaStreamFormat *settings, ofaHub *hub )
{
	GList *dataset, *it;
	gchar *str;
	gboolean ok, with_headers;
	gulong count;

	dataset = ofo_paimeans_get_dataset( hub );
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
	static const gchar *thisfn = "ofo_paimeans_iimportable_iface_init";

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
 * ofo_paimeans_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - paimeans code
 * - label
 * - must_alone
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

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		bck_table = ofa_idbconnect_table_backup( ofa_hub_get_connect( parms->hub ), "OFA_T_PAIMEANS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_hub_get_collector( parms->hub ), OFO_TYPE_PAIMEANS );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_PAIMEANS );

		} else {
			ofa_idbconnect_table_restore( ofa_hub_get_connect( parms->hub ), bck_table, "OFA_T_PAIMEANS" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_paimeans_free_dataset( dataset );
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
	ofoPaimeans *paimeans;

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
		paimeans = iimportable_import_parse_main( importer, parms, numline, fields );

		if( paimeans ){
			dataset = g_list_prepend( dataset, paimeans );
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

static ofoPaimeans *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;
	ofoPaimeans *paimeans;

	paimeans = ofo_paimeans_new();

	/* paimeans code */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !my_strlen( cstr )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty mean of paiement identifier" ));
		parms->parse_errs += 1;
		g_object_unref( paimeans );
		return( NULL );
	}
	ofo_paimeans_set_code( paimeans, cstr );

	/* paimeans label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_paimeans_set_label( paimeans, cstr );
	}

	/* whether must be alone */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	ofo_paimeans_set_must_alone( paimeans, my_utils_boolean_from_str( cstr ));

	/* paimeans account */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_paimeans_set_account( paimeans, cstr );
	}

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_paimeans_set_notes( paimeans, splitted );
	g_free( splitted );

	return( paimeans );
}

/*
 * insert records
 */
static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	const gchar *code;
	gboolean insert;
	guint total;
	gchar *str;
	ofoPaimeans *paimeans;

	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		paimeans_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		paimeans = OFO_PAIMEANS( it->data );

		if( paimeans_get_exists( paimeans, connect )){
			parms->duplicate_count += 1;
			code = ofo_paimeans_get_code( paimeans );

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate mean of paiement, replacing previous one" ), code );
					paimeans_do_delete( paimeans, connect );
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
			if( paimeans_do_insert( paimeans, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
paimeans_get_exists( const ofoPaimeans *paimeans, const ofaIDBConnect *connect )
{
	const gchar *paimeans_id;
	gint count;
	gchar *str;

	count = 0;
	paimeans_id = ofo_paimeans_get_code( paimeans );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_PAIMEANS WHERE PAM_COUNT='%s'", paimeans_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
paimeans_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_PAIMEANS", TRUE ));
}

/*
 * ofaISignalHub interface management
 */
static void
isignal_hub_iface_init( ofaISignalHubInterface *iface )
{
	static const gchar *thisfn = "ofo_paimeans_isignal_hub_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect = isignal_hub_connect;
}

static void
isignal_hub_connect( ofaHub *hub )
{
	static const gchar *thisfn = "ofo_paimeans_isignal_hub_connect";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}
