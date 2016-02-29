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
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-class.h"

/* priv instance data
 */
enum {
	CLA_NUMBER = 1,
	CLA_LABEL,
	CLA_NOTES,
	CLA_UPD_USER,
	CLA_UPD_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( CLA_NUMBER ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( CLA_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CLA_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
										/* below data are not imported */
		{ OFA_BOX_CSV( CLA_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( CLA_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ 0 }
};

/* priv instance data
 */
struct _ofoClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

static ofoClass  *class_find_by_number( GList *set, gint number );
static void       class_set_upd_user( ofoClass *class, const gchar *user );
static void       class_set_upd_stamp( ofoClass *class, const GTimeVal *stamp );
static gboolean   class_do_insert( ofoClass *class, const ofaIDBConnect *connect );
static gboolean   class_do_update( ofoClass *class, gint prev_id, const ofaIDBConnect *connect );
static gboolean   class_do_delete( ofoClass *class, const ofaIDBConnect *connect );
static gint       class_cmp_by_number( const ofoClass *a, gpointer pnum );
static gint       class_cmp_by_ptr( const ofoClass *a, const ofoClass *b );
static void       icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList     *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void       iexportable_iface_init( ofaIExportableInterface *iface );
static guint      iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean   iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void       iimportable_iface_init( ofaIImportableInterface *iface );
static guint      iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean   iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub );
static gboolean   class_do_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoClass, ofo_class, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoClass )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

static void
class_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_class_finalize";

	g_debug( "%s: instance=%p (%s): %d - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_int( OFO_BASE( instance )->prot->fields, CLA_NUMBER ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, CLA_LABEL ));

	g_return_if_fail( instance && OFO_IS_CLASS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_class_parent_class )->finalize( instance );
}

static void
class_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_CLASS( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_class_parent_class )->dispose( instance );
}

static void
ofo_class_init( ofoClass *self )
{
	static const gchar *thisfn = "ofo_class_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_class_class_init( ofoClassClass *klass )
{
	static const gchar *thisfn = "ofo_class_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_finalize;
}

/**
 * ofo_class_connect_to_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_class_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_class_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}

/**
 * ofo_class_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoClass dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_class_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_CLASS ));
}

/**
 * ofo_class_get_by_number:
 * @hub: the current #ofaHub object.
 *
 * Returns: the searched class, or %NULL.
 *
 * The returned object is owned by the #ofoClass class, and should
 * not be unreffed by the caller.
 */
ofoClass *
ofo_class_get_by_number( ofaHub *hub, gint number )
{
	GList *list;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	list = ofo_class_get_dataset( hub );

	return( class_find_by_number( list, number ));
}

static ofoClass *
class_find_by_number( GList *set, gint number )
{
	GList *found;

	found = g_list_find_custom(
				set, GINT_TO_POINTER( number ), ( GCompareFunc ) class_cmp_by_number );
	if( found ){
		return( OFO_CLASS( found->data ));
	}

	return( NULL );
}

/**
 * ofo_class_new:
 */
ofoClass *
ofo_class_new( void )
{
	ofoClass *class;

	class = g_object_new( OFO_TYPE_CLASS, NULL );
	OFO_BASE( class )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( class );
}

/**
 * ofo_class_get_number:
 */
gint
ofo_class_get_number( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , int, 0, CLA_NUMBER );
}

/**
 * ofo_class_get_label:
 */
const gchar *
ofo_class_get_label( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_LABEL );
}

/**
 * ofo_class_get_notes:
 */
const gchar *
ofo_class_get_notes( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_NOTES );
}

/**
 * ofo_class_get_upd_user:
 */
const gchar *
ofo_class_get_upd_user( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_UPD_USER );
}

/**
 * ofo_class_get_upd_stamp:
 */
const GTimeVal *
ofo_class_get_upd_stamp( const ofoClass *class )
{
	ofo_base_getter( CLASS, class, timestamp, NULL, CLA_UPD_STAMP );
}

