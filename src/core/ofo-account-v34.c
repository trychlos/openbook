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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account-v34.h"
#include "api/ofo-entry.h"

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
	ACC_FUT_DEBIT,
	ACC_FUT_CREDIT,
	ACC_ARC_DATE,
	ACC_ARC_DEBIT,
	ACC_ARC_CREDIT,
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

static const ofsBoxDef st_archive_defs[] = {
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ARC_DATE ),
				OFA_TYPE_DATE,
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

typedef struct {
	GList *archives;					/* archived balances of the account */
}
	ofoAccountv34Private;

#define account_get_amount(I)           ofo_base_getter(ACCOUNT_V34,account,amount,0,(I))
#define account_get_counter(I)          ofo_base_getter(ACCOUNT_V34,account,counter,0,(I))
#define account_get_date(I)             ofo_base_getter(ACCOUNT_V34,account,date,NULL,(I))
#define account_get_string(I)           ofo_base_getter(ACCOUNT_V34,account,string,NULL,(I))
#define account_get_timestamp(I)        ofo_base_getter(ACCOUNT_V34,account,timestamp,NULL,(I))

#define account_set_amount(I,V)         ofo_base_setter(ACCOUNT_V34,account,amount,(I),(V))
#define account_set_counter(I,V)        ofo_base_setter(ACCOUNT_V34,account,counter,(I),(V))
#define account_set_date(I,V)           ofo_base_setter(ACCOUNT_V34,account,date,(I),(V))
#define account_set_string(I,V)         ofo_base_setter(ACCOUNT_V34,account,string,(I),(V))
#define account_set_timestamp(I,V)      ofo_base_setter(ACCOUNT_V34,account,timestamp,(I),(V))

static void           archives_list_free_detail( GList *fields );
static void           archives_list_free( ofoAccountv34 *account );
static ofoAccountv34 *account_find_by_number( GList *set, const gchar *number );
static gboolean       entry_get_account_balance( ofoAccountv34 *account, const GDate *from_date, const GDate *to_date, ofxAmount *debit, ofxAmount *credit );
static gboolean       archive_do_add_dbms( ofoAccountv34 *account, const GDate *date, ofxAmount debit, ofxAmount credit );
static void           archive_do_add_list( ofoAccountv34 *account, const GDate *date, ofxAmount debit, ofxAmount credit );
static gint           archive_get_last_index( ofoAccountv34 *account, const GDate *requested );
static gint           account_cmp_by_number( const ofoAccountv34 *a, const gchar *number );
static const gchar   *account_get_string_ex( const ofoAccountv34 *account, gint data_id );

G_DEFINE_TYPE_EXTENDED( ofoAccountv34, ofo_account_v34, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoAccountv34 ))

static void
archives_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
archives_list_free( ofoAccountv34 *account )
{
	ofoAccountv34Private *priv;

	priv = ofo_account_v34_get_instance_private( account );

	if( priv->archives ){
		g_list_free_full( priv->archives, ( GDestroyNotify ) archives_list_free_detail );
		priv->archives = NULL;
	}
}

static void
account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_v34_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_NUMBER ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_LABEL ));

	g_return_if_fail( instance && OFO_IS_ACCOUNT_V34( instance ));

	/* free data members here */
	archives_list_free( OFO_ACCOUNT_V34( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_v34_parent_class )->finalize( instance );
}

static void
account_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_ACCOUNT_V34( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_v34_parent_class )->dispose( instance );
}

