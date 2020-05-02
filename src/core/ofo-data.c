/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-data.h"

/* priv instance data
 */
enum {
	DAT_KEY = 1,
	DAT_CRE_USER,
	DAT_CRE_STAMP,
	DAT_CONTENT,
	DAT_NOTES,
	DAT_UPD_USER,
	DAT_UPD_STAMP
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
		{ OFA_BOX_CSV( DAT_KEY ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( DAT_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DAT_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DAT_CONTENT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DAT_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DAT_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DAT_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ 0 }
};

#define DATA_TABLES_COUNT             1
#define DATA_EXPORT_VERSION           1

typedef struct {
	void *empty;							/* so that gcc -pedantic is happy */
}
	ofoDataPrivate;

static ofoData *data_find_by_key( GList *set, const gchar *key );
static void     data_set_cre_user( ofoData *data, const gchar *user );
static void     data_set_cre_stamp( ofoData *data, const myStampVal *stamp );
static void     data_set_upd_user( ofoData *data, const gchar *user );
static void     data_set_upd_stamp( ofoData *data, const myStampVal *stamp );
static gboolean data_do_insert( ofoData *data, const ofaIDBConnect *connect );
static gboolean data_insert_main( ofoData *data, const ofaIDBConnect *connect );
static gboolean data_do_update( ofoData *data, const gchar *prev_key, const ofaIDBConnect *connect );
static gboolean data_do_delete( ofoData *data, const ofaIDBConnect *connect );
static gint     data_cmp_by_key( const ofoData *a, const gchar *key );
static void     icollectionable_iface_init( myICollectionableInterface *iface );
static guint    icollectionable_get_interface_version( void );
static GList   *icollectionable_load_collection( void *user_data );
static void     iexportable_iface_init( ofaIExportableInterface *iface );
static guint    iexportable_get_interface_version( void );
static gchar   *iexportable_get_label( const ofaIExportable *instance );
static gboolean iexportable_get_published( const ofaIExportable *instance );
static gboolean iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean iexportable_export_default( ofaIExportable *exportable );
static void     iimportable_iface_init( ofaIImportableInterface *iface );
static guint    iimportable_get_interface_version( void );
static gchar   *iimportable_get_label( const ofaIImportable *instance );
static guint    iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList   *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void     iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean data_get_exists( const ofoData *data, const ofaIDBConnect *connect );
static gboolean data_drop_content( const ofaIDBConnect *connect );
static void     isignalable_iface_init( ofaISignalableInterface *iface );
static void     isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoData, ofo_data, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoData )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
data_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_data_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, DAT_KEY ));

	g_return_if_fail( instance && OFO_IS_DATA( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_data_parent_class )->finalize( instance );
}

static void
data_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_DATA( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_data_parent_class )->dispose( instance );
}

static void
ofo_data_init( ofoData *self )
{
	static const gchar *thisfn = "ofo_data_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_data_class_init( ofoDataClass *klass )
{
	static const gchar *thisfn = "ofo_data_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = data_dispose;
	G_OBJECT_CLASS( klass )->finalize = data_finalize;
}

/**
 * ofo_data_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoData dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_data_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;
	GList *collection;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	collection = my_icollector_collection_get( collector, OFO_TYPE_DATA, getter );

	return( collection );
}

/**
 * ofo_data_get_by_key:
 * @getter: a #ofaIGetter instance.
 * @key: the searched key.
 *
 * Returns: the searched data, or %NULL.
 *
 * The returned object is owned by the #ofoData class, and should
 * not be unreffed by the caller.
 */
ofoData *
ofo_data_get_by_key( ofaIGetter *getter, const gchar *key )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	dataset = ofo_data_get_dataset( getter );

	return( data_find_by_key( dataset, key ));
}

static ofoData *
data_find_by_key( GList *set, const gchar *key )
{
	GList *found;

	found = g_list_find_custom( set, key, ( GCompareFunc ) data_cmp_by_key );

	if( found ){
		return( OFO_DATA( found->data ));
	}

	return( NULL );
}

/**
 * ofo_data_new:
 * @getter: a #ofaIGetter instance.
 */
ofoData *
ofo_data_new( ofaIGetter *getter )
{
	ofoData *data;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	data = g_object_new( OFO_TYPE_DATA, "ofo-base-getter", getter, NULL );
	OFO_BASE( data )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( data );
}

/**
 * ofo_data_get_cre_user:
 */
const gchar *
ofo_data_get_cre_user( const ofoData *data )
{
	ofo_base_getter( DATA, data, string, NULL, DAT_CRE_USER );
}

/**
 * ofo_data_get_cre_stamp:
 */
const myStampVal *
ofo_data_get_cre_stamp( const ofoData *data )
{
	ofo_base_getter( DATA, data, timestamp, NULL, DAT_CRE_STAMP );
}