/**
 * ofo_class_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoClass a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_class_is_valid( gint number, const gchar *label )
{
	return( ofo_class_is_valid_number( number ) &&
			ofo_class_is_valid_label( label ));
}

/**
 * ofo_class_is_valid_number:
 *
 * Returns: %TRUE if the provided number is a valid class number
 */
gboolean
ofo_class_is_valid_number( gint number )
{
	return( number > 0 && number < 10 );
}

/**
 * ofo_class_is_valid_label:
 *
 * Returns: %TRUE if the provided label is a valid class label
 */
gboolean
ofo_class_is_valid_label( const gchar *label )
{
	return( my_strlen( label ) > 0 );
}

/**
 * ofo_class_is_deletable:
 *
 * Returns: %TRUE if the provided object may be safely deleted.
 *
 * Though the class in only used as tabs title in the accounts notebook,
 * and though we are providing default values, a class stays a reference
 * table. A row is only deletable if it is not referenced by any other
 * object (and the dossier is current).
 */
gboolean
ofo_class_is_deletable( const ofoClass *class )
{
	ofaHub *hub;
	gboolean used_by_accounts;
	gboolean deletable;

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( class ));
	used_by_accounts = ofo_account_use_class( hub, ofo_class_get_number( class ));
	deletable = !used_by_accounts;

	return( deletable );
}

/**
 * ofo_class_set_number:
 */
void
ofo_class_set_number( ofoClass *class, gint number )
{
	g_return_if_fail( ofo_class_is_valid_number( number ));

	ofo_base_setter( CLASS, class, int, CLA_NUMBER, number );
}

/**
 * ofo_class_set_label:
 */
void
ofo_class_set_label( ofoClass *class, const gchar *label )
{
	g_return_if_fail( ofo_class_is_valid_label( label ));

	ofo_base_setter( CLASS, class, string, CLA_LABEL, label );
}

/**
 * ofo_class_set_notes:
 */
void
ofo_class_set_notes( ofoClass *class, const gchar *notes )
{
	ofo_base_setter( CLASS, class, string, CLA_NOTES, notes );
}

/*
 * ofo_class_set_upd_user:
 */
static void
class_set_upd_user( ofoClass *class, const gchar *user )
{
	ofo_base_setter( CLASS, class, string, CLA_UPD_USER, user );
}

/*
 * ofo_class_set_upd_stamp:
 */
static void
class_set_upd_stamp( ofoClass *class, const GTimeVal *stamp )
{
	ofo_base_setter( CLASS, class, timestamp, CLA_UPD_STAMP, stamp );
}

/**
 * ofo_class_insert:
 * @class:
 * @hub: the current #ofaHub object.
 *
 * Returns: %TRUE if the insertion has been successful, %FALSE else.
 */
