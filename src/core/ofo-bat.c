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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-counters.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

/* priv instance data
 */
enum {
	BAT_ID = 1,
	BAT_URI,
	BAT_FORMAT,
	BAT_BEGIN,
	BAT_END,
	BAT_RIB,
	BAT_CURRENCY,
	BAT_SOLDE_BEGIN,
	BAT_SOLDE_BEGIN_SET,
	BAT_SOLDE_END,
	BAT_SOLDE_END_SET,
	BAT_CRE_USER,
	BAT_CRE_STAMP,
	BAT_NOTES,
	BAT_UPD_USER,
	BAT_UPD_STAMP,
	BAT_ACCOUNT,
	BAT_ACC_USER,
	BAT_ACC_STAMP,
	BAT_DOC_ID,
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
		{ OFA_BOX_CSV( BAT_ID ),
				OFA_TYPE_COUNTER,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( BAT_URI ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_FORMAT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_BEGIN ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_RIB ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_SOLDE_BEGIN ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_SOLDE_BEGIN_SET ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_SOLDE_END ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_SOLDE_END_SET ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( BAT_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( BAT_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( BAT_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( BAT_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_ACC_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( BAT_ACC_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( BAT_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( BAT_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define BAT_TABLES_COUNT                2
#define BAT_EXPORT_VERSION              1

/* priv instance data
 */
typedef struct {
	GList *docs;
}
	ofoBatPrivate;

static ofoBat     *bat_find_by_id( GList *set, ofxCounter id );
static void        bat_set_id( ofoBat *bat, ofxCounter id );
static void        bat_set_cre_user( ofoBat *bat, const gchar *user );
static void        bat_set_cre_stamp( ofoBat *bat, const GTimeVal *stamp );
static void        bat_set_notes( ofoBat *bat, const gchar *account );
static void        bat_set_upd_user( ofoBat *bat, const gchar *user );
static void        bat_set_upd_stamp( ofoBat *bat, const GTimeVal *stamp );
static void        bat_set_account( ofoBat *bat, const gchar *account );
static void        bat_set_acc_user( ofoBat *bat, const gchar *user );
static void        bat_set_acc_stamp( ofoBat *bat, const GTimeVal *stamp );
static GList      *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean    bat_do_insert( ofoBat *bat, ofaIGetter *getter );
static gboolean    bat_insert_main( ofoBat *bat, ofaIGetter *getter );
static gboolean    bat_do_update_notes( ofoBat *bat );
static gboolean    bat_do_update_account( ofoBat *bat );
static gboolean    bat_do_delete_by_where( ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id );
static gboolean    bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id );
static gboolean    bat_do_delete_doc( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id );
static gint        bat_cmp_by_id( ofoBat *a, ofxCounter id );
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
static ofoBat     *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static ofoBatLine *iimportable_import_parse_line( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gint year );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    bat_get_exists( ofoBat *bat, const ofaIDBConnect *connect );
static gchar      *bat_get_where( ofoBat *bat );
static ofxCounter  bat_get_id_by_where( ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_drop_content( const ofaIDBConnect *connect );
static void        isignalable_iface_init( ofaISignalableInterface *iface );
static void        isignalable_connect_to( ofaISignaler *signaler );
static gboolean    signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean    signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account );
static gboolean    signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency );
static void        signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static void        signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *prev_id, const gchar *new_id );
static void        signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_code, const gchar *new_code );

G_DEFINE_TYPE_EXTENDED( ofoBat, ofo_bat, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoBat )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
bat_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, BAT_URI ));

	g_return_if_fail( instance && OFO_IS_BAT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->finalize( instance );
}

static void
bat_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_BAT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->dispose( instance );
}

static void
ofo_bat_init( ofoBat *self )
{
	static const gchar *thisfn = "ofo_bat_init";
	ofoBatPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_bat_get_instance_private( self );

	priv->docs = NULL;
}

static void
ofo_bat_class_init( ofoBatClass *klass )
{
	static const gchar *thisfn = "ofo_bat_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_finalize;
}

/**
 * ofo_bat_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoBat dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_bat_get_dataset( ofaIGetter *getter )
{
	myICollector * collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_BAT, getter ));
}

/**
 * ofo_bat_get_by_id:
 * @getter: a #ofaIGetter instance.
 * @id: the requested identifier.
 *
 * Returns: the searched BAT object, or %NULL.
 *
 * The returned object is owned by the #ofoBat class, and should
 * not be unreffed by the caller.
 */
ofoBat *
ofo_bat_get_by_id( ofaIGetter *getter, ofxCounter id )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	dataset = ofo_bat_get_dataset( getter );

	return( bat_find_by_id( dataset, id ));
}

static ofoBat *
bat_find_by_id( GList *set, ofxCounter id )
{
	GList *found;

	found = g_list_find_custom( set, ( gconstpointer ) id, ( GCompareFunc ) bat_cmp_by_id );
	if( found ){
		return( OFO_BAT( found->data ));
	}

	return( NULL );
}

/**
 * ofo_bat_new:
 * @getter: a #ofaIGetter instance.
 */
ofoBat *
ofo_bat_new( ofaIGetter *getter )
{
	ofoBat *bat;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bat = g_object_new( OFO_TYPE_BAT, "ofo-base-getter", getter, NULL );

	OFO_BASE( bat )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( bat );
}

/**
 * ofo_bat_get_id:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat identifier.
 */
ofxCounter
ofo_bat_get_id( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, counter, 0, BAT_ID );
}

/**
 * ofo_bat_get_uri:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat source URI.
 */
const gchar *
ofo_bat_get_uri( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_URI );
}

