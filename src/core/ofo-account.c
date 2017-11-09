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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-icollectionable.h"
#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
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
#include "api/ofa-prefs.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-account-v34.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofs-account-balance.h"

/* priv instance data
 */
enum {
	ACC_NUMBER = 1,
	ACC_CRE_USER,
	ACC_CRE_STAMP,
	ACC_LABEL,
	ACC_CURRENCY,
	ACC_ROOT,
	ACC_SETTLEABLE,
	ACC_KEEP_UNSETTLED,
	ACC_RECONCILIABLE,
	ACC_KEEP_UNRECONCILIATED,
	ACC_FORWARDABLE,
	ACC_CLOSED,
	ACC_NOTES,
	ACC_UPD_USER,
	ACC_UPD_STAMP,
	ACC_CR_DEBIT,
	ACC_CR_CREDIT,
	ACC_CV_DEBIT,
	ACC_CV_CREDIT,
	ACC_FR_DEBIT,
	ACC_FR_CREDIT,
	ACC_FV_DEBIT,
	ACC_FV_CREDIT,
	ACC_ARC_DATE,
	ACC_ARC_TYPE,
	ACC_ARC_DEBIT,
	ACC_ARC_CREDIT,
	ACC_DOC_ID,
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
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( ACC_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ROOT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_SETTLEABLE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_KEEP_UNSETTLED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_RECONCILIABLE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_KEEP_UNRECONCILIATED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FORWARDABLE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CLOSED ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CR_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CR_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CV_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CV_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FR_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FR_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FV_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FV_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_archive_defs[] = {
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ARC_DATE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ARC_TYPE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ARC_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ARC_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define ACCOUNT_TABLES_COUNT            3
#define ACCOUNT_EXPORT_VERSION          2

typedef struct {
	GList *archives;					/* archived balances of the account */
	GList *docs;
}
	ofoAccountPrivate;

#define account_get_amount(I)           ofo_base_getter(ACCOUNT,account,amount,0,(I))
#define account_get_counter(I)          ofo_base_getter(ACCOUNT,account,counter,0,(I))
#define account_get_date(I)             ofo_base_getter(ACCOUNT,account,date,NULL,(I))
#define account_get_string(I)           ofo_base_getter(ACCOUNT,account,string,NULL,(I))
#define account_get_timestamp(I)        ofo_base_getter(ACCOUNT,account,timestamp,NULL,(I))

#define account_set_amount(I,V)         ofo_base_setter(ACCOUNT,account,amount,(I),(V))
#define account_set_counter(I,V)        ofo_base_setter(ACCOUNT,account,counter,(I),(V))
#define account_set_date(I,V)           ofo_base_setter(ACCOUNT,account,date,(I),(V))
#define account_set_string(I,V)         ofo_base_setter(ACCOUNT,account,string,(I),(V))
#define account_set_timestamp(I,V)      ofo_base_setter(ACCOUNT,account,timestamp,(I),(V))

/* whether a root account has children, and wich are they ?
 */
typedef struct {
	const ofoAccount *account;
	gint              children_count;
	GList            *children_list;
}
	sChildren;

/* these are legacy exported strings, in case we would import old datas
 */
#define EXPORTED_TYPE_ROOT              "R"
#define EXPORTED_TYPE_DETAIL            "D"
#define EXPORTED_SETTLEABLE             "S"
#define EXPORTED_RECONCILIABLE          "R"
#define EXPORTED_FORWARDABLE            "F"
#define EXPORTED_CLOSED                 "C"

/* manage the type of the account archived balance
 * @dbms: the character stored in dbms
 * @sls: a short localized string
 * @lls: a long localized string
 */
typedef struct {
	ofeAccountType type;
	const gchar   *dbms;
	const gchar   *sls;
	const gchar   *lls;
}
	sType;

static sType st_type[] = {
		{ ACC_TYPE_OPEN,   "O", N_( "O" ), N_( "Opening" ) },
		{ ACC_TYPE_NORMAL, "N", N_( "N" ), N_( "Normal" ) },
		{ 0 },
};

static void         archives_list_free_detail( GList *fields );
static void         archives_list_free( ofoAccount *account );
static ofoAccount  *account_find_by_number( GList *set, const gchar *number );
static const gchar *account_get_string_ex( const ofoAccount *account, gint data_id );
static void         account_get_children( const ofoAccount *account, sChildren *child_str );
static void         account_iter_children( const ofoAccount *account, sChildren *child_str );
static void         account_set_cre_user( ofoAccount *account, const gchar *user );
static void         account_set_cre_stamp( ofoAccount *account, const GTimeVal *stamp );
static void         account_set_upd_user( ofoAccount *account, const gchar *user );
static void         account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp );
static gboolean     archive_do_add_dbms( ofoAccount *account, const GDate *date, ofeAccountType type, ofxAmount debit, ofxAmount credit );
static gboolean     archive_do_add_list( ofoAccount *account, const GDate *date, ofeAccountType type, ofxAmount debit, ofxAmount credit );
static gint         archive_get_last_index( ofoAccount *account, const GDate *requested );
static GList       *get_orphans( ofaIGetter *getter, const gchar *table );
static gboolean     account_do_insert( ofoAccount *account, const ofaIDBConnect *connect );
static gboolean     account_do_update( ofoAccount *account, const ofaIDBConnect *connect, const gchar *prev_number );
static gboolean     account_do_update_arc( ofoAccount *account, const ofaIDBConnect *connect, const gchar *prev_number );
static gboolean     account_do_update_amounts( ofoAccount *account, ofaIGetter *getter );
static gboolean     account_do_delete( ofoAccount *account, const ofaIDBConnect *connect );
static gint         account_cmp_by_number( const ofoAccount *a, const gchar *number );
static void         icollectionable_iface_init( myICollectionableInterface *iface );
static guint        icollectionable_get_interface_version( void );
static GList       *icollectionable_load_collection( void *user_data );
static void         idoc_iface_init( ofaIDocInterface *iface );
static guint        idoc_get_interface_version( void );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( void );
static gchar       *iexportable_get_label( const ofaIExportable *instance );
static gboolean     iexportable_get_published( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean     iexportable_export_default( ofaIExportable *exportable );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( void );
static gchar       *iimportable_get_label( const ofaIImportable *instance );
static guint        iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList       *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static void         iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean     account_get_exists( const ofoAccount *account, const ofaIDBConnect *connect );
static gboolean     account_drop_content( const ofaIDBConnect *connect );
static void         isignalable_iface_init( ofaISignalableInterface *iface );
static void         isignalable_connect_to( ofaISignaler *signaler );
static gboolean     signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean     signaler_is_deletable_class( ofaISignaler *signaler, ofoClass *class );
static gboolean     signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency );
static void         signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, void *empty );
static void         signaler_on_new_base_entry( ofaISignaler *signaler, ofoEntry *entry );
static void         signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static void         signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code );
static void         signaler_on_entry_period_status_changed( ofaISignaler *signaler, ofoEntry *entry, ofeEntryPeriod prev_period, ofeEntryStatus prev_status, ofeEntryPeriod new_period, ofeEntryStatus new_status, void *empty );

G_DEFINE_TYPE_EXTENDED( ofoAccount, ofo_account, OFO_TYPE_ACCOUNT_V34, 0,
		G_ADD_PRIVATE( ofoAccount )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

static void
archives_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
archives_list_free( ofoAccount *account )
{
	ofoAccountPrivate *priv;

	priv = ofo_account_get_instance_private( account );

	if( priv->archives ){
		g_list_free_full( priv->archives, ( GDestroyNotify ) archives_list_free_detail );
		priv->archives = NULL;
	}
}

static void
account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_NUMBER ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_LABEL ));

	g_return_if_fail( instance && OFO_IS_ACCOUNT( instance ));

	/* free data members here */
	archives_list_free( OFO_ACCOUNT( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->finalize( instance );
}

static void
account_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_ACCOUNT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->dispose( instance );
}

static void
ofo_account_init( ofoAccount *self )
{
	static const gchar *thisfn = "ofo_account_init";
	ofoAccountPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_account_get_instance_private( self );

	priv->archives = NULL;
	priv->docs = NULL;
}

static void
ofo_account_class_init( ofoAccountClass *klass )
{
	static const gchar *thisfn = "ofo_account_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_finalize;
}

/**
 * ofo_account_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoAccount dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_account_get_dataset( ofaIGetter *getter )
{
	myICollector * collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );

	return( my_icollector_collection_get( collector, OFO_TYPE_ACCOUNT, getter ));
}

/**
 * ofo_account_get_dataset_for_solde:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the #ofoAccount dataset, without the solde accounts.
 *
 * The returned list should be #ofo_account_free_dataset() by the caller.
 */
GList *
ofo_account_get_dataset_for_solde( ofaIGetter *getter )
{
	GList *dataset;
	gchar *query;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	query = g_strdup_printf( "OFA_T_ACCOUNTS WHERE "
			"	ACC_ROOT!='Y' AND "
			"	ACC_NUMBER NOT IN (SELECT DOS_SLD_ACCOUNT FROM OFA_T_DOSSIER_CUR)" );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query,
					OFO_TYPE_ACCOUNT,
					getter );

	g_free( query );

	return( dataset );
}