static void
ofo_account_v34_init( ofoAccountv34 *self )
{
	static const gchar *thisfn = "ofo_account_v34_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_account_v34_class_init( ofoAccountv34Class *klass )
{
	static const gchar *thisfn = "ofo_account_v34_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_finalize;
}

/**
 * ofo_account_v34_get_dataset:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the full #ofoAccountv34 dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_account_v34_get_dataset( ofaIGetter *getter )
{
	GList *dataset, *it;
	ofoAccountv34 *account;
	ofoAccountv34Private *priv;
	gchar *from;
	ofaHub *hub;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"OFA_T_ACCOUNTS",
					OFO_TYPE_ACCOUNT_V34,
					getter );

	hub = ofa_igetter_get_hub( getter );

	for( it=dataset ; it ; it=it->next ){
		account = OFO_ACCOUNT_V34( it->data );
		priv = ofo_account_v34_get_instance_private( account );
		from = g_strdup_printf(
				"OFA_T_ACCOUNTS_ARC WHERE ACC_NUMBER='%s'",
				ofo_account_v34_get_number( account ));
		priv->archives =
				ofo_base_load_rows( st_archive_defs, ofa_hub_get_connect( hub ), from );
		g_free( from );
	}

	return( dataset );
}

/**
 * ofo_account_v34_get_by_number:
 * @dataset: the previously loaded dataset.
 * @number: the required account number.
 *
 * Returns: the searched #ofoAccountv34 account, or %NULL.
 *
 * The returned object is owned by the #ofoAccountv34 class, and should
 * not be unreffed by the caller.
 */
ofoAccountv34 *
ofo_account_v34_get_by_number( GList *dataset, const gchar *number )
{
	if( !my_strlen( number )){
		return( NULL );
	}

	return( account_find_by_number( dataset, number ));
}

static ofoAccountv34 *
account_find_by_number( GList *set, const gchar *number )
{
	GList *found;

	found = g_list_find_custom(
				set, number, ( GCompareFunc ) account_cmp_by_number );
	if( found ){
		return( OFO_ACCOUNT_V34( found->data ));
	}

	return( NULL );
}

/**
 * ofo_account_v34_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofoAccountv34 object.
 */
ofoAccountv34 *
ofo_account_v34_new( ofaIGetter *getter )
{
	ofoAccountv34 *account;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	account = g_object_new( OFO_TYPE_ACCOUNT_V34, "ofo-base-getter", getter, NULL );
	OFO_BASE( account )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( account );
}

/**
 * ofo_account_v34_get_number:
 * @account: the #ofoAccountv34 account
 *
 * Returns: the number of the @account.
 */
const gchar *
ofo_account_v34_get_number( const ofoAccountv34 *account )
{
	account_get_string( ACC_NUMBER );
}

/**
 * ofo_account_v34_get_currency:
 * @account: the #ofoAccountv34 account
 *
 * Returns: the currency ISO 3A code of the @account.
 */
const gchar *
ofo_account_v34_get_currency( const ofoAccountv34 *account )
{
	account_get_string( ACC_CURRENCY );
}

/**
 * ofo_account_v34_is_root:
 * @account: the #ofoAccountv34 account.
 *
 * Returns: %TRUE if the @account is a root account, %FALSE if this is
 * a detail account.
 */
gboolean
ofo_account_v34_is_root( const ofoAccountv34 *account )
{
	const gchar *cstr;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );

	cstr = account_get_string_ex( account, ACC_ROOT );

	return( !my_collate( cstr, "Y" ));
}

/**
 * ofo_account_v34_archive_balances:
 * @account: this #ofoAccountv34 object.
 * @exe_begin: the beginning of the exercice.
 * @archive_date: the archived date.
 *
 * Compute the account balance at @archive_date, and archive it into DBMS.
 */