/**
 * ofo_data_get_key:
 */
const gchar *
ofo_data_get_key( const ofoData *data )
{
	ofo_base_getter( DATA, data, string, NULL, DAT_KEY );
}

/**
 * ofo_data_get_content:
 */
const gchar *
ofo_data_get_content( const ofoData *data )
{
	ofo_base_getter( DATA, data, string, NULL, DAT_CONTENT );
}

/**
 * ofo_data_get_notes:
 */
const gchar *
ofo_data_get_notes( const ofoData *data )
{
	ofo_base_getter( DATA, data, string, NULL, DAT_NOTES );
}

/**
 * ofo_data_get_upd_user:
 */
const gchar *
ofo_data_get_upd_user( const ofoData *data )
{
	ofo_base_getter( DATA, data, string, NULL, DAT_UPD_USER );
}

/**
 * ofo_data_get_upd_stamp:
 */
const myStampVal *
ofo_data_get_upd_stamp( const ofoData *data )
{
	ofo_base_getter( DATA, data, timestamp, NULL, DAT_UPD_STAMP );
}

/*
 * data_set_cre_user:
 */
static void
data_set_cre_user( ofoData *data, const gchar *user )
{
	ofo_base_setter( DATA, data, string, DAT_CRE_USER, user );
}

/*
 * data_set_cre_stamp:
 */
static void
data_set_cre_stamp( ofoData *data, const myStampVal *stamp )
{
	ofo_base_setter( DATA, data, timestamp, DAT_CRE_STAMP, stamp );
}

/**
 * ofo_data_set_key:
 */
void
ofo_data_set_key( ofoData *data, const gchar *key )
{
	ofo_base_setter( DATA, data, string, DAT_KEY, key );
}

/**
 * ofo_data_set_content:
 */
void
ofo_data_set_content( ofoData *data, const gchar *content )
{
	ofo_base_setter( DATA, data, string, DAT_CONTENT, content );
}

/**
 * ofo_data_set_notes:
 */
void
ofo_data_set_notes( ofoData *data, const gchar *notes )
{
	ofo_base_setter( DATA, data, string, DAT_NOTES, notes );
}

/*
 * data_set_upd_user:
 */
static void
data_set_upd_user( ofoData *data, const gchar *user )
{
	ofo_base_setter( DATA, data, string, DAT_UPD_USER, user );
}

/*
 * data_set_upd_stamp:
 */
static void
data_set_upd_stamp( ofoData *data, const myStampVal *stamp )
{
	ofo_base_setter( DATA, data, timestamp, DAT_UPD_STAMP, stamp );
}

/**
 * ofo_data_insert:
 *
 * Only insert here a new data, so only the main properties
 */
