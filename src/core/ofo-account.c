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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "api/ofa-preferences.h"

/* priv instance data
 */
enum {
	ACC_NUMBER = 1,
	ACC_LABEL,
	ACC_CURRENCY,
	ACC_ROOT,
	ACC_SETTLEABLE,
	ACC_RECONCILIABLE,
	ACC_FORWARDABLE,
	ACC_CLOSED,
	ACC_NOTES,
	ACC_UPD_USER,
	ACC_UPD_STAMP,
	ACC_VAL_DEBIT,
	ACC_VAL_CREDIT,
	ACC_ROUGH_DEBIT,
	ACC_ROUGH_CREDIT,
	ACC_OPEN_DEBIT,
	ACC_OPEN_CREDIT,
	ACC_FUT_DEBIT,
	ACC_FUT_CREDIT,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
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
		{ OFA_BOX_CSV( ACC_RECONCILIABLE ),
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
										/* below data are not imported */
		{ OFA_BOX_CSV( ACC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_VAL_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_VAL_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ROUGH_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ROUGH_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_OPEN_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_OPEN_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FUT_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FUT_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

struct _ofoAccountPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

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
	const gchar *number;
	gint         children_count;
	GList       *children_list;
}
	sChildren;

static void         on_hub_new_object( ofaHub *hub, ofoBase *object, void *empty );
static void         on_new_object_entry( ofaHub *hub, ofoEntry *entry );
static void         on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty );
static void         on_updated_object_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code );
static void         on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty );
static ofoAccount  *account_find_by_number( GList *set, const gchar *number );
static gint         account_count_for_currency( const ofaIDBConnect *connect, const gchar *currency );
static gint         account_count_for( const ofaIDBConnect *connect, const gchar *field, const gchar *mnemo );
static gint         account_count_for_like( const ofaIDBConnect *connect, const gchar *field, gint number );
static const gchar *account_get_string_ex( const ofoAccount *account, gint data_id );
static void         account_get_children( const ofoAccount *account, sChildren *child_str );
static void         account_iter_children( const ofoAccount *account, sChildren *child_str );
static gboolean     do_archive_open_balances( ofoAccount *account, const ofaIDBConnect *connect );
static void         account_set_upd_user( ofoAccount *account, const gchar *user );
static void         account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp );
static void         account_set_open_debit( ofoAccount *account, ofxAmount amount );
static void         account_set_open_credit( ofoAccount *account, ofxAmount amount );
static gboolean     account_do_insert( ofoAccount *account, const ofaIDBConnect *connect );
static gboolean     account_do_update( ofoAccount *account, const ofaIDBConnect *connect, const gchar *prev_number );
static gboolean     account_do_update_amounts( ofoAccount *account, const ofaIDBConnect *connect );
static gboolean     account_do_delete( ofoAccount *account, const ofaIDBConnect *connect );
static gint         account_cmp_by_number( const ofoAccount *a, const gchar *number );
static gint         account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b );
static void         icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint        icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList       *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean     iimportable_import( ofaIImportable *exportable, GSList *lines, const ofaFileFormat *settings, ofaHub *hub );
static gboolean     account_do_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoAccount, ofo_account, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoAccount )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
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
 * ofo_account_connect_to_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_account_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_account_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	g_signal_connect( G_OBJECT( hub ),
			SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), NULL );

	g_signal_connect( G_OBJECT( hub ),
			SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), NULL );

	g_signal_connect( G_OBJECT( hub ),
			SIGNAL_HUB_STATUS_CHANGE, G_CALLBACK( on_hub_entry_status_change ), NULL );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_account_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	if( OFO_IS_ENTRY( object )){
		on_new_object_entry( hub, OFO_ENTRY( object ));
	}
}

/*
 * a new entry has been recorded, so update the daily balances
 */
