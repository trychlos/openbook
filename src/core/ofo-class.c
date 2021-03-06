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
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-class.h"

/* priv instance data
 */
enum {
	CLA_NUMBER = 1,
	CLA_CRE_USER,
	CLA_CRE_STAMP,
	CLA_LABEL,
	CLA_NOTES,
	CLA_UPD_USER,
	CLA_UPD_STAMP,
	CLA_DOC_ID,
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
		{ OFA_BOX_CSV( CLA_NUMBER ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( CLA_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( CLA_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( CLA_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CLA_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
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

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( CLA_NUMBER ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( CLA_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define CLASS_TABLES_COUNT              2
#define CLASS_EXPORT_VERSION            2

/* priv instance data
 */
typedef struct {
	GList *docs;
}
	ofoClassPrivate;

static ofoClass  *class_find_by_number( GList *set, gint number );
static void       class_set_cre_user( ofoClass *class, const gchar *user );
static void       class_set_cre_stamp( ofoClass *class, const myStampVal *stamp );
static void       class_set_upd_user( ofoClass *class, const gchar *user );
static void       class_set_upd_stamp( ofoClass *class, const myStampVal *stamp );
static GList     *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean   class_do_insert( ofoClass *class, const ofaIDBConnect *connect );
static gboolean   class_do_update( ofoClass *class, gint prev_id, const ofaIDBConnect *connect );
static gboolean   class_do_delete( ofoClass *class, const ofaIDBConnect *connect );
static gint       class_cmp_by_number( const ofoClass *a, gpointer pnum );
static void       icollectionable_iface_init( myICollectionableInterface *iface );
static guint      icollectionable_get_interface_version( void );
static GList     *icollectionable_load_collection( void *user_data );
static void       idoc_iface_init( ofaIDocInterface *iface );
static guint      idoc_get_interface_version( void );
static void       iexportable_iface_init( ofaIExportableInterface *iface );
static guint      iexportable_get_interface_version( void );
static gchar     *iexportable_get_label( const ofaIExportable *instance );
static gboolean   iexportable_get_published( const ofaIExportable *instance );
static gboolean   iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean   iexportable_export_default( ofaIExportable *exportable );
static void       iimportable_iface_init( ofaIImportableInterface *iface );
static guint      iimportable_get_interface_version( void );
static gchar     *iimportable_get_label( const ofaIImportable *instance );
static guint      iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList     *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void       iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean   class_get_exists( const ofoClass *class, const ofaIDBConnect *connect );
static gboolean   class_drop_content( const ofaIDBConnect *connect );
static void       isignalable_iface_init( ofaISignalableInterface *iface );
static void       isignalable_connect_to( ofaISignaler *signaler );

G_DEFINE_TYPE_EXTENDED( ofoClass, ofo_class, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoClass )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoClassPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_class_get_instance_private( self );

	priv->docs = NULL;
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
 * ofo_class_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoClass dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_class_get_dataset( ofaIGetter *getter )
{
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_CLASS, getter ));
}

/**
 * ofo_class_get_by_number:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the searched class, or %NULL.
 *
 * The returned object is owned by the #ofoClass class, and should
 * not be unreffed by the caller.
 */
ofoClass *
ofo_class_get_by_number( ofaIGetter *getter, gint number )
{
	GList *list;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	list = ofo_class_get_dataset( getter );

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
 * @getter: a #ofaIGetter instance.
 */
ofoClass *
ofo_class_new( ofaIGetter *getter )
{
	ofoClass *class;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	class = g_object_new( OFO_TYPE_CLASS, "ofo-base-getter", getter, NULL );

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
 * ofo_class_get_cre_user:
 */
const gchar *
ofo_class_get_cre_user( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_CRE_USER );
}

/**
 * ofo_class_get_cre_stamp:
 */
const myStampVal *
ofo_class_get_cre_stamp( const ofoClass *class )
{
	ofo_base_getter( CLASS, class, timestamp, NULL, CLA_CRE_STAMP );
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
const myStampVal *
ofo_class_get_upd_stamp( const ofoClass *class )
{
	ofo_base_getter( CLASS, class, timestamp, NULL, CLA_UPD_STAMP );
}

/**
 * ofo_class_is_valid_data:
 *
 * Returns: %TRUE if the provided data makes the ofoClass a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_class_is_valid_data( gint number, const gchar *label, gchar **msgerr )
{
	if( msgerr ){
		*msgerr = NULL;
	}
	if( !ofo_class_is_valid_number( number )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Class identifier is not valid (must be a [1-9] digit)" ));
		}
		return( FALSE );
	}
	if( !ofo_class_is_valid_label( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Class label is empty" ));
		}
		return( FALSE );
	}
	return( TRUE );
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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean deletable;

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( class ));

	if( deletable ){
		signaler = ofa_igetter_get_signaler( getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, class, &deletable );
	}

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

/*
 * class_set_cre_user:
 */
static void
class_set_cre_user( ofoClass *class, const gchar *user )
{
	ofo_base_setter( CLASS, class, string, CLA_CRE_USER, user );
}

/*
 * class_set_cre_stamp:
 */
static void
class_set_cre_stamp( ofoClass *class, const myStampVal *stamp )
{
	ofo_base_setter( CLASS, class, timestamp, CLA_CRE_STAMP, stamp );
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
 * class_set_upd_user:
 */
static void
class_set_upd_user( ofoClass *class, const gchar *user )
{
	ofo_base_setter( CLASS, class, string, CLA_UPD_USER, user );
}

/*
 * class_set_upd_stamp:
 */
static void
class_set_upd_stamp( ofoClass *class, const myStampVal *stamp )
{
	ofo_base_setter( CLASS, class, timestamp, CLA_UPD_STAMP, stamp );
}

/**
 * ofo_class_doc_get_count:
 * @class: this #ofoClass object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_class_doc_get_count( ofoClass *class )
{
	ofoClassPrivate *priv;

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), 0 );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, 0 );

	priv = ofo_class_get_instance_private( class );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_class_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown ledger mnemos in OFA_T_CLASSES_DOC
 * child table.
 *
 * The returned list should be #ofo_class_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_class_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_CLASSES_DOC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(CLA_NUMBER) FROM %s "
			"	WHERE CLA_NUMBER NOT IN (SELECT CLA_NUMBER FROM OFA_T_CLASSES)", table );

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
 * ofo_class_insert:
 * @class:
 *
 * Returns: %TRUE if the insertion has been successful, %FALSE else.
 */
gboolean
ofo_class_insert( ofoClass *class )
{
	static const gchar *thisfn = "ofo_class_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: class=%p", thisfn, ( void * ) class );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( class ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	/* rationale: see ofo-account.c */
	ofo_class_get_dataset( getter );

	if( class_do_insert( class, ofa_hub_get_connect( hub ))){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( class ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, class );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
class_do_insert( ofoClass *class, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	myStampVal *stamp;
	const gchar *userid;

	query = g_string_new( "INSERT INTO OFA_T_CLASSES " );

	userid = ofa_idbconnect_get_account( connect );
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

	label = my_utils_quote_sql( ofo_class_get_label( class ));
	notes = my_utils_quote_sql( ofo_class_get_notes( class ));

	g_string_append_printf( query,
			"	(CLA_NUMBER,CLA_CRE_USER,CLA_CRE_STAMP,CLA_LABEL,CLA_NOTES)"
			"	VALUES "
			"	(%d,'%s','%s','%s',",
					ofo_class_get_number( class ),
					userid,
					stamp_str,
					label );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	if( ok ){
		class_set_cre_user( class, userid );
		class_set_cre_stamp( class, stamp );
	}

	g_free( label );
	g_free( stamp_str );
	my_stamp_free( stamp );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: class=%p, prev_id=%d", thisfn, ( void * ) class, prev_id );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( class ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( class_do_update( class, prev_id, ofa_hub_get_connect( hub ))){
		str = g_strdup_printf( "%d", prev_id );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, class, str );
		g_free( str );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
class_do_update( ofoClass *class, gint prev_id, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	myStampVal *stamp;
	gboolean ok;
	const gchar *userid;

	ok = FALSE;
	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_class_get_label( class ));
	notes = my_utils_quote_sql( ofo_class_get_notes( class ));
	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );

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
		class_set_upd_stamp( class, stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp_str );
	my_stamp_free( stamp );

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
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;

	g_debug( "%s: class=%p", thisfn, ( void * ) class );

	g_return_val_if_fail( class && OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( ofo_class_is_deletable( class ), FALSE );
	g_return_val_if_fail( !OFO_BASE( class )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( class ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( class_do_delete( class, ofa_hub_get_connect( hub ))){
		g_object_ref( class );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( class ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, class );
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

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_icollectionable_iface_init";

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
	GList *list;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	list = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_CLASSES",
					OFO_TYPE_CLASS,
					OFA_IGETTER( user_data ));

	return( list );
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
	static const gchar *thisfn = "ofo_class_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : account cla_sses" )));
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
 * Exports all the classes.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_class_iexportable_export";

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
	ofoClass *class;
	ofoClassPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_class_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += CLASS_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		class = OFO_CLASS( it->data );
		count += ofo_class_doc_get_count( class );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, CLASS_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					CLASS_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		class = OFO_CLASS( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( class )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_class_get_instance_private( class );

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
 * ofo_class_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - class number
 * - label
 * - notes (opt)
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_CLASSES" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_CLASS );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_CLASS );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_CLASSES" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_class_free_dataset( dataset );
	}

	return( parms->parse_errs+parms->insert_errs );
}

#if 0
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	guint numline, total, number;
	const gchar *cstr;
	ofoClass *class;
	gchar *str, *splitted;
	GSList *itl, *fields, *itf;

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
		class = ofo_class_new( parms->getter );

		/* class number */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		number = 0;
		if( my_strlen( cstr )){
			number = atoi( cstr );
		}
		if( number < 1 || number > 9 ){
			str = g_strdup_printf( _( "invalid class number: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_class_set_number( class, number );

		/* class label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty class label" ));
			parms->parse_errs += 1;
			continue;
		} else {
			ofo_class_set_label( class, cstr );
		}

		/* notes */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_class_set_notes( class, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, class );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( dataset );
}
#endif

static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	guint numline, total, number;
	const gchar *cstr;
	ofoClass *class;
	gchar *str, *splitted;
	GSList *itl, *fields, *itf;
	myStampVal *stamp;

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
		class = ofo_class_new( parms->getter );

		/* class number */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		number = 0;
		if( my_strlen( cstr )){
			number = atoi( cstr );
		}
		if( number < 1 || number > 9 ){
			str = g_strdup_printf( _( "invalid class number: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_class_set_number( class, number );

		/* creation user */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			class_set_cre_user( class, cstr );
		}

		/* creation timestamp */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			stamp = my_stamp_new_from_sql( cstr );
			class_set_cre_stamp( class, stamp );
			my_stamp_free( stamp );
		}

		/* class label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty class label" ));
			parms->parse_errs += 1;
			continue;
		} else {
			ofo_class_set_label( class, cstr );
		}

		/* notes */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_class_set_notes( class, splitted );
		g_free( splitted );

		dataset = g_list_prepend( dataset, class );
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
	ofoClass *class;
	gint class_id;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		class_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		class = OFO_CLASS( it->data );

		if( class_get_exists( class, connect )){
			parms->duplicate_count += 1;
			class_id = ofo_class_get_number( class );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%d: duplicate class, replacing previous one" ), class_id );
					class_do_delete( class, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%d: duplicate class, ignored (skipped)" ), class_id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%d: erroneous duplicate class" ), class_id );
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
			if( class_do_insert( class, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
class_get_exists( const ofoClass *class, const ofaIDBConnect *connect )
{
	gint class_id;
	gint count;
	gchar *str;

	count = 0;
	class_id = ofo_class_get_number( class );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_CLASSES WHERE CLA_NUMBER=%d", class_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
class_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_CLASSES", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_class_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));
}