/**
 * ofo_bat_get_format:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat import format.
 */
const gchar *
ofo_bat_get_format( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_FORMAT );
}

/**
 * ofo_bat_get_begin_date:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat beginning date.
 */
const GDate *
ofo_bat_get_begin_date( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, date, NULL, BAT_BEGIN );
}

/**
 * ofo_bat_get_begin_solde:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat beginning solde.
 */
ofxAmount
ofo_bat_get_begin_solde( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, amount, 0, BAT_SOLDE_BEGIN );
}

/**
 * ofo_bat_get_begin_solde_set:
 * @bat: this #ofoBat object.
 *
 * Returns: %TRUE if the beginning solde is set.
 */
gboolean
ofo_bat_get_begin_solde_set( ofoBat *bat )
{
	const gchar *cstr;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( bat )->prot->fields, BAT_SOLDE_BEGIN_SET );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_bat_get_end_date:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat ending date.
 */
const GDate *
ofo_bat_get_end_date( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, date, NULL, BAT_END );
}

/**
 * ofo_bat_get_end_solde:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat ending solde.
 */
ofxAmount
ofo_bat_get_end_solde( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, amount, 0, BAT_SOLDE_END );
}

/**
 * ofo_bat_get_end_solde_set:
 * @bat: this #ofoBat object.
 *
 * Returns: %TRUE if the ending solde is set.
 */
gboolean
ofo_bat_get_end_solde_set( ofoBat *bat )
{
	const gchar *cstr;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( bat )->prot->fields, BAT_SOLDE_END_SET );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_bat_get_rib:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat RIB.
 */
const gchar *
ofo_bat_get_rib( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_RIB );
}

/**
 * ofo_bat_get_currency:
 * @bat: this #ofoBat object.
 *
 * Returns: the @bat currency.
 */
const gchar *
ofo_bat_get_currency( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_CURRENCY );
}

/**
 * ofo_bat_get_cre_user:
 * @bat: this #ofoBat object.
 *
 * Returns: the importer user.
 */
const gchar *
ofo_bat_get_cre_user( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_CRE_USER );
}

/**
 * ofo_bat_get_cre_stamp:
 * @bat: this #ofoBat object.
 *
 * Returns: the import timestamp.
 */
const GTimeVal *
ofo_bat_get_cre_stamp( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, timestamp, NULL, BAT_CRE_STAMP );
}

/**
 * ofo_bat_get_notes:
 * @bat: this #ofoBat object.
 *
 * Returns: the notes associated to the @bat.
 */
const gchar *
ofo_bat_get_notes( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_NOTES );
}

/**
 * ofo_bat_get_upd_user:
 * @bat: this #ofoBat object.
 *
 * Returns: the importer user.
 */
const gchar *
ofo_bat_get_upd_user( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_UPD_USER );
}

/**
 * ofo_bat_get_upd_stamp:
 * @bat: this #ofoBat object.
 *
 * Returns: the import timestamp.
 */
const GTimeVal *
ofo_bat_get_upd_stamp( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, timestamp, NULL, BAT_UPD_STAMP );
}

/**
 * ofo_bat_get_account:
 * @bat: this #ofoBat object.
 *
 * Returns: the Openbook account associated to the @bat.
 */
const gchar *
ofo_bat_get_account( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_ACCOUNT );
}

/**
 * ofo_bat_get_acc_user:
 * @bat: this #ofoBat object.
 *
 * Returns: the importer user.
 */
const gchar *
ofo_bat_get_acc_user( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, string, NULL, BAT_ACC_USER );
}

/**
 * ofo_bat_get_acc_stamp:
 * @bat: this #ofoBat object.
 *
 * Returns: the import timestamp.
 */
const GTimeVal *
ofo_bat_get_acc_stamp( ofoBat *bat )
{
	ofo_base_getter( BAT, bat, timestamp, NULL, BAT_ACC_STAMP );
}

/**
 * ofo_bat_exists:
 * @getter: a #ofaIGetter instance.
 * @rib:
 * @begin:
 * @end:
 *
 * Returns %TRUE if the Bank Account Transaction file has already been
 * imported, and display a message dialog box in this case.
 */