/**
 * ofo_account_get_by_number:
 * @getter: a #ofaIGetter instance.
 * @number: the required account number.
 *
 * Returns: the searched #ofoAccount account, or %NULL.
 *
 * The returned object is owned by the #ofoAccount class, and should
 * not be unreffed by the caller.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
ofoAccount *
ofo_account_get_by_number( ofaIGetter *getter, const gchar *number )
{
	GList *dataset;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !my_strlen( number )){
		return( NULL );
	}

	dataset = ofo_account_get_dataset( getter );

	return( account_find_by_number( dataset, number ));
}

static ofoAccount *
account_find_by_number( GList *set, const gchar *number )
{
	GList *found;

	found = g_list_find_custom(
				set, number, ( GCompareFunc ) account_cmp_by_number );
	if( found ){
		return( OFO_ACCOUNT( found->data ));
	}

	return( NULL );
}

/**
 * ofo_account_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofoAccount object.
 */
ofoAccount *
ofo_account_new( ofaIGetter *getter )
{
	ofoAccount *account;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	account = g_object_new( OFO_TYPE_ACCOUNT, "ofo-base-getter", getter, NULL );
	OFO_BASE( account )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( account );
}

/**
 * ofo_account_get_class:
 * @account: the #ofoAccount account
 *
 * Returns: the class number of the @account.
 */
gint
ofo_account_get_class( const ofoAccount *account )
{
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	return( ofo_account_get_class_from_number( ofo_account_get_number( account )));
}

/**
 * ofo_account_get_class_from_number:
 * @account_number: an account number.
 *
 * Returns: the class number of this @account_number.
 *
 * TODO: make this UTF-8-compliant as first character (a digit) may not
 * always be one byte wide (or is it ?)
 */
gint
ofo_account_get_class_from_number( const gchar *account_number )
{
	gchar *number;
	gint class;

	g_return_val_if_fail( my_strlen( account_number ), 0 );

	number = g_strdup( account_number );
	number[1] = '\0';
	class = atoi( number );
	g_free( number );

	return( class );
}

/**
 * ofo_account_get_level_from_number:
 * @account_number: an account number.
 *
 * Returns: the level number of this @account_number.
 *
 * The level is defined as the count of digits.
 * A class is defined as of level 1.
 * Any actual account is at least of level 2.
 *
 * Note: this is UTF-8-compliant as g_utf8_strlen() rightly returns
 *  the count of characters.
 */
gint
ofo_account_get_level_from_number( const gchar *account_number )
{
	return( my_strlen( account_number ));
}

/**
 * ofo_account_get_number:
 * @account: the #ofoAccount account
 *
 * Returns: the number of the @account.
 */
const gchar *
ofo_account_get_number( const ofoAccount *account )
{
	account_get_string( ACC_NUMBER );
}

/**
 * ofo_account_get_cre_user:
 * @account: the #ofoAccount account
 *
 * Returns: the user name responsible of the last properties update.
 */
const gchar *
ofo_account_get_cre_user( const ofoAccount *account )
{
	account_get_string( ACC_CRE_USER );
}

/**
 * ofo_account_get_cre_stamp:
 * @account: the #ofoAccount account
 *
 * Returns: the timestamp of the last properties update.
 */
const GTimeVal *
ofo_account_get_cre_stamp( const ofoAccount *account )
{
	account_get_timestamp( ACC_CRE_STAMP );
}

/**
 * ofo_account_get_label:
 * @account: the #ofoAccount account
 *
 * Returns: the label of the @account.
 */
const gchar *
ofo_account_get_label( const ofoAccount *account )
{
	account_get_string( ACC_LABEL );
}

/**
 * ofo_account_get_currency:
 * @account: the #ofoAccount account
 *
 * Returns: the currency ISO 3A code of the @account.
 */
const gchar *
ofo_account_get_currency( const ofoAccount *account )
{
	account_get_string( ACC_CURRENCY );
}

/**
 * ofo_account_is_root:
 * @account: the #ofoAccount account.
 *
 * Returns: %TRUE if the @account is a root account, %FALSE if this is
 * a detail account.
 */
gboolean
ofo_account_is_root( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_ROOT );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_is_settleable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is settleable.
 */
gboolean
ofo_account_is_settleable( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_SETTLEABLE );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_get_keep_unsettled:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if unsettled entries on this account should be kept
 * on exercice closing.
 *
 * Only unsettled entries written on settleable accounts whith this
 * flag set will be reported on next exercice at closing time.
 */
gboolean
ofo_account_get_keep_unsettled( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_KEEP_UNSETTLED );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_is_reconciliable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is reconciliable.
 */
gboolean
ofo_account_is_reconciliable( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_RECONCILIABLE );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_get_keep_unreconciliated:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if unreconciliated entries on this account should be
 * kept on exercice closing.
 *
 * Only unreconciliated entries written on reconciliable accounts whith
 * this flag set will be reported on next exercice at closing time.
 */
gboolean
ofo_account_get_keep_unreconciliated( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_KEEP_UNRECONCILIATED );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_is_forwardable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account supports carried forward entries.
 */
gboolean
ofo_account_is_forwardable( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_FORWARDABLE );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_is_closed:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is closed.
 */
gboolean
ofo_account_is_closed( const ofoAccount *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_CLOSED );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_get_notes:
 * @account: the #ofoAccount account
 *
 * Returns: the notes attached to the @account.
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	account_get_string( ACC_NOTES );
}

/**
 * ofo_account_get_upd_user:
 * @account: the #ofoAccount account
 *
 * Returns: the user name responsible of the last properties update.
 */
const gchar *
ofo_account_get_upd_user( const ofoAccount *account )
{
	account_get_string( ACC_UPD_USER );
}

/**
 * ofo_account_get_upd_stamp:
 * @account: the #ofoAccount account
 *
 * Returns: the timestamp of the last properties update.
 */
const GTimeVal *
ofo_account_get_upd_stamp( const ofoAccount *account )
{
	account_get_timestamp( ACC_UPD_STAMP );
}

/**
 * ofo_account_get_current_rough_debit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of rough entries for the current exercice.
 */
ofxAmount
ofo_account_get_current_rough_debit( const ofoAccount *account )
{
	account_get_amount( ACC_CR_DEBIT );
}

/**
 * ofo_account_get_current_rough_credit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of credits of rough entries for the current exercice.
 */
ofxAmount
ofo_account_get_current_rough_credit( const ofoAccount *account )
{
	account_get_amount( ACC_CR_CREDIT );
}

/**
 * ofo_account_get_current_val_debit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of validated entries for the current exercice.
 */
ofxAmount
ofo_account_get_current_val_debit( const ofoAccount *account )
{
	account_get_amount( ACC_CV_DEBIT );
}

/**
 * ofo_account_get_current_val_credit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of credits of validated entries for the current exercice.
 */
ofxAmount
ofo_account_get_current_val_credit( const ofoAccount *account )
{
	account_get_amount( ACC_CV_CREDIT );
}

/**
 * ofo_account_get_futur_rough_debit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of rought entries for a future exercice.
 */
ofxAmount
ofo_account_get_futur_rough_debit( const ofoAccount *account )
{
	account_get_amount( ACC_FR_DEBIT );
}

/**
 * ofo_account_get_futur_rough_credit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of rought entries for a future exercice.
 */
ofxAmount
ofo_account_get_futur_rough_credit( const ofoAccount *account )
{
	account_get_amount( ACC_FR_CREDIT );
}

/**
 * ofo_account_get_futur_val_debit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of validated entries for a future exercice.
 */
ofxAmount
ofo_account_get_futur_val_debit( const ofoAccount *account )
{
	account_get_amount( ACC_FV_DEBIT );
}

/**
 * ofo_account_get_futur_val_credit:
 * @account: the #ofoAccount account.
 *
 * Returns: the sum of debits of validated entries for a future exercice.
 */
ofxAmount
ofo_account_get_futur_val_credit( const ofoAccount *account )
{
	account_get_amount( ACC_FV_CREDIT );
}

/**
 * ofo_account_get_solde_at_date:
 * @account: the #ofoAccount account.
 * @date: [allow-none]: the requested effect date;
 *  if unset, then all entries are taken into account (including future).
 * @deffect: [out][allow-none]: the actual greatest effect date found.
 * @debit: [out][allow-none]: the total of the debits to be considered.
 * @credit: [out][allow-none]: the total of the credits to be considered.
 *
 * Compute the actual solde of the @account at the requested @date.
 *
 * This take into account all rough+validated entries from current and
 * future effect dates, until the given @date.
 *
 * Returns: the solde at @date as @credit - @debit.
 */