static void
on_new_object_entry( ofaHub *hub, ofoEntry *entry )
{
	ofaEntryStatus status;
	ofoAccount *account;
	gdouble debit, credit, prev;

	/* the only case where an entry is created with a 'past' status
	 *  is an imported entry in the past (before the beginning of the
	 *  exercice) - in this case, the 'new_object' message should not be
	 *  sent
	 * if not in the past, only allowed status are 'rough' or 'future' */
	status = ofo_entry_get_status( entry );
	g_return_if_fail( status != ENT_STATUS_PAST );
	g_return_if_fail( status == ENT_STATUS_ROUGH || status == ENT_STATUS_FUTURE );

	account = ofo_account_get_by_number( hub, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	/* impute the new entry either to the debit or the credit of daily
	 * or futur balances depending of the position of the effect date
	 * vs. ending date of the exercice
	 */
	switch( status ){
		case ENT_STATUS_ROUGH:
			if( debit ){
				prev = ofo_account_get_rough_debit( account );
				ofo_account_set_rough_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_rough_credit( account );
				ofo_account_set_rough_credit( account, prev+credit );
			}
			break;

		case ENT_STATUS_FUTURE:
			if( debit ){
				prev = ofo_account_get_futur_debit( account );
				ofo_account_set_futur_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_futur_credit( account );
				ofo_account_set_futur_credit( account, prev+credit );
			}
			break;

		default:
			g_return_if_reached();
			break;
	}

	ofo_account_update_amounts( account );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_account_on_hub_updated_object";
	const gchar *code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( hub, prev_id, code );
			}
		}
	}
}

/*
 * the currency code iso has been modified: update the accounts which
 * use it
 */
static void
on_updated_object_currency_code( ofaHub *hub, const gchar *prev_id, const gchar *code )
{
	gchar *query;
	GList *collection;

	query = g_strdup_printf(
					"UPDATE OFA_T_ACCOUNTS SET ACC_CURRENCY='%s' WHERE ACC_CURRENCY='%s'",
						code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	collection = ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_ACCOUNT );
	if( collection && g_list_length( collection )){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_ACCOUNT );
	}
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, void *empty )
{
	static const gchar *thisfn = "ofo_account_on_hub_entry_status_change";
	ofoAccount *account;
	ofxAmount debit, credit, amount;

	g_debug( "%s: hub=%p, entry=%p, prev_status=%u, new_status=%u, empty=%p",
			thisfn, ( void * ) hub, ( void * ) entry, prev_status, new_status, ( void * ) empty );

	account = ofo_account_get_by_number( hub, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	switch( prev_status ){
		case ENT_STATUS_ROUGH:
			amount = ofo_account_get_rough_debit( account );
			ofo_account_set_rough_debit( account, amount-debit );
			amount = ofo_account_get_rough_credit( account );
			ofo_account_set_rough_credit( account, amount-credit );
			break;
		case ENT_STATUS_VALIDATED:
			amount = ofo_account_get_val_debit( account );
			ofo_account_set_val_debit( account, amount-debit );
			amount = ofo_account_get_val_credit( account );
			ofo_account_set_val_credit( account, amount-credit );
			break;
		case ENT_STATUS_FUTURE:
			amount = ofo_account_get_futur_debit( account );
			ofo_account_set_futur_debit( account, amount-debit );
			amount = ofo_account_get_futur_credit( account );
			ofo_account_set_futur_credit( account, amount-credit );
			break;
		default:
			break;
	}

	switch( new_status ){
		case ENT_STATUS_ROUGH:
			amount = ofo_account_get_rough_debit( account );
			ofo_account_set_rough_debit( account, amount+debit );
			amount = ofo_account_get_rough_credit( account );
			ofo_account_set_rough_credit( account, amount+credit );
			break;
		case ENT_STATUS_VALIDATED:
			amount = ofo_account_get_val_debit( account );
			ofo_account_set_val_debit( account, amount+debit );
			amount = ofo_account_get_val_credit( account );
			ofo_account_set_val_credit( account, amount+credit );
			break;
		case ENT_STATUS_FUTURE:
			amount = ofo_account_get_futur_debit( account );
			ofo_account_set_futur_debit( account, amount+debit );
			amount = ofo_account_get_futur_credit( account );
			ofo_account_set_futur_credit( account, amount+credit );
			break;
		default:
			break;
	}

	ofo_account_update_amounts( account );
}

/**
 * ofo_account_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoAccount dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_account_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_ACCOUNT ));
}

/**
 * ofo_account_get_dataset_for_solde:
 * @hub: the #ofaHub object.
 *
 * Returns: the #ofoAccount dataset, without the solde accounts.
 *
 * The returned list should be #ofo_account_free_dataset() by the caller.
 */
GList *
ofo_account_get_dataset_for_solde( ofaHub *hub )
{
	GList *dataset;
	gchar *query;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	query = g_strdup_printf( "OFA_T_ACCOUNTS WHERE "
			"	ACC_ROOT!='Y' AND "
			"	ACC_NUMBER NOT IN (SELECT DOS_SLD_ACCOUNT FROM OFA_T_DOSSIER_CUR)" );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					query,
					OFO_TYPE_ACCOUNT,
					hub );

	g_free( query );

	return( dataset );
}