gboolean
ofo_bat_exists( ofaIGetter *getter, const gchar *rib, const GDate *begin, const GDate *end )
{
	ofaHub *hub;
	gboolean exists;
	gchar *sbegin, *send;
	GString *query;
	gint count;
	gchar *primary, *secondary;
	GtkWidget *dialog;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	exists = FALSE;
	hub = ofa_igetter_get_hub( getter );

	sbegin = my_date_to_str( begin, MY_DATE_SQL );
	send = my_date_to_str( end, MY_DATE_SQL );

	query = g_string_new( "SELECT COUNT(*) FROM OFA_T_BAT WHERE ");

	g_string_append_printf( query, "BAT_RIB='%s' ", rib );

	if( my_date_is_valid( begin )){
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sbegin );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN IS NULL " );
	}

	if( my_date_is_valid( end )){
		g_string_append_printf( query, "AND BAT_END='%s'", send );
	} else {
		g_string_append_printf( query, "AND BAT_END IS NULL" );
	}

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query->str, &count, TRUE );

	if( count > 0 ){
		exists = TRUE;
		primary = g_strdup(
				_( "The candidate Bank Account Transaction file has already been imported.\n"
						"A new import is refused." ));
		secondary = g_strdup_printf(
				_( "\tRIB\t\t= '%s'\n"
						"\tBegin\t= '%s'\n"
						"\tEnd\t\t= '%s'" ), rib, sbegin, send );
		dialog = gtk_message_dialog_new(
						NULL,
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_WARNING,
						GTK_BUTTONS_CLOSE,
						"%s", primary );
		gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dialog ), "%s", secondary );
		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );
		g_free( secondary );
		g_free( primary );
	}

	g_string_free( query, TRUE );
	g_free( send );
	g_free( sbegin );

	return( exists );
}

/**
 * ofo_bat_is_deletable:
 *
 * An imported BAT file may be removed from the database if none of its
 * lines has been yet reconciliated.
 */
gboolean
ofo_bat_is_deletable( ofoBat *bat )
{
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	count = ofo_bat_get_used_count( bat );

	return( count == 0 );
}

/**
 * ofo_bat_get_lines_count:
 * @bat: this #ofoBat instance.
 *
 * Returns: the count of lines in this BAT.
 */