ofxAmount
ofo_account_get_solde_at_date( ofoAccount *account, const GDate *date, GDate *deffect, ofxAmount *debit, ofxAmount *credit )
{
	static const gchar *thisfn = "ofo_account_get_solde_at_date";
	ofxAmount sdebit, scredit;
	gint idx, cmp;
	const GDate *arc_date, *ent_deffect;
	const gchar *acc_number;
	GDate max_deffect;
	GList *dataset, *it;
	ofoEntry *entry;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofeAccountType arc_type;
	ofeEntryRule rule;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	sdebit = 0;
	scredit = 0;
	idx = archive_get_last_index( account, date );
	if( idx == -1 ){
		arc_date = NULL;
		arc_type = 0;
		g_debug( "%s: no archive found", thisfn );
	} else {
		arc_date = ofo_account_archive_get_date( account, idx );
		arc_type = ofo_account_archive_get_type( account, idx );
		sdebit = ofo_account_archive_get_debit( account, idx );
		scredit = ofo_account_archive_get_credit( account, idx );
		gchar *str = my_date_to_str( arc_date, MY_DATE_SQL );
		g_debug( "%s: found archive date=%s, debit=%lf, credit=%lf", thisfn, str, sdebit, scredit );
		g_free( str );
	}

	my_date_clear( &max_deffect );
	acc_number = ofo_account_get_number( account );
	dataset = ofo_entry_get_dataset( ofo_base_get_getter( OFO_BASE( account )));

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( my_collate( ofo_entry_get_account( entry ), acc_number ) != 0 ){
			continue;
		}
		status = ofo_entry_get_status( entry );
		if( status == ENT_STATUS_DELETED ){
			continue;
		}
		period = ofo_entry_get_period( entry );
		if( period == ENT_PERIOD_PAST ){
			continue;
		}
		ent_deffect = ofo_entry_get_deffect( entry );
		/* must have ent_deffect > arc_date if set
		 *   unless arc_type is 'opening' which means we should also consider this date
		 * if arc_date is not set, then the entry is candidate */
		if( arc_date ){
			cmp = my_date_compare( ent_deffect, arc_date );
			if( cmp < 0 ){
				continue;
			}
			if( arc_type == ACC_TYPE_NORMAL && cmp == 0 ){
				continue;
			}
			if( arc_type == ACC_TYPE_OPEN ){
				rule = ofo_entry_get_rule( entry );
				if( rule == ENT_RULE_FORWARD ){
					continue;
				}
			}
		}
		/* only consider entries before or equal to the requested date (if set) */
		if( my_date_is_valid( date ) && my_date_compare( ent_deffect, date ) > 0 ){
			//g_debug( "%s: %lu is after requested date", thisfn, ofo_entry_get_number( entry ));
			continue;
		}

		sdebit += ofo_entry_get_debit( entry );
		scredit += ofo_entry_get_credit( entry );
		//g_debug( "%s: adding %lu entry, solde=%lf", thisfn, ofo_entry_get_number( entry ), solde );

		/* compute the max deffect */
		if( deffect &&
				( !my_date_is_valid( &max_deffect ) || my_date_compare( &max_deffect, ent_deffect ) < 0 )){

			my_date_set_from_date( &max_deffect, ent_deffect );
		}
	}

	if( deffect ){
		my_date_clear( deffect );
		if( my_date_is_valid( &max_deffect )){
			my_date_set_from_date( deffect, &max_deffect );
		}
	}

	if( debit ){
		*debit = sdebit;
	}
	if( credit ){
		*credit = scredit;
	}

	return( scredit-sdebit );
}

/**
 * ofo_account_is_deletable:
 * @account: the #ofoAccount account
 *
 * A account is considered to be deletable if no entry is referencing
 * it.
 *
 * Whether a root account with children is deletable is a user
 * preference.
 *
 * To be deletable, all children must also be deletable.
 *
 * It is up to the caller to decide if the account may be deleted,
 * regarding if the currently opened dossier is current or an archive.
 */
gboolean
ofo_account_is_deletable( const ofoAccount *account )
{
	ofaIGetter *getter;
	gboolean deletable;
	GList *children, *it;
	ofaISignaler *signaler;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	deletable = TRUE;
	getter = ofo_base_get_getter( OFO_BASE( account ));

	if( ofo_account_is_root( account ) && ofa_prefs_account_get_delete_with_children( getter )){
		children = ofo_account_get_children( account );
		for( it=children ; it && deletable ; it=it->next ){
			deletable &= ofo_account_is_deletable( OFO_ACCOUNT( it->data ));
		}
		g_list_free( children );
	}

	if( deletable ){
		signaler = ofa_igetter_get_signaler( getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_IS_DELETABLE, account, &deletable );
	}

	return( deletable );
}

static const gchar *
account_get_string_ex( const ofoAccount *account, gint data_id )
{
	account_get_string( data_id );
}

/**
 * ofo_account_is_valid_data:
 * @number:
 * @label:
 * @currency:
 * @root:
 * @msgerr: [allow-none]:
 *
 * Returns: %TRUE if provided datas are valid for an account.
 */