gboolean
ofo_class_insert( ofoClass *class, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_class_insert";
	gboolean ok;

	g_debug( "%s: class=%p, hub=%p",
			thisfn, ( void * ) class, ( void * ) hub );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	if( class_do_insert( class, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( class ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( class ), ( GCompareFunc ) class_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, class );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
class_do_insert( ofoClass *class, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	query = g_string_new( "INSERT INTO OFA_T_CLASSES " );

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_class_get_label( class ));
	notes = my_utils_quote_single( ofo_class_get_notes( class ));

	g_string_append_printf( query,
			"	(CLA_NUMBER,CLA_LABEL,CLA_NOTES,"
			"	 CLA_UPD_USER,CLA_UPD_STAMP) VALUES "
			"	(%d,'%s',",
					ofo_class_get_number( class ),
					label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	if( ok ){
		class_set_upd_user( class, userid );
		class_set_upd_stamp( class, &stamp );
	}

	g_free( label );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_class_update:
 */
gboolean
ofo_class_update( ofoClass *class, gint prev_id )
{
	static const gchar *thisfn = "ofo_class_update";
	gchar *str;
	gboolean ok;
	ofaHub *hub;

	g_debug( "%s: class=%p, prev_id=%d", thisfn, ( void * ) class, prev_id );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( class ));

	if( class_do_update( class, prev_id, ofa_hub_get_connect( hub ))){
		str = g_strdup_printf( "%d", prev_id );
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_CLASS, ( GCompareFunc ) class_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, class, str );
		g_free( str );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
class_do_update( ofoClass *class, gint prev_id, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str, *userid;
	GTimeVal stamp;
	gboolean ok;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_class_get_label( class ));
	notes = my_utils_quote_single( ofo_class_get_notes( class ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_CLASSES SET " );

	g_string_append_printf( query, "	CLA_NUMBER=%d,", ofo_class_get_number( class ));
	g_string_append_printf( query, "	CLA_LABEL='%s',", label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "CLA_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CLA_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CLA_UPD_USER='%s',CLA_UPD_STAMP='%s'"
			"	WHERE CLA_NUMBER=%d", userid, stamp_str, prev_id );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		class_set_upd_user( class, userid );
		class_set_upd_stamp( class, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

/**
 * ofo_class_delete:
 */
gboolean
ofo_class_delete( ofoClass *class )
{
	static const gchar *thisfn = "ofo_class_delete";
	gboolean ok;
	ofaHub *hub;

	g_debug( "%s: class=%p", thisfn, ( void * ) class );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( ofo_class_is_deletable( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( class ));

	if( class_do_delete( class, ofa_hub_get_connect( hub ))){
		g_object_ref( class );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( class ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, class );
		g_object_unref( class );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
class_do_delete( ofoClass *class, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CLASSES"
			"	WHERE CLA_NUMBER=%d",
					ofo_class_get_number( class ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
class_cmp_by_number( const ofoClass *a, gpointer pnum )
{
	gint anum, bnum;

	anum = ofo_class_get_number( a );
	bnum = GPOINTER_TO_INT( pnum );

	if( anum < bnum ){
		return( -1 );
	}
	if( anum > bnum ){
		return( 1 );
	}
	return( 0 );
}

static gint
class_cmp_by_ptr( const ofoClass *a, const ofoClass *b )
{
	gint bnum;

	bnum = ofo_class_get_number( b );
	return( class_cmp_by_number( a, GINT_TO_POINTER( bnum )));
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
	GList *list;

	list = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_CLASSES ORDER BY CLA_NUMBER ASC",
					OFO_TYPE_CLASS,
					hub );

	return( list );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
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
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	GList *dataset, *it;
	GSList *lines;
	gchar *str;
	gboolean with_headers, ok;
	gulong count;
	gchar field_sep;

	dataset = ofo_class_get_dataset( hub );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, '\0' );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
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
	static const gchar *thisfn = "ofo_class_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/*
 * ofo_class_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - class number
 * - label
 * - notes (opt)
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
	ofoClass *class;
	GList *dataset, *it;
	guint errors, number, line;
	gchar *msg, *splitted;
	const ofaIDBConnect *connect;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		class = ofo_class_new();
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		number = 0;
		if( cstr ){
			number = atoi( cstr );
		}
		if( number < 1 || number > 9 ){
			msg = g_strdup_printf( _( "invalid class number: %s" ), cstr );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			errors += 1;
			continue;
		}
		ofo_class_set_number( class, number );

		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty class label" ));
			errors += 1;
			continue;
		} else {
			ofo_class_set_label( class, cstr );
		}

		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_class_set_notes( class, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, class );
	}

	if( !errors ){
		connect = ofa_hub_get_connect( hub );
		class_do_drop_content( connect );

		for( it=dataset ; it ; it=it->next ){
			if( !class_do_insert( OFO_CLASS( it->data ), connect )){
				errors -= 1;
			}
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		ofo_class_free_dataset( dataset );
		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_CLASS );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_CLASS );
	}

	return( errors );
}

static gboolean
class_do_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_CLASSES", TRUE ));
}