gboolean
ofo_data_insert( ofoData *data )
{
	static const gchar *thisfn = "ofo_data_insert";
	gboolean ok;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: data=%p", thisfn, ( void * ) data );

	g_return_val_if_fail( data && OFO_IS_DATA( data ), FALSE );
	g_return_val_if_fail( !OFO_BASE( data )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( data ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_data_get_dataset( getter );

	if( data_do_insert( data, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( data ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, data );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
data_do_insert( ofoData *data, const ofaIDBConnect *connect )
{
	return( data_insert_main( data, connect ));
}

static gboolean
data_insert_main( ofoData *data, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *content, *notes, *stamp_str;
	gboolean ok;
	myStampVal *stamp;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	content = my_utils_quote_sql( ofo_data_get_content( data ));
	notes = my_utils_quote_sql( ofo_data_get_notes( data ));
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_DATA" );

	g_string_append_printf( query,
			"	(DAT_KEY,DAT_CRE_USER,DAT_CRE_STAMP,DAT_CONTENT,DAT_NOTES)"
			"	VALUES ('%s','%s','%s','%s',",
			ofo_data_get_key( data ),
			userid,
			stamp_str,
			content );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		data_set_cre_user( data, userid );
		data_set_cre_stamp( data, stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( content );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_data_update:
 *
 * We only update here the user properties, so do not care with the
 * details of balances per currency.
 */
gboolean
ofo_data_update( ofoData *data, const gchar *prev_key )
{
	static const gchar *thisfn = "ofo_data_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: data=%p, prev_key=%s",
			thisfn, ( void * ) data, prev_key );

	g_return_val_if_fail( data && OFO_IS_DATA( data ), FALSE );
	g_return_val_if_fail( my_strlen( prev_key ), FALSE );
	g_return_val_if_fail( !OFO_BASE( data )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( data ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( data_do_update( data, prev_key, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, data, prev_key );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
data_do_update( ofoData *data, const gchar *prev_key, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *content, *notes;
	gboolean ok;
	gchar *stamp_str;
	myStampVal *stamp;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	content = my_utils_quote_sql( ofo_data_get_content( data ));
	notes = my_utils_quote_sql( ofo_data_get_notes( data ));
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_DATA SET " );

	g_string_append_printf( query, "DAT_KEY='%s',", ofo_data_get_key( data ));
	g_string_append_printf( query, "DAT_CONTENT='%s',", content );

	if( my_strlen( notes )){
		g_string_append_printf( query, "DAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	DAT_UPD_USER='%s',DAT_UPD_STAMP='%s'"
			"	WHERE DAT_KEY='%s'", userid, stamp_str, prev_key );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		data_set_upd_user( data, userid );
		data_set_upd_stamp( data, stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( content );
	g_free( stamp_str );
	my_stamp_free( stamp );

	return( ok );
}

/**
 * ofo_data_delete:
 */
gboolean
ofo_data_delete( ofoData *data )
{
	static const gchar *thisfn = "ofo_data_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: data=%p", thisfn, ( void * ) data );

	g_return_val_if_fail( data && OFO_IS_DATA( data ), FALSE );
	//g_return_val_if_fail( ofo_data_is_deletable( data ), FALSE );
	g_return_val_if_fail( !OFO_BASE( data )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( data ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( data_do_delete( data, ofa_hub_get_connect( hub ))){
		g_object_ref( data );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( data ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, data );
		g_object_unref( data );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
data_do_delete( ofoData *data, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( data && OFO_IS_DATA( data ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_DATA WHERE DAT_KEY='%s'",
					ofo_data_get_key( data ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
data_cmp_by_key( const ofoData *a, const gchar *key )
{
	const gchar *key_a;

	key_a = ofo_data_get_key( a );

	return( my_collate( key_a, key ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_data_icollectionable_iface_init";

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
					"OFA_T_DATA",
					OFO_TYPE_DATA,
					OFA_IGETTER( user_data ));

	return( dataset );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_data_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _keyed datas" )));
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
 * Exports all the datas.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_data_iexportable_export";

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
	GList *dataset, *it;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;
	ofoData *data;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_data_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += DATA_TABLES_COUNT;
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, DATA_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					DATA_TABLES_COUNT, st_boxed_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		data = OFO_DATA( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( data )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	return( ok );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_data_iimportable_iface_init";

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
 * ofo_data_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - data key
 * - content
 * - notes (opt)
 *
 * Replace the main table with the provided datas, initializing the
 * balances to zero.
 *
 * In order to be able to import a previously exported file:
 * - we accept that the first field of the first line be "1" or "2"
 * - we silently ignore other lines.
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_DATA" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_DATA );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_DATA );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_DATA" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_data_free_dataset( dataset );
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
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	guint numline, total;
	gchar *splitted, *str;
	gboolean have_prefix;
	ofoData *data;
	myStampVal *stamp;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );
	have_prefix = FALSE;

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		data = ofo_data_new( parms->getter );

		/* data key or "1" */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( numline == 1 ){
			have_prefix = ( my_strlen( cstr ) &&
					( !my_collate( cstr, "1" ) || !my_collate( cstr, "2" )));
		}
		if( have_prefix ){
			if( my_collate( cstr, "1" )){
				str = g_strdup_printf( _( "ignoring line with prefix=%s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				total -= 1;
				g_free( str );
				continue;
			}
			itf = itf ? itf->next : NULL;
			cstr = itf ? ( const gchar * ) itf->data : NULL;
		}
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty data key" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_data_set_key( data, cstr );

		/* creation user */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			data_set_cre_user( data, cstr );
		}

		/* creation timestamp */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			stamp = my_stamp_new_from_sql( cstr );
			data_set_cre_stamp( data, stamp );
			my_stamp_free( stamp );
		}

		/* data content */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty data content" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_data_set_content( data, cstr );

		/* notes
		 * we are tolerant on the last field... */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_data_set_notes( data, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, data );
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
	gboolean insert;
	guint total, type;
	gchar *str;
	ofoData *data;
	const gchar *key;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );

	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		data_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		data = OFO_DATA( it->data );

		if( data_get_exists( data, connect )){
			parms->duplicate_count += 1;
			key = ofo_data_get_key( data );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate data, replacing previous one" ), key );
					data_do_delete( data, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate data, ignored (skipped)" ), key );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate data" ), key );
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
			if( data_do_insert( data, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
data_get_exists( const ofoData *data, const ofaIDBConnect *connect )
{
	const gchar *key;
	gint count;
	gchar *str;

	count = 0;
	key = ofo_data_get_key( data );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DATA WHERE DAT_KEY='%s'", key );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
data_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_DATA", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_data_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_data_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