gboolean
ofo_account_is_valid_data( const gchar *number, const gchar *label, const gchar *currency, gboolean root, gchar **msgerr )
{
	static const gchar *thisfn = "ofo_account_is_valid_data";
	gunichar code;
	gint value;

	if( 0 ){
		g_debug( "%s: number=%s, label=%s, currency=%s, root=%s",
				thisfn, number, label, currency, root ? "True":"False" );
	}

	if( msgerr ){
		*msgerr = NULL;
	}

	/* is account number valid ?
	 * must begin with a digit, and be at least two chars
	 */
	if( my_strlen( number ) < 2 ){
		if( msgerr ){
			if( !my_strlen( number )){
				*msgerr = g_strdup( _( "Account identifier is not set" ));
			} else {
				*msgerr = g_strdup_printf( _( "Account identifier '%s' is too short" ), number );
			}
		}
		return( FALSE );
	}
	code = g_utf8_get_char( number );
	value = g_unichar_digit_value( code );
	/*g_debug( "ofo_account_is_valid_data: number=%s, code=%c, value=%d", number, code, value );*/
	if( value < 1 ){
		if( msgerr ){
			*msgerr = g_strdup( _( "Account class is expected to be numeric" ));
		}
		return( FALSE );
	}

	/* label */
	if( !my_strlen( label )){
		if( msgerr ){
			*msgerr = g_strdup( _( "Account label is empty" ));
		}
		return( FALSE );
	}

	/* currency must be set for detail account */
	if( !root ){
		if( !my_strlen( currency )){
			if( msgerr ){
				*msgerr = g_strdup( _( "Currency must be set for detail account" ));
			}
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_account_has_children:
 * @account: the #ofoAccount account
 *
 * Whether an account has children is only relevant for a root account.
 */
gboolean
ofo_account_has_children( const ofoAccount *account )
{
	sChildren child_str;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	if( !ofo_account_is_root( account )){
		return( FALSE );
	}

	account_get_children( account, &child_str );
	g_list_free( child_str.children_list );

	return( child_str.children_count > 0 );
}

/**
 * ofo_account_get_children:
 * @account: the #ofoAccount account
 *
 * Returns: the list of children accounts.
 *
 * The list may be freed with g_list_free().
 */
GList *
ofo_account_get_children( const ofoAccount *account )
{
	sChildren child_str;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, NULL );

	account_get_children( account, &child_str );

	return( child_str.children_list );
}

static void
account_get_children( const ofoAccount *account, sChildren *child_str )
{
	GList *dataset;

	memset( child_str, '\0' ,sizeof( sChildren ));
	child_str->account = account;
	child_str->children_count = 0;
	child_str->children_list = NULL;

	dataset = ofo_account_get_dataset( ofo_base_get_getter( OFO_BASE( account )));

	g_list_foreach( dataset, ( GFunc ) account_iter_children, child_str );
}

static void
account_iter_children( const ofoAccount *account, sChildren *child_str )
{
	const gchar *number;

	number = ofo_account_get_number( account );

	if( ofo_account_is_child_of( child_str->account, number )){
		child_str->children_count += 1;
		child_str->children_list = g_list_append( child_str->children_list, ( gpointer ) account );
	}
}

/**
 * ofo_account_is_child_of:
 * @account: the #ofoAccount account
 * @candidate: another account identifier.
 *
 * Returns: %TRUE if the @candidate is a child number of @account.
 */
gboolean
ofo_account_is_child_of( const ofoAccount *account, const gchar *candidate )
{
	const gchar *account_number;
	gboolean is_child;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( my_strlen( candidate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	account_number = ofo_account_get_number( account );

	is_child = g_str_has_prefix( candidate, account_number ) &&
				g_utf8_collate( candidate, account_number ) > 0;

	return( is_child );
}

/**
 * ofo_account_is_allowed:
 * @account: this #ofoAccount instance.
 * @allowed: the type of allowed account.
 *
 * Returns %TRUE if the @account is of the specified @allowed type.
 */
gboolean
ofo_account_is_allowed( const ofoAccount *account, gint allowed )
{
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	if( ofo_account_is_closed( account )){
		return( FALSE );
	}

	if( allowed == ACCOUNT_ALLOW_ALL ){
		return( TRUE );
	}

	if( allowed == ACCOUNT_ALLOW_DETAIL ){
		return( !ofo_account_is_root( account ));
	}

	if( allowed == ACCOUNT_ALLOW_SETTLEABLE ){
		return( ofo_account_is_settleable( account ));
	}

	if( allowed == ACCOUNT_ALLOW_RECONCILIABLE ){
		return( ofo_account_is_reconciliable( account ));
	}

	if( allowed == ACCOUNT_ALLOW_FORWARDABLE ){
		return( ofo_account_is_forwardable( account ));
	}

	return( FALSE );
}

/**
 * ofo_account_get_balance_type_dbms:
 * @type: an #ofeAccountType.
 *
 * Returns the dbms indicator corresponding to @type.
 *
 * The returned string is owned by #ofoAccount class, and should not be
 * released by the caller.
 */
const gchar *
ofo_account_get_balance_type_dbms( ofeAccountType type )
{
	static const gchar *thisfn = "ofo_account_get_balance_type_dbms";
	gint i;

	for( i=0 ; st_type[i].type ; ++i ){
		if( st_type[i].type == type ){
			return( st_type[i].dbms );
		}
	}

	g_warning( "%s: unknown type %u", thisfn, type );
	return( NULL );
}

/**
 * ofo_account_get_balance_type_short:
 * @type: an #ofeAccountType.
 *
 * Returns the short localized string corresponding to @type.
 *
 * The returned string is owned by #ofoAccount class, and should not be
 * released by the caller.
 */
const gchar *
ofo_account_get_balance_type_short( ofeAccountType type )
{
	static const gchar *thisfn = "ofo_account_get_balance_type_short";
	gint i;

	for( i=0 ; st_type[i].type ; ++i ){
		if( st_type[i].type == type ){
			return( gettext( st_type[i].sls ));
		}
	}

	g_warning( "%s: unknown type %u", thisfn, type );
	return( NULL );
}

/**
 * ofo_account_set_number:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_number( ofoAccount *account, const gchar *number )
{
	account_set_string( ACC_NUMBER, number );
}

/*
 * account_set_cre_user:
 * @account: the #ofoAccount account
 */
static void
account_set_cre_user( ofoAccount *account, const gchar *user )
{
	account_set_string( ACC_CRE_USER, user );
}

/*
 * account_set_cre_stamp:
 * @account: the #ofoAccount account
 */
static void
account_set_cre_stamp( ofoAccount *account, const GTimeVal *stamp )
{
	account_set_timestamp( ACC_CRE_STAMP, stamp );
}

/**
 * ofo_account_set_label:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_label( ofoAccount *account, const gchar *label )
{
	account_set_string( ACC_LABEL, label );
}

/**
 * ofo_account_set_currency:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_currency( ofoAccount *account, const gchar *currency )
{
	account_set_string( ACC_CURRENCY, currency );
}

/**
 * ofo_account_set_root:
 * @account: the #ofoAccount account
 * @root: %TRUE if @account is a root account, %FALSE if this is a
 *  detail account.
 */
void
ofo_account_set_root( ofoAccount *account, gboolean root )
{
	account_set_string( ACC_ROOT, root ? "Y":"N" );
}

/**
 * ofo_account_set_settleable:
 * @account: the #ofoAccount account
 * @settleable: %TRUE if the account is to be set settleable
 */
void
ofo_account_set_settleable( ofoAccount *account, gboolean settleable )
{
	account_set_string( ACC_SETTLEABLE, settleable ? "Y":"N" );
}

/**
 * ofo_account_set_keep_unsettled:
 * @account: the #ofoAccount account.
 * @keep: whether the unsettled entries should be kept.
 */
void
ofo_account_set_keep_unsettled( ofoAccount *account, gboolean keep )
{
	account_set_string( ACC_KEEP_UNSETTLED, keep ? "Y":"N" );
}

/**
 * ofo_account_set_reconciliable:
 * @account: the #ofoAccount account
 * @reconciliable: %TRUE if the account is to be set reconciliable
 */
void
ofo_account_set_reconciliable( ofoAccount *account, gboolean reconciliable )
{
	account_set_string( ACC_RECONCILIABLE, reconciliable ? "Y":"N" );
}

/**
 * ofo_account_set_keep_unreconciliated:
 * @account: the #ofoAccount account.
 * @keep: whether the unreconciliated entries should be kept.
 */
void
ofo_account_set_keep_unreconciliated( ofoAccount *account, gboolean keep )
{
	account_set_string( ACC_KEEP_UNRECONCILIATED, keep ? "Y":"N" );
}

/**
 * ofo_account_set_forwardable:
 * @account: the #ofoAccount account
 * @forwardable: %TRUE if the account supports carried forward entries
 */
void
ofo_account_set_forwardable( ofoAccount *account, gboolean forwardable )
{
	account_set_string( ACC_FORWARDABLE, forwardable ? "Y":"N" );
}

/**
 * ofo_account_set_closed:
 * @account: the #ofoAccount account
 * @closed: %TRUE if the account is closed
 */
void
ofo_account_set_closed( ofoAccount *account, gboolean closed )
{
	account_set_string( ACC_CLOSED, closed ? "Y":"N" );
}

/**
 * ofo_account_set_notes:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	account_set_string( ACC_NOTES, notes );
}

/*
 * ofo_account_set_upd_user:
 * @account: the #ofoAccount account
 */
static void
account_set_upd_user( ofoAccount *account, const gchar *user )
{
	account_set_string( ACC_UPD_USER, user );
}

/*
 * ofo_account_set_upd_stamp:
 * @account: the #ofoAccount account
 */
static void
account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp )
{
	account_set_timestamp( ACC_UPD_STAMP, stamp );
}

/**
 * ofo_ofo_account_set_current_rough_debit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of debits for rought entries in the current exercice.
 */
void
ofo_account_set_current_rough_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_CR_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_current_rough_credit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of credits for rought entries in the current exercice.
 */
void
ofo_account_set_current_rough_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_CR_CREDIT, amount );
}

/**
 * ofo_ofo_account_set_current_val_debit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of debits for validated entries in the current exercice.
 */
void
ofo_account_set_current_val_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_CV_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_current_val_credit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of credits for validated entries in the current exercice.
 */
void
ofo_account_set_current_val_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_CV_CREDIT, amount );
}

/**
 * ofo_ofo_account_set_futur_rough_debit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of debits for rought entries in a future exercice.
 */
void
ofo_account_set_futur_rough_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FR_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_futur_rough_credit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of credits for rought entries in a future exercice.
 */
void
ofo_account_set_futur_rough_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FR_CREDIT, amount );
}

/**
 * ofo_ofo_account_set_futur_val_debit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of debits for validated entries in a future exercice.
 */
void
ofo_account_set_futur_val_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FV_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_futur_val_credit:
 * @account: the #ofoAccount account.
 *
 * Set the sum of credits for validated entries in a future exercice.
 */
void
ofo_account_set_futur_val_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FV_CREDIT, amount );
}

/**
 * ofo_account_archive_openings:
 * @getter: the #ofaIGetter of the application.
 * @exe_begin: the beginning of the exercice
 *
 * Archive the balance of the accounts at the beginning of the exercice.
 */
gboolean
ofo_account_archive_openings( ofaIGetter *getter, const GDate *exe_begin )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	gchar *sdbegin, *query;
	const gchar *open_type, *forward_rule;
	gboolean ok;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	sdbegin = my_date_to_str( exe_begin, MY_DATE_SQL );
	open_type = ofo_account_get_balance_type_dbms( ACC_TYPE_OPEN );
	forward_rule = ofo_entry_rule_get_dbms( ENT_RULE_FORWARD );

	query = g_strdup_printf(
			"INSERT INTO OFA_T_ACCOUNTS_ARC "
			"	(ACC_NUMBER,ACC_ARC_DATE,ACC_ARC_TYPE,ACC_ARC_DEBIT,ACC_ARC_CREDIT) "
			"		SELECT ENT_ACCOUNT,'%s','%s',SUM(ENT_DEBIT),SUM(ENT_CREDIT) "
			"			FROM OFA_T_ENTRIES WHERE ENT_RULE='%s' GROUP BY ENT_ACCOUNT",
				sdbegin, open_type, forward_rule );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );
	g_free( sdbegin );

	if( ok ){
		collector = ofa_igetter_get_collector( getter );
		my_icollector_collection_free( collector, OFO_TYPE_ACCOUNT );
	}

	return( ok );
}

/**
 * ofo_account_archive_balances:
 * @account: this #ofoAccount object.
 * @archive_date: the archived date.
 *
 * Archiving an account balance is only relevant when user is sure that
 * no more entries will be set on this account (e.g. because the user
 * has closed the period).
 *
 * Archived_solde_at_@archive_date =
 *   archived_solde_at_previous_date (resp. at_exercice_beginning) +
 *   validated_entries_between_previous_date_and_@archive_date.
 *
 * In order for the archived balance to be worthy, it is so of the
 * biggest interest to have validated all entries until @archive_date,
 * so that no rough entry is left. This is to the caller to take care
 * of that.
 *
 * If no previous archived date is found, the we consider the entries
 * since the beginning of the exercice.
 */
gboolean
ofo_account_archive_balances( ofoAccount *account, const GDate *archive_date )
{
	gboolean ok;
	ofxAmount debit, credit;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );
	g_return_val_if_fail( !ofo_account_is_root( account ), FALSE );

	ofo_account_get_solde_at_date( account, archive_date, NULL, &debit, &credit );

	ok = archive_do_add_dbms( account, archive_date, ACC_TYPE_NORMAL, debit, credit ) &&
			archive_do_add_list( account, archive_date, ACC_TYPE_NORMAL, debit, credit );

	return( ok );
}