gint
ofo_bat_get_lines_count( ofoBat *bat )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	query = g_strdup_printf(
					"SELECT COUNT(*) FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	getter = ofo_base_get_getter( OFO_BASE( bat ));
	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_bat_get_used_count:
 * @bat: this #ofoBat instance.
 *
 * Returns the count of used lines from this BAT file, i.e. the count
 * of lines which belong to a conciliation group.
 */
gint
ofo_bat_get_used_count( ofoBat *bat )
{
	gchar *query;
	ofaIGetter *getter;
	ofaHub *hub;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_CONCIL_IDS WHERE "
			"	REC_IDS_TYPE='%s' AND REC_IDS_OTHER IN "
			"		(SELECT BAT_LINE_ID FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld)",
			CONCIL_TYPE_BAT, ofo_bat_get_id( bat ));

	getter = ofo_base_get_getter( OFO_BASE( bat ));
	hub = ofa_igetter_get_hub( getter );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

/*
 * bat_set_id:
 * @bat: this #ofoBat object.
 * @id: the @bat identifier.
 *
 * Set @id.
 */
static void
bat_set_id( ofoBat *bat, ofxCounter id )
{
	ofo_base_setter( BAT, bat, counter, BAT_ID, id );
}

/**
 * ofo_bat_set_uri:
 * @bat: this #ofoBat object.
 * @uri: the source URI.
 *
 * Set @uri.
 */
void
ofo_bat_set_uri( ofoBat *bat, const gchar *uri )
{
	ofo_base_setter( BAT, bat, counter, BAT_URI, uri );
}

/**
 * ofo_bat_set_format:
 * @bat: this #ofoBat object.
 * @format: the import format.
 *
 * Set @format.
 */
void
ofo_bat_set_format( ofoBat *bat, const gchar *format )
{
	ofo_base_setter( BAT, bat, counter, BAT_FORMAT, format );
}

/**
 * ofo_bat_set_begin_date:
 * @bat: this #ofoBat object.
 * @date: the beginning date.
 *
 * Set @date.
 */
void
ofo_bat_set_begin_date( ofoBat *bat, const GDate *date )
{
	ofo_base_setter( BAT, bat, date, BAT_BEGIN, date );
}

/**
 * ofo_bat_set_begin_solde:
 * @bat: this #ofoBat object.
 * @solde: the beginning solde.
 *
 * Set @solde.
 */
void
ofo_bat_set_begin_solde( ofoBat *bat, ofxAmount solde )
{
	ofo_base_setter( BAT, bat, amount, BAT_SOLDE_BEGIN, solde );
}

/**
 * ofo_bat_set_begin_solde_set:
 */
void
ofo_bat_set_begin_solde_set( ofoBat *bat, gboolean set )
{
	ofo_base_setter( BAT, bat, string, BAT_SOLDE_BEGIN_SET, set ? "Y":"N" );
}

/**
 * ofo_bat_set_end_date:
 * @bat: this #ofoBat object.
 * @date: the ending date.
 *
 * Set @date.
 */
void
ofo_bat_set_end_date( ofoBat *bat, const GDate *date )
{
	ofo_base_setter( BAT, bat, date, BAT_END, date );
}

/**
 * ofo_bat_set_end_solde:
 * @bat: this #ofoBat object.
 * @solde: the ending solde.
 *
 * Set @solde.
 */
void
ofo_bat_set_end_solde( ofoBat *bat, ofxAmount solde )
{
	ofo_base_setter( BAT, bat, amount, BAT_SOLDE_END, solde );
}

/**
 * ofo_bat_set_end_solde_set:
 */
void
ofo_bat_set_end_solde_set( ofoBat *bat, gboolean set )
{
	ofo_base_setter( BAT, bat, string, BAT_SOLDE_END_SET, set ? "Y":"N" );
}

/**
 * ofo_bat_set_rib:
 * @bat: this #ofoBat object.
 * @rib: the provided RIB.
 *
 * Set @rib.
 */
void
ofo_bat_set_rib( ofoBat *bat, const gchar *rib )
{
	ofo_base_setter( BAT, bat, string, BAT_RIB, rib );
}

/**
 * ofo_bat_set_currency:
 * @bat: this #ofoBat object.
 * @currency: the provided currency.
 *
 * Set @currency.
 */
void
ofo_bat_set_currency( ofoBat *bat, const gchar *currency )
{
	ofo_base_setter( BAT, bat, string, BAT_CURRENCY, currency );
}

/*
 * bat_set_cre_user:
 * @bat: this #ofoBat object.
 * @user: the user.
 *
 * Set @cre_user.
 */
static void
bat_set_cre_user( ofoBat *bat, const gchar *user )
{
	ofo_base_setter( BAT, bat, string, BAT_CRE_USER, user );
}

/*
 * bat_set_cre_stamp:
 * @bat: this #ofoBat object.
 * @stamp: the timestamp.
 *
 * Set @cre_stamp.
 */
static void
bat_set_cre_stamp( ofoBat *bat, const GTimeVal *stamp )
{
	ofo_base_setter( BAT, bat, timestamp, BAT_CRE_STAMP, stamp );
}

/*
 * bat_set_notes:
 */
static void
bat_set_notes( ofoBat *bat, const gchar *notes )
{
	ofo_base_setter( BAT, bat, string, BAT_NOTES, notes );
}

/*
 * bat_set_upd_user:
 * @bat: this #ofoBat object.
 * @user: the user.
 *
 * Set @upd_user.
 */
static void
bat_set_upd_user( ofoBat *bat, const gchar *user )
{
	ofo_base_setter( BAT, bat, string, BAT_UPD_USER, user );
}

/*
 * bat_set_upd_stamp:
 * @bat: this #ofoBat object.
 * @stamp: the timestamp.
 *
 * Set @upd_stamp.
 */
static void
bat_set_upd_stamp( ofoBat *bat, const GTimeVal *stamp )
{
	ofo_base_setter( BAT, bat, timestamp, BAT_UPD_STAMP, stamp );
}

/*
 * bat_set_account:
 */
static void
bat_set_account( ofoBat *bat, const gchar *account )
{
	ofo_base_setter( BAT, bat, string, BAT_ACCOUNT, account );
}

/*
 * bat_set_acc_user:
 * @bat: this #ofoBat object.
 * @user: the user.
 *
 * Set @acc_user.
 */
static void
bat_set_acc_user( ofoBat *bat, const gchar *user )
{
	ofo_base_setter( BAT, bat, string, BAT_ACC_USER, user );
}

/*
 * bat_set_acc_stamp:
 * @bat: this #ofoBat object.
 * @stamp: the timestamp.
 *
 * Set @acc_stamp.
 */
static void
bat_set_acc_stamp( ofoBat *bat, const GTimeVal *stamp )
{
	ofo_base_setter( BAT, bat, timestamp, BAT_ACC_STAMP, stamp );
}

/**
 * ofo_bat_doc_get_count:
 * @bat: this #ofoBat object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_bat_doc_get_count( ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	priv = ofo_bat_get_instance_private( bat );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_bat_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown BAT_ID in OFA_T_BAT_DOC child table.
 *
 * The returned list should be #ofo_bat_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_bat_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_BAT_DOC" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	ofxCounter batid;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DISTINCT(BAT_ID) FROM %s "
			"	WHERE BAT_ID NOT IN (SELECT BAT_ID FROM OFA_T_BAT)", table );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			batid = atol(( const gchar * ) icol->data );
			//g_debug( "bat_get_orphans: data=%s data=%p data=%lu batid=%lu",
			//		( const gchar * ) icol->data, ( void * ) icol->data, ( gulong ) icol->data, batid );
			orphans = g_list_prepend( orphans, ( gpointer ) batid );
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_bat_insert:
 * @bat: a new #ofoBat instance to be added to the database.
 *
 * Insert the new object in the database, updating simultaneously the
 * global dataset.
 *
 * Returns: %TRUE if insertion is successful, %FALSE else.
 */
gboolean
ofo_bat_insert( ofoBat *bat )
{
	static const gchar *thisfn = "ofo_bat_insert";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: bat=%p", thisfn, ( void * ) bat );

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	getter = ofo_base_get_getter( OFO_BASE( bat ));
	signaler = ofa_igetter_get_signaler( getter );
	bat_set_id( bat, ofo_counters_get_next_bat_id( getter ));

	/* rationale: see ofo-account.c */
	ofo_bat_get_dataset( getter );

	if( bat_do_insert( bat, getter )){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( bat ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, bat );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
bat_do_insert( ofoBat *bat, ofaIGetter *getter )
{
	return( bat_insert_main( bat, getter ));
}

static gboolean
bat_insert_main( ofoBat *bat, ofaIGetter *getter )
{
	GString *query;
	ofaHub *hub;
	gchar *suri, *str, *stamp_str;
	const GDate *begin, *end;
	gboolean ok, is_set;
	GTimeVal stamp;
	const gchar *userid, *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	cur_code = ofo_bat_get_currency( bat );
	cur_obj = my_strlen( cur_code ) ? ofo_currency_get_by_code( getter, cur_code ) : NULL;
	g_return_val_if_fail( !cur_obj || OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	ok = FALSE;
	my_stamp_set_now( &stamp );
	suri = my_utils_quote_sql( ofo_bat_get_uri( bat ));
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "INSERT INTO OFA_T_BAT" );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_CRE_USER,BAT_CRE_STAMP,BAT_URI,BAT_FORMAT,BAT_BEGIN,BAT_END,"
			"	 BAT_RIB,BAT_CURRENCY,"
			"	 BAT_SOLDE_BEGIN,BAT_SOLDE_BEGIN_SET,BAT_SOLDE_END,BAT_SOLDE_END_SET) "
			"	VALUES (%ld,'%s','%s','%s'",
					ofo_bat_get_id( bat ),
					userid,
					stamp_str,
					suri );

	g_free( suri );

	str = my_utils_quote_sql( ofo_bat_get_format( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, ",'%s'", str );
	} else {
		query = g_string_append( query, ",NULL" );
	}
	g_free( str );

	begin = ofo_bat_get_begin_date( bat );
	if( my_date_is_valid( begin )){
		str = my_date_to_str( begin, MY_DATE_SQL );
		g_string_append_printf( query, ",'%s'", str );
		g_free( str );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	end = ofo_bat_get_end_date( bat );
	if( my_date_is_valid( end )){
		str = my_date_to_str( end, MY_DATE_SQL );
		g_string_append_printf( query, ",'%s'", str );
		g_free( str );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	str = ( gchar * ) ofo_bat_get_rib( bat );
	if( my_strlen( str )){
		g_string_append_printf( query, ",'%s'", str );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	if( my_strlen( cur_code )){
		g_string_append_printf( query, ",'%s'", cur_code );
	} else {
		query = g_string_append( query, ",NULL" );
	}

	is_set = ofo_bat_get_begin_solde_set( bat );
	if( is_set ){
		str = ofa_amount_to_sql( ofo_bat_get_begin_solde( bat ), cur_obj );
		g_string_append_printf( query, ",%s,'Y'", str );
		g_free( str );
	} else {
		query = g_string_append( query, ",NULL,'N'" );
	}

	is_set = ofo_bat_get_end_solde_set( bat );
	if( is_set ){
		str = ofa_amount_to_sql( ofo_bat_get_end_solde( bat ), cur_obj );
		g_string_append_printf( query, ",%s,'Y'", str );
		g_free( str );
	} else {
		query = g_string_append( query, ",NULL,'N'" );
	}

	query = g_string_append( query, ")" );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		bat_set_cre_user( bat, userid );
		bat_set_cre_stamp( bat, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_update_notes:
 * @bat: this #ofoBat object.
 * @notes: the associated notes.
 *
 * Set @notes, and update the DBMS.
 *
 * Returns: %TRUE if the DBMS update has been successful.
 */
gboolean
ofo_bat_update_notes( ofoBat *bat, const gchar *notes )
{
	bat_set_notes( bat, notes );

	return( bat_do_update_notes( bat ));
}

static gboolean
bat_do_update_notes( ofoBat *bat )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GString *query;
	gchar *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	ok = FALSE;

	getter = ofo_base_get_getter( OFO_BASE( bat ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	notes = my_utils_quote_sql( ofo_bat_get_notes( bat ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_BAT SET " );

	if( my_strlen( notes )){
		g_string_append_printf( query, "BAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "BAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	BAT_UPD_USER='%s',BAT_UPD_STAMP='%s'"
			"	WHERE BAT_ID=%ld", userid, stamp_str, ofo_bat_get_id( bat ));

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		bat_set_upd_user( bat, userid );
		bat_set_upd_stamp( bat, &stamp );

		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, bat, NULL );

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_update_account:
 * @bat: this #ofoBat object.
 * @account: the account associated to @bat in Openbook.
 *
 * Set @account, and update the DBMS.
 *
 * Returns: %TRUE if the DBMS update has been successful.
 */
gboolean
ofo_bat_update_account( ofoBat *bat, const gchar *account )
{
	bat_set_account( bat, account );

	return( bat_do_update_account( bat ));
}

static gboolean
bat_do_update_account( ofoBat *bat )
{
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GString *query;
	gchar *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid, *caccount;

	ok = FALSE;

	getter = ofo_base_get_getter( OFO_BASE( bat ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_BAT SET " );

	caccount = ofo_bat_get_account( bat );
	if( my_strlen( caccount )){
		g_string_append_printf( query, "BAT_ACCOUNT='%s',", caccount );
	} else {
		query = g_string_append( query, "BAT_ACCOUNT=NULL," );
	}

	g_string_append_printf( query,
			"	BAT_ACC_USER='%s',BAT_ACC_STAMP='%s'"
			"	WHERE BAT_ID=%ld", userid, stamp_str, ofo_bat_get_id( bat ));

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		bat_set_acc_user( bat, userid );
		bat_set_acc_stamp( bat, &stamp );

		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, bat, NULL );

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_delete:
 */
gboolean
ofo_bat_delete( ofoBat *bat )
{
	static const gchar *thisfn = "ofo_bat_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gboolean ok;
	ofxCounter bat_id;

	g_debug( "%s: bat=%p", thisfn, ( void * ) bat );

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( ofo_bat_is_deletable( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( bat ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	bat_id = ofo_bat_get_id( bat );

	if( bat_do_delete_main( bat, connect, bat_id ) &&
			bat_do_delete_lines( bat, connect, bat_id ) &&
			bat_do_delete_doc( bat, connect, bat_id )){

		g_object_ref( bat );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( bat ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, bat );
		g_object_unref( bat );
		ok = TRUE;
	}

	return( ok );
}

/*
 * when the ofoBat does not yet have an identifier
 */
static gboolean
bat_do_delete_by_where( ofoBat *bat, const ofaIDBConnect *connect )
{
	ofxCounter bat_id;
	gboolean ok;

	ok = TRUE;
	bat_id = bat_get_id_by_where( bat, connect );

	if( bat_id > 0 ){
		ok &= bat_do_delete_main( bat, connect, bat_id );
		ok &= bat_do_delete_lines( bat, connect, bat_id );
		ok &= bat_do_delete_doc( bat, connect, bat_id );
	}

	return( ok );
}

static gboolean
bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf( "DELETE FROM OFA_T_BAT WHERE BAT_ID=%ld", bat_id );
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gboolean
bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf( "DELETE FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld", bat_id );
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gboolean
bat_do_delete_doc( ofoBat *bat, const ofaIDBConnect *connect, ofxCounter bat_id )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf( "DELETE FROM OFA_T_BAT_DOC WHERE BAT_ID=%ld", bat_id );
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	return( ok );
}

static gint
bat_cmp_by_id( ofoBat *a, ofxCounter id )
{
	ofxCounter a_id;

	a_id = ofo_bat_get_id( a );

	return( a_id < id ? -1 : ( a_id > id ? 1 : 0 ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_bat_icollectionable_iface_init";

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
					"OFA_T_BAT",
					OFO_TYPE_BAT,
					OFA_IGETTER( user_data ));

	return( dataset );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_bat_idoc_iface_init";

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
	return( g_strdup( _( "_Bank account transaction list" )));
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
 * Exports all the BAT files.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_bat_iexportable_export";

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
	ofoBat *bat;
	ofoBatPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_bat_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += BAT_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		count += ofo_bat_doc_get_count( bat );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, BAT_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					BAT_TABLES_COUNT, st_boxed_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		bat = OFO_BAT( it->data );

		str1 = ofa_box_csv_get_line( OFO_BASE( bat )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_bat_get_instance_private( bat );

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
	static const gchar *thisfn = "ofo_account_iimportable_iface_init";

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
	return( g_strdup( _( "_Bank account transaction list" )));
}

/*
 * ofo_bat_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 *
 * Returns: the total count of errors.
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_BAT" );
		bck_det_table = ofa_idbconnect_table_backup( connect, "OFA_T_BAT_LINES" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_BAT );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_BAT );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_BAT" );
			ofa_idbconnect_table_restore( connect, bck_det_table, "OFA_T_BAT_LINES" );
		}

		g_free( bck_table );
		g_free( bck_det_table );
	}

	if( dataset ){
		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
	}

	return( parms->parse_errs+parms->insert_errs );
}

/*
 * parse a dataset
 *
 * If the importer has been able to provide both begin and and soldes,
 * then we can check the completeness of the lines here.
 */
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	static const gchar *thisfn = "ofo_bat_iimportable_import_parse";
	GList *dataset;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	guint numline, total, type;
	gchar *str;
	ofoBat *bat;
	ofoBatLine *line;
	gint year;
	const GDate *date;
	gboolean checkable;
	ofxAmount end_solde, amount;

	year = 0;
	numline = 0;
	bat = NULL;
	dataset = NULL;
	amount = 0;
	checkable = FALSE;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );
	//my_utils_dump_gslist_str( lines );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;

		/* line type */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = cstr ? atoi( cstr ) : 0;

		/* identifier is a placeholder on import */
		itf = itf ? itf->next : NULL;

		switch( type ){
			case 1:
				bat = iimportable_import_parse_main( importer, parms, numline, itf );
				if( bat ){
					dataset = g_list_prepend( dataset, bat );
					parms->parsed_count += 1;
					checkable = ofo_bat_get_begin_solde_set( bat ) && ofo_bat_get_end_solde_set( bat );
					if( checkable ){
						amount = ofo_bat_get_begin_solde( bat );
					}
				}
				break;
			case 2:
				if( !bat ){
					str = g_strdup_printf( _( "invalid line type %d while BAT not defined" ), type );
					ofa_iimporter_progress_num_text( importer, parms, numline, str );
					g_free( str );
					parms->parse_errs += 1;
					total -= 1;
				} else {
					if( year == 0 ){
						date = ofo_bat_get_begin_date( bat );
						if( my_date_is_valid( date )){
							year = g_date_get_year( date );
						} else {
							date = ofo_bat_get_end_date( bat );
							if( my_date_is_valid( date )){
								year = g_date_get_year( date );
							}
						}
					}
					line = iimportable_import_parse_line( importer, parms, numline, itf, year );
					if( line ){
						dataset = g_list_prepend( dataset, line );
						parms->parsed_count += 1;
						if( checkable ){
							amount += ofo_bat_line_get_amount( line );
						}
					}
				}
				break;
			default:
				str = g_strdup_printf( _( "invalid line type: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				total -= 1;
				continue;
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	if( checkable ){
		end_solde = ofo_bat_get_end_solde( bat );
		if( !my_double_is_zero( amount-end_solde, 1+HUB_DEFAULT_DECIMALS_AMOUNT )){
			str = g_strdup_printf( _( "expected end solde %lf not equal to computed one %lf" ), end_solde, amount );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_debug( "%s: %s", thisfn, str );
			g_free( str );
			parms->parse_errs += 1;
		}
	}

	return( g_list_reverse( dataset ));
}

static ofoBat *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	ofoBat *bat;
	GDate date;

	bat = ofo_bat_new( parms->getter );

	/* uri */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_uri( bat, cstr );
	}

	/* importer label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_format( bat, cstr );
	}

	/* rib */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_rib( bat, cstr );
	}

	/* currency */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_currency( bat, cstr );
	}

	/* begin date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_date( bat, my_date_set_from_str( &date, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* begin solde */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_solde( bat, my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format )));
	}

	/* begin solde set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_solde_set( bat, my_utils_boolean_from_str( cstr ));
	}

	/* end date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_date( bat, my_date_set_from_str( &date, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* end solde */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_solde( bat, my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format ) ));
	}

	/* end solde set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_solde_set( bat, my_utils_boolean_from_str( cstr ));
	}

	return( bat );
}

static ofoBatLine *
iimportable_import_parse_line( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields, gint year )
{
	const gchar *cstr, *sref, *slabel;
	GSList *itf;
	GDate dope, deffect;
	ofoBatLine *batline;
	ofxAmount amount;
	gchar *sdope;
	gint eff_year, eff_month, ope_month;

	batline = ofo_bat_line_new( parms->getter );

	sref = NULL;
	slabel = NULL;
	my_date_clear( &dope );
	my_date_clear( &deffect );
	sdope = NULL;

	/* operation date
	 * LCL PDF provides an operation date without year, i.e. as 'dd.mm'
	 * have to wait until having dealt with effect date to compute the ope year */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		sdope = g_strdup( cstr );
	}

	/* effect date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_deffect( batline,
				my_date_set_from_str( &deffect, cstr, ofa_stream_format_get_date_format( parms->format )));

		/* remediate dope considering that we can have ope=31.12 and effect = 01.01 */
		if( my_strlen( sdope )){
			eff_year = g_date_get_year( &deffect );
			eff_month = g_date_get_month( &deffect );
			my_date_set_from_str_ex( &dope, sdope, ofa_stream_format_get_date_format( parms->format ), &eff_year );
			ope_month = g_date_get_month( &dope );
			if( ope_month > eff_month ){
				g_date_add_years( &dope, -1 );
			}
			ofo_bat_line_set_dope( batline, &dope );
		}
	}

	/* effect date is mandatory */
	if( !my_date_is_valid( &deffect )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "effect date is not set" ));
		parms->parse_errs += 1;
		g_object_unref( batline );
		return( NULL );
	}

	/* reference */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_ref( batline, cstr );
		sref = cstr;
	}

	/* label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_label( batline, cstr );
		slabel = cstr;
	}

	/* at least ref or label must be set */
	if( !my_strlen( sref ) && !my_strlen( slabel )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "neither reference nor label are set" ));
		parms->parse_errs += 1;
		g_object_unref( batline );
		return( NULL );
	}

	/* amount */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		amount = my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format ) );
		ofo_bat_line_set_amount( batline, amount );
	}

	/* currency */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_currency( batline, cstr );
	}

	g_free( sdope );

	return( batline );
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gboolean insert, skipped;
	guint total, type;
	gchar *str, *sdate;
	ofoBase *object;
	const gchar *rib, *label;
	gchar *sdbegin, *sdend;
	ofxCounter bat_id;

	bat_id = 0;
	skipped = FALSE;
	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		bat_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		object = OFO_BASE( it->data );

		if( OFO_IS_BAT( object )){

			/* must only pass here once */
			g_return_if_fail( bat_id == 0 );

			if( bat_get_exists( OFO_BAT( object ), connect )){
				parms->duplicate_count += 1;
				type = MY_PROGRESS_NORMAL;

				rib = ofo_bat_get_rib( OFO_BAT( object ));
				sdbegin = my_date_to_str( ofo_bat_get_begin_date( OFO_BAT( object )), ofa_prefs_date_get_display_format( parms->getter ));
				sdend = my_date_to_str( ofo_bat_get_end_date( OFO_BAT( object )), ofa_prefs_date_get_display_format( parms->getter ));

				switch( parms->mode ){
					case OFA_IDUPLICATE_REPLACE:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, replacing previous one" ), rib, sdbegin, sdend );
						bat_do_delete_by_where( OFO_BAT( object ), connect );
						break;
					case OFA_IDUPLICATE_IGNORE:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, skipped" ), rib, sdbegin, sdend );
						skipped = TRUE;
						insert = FALSE;
						total -= 1;
						break;
					case OFA_IDUPLICATE_ABORT:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, making it erroneous" ), rib, sdbegin, sdend );
						type = MY_PROGRESS_ERROR;
						insert = FALSE;
						total -= 1;
						parms->insert_errs += 1;
						break;
				}

				g_free( sdbegin );
				g_free( sdend );

				if( parms->progress ){
					ofa_iimporter_progress_text( importer, parms, type, str );
				} else {
					my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
				}
				g_free( str );
			}

			if( insert ){
				bat_set_id( OFO_BAT( object ), ofo_counters_get_next_bat_id( parms->getter ));
				if( bat_do_insert( OFO_BAT( object ), parms->getter )){
					parms->inserted_count += 1;
					bat_id = ofo_bat_get_id( OFO_BAT( object ));
					if( parms->importable_data ){
						(( ofsImportedBat * ) parms->importable_data )->bat_id = bat_id;
					}
				} else {
					parms->insert_errs += 1;
				}
			}

		} else {
			g_return_if_fail( OFO_IS_BAT_LINE( it->data ));

			if( bat_id <= 0 ){
				sdate = my_date_to_str( ofo_bat_line_get_dope( OFO_BAT_LINE( it->data )), ofa_prefs_date_get_display_format( parms->getter ));
				label = ofo_bat_line_get_label( OFO_BAT_LINE( it->data ));
				if( skipped ){
					str = g_strdup_printf( _( "%s %s: line ignored due to previous BAT being have been skipped" ), sdate, label );
				} else {
					str = g_strdup_printf( _( "%s %s: line ignored due to previous BAT being have been set erroneous" ), sdate, label );
				}
				ofa_iimporter_progress_text( importer, parms, MY_PROGRESS_NORMAL, str );
				g_free( str );
				g_free( sdate );
				total -= 1;

			} else {
				ofo_bat_line_set_bat_id( OFO_BAT_LINE( it->data ), bat_id );
				if( ofo_bat_line_insert( OFO_BAT_LINE( object ))){
					parms->inserted_count += 1;
				} else {
					parms->insert_errs += 1;
				}
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

/*
 * provided bat is an being-imported one: datas are set but the id
 */
static gboolean
bat_get_exists( ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query, *where;
	gint count;

	count = 0;
	where = bat_get_where( bat );
	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_BAT WHERE %s", where );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );
	g_free( where );

	return( count > 0 );
}

/*
 * build a where clause without using the id counter
 */
static gchar *
bat_get_where( ofoBat *bat )
{
	const GDate *dbegin, *dend;
	const gchar *rib;
	gchar *sdate;
	GString *query;

	dbegin = ofo_bat_get_begin_date( bat );
	dend = ofo_bat_get_end_date( bat );
	rib = ofo_bat_get_rib( bat );

	query = g_string_new( "");

	if( my_strlen( rib )){
		g_string_append_printf( query, "BAT_RIB='%s' ", rib );
	} else {
		g_string_append_printf( query, "BAT_RIB IS NULL " );
	}

	if( my_date_is_valid( dbegin )){
		sdate = my_date_to_str( dbegin, MY_DATE_SQL );
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sdate );
		g_free( sdate );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN IS NULL " );
	}

	if( my_date_is_valid( dend )){
		sdate = my_date_to_str( dend, MY_DATE_SQL );
		g_string_append_printf( query, "AND BAT_END='%s'", sdate );
		g_free( sdate );
	} else {
		g_string_append_printf( query, "AND BAT_END IS NULL" );
	}

	return( g_string_free( query, FALSE ));
}

static ofxCounter
bat_get_id_by_where( ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query, *where;
	GSList *result, *irow, *icol;
	ofxCounter bat_id;

	bat_id = 0;
	where = bat_get_where( bat );
	query = g_strdup_printf( "SELECT BAT_ID FROM OFA_T_BAT WHERE %s", where );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		irow = result;
		icol = ( GSList * ) irow->data;
		bat_id = atol(( gchar * ) icol->data );

		ofa_idbconnect_free_results( result );
	}

	return( bat_id );
}