/**
 * ofo_account_get_by_number:
 * @hub: the #ofaHub object.
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
ofo_account_get_by_number( ofaHub *hub, const gchar *number )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	if( !my_strlen( number )){
		return( NULL );
	}

	dataset = ofo_account_get_dataset( hub );

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
 * ofo_account_use_class:
 * @hub: the #ofaHub object.
 * @number: the searched class number
 *
 * Returns: %TRUE if at least one recorded account makes use of the
 * specified class number.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
gboolean
ofo_account_use_class( ofaHub *hub, gint number )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ofo_account_get_dataset( hub );

	return( account_count_for_like( ofa_hub_get_connect( hub ), "ACC_NUMBER", number ) > 0 );
}

/**
 * ofo_account_use_currency:
 * @hub: the #ofaHub object.
 * @currency: the currency ISO 3A code
 *
 * Returns: %TRUE if at least one recorded account makes use of the
 * specified currency.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
gboolean
ofo_account_use_currency( ofaHub *hub, const gchar *currency )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( my_strlen( currency ), FALSE );

	ofo_account_get_dataset( hub );

	return( account_count_for_currency( ofa_hub_get_connect( hub ), currency ) > 0 );
}

static gint
account_count_for_currency( const ofaIDBConnect *connect, const gchar *currency )
{
	return( account_count_for( connect, "ACC_CURRENCY", currency ));
}

static gint
account_count_for( const ofaIDBConnect *connect, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE %s='%s'", field, mnemo );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( count );
}

static gint
account_count_for_like( const ofaIDBConnect *connect, const gchar *field, gint number )
{
	gint count;
	gchar *query;

	count = 0;
	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE %s LIKE '%d%%'", field, number );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_account_new:
 *
 * Returns: a new #ofoAccount object.
 */