static gboolean
archive_do_add_dbms( ofoAccount *account, const GDate *date, ofeAccountType type, ofxAmount debit, ofxAmount credit )
{
	ofaIGetter *getter;
	ofaHub *hub;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;
	gchar *query, *sdate, *sdebit, *scredit;
	gboolean ok;

	getter = ofo_base_get_getter( OFO_BASE( account ));

	cur_code = ofo_account_get_currency( account );
	cur_obj = ofo_currency_get_by_code( getter, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	sdate = my_date_to_str( date, MY_DATE_SQL );
	sdebit = ofa_amount_to_sql( debit, cur_obj );
	scredit = ofa_amount_to_sql( credit, cur_obj );

	query = g_strdup_printf(
				"INSERT INTO OFA_T_ACCOUNTS_ARC "
				"	(ACC_NUMBER,ACC_ARC_DATE,ACC_ARC_TYPE,ACC_ARC_DEBIT,ACC_ARC_CREDIT) VALUES "
				"	('%s','%s','%s',%s,%s)",
				ofo_account_get_number( account ),
				sdate, ofo_account_get_balance_type_dbms( type ), sdebit, scredit );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( sdate );
	g_free( sdebit );
	g_free( scredit );
	g_free( query );

	return( ok );
}

static gboolean
archive_do_add_list( ofoAccount *account, const GDate *date, ofeAccountType type, ofxAmount debit, ofxAmount credit )
{
	ofoAccountPrivate *priv;
	GList *fields;

	priv = ofo_account_get_instance_private( account );

	fields = ofa_box_init_fields_list( st_archive_defs );
	ofa_box_set_string( fields, ACC_NUMBER, ofo_account_get_number( account ));
	ofa_box_set_date( fields, ACC_ARC_DATE, date );
	ofa_box_set_string( fields, ACC_ARC_TYPE, ofo_account_get_balance_type_dbms( type ));
	ofa_box_set_amount( fields, ACC_ARC_DEBIT, debit );
	ofa_box_set_amount( fields, ACC_ARC_CREDIT, credit );

	priv->archives = g_list_append( priv->archives, fields );

	return( TRUE );
}

/**
 * ofo_account_archive_get_count:
 * @account: the #ofoAccount account.
 *
 * Returns: the count of archived balances.
 */
guint
ofo_account_archive_get_count( ofoAccount *account )
{
	ofoAccountPrivate *priv;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_get_instance_private( account );

	return( g_list_length( priv->archives ));
}

/**
 * ofo_account_archive_get_date:
 * @account: the #ofoAccount account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the effect date of the balance.
 */
const GDate *
ofo_account_archive_get_date( ofoAccount *account, guint idx )
{
	ofoAccountPrivate *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, NULL );

	priv = ofo_account_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_date( nth->data, ACC_ARC_DATE ));
	}

	return( NULL );
}

/**
 * ofo_account_archive_get_type:
 * @account: the #ofoAccount account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the balance type.
 */
ofeAccountType
ofo_account_archive_get_type( ofoAccount *account, guint idx )
{
	ofoAccountPrivate *priv;
	GList *nth;
	const gchar *cstr;
	gint i;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), ACC_TYPE_NORMAL );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, ACC_TYPE_NORMAL );

	priv = ofo_account_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		cstr = ofa_box_get_string( nth->data, ACC_ARC_TYPE );

		for( i=0 ; st_type[i].type ; ++i ){
			if( !my_collate( st_type[i].dbms, cstr )){
				return( st_type[i].type );
			}
		}
	}

	return( ACC_TYPE_NORMAL );
}

/**
 * ofo_account_archive_get_debit:
 * @account: the #ofoAccount account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived debit.
 */
ofxAmount
ofo_account_archive_get_debit( ofoAccount *account, guint idx )
{
	ofoAccountPrivate *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_amount( nth->data, ACC_ARC_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_account_archive_get_credit:
 * @account: the #ofoAccount account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived credit.
 */
ofxAmount
ofo_account_archive_get_credit( ofoAccount *account, guint idx )
{
	ofoAccountPrivate *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_amount( nth->data, ACC_ARC_CREDIT ));
	}

	return( 0 );
}

/*
 * Returns the index in archives list of the most recent archive before
 * the @requested date, or -1 if not found.
 */
static gint
archive_get_last_index( ofoAccount *account, const GDate *requested )
{
	ofoAccountPrivate *priv;
	GDate max_date;
	const GDate *it_date;
	gint count, i, found;

	priv = ofo_account_get_instance_private( account );

	found = -1;
	count = g_list_length( priv->archives );
	my_date_clear( &max_date );

	for( i=0 ; i<count ; ++i ){
		it_date = ofo_account_archive_get_date( account, i );
		if( my_date_compare( it_date, requested ) < 0 && my_date_compare_ex( &max_date, it_date, TRUE ) < 0 ){
			my_date_set_from_date( &max_date, it_date );
			found = i;
		}
	}

	return( found );
}

/**
 * ofo_account_archive_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown account numbers in OFA_T_ACCOUNT_ARC child table.
 *
 * The returned list should be #ofo_account_archive_free_orphans() by the
 * caller.
 */
GList *
ofo_account_archive_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_ACCOUNTS_ARC" ));
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

	query = g_strdup_printf( "SELECT DISTINCT(ACC_NUMBER) FROM %s "
			"	WHERE ACC_NUMBER NOT IN (SELECT ACC_NUMBER FROM OFA_T_ACCOUNTS)", table );

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
 * ofo_account_doc_get_count:
 * @account: this #ofoAccount object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_account_doc_get_count( ofoAccount *account )
{
	ofoAccountPrivate *priv;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_get_instance_private( account );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_account_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown account numbers in OFA_T_ACCOUNT_DOC child table.
 *
 * The returned list should be #ofo_account_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_account_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_ACCOUNTS_DOC" ));
}

/**
 * ofo_account_insert:
 * @account: the new #ofoAccount account to be inserted.
 *
 * This function is only of use when the user creates a new account.
 * It is no worth to deal here with amounts and/or debit/credit agregates.
 *
 * Returns: %TRUE if the insertion has been successful.
 */