gboolean
ofo_account_v34_archive_balances_ex( ofoAccountv34 *account, const GDate *exe_begin, const GDate *archive_date )
{
	gboolean ok;
	ofxAmount debit, credit;
	gint last_index;
	GDate from_date;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), FALSE );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, FALSE );
	g_return_val_if_fail( !ofo_account_v34_is_root( account ), FALSE );

	ok = FALSE;
	my_date_clear( &from_date );
	last_index = archive_get_last_index( account, archive_date );

	if( last_index >= 0 ){
		my_date_set_from_date( &from_date, ofo_account_v34_archive_get_date( account, last_index ));
		g_return_val_if_fail( my_date_is_valid( &from_date ), FALSE );
		g_date_add_days( &from_date, 1 );

	} else {
		my_date_set_from_date( &from_date, exe_begin );
		/* if beginning date of the exercice is not set, then we will
		 * consider all found entries */
	}

	/* Get balance of entries between two dates
	 * ofoEntry considers all rough+validated entries, and returns
	 *   one line for this account
	 * It is up to the caller to take care of having no rough entries left here */
	if( entry_get_account_balance( account, &from_date, archive_date, &debit, &credit )){
		if( last_index >= 0 ){
			debit += ofo_account_v34_archive_get_debit( account, last_index );
			credit += ofo_account_v34_archive_get_credit( account, last_index );
		}
		if( archive_do_add_dbms( account, archive_date, debit, credit )){
			archive_do_add_list( account, archive_date, debit, credit );
			ok = TRUE;
		}
	}

	return( ok );
}

/**
 * @account: the considered account.
 * @from_date: the starting effect date.
 * @to_date: the ending effect date.
 * @debit: [out]: debit placeholder.
 * @credit: [out]: credit placeholder.
 *
 * Compute the balance for non-deleted entries for the @account, between
 * the specified effect dates.
 *
 * Returns: %TRUE if no error happened.
 */
static gboolean
entry_get_account_balance( ofoAccountv34 *account, const GDate *from_date, const GDate *to_date, ofxAmount *debit, ofxAmount *credit )
{
	static const gchar *thisfn = "ofo_account_v34_entry_get_account_balance";
	ofaIGetter *getter;
	ofaHub *hub;
	ofaIDBConnect *connect;
	GString *query;
	gchar *str;
	GSList *result, *irow, *icol;
	const gchar *acc_number;
	gboolean ok;

	getter = ofo_base_get_getter( OFO_BASE( account ));
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	*debit = 0;
	*credit = 0;
	query = g_string_new( NULL );
	acc_number = ofo_account_v34_get_number( account );

	g_string_printf( query,
			"SELECT SUM(ENT_DEBIT),SUM(ENT_CREDIT) FROM OFA_T_ENTRIES WHERE ENT_ACCOUNT='%s' ",
			acc_number );

	if( my_date_is_valid( from_date )){
		str = my_date_to_str( from_date, MY_DATE_SQL );
		g_string_append_printf( query, "AND ENT_DEFFECT>='%s' ", str );
		g_free( str );
	}
	if( my_date_is_valid( to_date )){
		str = my_date_to_str( to_date, MY_DATE_SQL );
		g_string_append_printf( query, "AND ENT_DEFFECT<='%s' ", str );
		g_free( str );
	}

	g_string_append_printf( query, "AND ENT_STATUS!='%s' ", ofo_entry_status_get_dbms( ENT_STATUS_DELETED ));

	ok = ofa_idbconnect_query_ex( connect, query->str, &result, TRUE );

	if( ok ){
		irow =result;
		icol = ( GSList * ) irow->data;
		*debit = my_double_set_from_sql(( const gchar * ) icol->data );
		icol = icol->next;
		*credit = my_double_set_from_sql(( const gchar * ) icol->data );
		g_debug( "%s: account=%s, debit=%lf, credit=%lf",
					thisfn, acc_number, *debit, *credit );
		ofa_idbconnect_free_results( result );
	}

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
archive_do_add_dbms( ofoAccountv34 *account, const GDate *date, ofxAmount debit, ofxAmount credit )
{
	ofaIGetter *getter;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query, *sdate, *sdebit, *scredit;
	gboolean ok;

	getter = ofo_base_get_getter( OFO_BASE( account ));
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	sdate = my_date_to_str( date, MY_DATE_SQL );
	sdebit = ofa_amount_to_sql( debit, NULL );
	scredit = ofa_amount_to_sql( credit, NULL );

	query = g_strdup_printf(
				"INSERT INTO OFA_T_ACCOUNTS_ARC "
				"	(ACC_NUMBER, ACC_ARC_DATE, ACC_ARC_DEBIT, ACC_ARC_CREDIT) VALUES "
				"	('%s','%s',%s,%s)",
				ofo_account_v34_get_number( account ),
				sdate, sdebit, scredit );

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( sdate );
	g_free( sdebit );
	g_free( scredit );
	g_free( query );

	return( ok );
}

static void
archive_do_add_list( ofoAccountv34 *account, const GDate *date, ofxAmount debit, ofxAmount credit )
{
	ofoAccountv34Private *priv;
	GList *fields;

	priv = ofo_account_v34_get_instance_private( account );

	fields = ofa_box_init_fields_list( st_archive_defs );
	ofa_box_set_string( fields, ACC_NUMBER, ofo_account_v34_get_number( account ));
	ofa_box_set_date( fields, ACC_ARC_DATE, date );
	ofa_box_set_amount( fields, ACC_ARC_DEBIT, debit );
	ofa_box_set_amount( fields, ACC_ARC_CREDIT, credit );

	priv->archives = g_list_append( priv->archives, fields );
}

/**
 * ofo_account_v34_archive_get_count:
 * @account: the #ofoAccountv34 account.
 *
 * Returns: the count of archived balances.
 */
guint
ofo_account_v34_archive_get_count( ofoAccountv34 *account )
{
	ofoAccountv34Private *priv;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_v34_get_instance_private( account );

	return( g_list_length( priv->archives ));
}

/**
 * ofo_account_v34_archive_get_date:
 * @account: the #ofoAccountv34 account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the effect date of the balance.
 */
const GDate *
ofo_account_v34_archive_get_date( ofoAccountv34 *account, guint idx )
{
	ofoAccountv34Private *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), NULL );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, NULL );

	priv = ofo_account_v34_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_date( nth->data, ACC_ARC_DATE ));
	}

	return( NULL );
}