ofoAccount *
ofo_account_new( void )
{
	ofoAccount *account;

	account = g_object_new( OFO_TYPE_ACCOUNT, NULL );
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
 * ofo_account_get_closed:
 * @account: the #ofoAccount account
 *
 * Returns: the 'Closed' code or %NULL.
 */
const gchar *
ofo_account_get_closed( const ofoAccount *account )
{
	account_get_string( ACC_CLOSED );
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
 * ofo_account_get_val_debit:
 * @account: the #ofoAccount account
 *
 * Returns: the validated debit balance of the @account.
 */
ofxAmount
ofo_account_get_val_debit( const ofoAccount *account )
{
	account_get_amount( ACC_VAL_DEBIT );
}

/**
 * ofo_account_get_val_credit:
 * @account: the #ofoAccount account
 *
 * Returns: the validated credit balance of the @account.
 */
ofxAmount
ofo_account_get_val_credit( const ofoAccount *account )
{
	account_get_amount( ACC_VAL_CREDIT );
}

/**
 * ofo_account_get_rough_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_rough_debit( const ofoAccount *account )
{
	account_get_amount( ACC_ROUGH_DEBIT );
}

/**
 * ofo_account_get_rough_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_rough_credit( const ofoAccount *account )
{
	account_get_amount( ACC_ROUGH_CREDIT );
}

/**
 * ofo_account_get_open_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_open_debit( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_DEBIT );
}

/**
 * ofo_account_get_open_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_open_credit( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_CREDIT );
}

/**
 * ofo_account_get_futur_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_futur_debit( const ofoAccount *account )
{
	account_get_amount( ACC_FUT_DEBIT );
}

/**
 * ofo_account_get_futur_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_futur_credit( const ofoAccount *account )
{
	account_get_amount( ACC_FUT_CREDIT );
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
	ofaHub *hub;
	gboolean deletable;
	GList *children, *it;
	const gchar *number;
	ofoDossier *dossier;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	hub = ofo_base_get_hub( OFO_BASE( account ));
	dossier = ofa_hub_get_dossier( hub );
	number = ofo_account_get_number( account );
	deletable = !ofo_entry_use_account( hub, number ) &&
				!ofo_dossier_use_account( dossier, number );

	if( ofo_account_is_root( account ) && ofa_prefs_account_delete_root_with_children()){
		children = ofo_account_get_children( account );
		for( it=children ; it ; it=it->next ){
			deletable &= ofo_account_is_deletable( OFO_ACCOUNT( it->data ));
		}
		g_list_free( children );
	}

	return( deletable );
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
 * Returns: %TRUE if the account is settleable
 *
 * All accounts are actually settleable, i.e. all entries may be
 * settled. But only unsettled entries written on settleable accounts
 * will be reported on next exercice at closing time.
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
 * ofo_account_is_reconciliable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is reconciliable
 *
 * All accounts are actually reconciliable, i.e. all entries may be
 * reconciliated. But only unreconciliated entries written on
 * reconciliable accounts will be reported on next exercice at closing
 * time.
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
			*msgerr = g_strdup_printf( _( "Account identifier '%s' is too short" ), number );
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
 * ofo_account_get_global_deffect:
 * @account: the #ofoAccount account.
 * @date: [out]: where to store the returned date.
 *
 * Returns the most recent effect date of the account, keeping into
 * account both validated, rough and future entries.
 *
 * This is used when printing reconciliation summary to qualify the
 * starting date of the print.
 *
 * The returned value may be %NULL or not valid if no entry has ever
 * been recorded on this @account.
 */
GDate *
ofo_account_get_global_deffect( const ofoAccount *account, GDate *date )
{
	ofaHub *hub;
	GDate max_val, max_rough, max_futur;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	hub = ofo_base_get_hub( OFO_BASE( account ));

	ofo_entry_get_max_val_deffect( hub, ofo_account_get_number( account ), &max_val );
	ofo_entry_get_max_rough_deffect( hub, ofo_account_get_number( account ), &max_rough );
	ofo_entry_get_max_futur_deffect( hub, ofo_account_get_number( account ), &max_futur );

	my_date_clear( date );

	if( my_date_is_valid( &max_val )){
		my_date_set_from_date( date, &max_val );
	}

	if( my_date_is_valid( &max_rough ) &&
			( !my_date_is_valid( date ) || my_date_compare( date, &max_rough ) < 0 )){
		my_date_set_from_date( date, &max_rough );
	}

	if( my_date_is_valid( &max_futur ) &&
			( !my_date_is_valid( date ) || my_date_compare( date, &max_futur ) < 0 )){
		my_date_set_from_date( date, &max_futur );
	}

	return( date );
}

/**
 * ofo_account_get_global_solde:
 * @account: the #ofoAccount account
 *
 * Returns: the current global balance of the @account, taking into
 * account both validated and rough balances.
 *
 * This is used when printing reconciliation summary to qualify the
 * starting solde of the print.
 *
 * The returned value is lesser than zero for a debit, or greater than
 * zero for a credit.
 */
ofxAmount
ofo_account_get_global_solde( const ofoAccount *account )
{
	ofxAmount amount;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	amount -= ofo_account_get_val_debit( account );
	amount += ofo_account_get_val_credit( account );
	amount -= ofo_account_get_rough_debit( account );
	amount += ofo_account_get_rough_credit( account );

	return( amount );
}

/**
 * ofo_account_has_children:
 * @account: the #ofoAccount account
 *
 * Whether an account has children is only relevant for a root account
 * (but this is not checked here).
 */
gboolean
ofo_account_has_children( const ofoAccount *account )
{
	sChildren child_str;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

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
	child_str->number = ofo_account_get_number( account );
	child_str->children_count = 0;
	child_str->children_list = NULL;

	dataset = ofo_account_get_dataset( ofo_base_get_hub( OFO_BASE( account )));

	g_list_foreach( dataset, ( GFunc ) account_iter_children, child_str );
}

static void
account_iter_children( const ofoAccount *account, sChildren *child_str )
{
	const gchar *number;

	number = ofo_account_get_number( account );
	if( g_str_has_prefix( number, child_str->number ) &&
			g_utf8_collate( number, child_str->number ) > 0 ){

		child_str->children_count += 1;
		child_str->children_list = g_list_append( child_str->children_list, ( gpointer ) account );
	}
}

/**
 * ofo_account_is_child_of:
 * @account: the #ofoAccount account
 * @candidate: another account to be compared relatively to @account
 *
 * Returns: %TRUE if the @number should logically be a child of @æccount.
 */
gboolean
ofo_account_is_child_of( const ofoAccount *account, const ofoAccount *candidate )
{
	const gchar *account_number;
	const gchar *candidate_number;
	gboolean is_child;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( candidate && OFO_IS_ACCOUNT( candidate ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	account_number = ofo_account_get_number( account );
	candidate_number = ofo_account_get_number( candidate );
	is_child = g_str_has_prefix( candidate_number, account_number );

	return( is_child );
}

/**
 * ofo_account_is_allowed:
 * @account:
 * @allowables:
 *
 * Returns: %TRUE if the @account is allowed regarding the specifications
 * if @allowables.
 */
gboolean
ofo_account_is_allowed( const ofoAccount *account, gint allowables )
{
	gboolean ok;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	if( !ofo_account_is_closed( account ) || ( allowables & ACCOUNT_ALLOW_CLOSED )){

		if( !ok && ( allowables & ACCOUNT_ALLOW_ALL )){
			ok = TRUE;
		}
		if( !ok && ( allowables & ACCOUNT_ALLOW_ROOT )){
			if( ofo_account_is_root( account )){
				ok = TRUE;
			}
		}
		if( !ok && ( allowables & ACCOUNT_ALLOW_DETAIL )){
			if( !ofo_account_is_root( account )){
				ok = TRUE;
			}
		}
		if( !ok && ( allowables & ACCOUNT_ALLOW_SETTLEABLE )){
			if( ofo_account_is_settleable( account )){
				ok = TRUE;
			}
		}
		if( !ok && ( allowables & ACCOUNT_ALLOW_RECONCILIABLE )){
			if( ofo_account_is_reconciliable( account )){
				ok = TRUE;
			}
		}
	}

	return( ok );
}

/**
 * ofo_account_has_open_balances:
 * @hub: the current #ofaHub object.
 *
 * Returns: %TRUE if at least one account has an opening balance.
 * This means that there has been an archived exercice before this one,
 * and thus that the beginning date of the exercice cannot be modified.
 */
gboolean
ofo_account_has_open_balance( const ofaHub *hub )
{
	gint count;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ofa_idbconnect_query_int(
			ofa_hub_get_connect( hub ),
			"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE ACC_OPEN_DEBIT>0 OR ACC_OPEN_CREDIT>0",
			&count, TRUE );

	return( count > 0 );
}

/**
 * ofo_account_archive_open_balances:
 * @account: this #ofoAccount object.
 * @hub: the current #ofaHub object.
 *
 * As part of the closing of the exercice N, we archive the balances
 * of the accounts at the beginning of the exercice N+1. These balances
 * take into account the carried forward entries (fr: reports à
 * nouveau).
 *
 * At this time, all ledgers on exercice N should have been closed, and
 * no entries should have been recorded in ledgers for exercice N+1.
 */
gboolean
ofo_account_archive_open_balances( ofoAccount *account )
{
	gboolean ok;
	ofaHub *hub;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	if( ofo_account_is_root( account )){
		ok = TRUE;

	} else {
		hub = ofo_base_get_hub( OFO_BASE( account ));
		ok = do_archive_open_balances( account, ofa_hub_get_connect( hub ));
	}

	return( ok );
}

static gboolean
do_archive_open_balances( ofoAccount *account, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *samount;
	ofxAmount amount;
	gboolean ok;

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	amount = ofo_account_get_rough_debit( account )+ofo_account_get_val_debit( account );
	account_set_open_debit( account, amount );
	samount = my_double_to_sql( amount );
	g_string_append_printf( query, "ACC_OPEN_DEBIT=%s,", samount );
	g_free( samount );

	amount = ofo_account_get_rough_credit( account )+ofo_account_get_val_credit( account );
	account_set_open_credit( account, amount );
	samount = my_double_to_sql( amount );
	g_string_append_printf( query, "ACC_OPEN_CREDIT=%s ", samount );
	g_free( samount );

	g_string_append_printf( query,
			"WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
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
 * ofo_account_set_notes:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	account_set_string( ACC_NOTES, notes );
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
 * ofo_ofo_account_set_val_debit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_val_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_VAL_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_val_credit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_val_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_VAL_CREDIT, amount );
}

/**
 * ofo_ofo_account_set_rough_debit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_rough_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_ROUGH_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_rough_credit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_rough_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_ROUGH_CREDIT, amount );
}

/*
 * ofo_account_set_open_debit:
 * @account: the #ofoAccount account
 */
static void
account_set_open_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_OPEN_DEBIT, amount );
}

/*
 * ofo_account_set_open_credit:
 * @account: the #ofoAccount account
 */
static void
account_set_open_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_OPEN_CREDIT, amount );
}