gboolean
ofo_account_insert( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_insert";
	const ofaIDBConnect *connect;
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( account ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	/* make sure the dataset is loaded before insertion
	 * so that my_icollector_collection_add_object() doesn't then double
	 * add the same record another time */
	ofo_account_get_dataset( getter );

	if( account_do_insert( account, connect )){
		my_icollector_collection_add_object(
				ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( account ), NULL, getter );
		g_signal_emit_by_name( signaler, SIGNALER_BASE_NEW, account );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
account_do_insert( ofoAccount *account, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	GTimeVal stamp;
	const gchar *userid;

	ok = FALSE;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_account_get_label( account ));
	notes = my_utils_quote_sql( ofo_account_get_notes( account ));
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ACCOUNTS" );

	g_string_append_printf( query,
			"	(ACC_NUMBER,ACC_UPD_USER, ACC_UPD_STAMP,ACC_LABEL,ACC_CURRENCY,"
			"	 ACC_ROOT,ACC_SETTLEABLE,ACC_KEEP_UNSETTLED,ACC_RECONCILIABLE,"
			"	 ACC_KEEP_UNRECONCILIATED,ACC_FORWARDABLE,ACC_CLOSED,"
			"	 ACC_NOTES) "
			"	VALUES ('%s','%s','%s','%s',",
					ofo_account_get_number( account ),
					userid,
					stamp_str,
					label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "'%s',", ofo_account_get_currency( account ));
	}

	g_string_append_printf( query, "'%s',", ofo_account_is_root( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_settleable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_get_keep_unsettled( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_reconciliable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_get_keep_unreconciliated( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_forwardable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_closed( account ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		account_set_cre_user( account, userid );
		account_set_cre_stamp( account, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_account_update:
 * @account: this #ofoAccount instance.
 * @prev_number: [allow-none]: the previous account identifier if it
 *  has changed.
 *
 * We deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats.
 *
 * Returns: %TRUE if the update is successfull, %FALSE else.
 */
gboolean
ofo_account_update( ofoAccount *account, const gchar *prev_number )
{
	static const gchar *thisfn = "ofo_account_update";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p, prev_number=%s",
				thisfn, ( void * ) account, prev_number );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( account ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( account_do_update( account, ofa_hub_get_connect( hub ), prev_number ) &&
			account_do_update_arc( account, ofa_hub_get_connect( hub ), prev_number )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, account, prev_number );
		ok = TRUE;
	}

	return( ok );
}

/*
 * @prev_number: may be %NULL if the identifier has not changed.
 */
static gboolean
account_do_update( ofoAccount *account, const ofaIDBConnect *connect, const gchar *prev_number )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	gboolean ok;
	const gchar *new_number;
	GTimeVal stamp;
	const gchar *userid;

	ok = FALSE;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_sql( ofo_account_get_label( account ));
	notes = my_utils_quote_sql( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	my_stamp_set_now( &stamp );
	stamp_str = my_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	if( prev_number && my_collate( new_number, prev_number )){
		g_string_append_printf( query, "ACC_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query, "ACC_LABEL='%s',", label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "ACC_CURRENCY=NULL," );
	} else {
		g_string_append_printf( query, "ACC_CURRENCY='%s',", ofo_account_get_currency( account ));
	}

	g_string_append_printf( query,
			"	ACC_ROOT='%s',", ofo_account_is_root( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_SETTLEABLE='%s',", ofo_account_is_settleable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_KEEP_UNSETTLED='%s',", ofo_account_get_keep_unsettled( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_RECONCILIABLE='%s',", ofo_account_is_reconciliable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_KEEP_UNRECONCILIATED='%s',", ofo_account_get_keep_unreconciliated( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_FORWARDABLE='%s',", ofo_account_is_forwardable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_CLOSED='%s',", ofo_account_is_closed( account ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "ACC_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "ACC_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	ACC_UPD_USER='%s',ACC_UPD_STAMP='%s'"
			"	WHERE ACC_NUMBER='%s'",
					userid,
					stamp_str,
					prev_number );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		account_set_upd_user( account, userid );
		account_set_upd_stamp( account, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );
	g_free( notes );
	g_free( label );

	return( ok );
}

/*
 * @prev_number: may be %NULL if the identifier has not changed.
 */
static gboolean
account_do_update_arc( ofoAccount *account, const ofaIDBConnect *connect, const gchar *prev_number )
{
	GString *query;
	gboolean ok;
	const gchar *new_number;

	ok = TRUE;
	new_number = ofo_account_get_number( account );

	if( prev_number && my_collate( new_number, prev_number )){

		ok = FALSE;
		query = g_string_new( "UPDATE OFA_T_ACCOUNTS_ARC SET " );
		g_string_append_printf( query, "ACC_NUMBER='%s' ", new_number );
		g_string_append_printf( query, "WHERE ACC_NUMBER='%s'", prev_number );

		if( ofa_idbconnect_query( connect, query->str, TRUE )){
			ok = TRUE;
		}

		g_string_free( query, TRUE );
	}

	return( ok );
}

/**
 * ofo_account_update_amounts:
 */
gboolean
ofo_account_update_amounts( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_do_update_amounts";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	gboolean ok;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( account ));
	signaler = ofa_igetter_get_signaler( getter );

	if( account_do_update_amounts( account, getter )){
		g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, account, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
account_do_update_amounts( ofoAccount *account, ofaIGetter *getter )
{
	GString *query;
	ofaHub *hub;
	gboolean ok;
	gchar *samount;
	ofxAmount amount;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	cur_code = ofo_account_get_currency( account );
	cur_obj = ofo_currency_get_by_code( getter, cur_code );
	g_return_val_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ), FALSE );

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	/* current rough debit */
	amount = ofo_account_get_current_rough_debit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_CR_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_CR_DEBIT=NULL," );
	}

	/* current rough credit */
	amount = ofo_account_get_current_rough_credit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_CR_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_CR_CREDIT=NULL," );
	}

	/* current validated debit */
	amount = ofo_account_get_current_val_debit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_CV_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_CV_DEBIT=NULL," );
	}

	/* current validated credit */
	amount = ofo_account_get_current_val_credit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_CV_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_CV_CREDIT=NULL," );
	}

	/* future rough debit */
	amount = ofo_account_get_futur_rough_debit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_FR_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FR_DEBIT=NULL," );
	}

	/* future rough credit */
	amount = ofo_account_get_futur_rough_credit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_FR_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FR_CREDIT=NULL," );
	}

	/* future validated debit */
	amount = ofo_account_get_futur_val_debit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_FV_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FV_DEBIT=NULL," );
	}

	/* future validated credit */
	amount = ofo_account_get_futur_val_credit( account );
	if( amount ){
		samount = ofa_amount_to_sql( amount, cur_obj );
		g_string_append_printf( query, "ACC_FV_CREDIT=%s ", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FV_CREDIT=NULL " );
	}

	g_string_append_printf( query,
				"WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_account_delete:
 */
gboolean
ofo_account_delete( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_delete";
	ofaIGetter *getter;
	ofaISignaler *signaler;
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( ofo_account_is_deletable( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( account ));
	signaler = ofa_igetter_get_signaler( getter );
	hub = ofa_igetter_get_hub( getter );

	if( account_do_delete( account, ofa_hub_get_connect( hub ))){
		g_object_ref( account );
		my_icollector_collection_remove_object( ofa_igetter_get_collector( getter ), MY_ICOLLECTIONABLE( account ));
		g_signal_emit_by_name( signaler, SIGNALER_BASE_DELETED, account );
		g_object_unref( account );
		ok = TRUE;
	}

	return( ok );
}

/**
 * ofo_account_delete_with_children:
 * @account: the #ofoAccount to be deleted.
 *
 * Delete the @account and all its children without any further confirmation.
 *
 * Returns: %TRUE if deletion is successful.
 */
gboolean
ofo_account_delete_with_children( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_delete_with_children";
	gboolean ok;
	GList *children, *it, *refs;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = TRUE;
	children = ofo_account_get_children( account );
	children = g_list_prepend( children, account );

	/* get a ref on all children to make sure they stay alive during the deletion */
	refs = NULL;
	for( it=children ; it ; it=it->next ){
		refs = g_list_prepend( refs, g_object_ref( it->data ));
	}
	g_list_free( children );

	/* next delete the children */
	for( it=refs ; it ; it=it->next ){
		ok &= ofo_account_delete( OFO_ACCOUNT( it->data ));
	}
	g_list_free_full( refs, ( GDestroyNotify ) g_object_unref );

	return( ok );
}

static gboolean
account_do_delete( ofoAccount *account, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_ACCOUNTS WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));
	ok = ofa_idbconnect_query( connect, query, TRUE );
	g_free( query );

	if( ok ){
		query = g_strdup_printf(
				"DELETE FROM OFA_T_ACCOUNTS_ARC WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	return( ok );
}

static gint
account_cmp_by_number( const ofoAccount *a, const gchar *number )
{
	return( my_collate( ofo_account_get_number( a ), number ));
}

/*
 * myICollectionable interface management
 */
static void
icollectionable_iface_init( myICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_icollectionable_iface_init";

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
	GList *dataset, *it;
	ofoAccount *account;
	ofoAccountPrivate *priv;
	gchar *from;
	ofaHub *hub;

	g_return_val_if_fail( user_data && OFA_IS_IGETTER( user_data ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_ACCOUNTS",
					OFO_TYPE_ACCOUNT,
					OFA_IGETTER( user_data ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( user_data ));

	for( it=dataset ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );
		priv = ofo_account_get_instance_private( account );
		from = g_strdup_printf(
				"OFA_T_ACCOUNTS_ARC WHERE ACC_NUMBER='%s'",
				ofo_account_get_number( account ));
		priv->archives =
				ofo_base_load_rows( st_archive_defs, ofa_hub_get_connect( hub ), from );
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
	static const gchar *thisfn = "ofo_account_idoc_iface_init";

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
	static const gchar *thisfn = "ofo_account_iexportable_iface_init";

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
	return( g_strdup( _( "Reference : _accounts chart" )));
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
 * Exports all the accounts.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_account_iexportable_export";

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
	gchar *str1, *str2;
	GList *dataset, *it, *itc, *itd;
	gboolean ok;
	gulong count;
	ofoCurrency *currency;
	const gchar *cur_code;
	ofoAccount *account;
	ofoAccountPrivate *priv;
	gchar field_sep;

	getter = ofa_iexportable_get_getter( exportable );
	dataset = ofo_account_get_dataset( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( dataset );
	if( ofa_stream_format_get_with_headers( stformat )){
		count += ACCOUNT_TABLES_COUNT;
	}
	for( it=dataset ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );
		count += ofo_account_archive_get_count( account );
		count += ofo_account_doc_get_count( account );
	}
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, ACCOUNT_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					ACCOUNT_TABLES_COUNT, st_boxed_defs, st_archive_defs, st_doc_defs );
	}

	/* export the dataset */
	for( it=dataset ; it && ok ; it=it->next ){
		account = OFO_ACCOUNT( it->data );

		cur_code = ofo_account_get_currency( account );
		currency = my_strlen( cur_code ) ? ofo_currency_get_by_code( getter, cur_code ) : NULL;
		str1 = ofa_box_csv_get_line( OFO_BASE( account )->prot->fields, stformat, currency );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );

		priv = ofo_account_get_instance_private( account );

		for( itc=priv->archives ; itc && ok ; itc=itc->next ){
			str1 = ofa_box_csv_get_line( itc->data, stformat, currency );
			str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
			ok = ofa_iexportable_append_line( exportable, str2 );
			g_free( str2 );
			g_free( str1 );
		}

		for( itd=priv->docs ; itd && ok ; itd=itd->next ){
			str1 = ofa_box_csv_get_line( itd->data, stformat, currency );
			str2 = g_strdup_printf( "1%c3%c%s", field_sep, field_sep, str1 );
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
	return( iexportable_get_label( OFA_IEXPORTABLE( instance )));
}

/*
 * ofo_account_iimportable_import:
 * @importable: a fake object to address this class.
 * @importer: the #ofaIImporter instance.
 * @parms: the #ofsImporterParms arguments.
 * @lines: the list of fields-splitted lines.
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - account number
 * - label
 * - currency iso 3a code (mandatory for detail accounts, default to
 *   dossier currency)
 * - is_root = {N|Y} (defaults to no)
 * - is_settleable = {N|Y} (defaults to no)
 * - is_reconciliable = {N|Y} (defaults to no)
 * - carried forwardable on new exercice = {N|Y} (defaults to no)
 * - is_closed = {N|Y} (defaults to no)
 * - notes (opt)
 *
 * All the balances are set to NULL.
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
		bck_table = ofa_idbconnect_table_backup( connect, "OFA_T_ACCOUNTS" );
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			my_icollector_collection_free( ofa_igetter_get_collector( parms->getter ), OFO_TYPE_ACCOUNT );
			g_signal_emit_by_name( signaler, SIGNALER_COLLECTION_RELOAD, OFO_TYPE_ACCOUNT );

		} else {
			ofa_idbconnect_table_restore( connect, bck_table, "OFA_T_ACCOUNTS" );
		}

		g_free( bck_table );
	}

	if( dataset ){
		ofo_account_free_dataset( dataset );
	}

	return( parms->parse_errs+parms->insert_errs );
}

#if 0
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	ofaHub *hub;
	GList *dataset;
	guint class_num, numline, total;
	ofoDossier *dossier;
	const gchar *def_dev_code, *cstr, *dev_code;
	GSList *itl, *fields, *itf;
	ofoAccount *account;
	gchar *str, *splitted;
	ofoCurrency *currency;
	ofoClass *class_obj;
	gboolean is_root;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );
	hub = ofa_igetter_get_hub( parms->getter );

	/* may be NULL
	 * eg. when importing accounts on dossier creation */
	dossier = ofa_hub_get_dossier( hub );
	def_dev_code = dossier ? ofo_dossier_get_default_currency( dossier ) : NULL;

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		account = ofo_account_new( parms->getter );

		/* account number */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty account number" ));
			parms->parse_errs += 1;
			continue;
		}
		class_num = ofo_account_get_class_from_number( cstr );
		class_obj = ofo_class_get_by_number( parms->getter, class_num );
		if( !class_obj && !OFO_IS_CLASS( class_obj )){
			str = g_strdup_printf( _( "invalid class number for account %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_account_set_number( account, cstr );

		/* account label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty account label" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_account_set_label( account, cstr );

		/* currency code */
		itf = itf ? itf->next : NULL;
		dev_code = itf ? ( const gchar * ) itf->data : NULL;

		/* root account
		 * previous to DB model v27, root/detail accounts were marked with R/D
		 * starting with v27, root accounts are marked with Y/N */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			cstr = "N";
		} else if( my_collate( cstr, EXPORTED_TYPE_DETAIL ) &&
					my_collate( cstr, EXPORTED_TYPE_ROOT ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
			str = g_strdup_printf( _( "invalid account type: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		is_root = !my_collate( cstr, EXPORTED_TYPE_ROOT ) || !my_collate( cstr, "Y" );
		ofo_account_set_root( account, is_root );

		/* check the currency code if a detail account */
		if( !is_root ){
			if( !my_strlen( dev_code )){
				dev_code = def_dev_code;
			}
			if( !my_strlen( dev_code )){
				ofa_iimporter_progress_num_text( importer, parms, numline, _( "no currency set, and unable to get a default currency" ));
				parms->parse_errs += 1;
				continue;
			}
			currency = ofo_currency_get_by_code( parms->getter, dev_code );
			if( !currency ){
				str = g_strdup_printf( _( "invalid account currency: %s" ), dev_code );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			}
			ofo_account_set_currency( account, dev_code );
		}

		/* settleable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_SETTLEABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid settleable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_settleable( account,
						!my_collate( cstr, EXPORTED_SETTLEABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* reconciliable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_RECONCILIABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid reconciliable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_reconciliable( account,
						!my_collate( cstr, EXPORTED_RECONCILIABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* carried forwardable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_FORWARDABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid forwardable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_forwardable( account,
						!my_collate( cstr, EXPORTED_FORWARDABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* closed ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_CLOSED ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid closed account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_closed( account,
						!my_collate( cstr, EXPORTED_CLOSED ) || !my_collate( cstr, "Y" ));
			}
		}

		/* notes */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_account_set_notes( account, splitted );
		g_free( splitted );

		/* last update user
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* last update timestamp
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* validated debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* validated credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* rough debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* rough credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* future debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* future credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* keep unsettled entries ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid keep_unsettled account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_keep_unsettled( account, !my_collate( cstr, "Y" ));
			}
		}

		/* keep unreconciliated entries ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid keep_unreconciliated account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_keep_unreconciliated( account, !my_collate( cstr, "Y" ));
			}
		}

		dataset = g_list_prepend( dataset, account );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( dataset );
}
#endif

static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	ofaHub *hub;
	GList *dataset;
	guint class_num, numline, total;
	ofoDossier *dossier;
	const gchar *def_dev_code, *cstr, *dev_code;
	GSList *itl, *fields, *itf;
	ofoAccount *account;
	gchar *str, *splitted;
	ofoCurrency *currency;
	ofoClass *class_obj;
	gboolean is_root;
	GTimeVal stamp;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );
	hub = ofa_igetter_get_hub( parms->getter );

	/* may be NULL
	 * eg. when importing accounts on dossier creation */
	dossier = ofa_hub_get_dossier( hub );
	def_dev_code = dossier ? ofo_dossier_get_default_currency( dossier ) : NULL;

	ofa_iimporter_progress_start( importer, parms );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;
		account = ofo_account_new( parms->getter );

		/* account number */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty account number" ));
			parms->parse_errs += 1;
			continue;
		}
		class_num = ofo_account_get_class_from_number( cstr );
		class_obj = ofo_class_get_by_number( parms->getter, class_num );
		if( !class_obj && !OFO_IS_CLASS( class_obj )){
			str = g_strdup_printf( _( "invalid class number for account %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		ofo_account_set_number( account, cstr );

		/* creation user */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			account_set_cre_user( account, cstr );
		}

		/* creation timestamp */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			my_stamp_set_from_sql( &stamp, cstr );
			account_set_cre_stamp( account, &stamp );
		}

		/* account label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			ofa_iimporter_progress_num_text( importer, parms, numline, _( "empty account label" ));
			parms->parse_errs += 1;
			continue;
		}
		ofo_account_set_label( account, cstr );

		/* currency code */
		itf = itf ? itf->next : NULL;
		dev_code = itf ? ( const gchar * ) itf->data : NULL;

		/* root account
		 * previous to DB model v27, root/detail accounts were marked with R/D
		 * starting with v27, root accounts are marked with Y/N */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( !my_strlen( cstr )){
			cstr = "N";
		} else if( my_collate( cstr, EXPORTED_TYPE_DETAIL ) &&
					my_collate( cstr, EXPORTED_TYPE_ROOT ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
			str = g_strdup_printf( _( "invalid account type: %s" ), cstr );
			ofa_iimporter_progress_num_text( importer, parms, numline, str );
			g_free( str );
			parms->parse_errs += 1;
			continue;
		}
		is_root = !my_collate( cstr, EXPORTED_TYPE_ROOT ) || !my_collate( cstr, "Y" );
		ofo_account_set_root( account, is_root );

		/* check the currency code if a detail account */
		if( !is_root ){
			if( !my_strlen( dev_code )){
				dev_code = def_dev_code;
			}
			if( !my_strlen( dev_code )){
				ofa_iimporter_progress_num_text( importer, parms, numline, _( "no currency set, and unable to get a default currency" ));
				parms->parse_errs += 1;
				continue;
			}
			currency = ofo_currency_get_by_code( parms->getter, dev_code );
			if( !currency ){
				str = g_strdup_printf( _( "invalid account currency: %s" ), dev_code );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			}
			ofo_account_set_currency( account, dev_code );
		}

		/* settleable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_SETTLEABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid settleable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_settleable( account,
						!my_collate( cstr, EXPORTED_SETTLEABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* keep unsettled entries ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid keep_unsettled account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_keep_unsettled( account, !my_collate( cstr, "Y" ));
			}
		}

		/* reconciliable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_RECONCILIABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid reconciliable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_reconciliable( account,
						!my_collate( cstr, EXPORTED_RECONCILIABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* keep unreconciliated entries ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, "Y" ) && my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid keep_unreconciliated account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_keep_unreconciliated( account, !my_collate( cstr, "Y" ));
			}
		}

		/* carried forwardable ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_FORWARDABLE ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid forwardable account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_forwardable( account,
						!my_collate( cstr, EXPORTED_FORWARDABLE ) || !my_collate( cstr, "Y" ));
			}
		}

		/* closed ? */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		if( my_strlen( cstr )){
			if( my_collate( cstr, EXPORTED_CLOSED ) &&
					my_collate( cstr, "Y" ) &&
					my_collate( cstr, "N" )){
				str = g_strdup_printf( _( "invalid closed account indicator: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				continue;
			} else {
				ofo_account_set_closed( account,
						!my_collate( cstr, EXPORTED_CLOSED ) || !my_collate( cstr, "Y" ));
			}
		}

		/* notes */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		splitted = my_utils_import_multi_lines( cstr );
		ofo_account_set_notes( account, splitted );
		g_free( splitted );

		/* last update user
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* last update timestamp
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* validated debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* validated credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* rough debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* rough credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* future debit
		 * do not import */
		itf = itf ? itf->next : NULL;

		/* future credit
		 * do not import */
		itf = itf ? itf->next : NULL;

		dataset = g_list_prepend( dataset, account );
		parms->parsed_count += 1;
		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( dataset );
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	ofaHub *hub;
	GList *it;
	const ofaIDBConnect *connect;
	const gchar *acc_id;
	gboolean insert;
	guint total, type;
	ofoAccount *account;
	gchar *str;

	total = g_list_length( dataset );
	hub = ofa_igetter_get_hub( parms->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		account_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		account = OFO_ACCOUNT( it->data );

		if( account_get_exists( account, connect )){
			parms->duplicate_count += 1;
			acc_id = ofo_account_get_number( account );
			type = MY_PROGRESS_NORMAL;

			switch( parms->mode ){
				case OFA_IDUPLICATE_REPLACE:
					str = g_strdup_printf( _( "%s: duplicate account, replacing previous one" ), acc_id );
					account_do_delete( account, connect );
					break;
				case OFA_IDUPLICATE_IGNORE:
					str = g_strdup_printf( _( "%s: duplicate account, ignored (skipped)" ), acc_id );
					insert = FALSE;
					total -= 1;
					break;
				case OFA_IDUPLICATE_ABORT:
					str = g_strdup_printf( _( "%s: erroneous duplicate account" ), acc_id );
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
			if( account_do_insert( account, connect )){
				parms->inserted_count += 1;
			} else {
				parms->insert_errs += 1;
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

static gboolean
account_get_exists( const ofoAccount *account, const ofaIDBConnect *connect )
{
	const gchar *account_id;
	gint count;
	gchar *str;

	count = 0;
	account_id = ofo_account_get_number( account );
	str = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE ACC_NUMBER='%s'", account_id );
	ofa_idbconnect_query_int( connect, str, &count, FALSE );

	return( count > 0 );
}

static gboolean
account_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_ACCOUNTS", TRUE ));
}

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_account_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), NULL );
	g_signal_connect( signaler, SIGNALER_PERIOD_STATUS_CHANGE, G_CALLBACK( signaler_on_entry_period_status_changed ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_account_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_CLASS( object )){
		deletable = signaler_is_deletable_class( signaler, OFO_CLASS( object ));

	} else if( OFO_IS_CURRENCY( object )){
		deletable = signaler_is_deletable_currency( signaler, OFO_CURRENCY( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_class( ofaISignaler *signaler, ofoClass *class )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE ACC_NUMBER LIKE '%d%%'",
			ofo_class_get_number( class ));

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
			"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE ACC_CURRENCY='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_account_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_ENTRY( object )){
		signaler_on_new_base_entry( signaler, OFO_ENTRY( object ));
	}
}

/*
 * a new entry has been recorded, so update the daily balances
 */
static void
signaler_on_new_base_entry( ofaISignaler *signaler, ofoEntry *entry )
{
	ofaIGetter *getter;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofoAccount *account;
	gdouble debit, credit, prev;

	getter = ofa_isignaler_get_getter( signaler );

	/* the only case where an entry is created with a 'past' status
	 *  is an imported entry in the past (before the beginning of the
	 *  exercice) - in this case, the 'new_object' message should not be
	 *  sent
	 * if not in the past, only allowed periods are 'current' or 'future'
	 * in these two cases, status must be 'rough' */
	period = ofo_entry_get_period( entry );
	g_return_if_fail( period != ENT_PERIOD_PAST );
	g_return_if_fail( period == ENT_PERIOD_CURRENT || period == ENT_PERIOD_FUTURE );

	status = ofo_entry_get_status( entry );
	g_return_if_fail( status == ENT_STATUS_ROUGH );

	account = ofo_account_get_by_number( getter, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	/* impute the new entry either to the debit or the credit of daily
	 * or futur balances depending of the position of the effect date
	 * vs. ending date of the exercice
	 */
	switch( period ){
		case ENT_PERIOD_CURRENT:
			if( debit ){
				prev = ofo_account_get_current_rough_debit( account );
				ofo_account_set_current_rough_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_current_rough_credit( account );
				ofo_account_set_current_rough_credit( account, prev+credit );
			}
			break;

		case ENT_PERIOD_FUTURE:
			if( debit ){
				prev = ofo_account_get_futur_rough_debit( account );
				ofo_account_set_futur_rough_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_futur_rough_credit( account );
				ofo_account_set_futur_rough_credit( account, prev+credit );
			}
			break;

		default:
			g_return_if_reached();
			break;
	}

	ofo_account_update_amounts( account );
}

/*
 * SIGNALER_PERIOD_STATUS_CHANGE signal handler
 *
 * There is only one case where the entry changes both its period and
 * its status: when a current+rough entry becomes past(+validated).
 */
static void
signaler_on_entry_period_status_changed( ofaISignaler *signaler, ofoEntry *entry,
											ofeEntryPeriod prev_period, ofeEntryStatus prev_status,
											ofeEntryPeriod new_period, ofeEntryStatus new_status,
											void *empty )
{
	static const gchar *thisfn = "ofo_account_signaler_on_entry_period_status_changed";
	ofaIGetter *getter;
	ofoAccount *account;
	ofxAmount debit, credit, amount;
	ofeEntryStatus status;
	ofeEntryPeriod period;

	g_debug( "%s: signaler=%p, entry=%p, prev_period=%d, prev_status=%d, new_period=%d, new_status=%d, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) entry,
			prev_period, prev_status, new_period, new_status, ( void * ) empty );

	getter = ofa_isignaler_get_getter( signaler );
	account = ofo_account_get_by_number( getter, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	period = ( prev_period == -1 ? ofo_entry_get_period( entry ) : prev_period );
	status = ( prev_status == -1 ? ofo_entry_get_status( entry ) : prev_status );

	switch( period ){
		case ENT_PERIOD_CURRENT:
			switch( status ){
				case ENT_STATUS_ROUGH:
					amount = ofo_account_get_current_rough_debit( account );
					ofo_account_set_current_rough_debit( account, amount-debit );
					amount = ofo_account_get_current_rough_credit( account );
					ofo_account_set_current_rough_credit( account, amount-credit );
					break;
				case ENT_STATUS_VALIDATED:
					amount = ofo_account_get_current_val_debit( account );
					ofo_account_set_current_val_debit( account, amount-debit );
					amount = ofo_account_get_current_val_credit( account );
					ofo_account_set_current_val_credit( account, amount-credit );
					break;
				default:
					break;
			}
			break;
		case ENT_PERIOD_FUTURE:
			switch( status ){
				case ENT_STATUS_ROUGH:
					amount = ofo_account_get_futur_rough_debit( account );
					ofo_account_set_futur_rough_debit( account, amount-debit );
					amount = ofo_account_get_futur_rough_credit( account );
					ofo_account_set_futur_rough_credit( account, amount-credit );
					break;
				case ENT_STATUS_VALIDATED:
					amount = ofo_account_get_futur_val_debit( account );
					ofo_account_set_futur_val_debit( account, amount-debit );
					amount = ofo_account_get_futur_val_credit( account );
					ofo_account_set_futur_val_credit( account, amount-credit );
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	period = ( prev_period == -1 ? ofo_entry_get_period( entry ) : new_period );
	status = ( prev_status == -1 ? ofo_entry_get_status( entry ) : new_status );

	switch( period ){
		case ENT_PERIOD_CURRENT:
			switch( status ){
				case ENT_STATUS_ROUGH:
					amount = ofo_account_get_current_rough_debit( account );
					ofo_account_set_current_rough_debit( account, amount+debit );
					amount = ofo_account_get_current_rough_credit( account );
					ofo_account_set_current_rough_credit( account, amount+credit );
					break;
				case ENT_STATUS_VALIDATED:
					amount = ofo_account_get_current_val_debit( account );
					ofo_account_set_current_val_debit( account, amount+debit );
					amount = ofo_account_get_current_val_credit( account );
					ofo_account_set_current_val_credit( account, amount+credit );
					break;
				default:
					break;
			}
			break;
		case ENT_PERIOD_FUTURE:
			switch( status ){
				case ENT_STATUS_ROUGH:
					amount = ofo_account_get_futur_rough_debit( account );
					ofo_account_set_futur_rough_debit( account, amount+debit );
					amount = ofo_account_get_futur_rough_credit( account );
					ofo_account_set_futur_rough_credit( account, amount+credit );
					break;
				case ENT_STATUS_VALIDATED:
					amount = ofo_account_get_futur_val_debit( account );
					ofo_account_set_futur_val_debit( account, amount+debit );
					amount = ofo_account_get_futur_val_credit( account );
					ofo_account_set_futur_val_credit( account, amount+credit );
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	ofo_account_update_amounts( account );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_account_signaler_on_updated_base";
	const gchar *code;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				signaler_on_updated_currency_code( signaler, prev_id, code );
			}
		}
	}
}

/*
 * the currency code iso has been modified: update the accounts which
 * use it
 */
static void
signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_ACCOUNTS SET ACC_CURRENCY='%s' WHERE ACC_CURRENCY='%s'",
						code, prev_id );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}