static gboolean
bat_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_BAT", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_BAT_LINES", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_bat_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_bat_isignalable_connect_to";

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
	static const gchar *thisfn = "ofo_bat_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = signaler_is_deletable_account( signaler, OFO_ACCOUNT( object ));

	} else if( OFO_IS_CURRENCY( object )){
		deletable = signaler_is_deletable_currency( signaler, OFO_CURRENCY( object ));
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
			"SELECT COUNT(*) FROM OFA_T_BAT WHERE BAT_ACCOUNT='%s'",
			ofo_account_get_number( account ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_BAT WHERE BAT_CURRENCY='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	if( count == 0 ){

		query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_BAT_LINES WHERE BAT_LINE_CURRENCY='%s'",
				ofo_currency_get_code( currency ));

		ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

		g_free( query );
	}

	return( count == 0 );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_account_signaler_on_updated_base";
	const gchar *new_id, *new_code;

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
				signaler_on_updated_account_id( signaler, prev_id, new_id );
			}
		}

	} else if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			new_code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( my_collate( new_code, prev_id )){
				signaler_on_updated_currency_code( signaler, prev_id, new_code );
			}
		}
	}
}

static void
signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *prev_id, const gchar *new_id )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
					"UPDATE OFA_T_BAT SET BAT_ACCOUNT='%s' WHERE BAT_ACCOUNT='%s'",
						new_id, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}

static void
signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_code, const gchar *new_code )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
					"UPDATE OFA_T_BAT SET BAT_CURRENCY='%s' WHERE BAT_CURRENCY='%s'",
						new_code, prev_code );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	query = g_strdup_printf(
					"UPDATE OFA_T_BAT_LINES SET BAT_LINE_CURRENCY='%s' WHERE BAT_LINE_CURRENCY='%s'",
						new_code, prev_code );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}