/**
 * ofo_ofo_account_set_futur_debit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_futur_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FUT_DEBIT, amount );
}

/**
 * ofo_ofo_account_set_futur_credit:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_futur_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FUT_CREDIT, amount );
}

/**
 * ofo_account_insert:
 * @account: the new #ofoAccount account to be inserted.
 * @hub: the #ofaHub object.
 *
 * This function is only of use when the user creates a new account.
 * It is no worth to deal here with amounts and/or debit/credit agregates.
 *
 * Returns: %TRUE if the insertion has been successful.
 */
gboolean
ofo_account_insert( ofoAccount *account, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_account_insert";
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: account=%p, hub=%p",
			thisfn, ( void * ) account, ( void * ) hub );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	connect = ofa_hub_get_connect( hub );

	if( account_do_insert( account, connect )){
		ofo_base_set_hub( OFO_BASE( account ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( account ), ( GCompareFunc ) account_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, account );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
account_do_insert( ofoAccount *account, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *label, *notes, *userid;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;

	ok = FALSE;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_account_get_label( account ));
	notes = my_utils_quote_single( ofo_account_get_notes( account ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ACCOUNTS" );

	g_string_append_printf( query,
			"	(ACC_NUMBER,ACC_LABEL,ACC_CURRENCY,ACC_NOTES,"
			"	ACC_ROOT,ACC_SETTLEABLE,ACC_RECONCILIABLE,ACC_FORWARDABLE,"
			"	ACC_CLOSED,ACC_UPD_USER, ACC_UPD_STAMP)"
			"	VALUES ('%s','%s',",
					ofo_account_get_number( account ),
					label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "'%s',", ofo_account_get_currency( account ));
	}

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_account_is_root( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_settleable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_reconciliable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_forwardable( account ) ? "Y":"N" );
	g_string_append_printf( query, "'%s',", ofo_account_is_closed( account ) ? "Y":"N" );

	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		account_set_upd_user( account, userid );
		account_set_upd_stamp( account, &stamp );
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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p, prev_number=%s",
				thisfn, ( void * ) account, prev_number );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( account ));

	if( account_do_update( account, ofa_hub_get_connect( hub ), prev_number )){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_ACCOUNT, ( GCompareFunc ) account_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, account, prev_number );
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
	gchar *label, *notes, *userid;
	gboolean ok;
	const gchar *new_number;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;

	userid = ofa_idbconnect_get_account( connect );
	label = my_utils_quote_single( ofo_account_get_label( account ));
	notes = my_utils_quote_single( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	if( prev_number && g_utf8_collate( new_number, prev_number )){
		g_string_append_printf( query, "ACC_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query, "ACC_LABEL='%s',", label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "ACC_CURRENCY=NULL," );
	} else {
		g_string_append_printf( query, "ACC_CURRENCY='%s',", ofo_account_get_currency( account ));
	}

	if( my_strlen( notes )){
		g_string_append_printf( query, "ACC_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "ACC_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	ACC_ROOT='%s',", ofo_account_is_root( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_SETTLEABLE='%s',", ofo_account_is_settleable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_RECONCILIABLE='%s',", ofo_account_is_reconciliable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_FORWARDABLE='%s',", ofo_account_is_forwardable( account ) ? "Y":"N" );

	g_string_append_printf( query,
			"	ACC_CLOSED='%s',", ofo_account_is_closed( account ) ? "Y":"N" );

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
	g_free( userid );

	return( ok );
}

/**
 * ofo_account_update_amounts:
 */
gboolean
ofo_account_update_amounts( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_do_update_amounts";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( account ));

	if( account_do_update_amounts( account, ofa_hub_get_connect( hub ))){
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, account, NULL );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
account_do_update_amounts( ofoAccount *account, const ofaIDBConnect *connect )
{
	GString *query;
	gboolean ok;
	gchar *samount;
	ofxAmount amount;

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	/* validated debit */
	amount = ofo_account_get_val_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_VAL_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_VAL_DEBIT=NULL," );
	}

	/* validated credit */
	amount = ofo_account_get_val_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_VAL_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_VAL_CREDIT=NULL," );
	}

	/* rough debit */
	amount = ofo_account_get_rough_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_ROUGH_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_ROUGH_DEBIT=NULL," );
	}

	/* rough credit */
	amount = ofo_account_get_rough_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_ROUGH_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_ROUGH_CREDIT=NULL," );
	}

	/* future debit */
	amount = ofo_account_get_futur_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_FUT_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FUT_DEBIT=NULL," );
	}

	/* future credit */
	amount = ofo_account_get_futur_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_FUT_CREDIT=%s ", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FUT_CREDIT=NULL " );
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
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: account=%p", thisfn, ( void * ) account );

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( ofo_account_is_deletable( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( account ));

	if( account_do_delete( account, ofa_hub_get_connect( hub ))){
		g_object_ref( account );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( account ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, account );
		g_object_unref( account );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
account_do_delete( ofoAccount *account, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_ACCOUNTS"
			"	WHERE ACC_NUMBER='%s'",
					ofo_account_get_number( account ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
account_cmp_by_number( const ofoAccount *a, const gchar *number )
{
	return( g_utf8_collate( ofo_account_get_number( a ), number ));
}

static gint
account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b )
{
	return( account_cmp_by_number( a, ofo_account_get_number( b )));
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
					"OFA_T_ACCOUNTS ORDER BY ACC_NUMBER ASC",
					OFO_TYPE_ACCOUNT,
					hub );

	return( list );
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
 * Exports the accounts line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	GSList *lines;
	gchar *str;
	GList *dataset, *it;
	gboolean with_headers, ok;
	gulong count;

	dataset = ofo_account_get_dataset( hub );
	with_headers = ofa_file_format_has_headers( settings );

	count = ( gulong ) g_list_length( dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=dataset ; it ; it=it->next ){
		str = ofa_box_csv_get_line( OFO_BASE( it->data )->prot->fields, settings );
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
	static const gchar *thisfn = "ofo_account_iimportable_iface_init";

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
 * ofo_account_iimportable_import:
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
 * Replace the whole table with the provided datas.
 * All the balances are set to NULL.
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
	ofoDossier *dossier;
	const gchar *def_dev_code;
	ofoAccount *account;
	GList *dataset, *it;
	guint errors, class_num, line;
	gchar *msg, *splitted, *dev_code;
	ofoCurrency *currency;
	ofoClass *class_obj;
	gchar *str;
	const ofaIDBConnect *connect;

	line = 0;
	errors = 0;
	dataset = NULL;
	dossier = ofa_hub_get_dossier( hub );
	def_dev_code = ofo_dossier_get_default_currency( dossier );

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		account = ofo_account_new();
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );
		itf = fields;

		/* account number */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !str ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty account number" ));
			errors += 1;
			continue;
		}
		class_num = ofo_account_get_class_from_number( str );
		class_obj = ofo_class_get_by_number( hub, class_num );
		if( !class_obj && !OFO_IS_CLASS( class_obj )){
			msg = g_strdup_printf( _( "invalid account class number: %s" ), str );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( str );
			errors += 1;
			continue;
		}
		ofo_account_set_number( account, str );
		g_free( str );

		/* account label */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !str ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty account label" ));
			errors += 1;
			continue;
		}
		ofo_account_set_label( account, str );

		/* currency code */
		str = ofa_iimportable_get_string( &itf, settings );
		if( str ){
			dev_code = g_strdup( str );
		} else {
			dev_code = g_strdup( def_dev_code );
		}
		g_free( str );
		currency = ofo_currency_get_by_code( hub, dev_code );
		if( !currency ){
			msg = g_strdup_printf( _( "invalid account currency: %s" ), dev_code );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( dev_code );
			errors += 1;
			continue;
		}
		ofo_account_set_currency( account, dev_code );
		g_free( dev_code );

		/* root account
		 * previous to DB model v27, root/detail accounts were marked with R/D
		 * starting with v27, root accounts are marked with Y/N */
		str = ofa_iimportable_get_string( &itf, settings );
		if( !str ){
			str = g_strdup( "N" );
		} else if( g_utf8_collate( str, ACCOUNT_TYPE_DETAIL ) &&
					g_utf8_collate( str, ACCOUNT_TYPE_ROOT ) &&
					g_utf8_collate( str, "Y" ) &&
					g_utf8_collate( str, "N" )){
			msg = g_strdup_printf( _( "invalid account type: %s" ), str );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( str );
			errors += 1;
			continue;
		}
		ofo_account_set_root( account,
				!my_collate( str, ACCOUNT_TYPE_ROOT ) || !my_collate( str, "Y" ));
		g_free( str );

		/* settleable ? */
		str = ofa_iimportable_get_string( &itf, settings );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_SETTLEABLE ) &&
					g_utf8_collate( str, "Y" ) && g_utf8_collate( str, "N" )){
				msg = g_strdup_printf( _( "invalid settleable account indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_settleable( account,
						!my_collate( str, ACCOUNT_SETTLEABLE ) || !my_collate( str, "Y" ));
			}
		}
		g_free( str );

		/* reconciliable ? */
		str = ofa_iimportable_get_string( &itf, settings );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_RECONCILIABLE ) &&
					g_utf8_collate( str, "Y" ) && g_utf8_collate( str, "N" )){
				msg = g_strdup_printf( _( "invalid reconciliable account indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_reconciliable( account,
						!my_collate( str, ACCOUNT_RECONCILIABLE ) || !my_collate( str, "Y" ));
			}
		}
		g_free( str );

		/* carried forwardable ? */
		str = ofa_iimportable_get_string( &itf, settings );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_FORWARDABLE ) &&
					g_utf8_collate( str, "Y" ) && g_utf8_collate( str, "N" )){
				msg = g_strdup_printf( _( "invalid forwardable account indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_forwardable( account,
						!my_collate( str, ACCOUNT_FORWARDABLE ) || !my_collate( str, "Y" ));
			}
		}
		g_free( str );

		/* closed ? */
		str = ofa_iimportable_get_string( &itf, settings );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_CLOSED ) &&
					g_utf8_collate( str, "Y" ) && g_utf8_collate( str, "N" )){
				msg = g_strdup_printf( _( "invalid closed account indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_closed( account,
						!my_collate( str, ACCOUNT_CLOSED ) || !my_collate( str, "Y" ));
			}
		}
		g_free( str );

		/* notes
		 * we are tolerant on the last field... */
		str = ofa_iimportable_get_string( &itf, settings );
		splitted = my_utils_import_multi_lines( str );
		ofo_account_set_notes( account, splitted );
		g_free( splitted );
		g_free( str );

		dataset = g_list_prepend( dataset, account );
	}

	if( !errors ){
		connect = ofa_hub_get_connect( hub );
		account_do_drop_content( connect );

		for( it=dataset ; it ; it=it->next ){
			if( !account_do_insert( OFO_ACCOUNT( it->data ), connect )){
				errors -= 1;
			}
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		ofo_account_free_dataset( dataset );
		ofa_icollector_free_collection( OFA_ICOLLECTOR( hub ), OFO_TYPE_ACCOUNT );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_ACCOUNT );
	}

	return( errors );
}

static gboolean
account_do_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_ACCOUNTS", TRUE ));
}