/**
 * ofo_account_v34_archive_get_debit:
 * @account: the #ofoAccountv34 account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived debit.
 */
ofxAmount
ofo_account_v34_archive_get_debit( ofoAccountv34 *account, guint idx )
{
	ofoAccountv34Private *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_v34_get_instance_private( account );

	nth = g_list_nth( priv->archives, idx );
	if( nth ){
		return( ofa_box_get_amount( nth->data, ACC_ARC_DEBIT ));
	}

	return( 0 );
}

/**
 * ofo_account_v34_archive_get_credit:
 * @account: the #ofoAccountv34 account.
 * @idx: the desired index, counted from zero.
 *
 * Returns: the archived credit.
 */
ofxAmount
ofo_account_v34_archive_get_credit( ofoAccountv34 *account, guint idx )
{
	ofoAccountv34Private *priv;
	GList *nth;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT_V34( account ), 0 );
	g_return_val_if_fail( !OFO_BASE( account )->prot->dispose_has_run, 0 );

	priv = ofo_account_v34_get_instance_private( account );

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
archive_get_last_index( ofoAccountv34 *account, const GDate *requested )
{
	ofoAccountv34Private *priv;
	GDate max_date;
	const GDate *it_date;
	gint count, i, found;

	priv = ofo_account_v34_get_instance_private( account );

	found = -1;
	count = g_list_length( priv->archives );
	my_date_clear( &max_date );

	for( i=0 ; i<count ; ++i ){
		it_date = ofo_account_v34_archive_get_date( account, i );
		if( my_date_compare( it_date, requested ) < 0 && my_date_compare_ex( &max_date, it_date, TRUE ) < 0 ){
			my_date_set_from_date( &max_date, it_date );
			found = i;
		}
	}

	return( found );
}

static gint
account_cmp_by_number( const ofoAccountv34 *a, const gchar *number )
{
	return( my_collate( ofo_account_v34_get_number( a ), number ));
}

static const gchar *
account_get_string_ex( const ofoAccountv34 *account, gint data_id )
{
	account_get_string( data_id );
}
