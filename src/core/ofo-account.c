/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-boxed.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-sgbd.h"

#include "core/ofa-preferences.h"

/* priv instance data
 */
enum {
	ACC_NUMBER = 1,
	ACC_LABEL,
	ACC_CURRENCY,
	ACC_NOTES,
	ACC_TYPE,
	ACC_UPD_USER,
	ACC_UPD_STAMP,
	ACC_DEB_ENTRY,
	ACC_DEB_DATE,
	ACC_DEB_AMOUNT,
	ACC_CRE_ENTRY,
	ACC_CRE_DATE,
	ACC_CRE_AMOUNT,
	ACC_DAY_DEB_ENTRY,
	ACC_DAY_DEB_DATE,
	ACC_DAY_DEB_AMOUNT,
	ACC_DAY_CRE_ENTRY,
	ACC_DAY_CRE_DATE,
	ACC_DAY_CRE_AMOUNT,
	ACC_OPEN_DEB_ENTRY,
	ACC_OPEN_DEB_DATE,
	ACC_OPEN_DEB_AMOUNT,
	ACC_OPEN_CRE_ENTRY,
	ACC_OPEN_CRE_DATE,
	ACC_OPEN_CRE_AMOUNT,
};

static const ofsBoxedDef st_boxed_defs[] = {
		{ OFA_BOXED_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_TYPE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_DEB_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_DEB_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_DEB_AMOUNT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_CRE_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_CRE_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_CRE_AMOUNT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_DAY_DEB_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_DAY_DEB_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_DAY_DEB_AMOUNT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_DAY_CRE_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_DAY_CRE_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_DAY_CRE_AMOUNT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_OPEN_DEB_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_OPEN_DEB_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_OPEN_DEB_AMOUNT ),
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_OPEN_CRE_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				TRUE },
		{ OFA_BOXED_CSV( ACC_OPEN_CRE_DATE ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( ACC_OPEN_CRE_AMOUNT ),
				OFA_TYPE_AMOUNT,
				TRUE,
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

G_DEFINE_TYPE( ofoAccount, ofo_account, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, account )

static void        on_new_object( const ofoDossier *dossier, ofoBase *object, gpointer user_data );
static void        on_new_object_entry( const ofoDossier *dossier, ofoEntry *entry );
static void        on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void        on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data );
static GList      *account_load_dataset( void );
static ofoAccount *account_find_by_number( GList *set, const gchar *number );
static gint        account_count_for_currency( const ofoSgbd *sgbd, const gchar *currency );
static gint        account_count_for( const ofoSgbd *sgbd, const gchar *field, const gchar *mnemo );
static void        archive_open_balances( ofoAccount *account, void *empty );
static void        account_get_children( const ofoAccount *account, sChildren *child_str );
static void        account_iter_children( const ofoAccount *account, sChildren *child_str );
static void        account_set_upd_user( ofoAccount *account, const gchar *user );
static void        account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp );
static void        account_set_deb_entry( ofoAccount *account, gint number );
static void        account_set_deb_date( ofoAccount *account, const GDate *effect );
static void        account_set_deb_amount( ofoAccount *account, gdouble amount );
static void        account_set_cre_entry( ofoAccount *account, gint number );
static void        account_set_cre_date( ofoAccount *account, const GDate *effect );
static void        account_set_cre_amount( ofoAccount *account, gdouble amount );
static void        account_set_day_deb_entry( ofoAccount *account, gint number );
static void        account_set_day_deb_date( ofoAccount *account, const GDate *effect );
static void        account_set_day_deb_amount( ofoAccount *account, gdouble amount );
static void        account_set_day_cre_entry( ofoAccount *account, gint number );
static void        account_set_day_cre_date( ofoAccount *account, const GDate *effect );
static void        account_set_day_cre_amount( ofoAccount *account, gdouble amount );
static void        account_set_open_deb_entry( ofoAccount *account, gint number );
static void        account_set_open_deb_date( ofoAccount *account, const GDate *effect );
static void        account_set_open_deb_amount( ofoAccount *account, gdouble amount );
static void        account_set_open_cre_entry( ofoAccount *account, gint number );
static void        account_set_open_cre_date( ofoAccount *account, const GDate *effect );
static void        account_set_open_cre_amount( ofoAccount *account, gdouble amount );
static gboolean    account_do_insert( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user );
static gboolean    account_do_update( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_number );
static gboolean    account_update_amounts( ofoAccount *account, const ofoSgbd *sgbd );
static gboolean    account_do_delete( ofoAccount *account, const ofoSgbd *sgbd );
static gint        account_cmp_by_number( const ofoAccount *a, const gchar *number );
static gint        account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b );
static gboolean    account_do_drop_content( const ofoSgbd *sgbd );

static void
account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_boxed_get_string( OFO_BASE( instance )->prot->fields, ACC_NUMBER ),
			ofa_boxed_get_string( OFO_BASE( instance )->prot->fields, ACC_LABEL ));

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_ACCOUNT, ofoAccountPrivate );
}

static void
ofo_account_class_init( ofoAccountClass *klass )
{
	static const gchar *thisfn = "ofo_account_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_finalize;

	g_type_class_add_private( klass, sizeof( ofoAccountPrivate ));
}

static void
free_account_balance( ofsAccountBalance *sbal )
{
	g_free( sbal->account );
	g_free( sbal );
}

/**
 * ofo_account_free_balances:
 *
 * Free a list of dynamically allocated ofsAccountBalance structures.
 */
void
ofo_account_free_balances( GList *balances )
{
	g_list_free_full( balances, ( GDestroyNotify ) free_account_balance );
}

/**
 * ofo_account_connect_handlers:
 *
 * This function is called once, when opening the dossier.
 */
void
ofo_account_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_connect_handlers";

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				OFA_SIGNAL_VALIDATED_ENTRY, G_CALLBACK( on_validated_entry ), NULL );
}

static void
on_new_object( const ofoDossier *dossier, ofoBase *object, gpointer user_data )
{
	static const gchar *thisfn = "ofo_account_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) user_data );

	if( OFO_IS_ENTRY( object )){
		on_new_object_entry( dossier, OFO_ENTRY( object ));
	}
}

static void
on_new_object_entry( const ofoDossier *dossier, ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_account_on_new_object_entry";
	ofoAccount *account;
	gdouble debit, credit;
	gint number;
	const GDate *deffect, *prev_deffect;
	gdouble prev;
	gint prev_ecr;

	if( ofo_entry_get_status( entry ) == ENT_STATUS_ROUGH ){

		account = ofo_account_get_by_number( dossier, ofo_entry_get_account( entry ));
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		debit = ofo_entry_get_debit( entry );
		credit = ofo_entry_get_credit( entry );
		number = ofo_entry_get_number( entry );
		deffect = ofo_entry_get_deffect( entry );

		/* impute the new entry either to the debit or the credit of
		 * daily balance
		 * + update last entry number (this should be always needed)
		 * + update last entry date (maybe)
		 */
		if( debit ){
			prev_ecr = ofo_account_get_day_deb_entry( account );
			if( prev_ecr >= number ){
				g_warning(
						"%s: new entry (%u) has a number less or equal to last entry (%u) imputed on the account",
						thisfn, number, prev_ecr );
			} else {
				account_set_day_deb_entry( account, number );
			}
			prev_deffect = ofo_account_get_day_deb_date( account );
			if( !my_date_is_valid( prev_deffect ) ||
					my_date_compare( prev_deffect, deffect ) < 0 ){
				account_set_day_deb_date( account, deffect );
			}
			prev = ofo_account_get_day_deb_amount( account );
			account_set_day_deb_amount( account, prev+debit );

		} else {
			prev_ecr = ofo_account_get_day_cre_entry( account );
			if( prev_ecr >= number ){
				g_warning(
						"%s: new entry (%u) has a number less or equal to last entry (%u) imputed on the account",
						thisfn, number, prev_ecr );
			} else {
				account_set_day_cre_entry( account, number );
			}
			prev_deffect = ofo_account_get_day_cre_date( account );
			if( !my_date_is_valid( prev_deffect ) ||
					my_date_compare( prev_deffect, deffect ) < 0 ){
				account_set_day_cre_date( account, deffect );
			}
			prev = ofo_account_get_day_cre_amount( account );
			account_set_day_cre_amount( account, prev+credit );
		}

		if( account_update_amounts( account, ofo_dossier_get_sgbd( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( account ), NULL );
		}

	} else {
		g_warning( "%s: new entry not in rough status", thisfn );
	}
}

static void
on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_account_on_updated_object";
	const gchar *code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_CURRENCY( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( dossier, prev_id, code );
			}
		}
	}
}

static void
on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_ACCOUNTS SET ACC_CURRENCY='%s' WHERE ACC_CURRENCY='%s'",
						code, prev_id );

	ofo_sgbd_query( ofo_dossier_get_sgbd( dossier ), query, TRUE );

	g_free( query );

	g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
	st_global->dataset = NULL;

	g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_ACCOUNT );
}

/*
 * an entry is validated, either individually or as the result of the
 * closing of a journal
 */
static void
on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data )
{
	static const gchar *thisfn = "ofo_account_on_validated_entry";
	const gchar *acc_number;
	ofoAccount *account;
	gdouble debit, credit, amount;
	const GDate *ent_deffect, *acc_date;
	gint number, acc_num;

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	acc_number = ofo_entry_get_account( entry );
	account = ofo_account_get_by_number( dossier, acc_number );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	if( debit ){
		amount = ofo_account_get_day_deb_amount( account );
		account_set_day_deb_amount( account, amount-debit );
		amount = ofo_account_get_deb_amount( account );
		account_set_deb_amount( account, amount+debit );
	}

	credit = ofo_entry_get_credit( entry );
	if( credit ){
		amount = ofo_account_get_day_cre_amount( account );
		account_set_day_cre_amount( account, amount-credit );
		amount = ofo_account_get_cre_amount( account );
		account_set_cre_amount( account, amount+credit );
	}

	ent_deffect = ofo_entry_get_deffect( entry );
	if( debit ){
		acc_date = ofo_account_get_deb_date( account );
		if( !my_date_is_valid( acc_date ) ||
				my_date_compare( acc_date, ent_deffect ) < 0 ){
			account_set_deb_date( account, ent_deffect );
		}
	} else {
		acc_date = ofo_account_get_cre_date( account );
		if( !my_date_is_valid( acc_date ) ||
				my_date_compare( acc_date, ent_deffect ) < 0 ){
			account_set_cre_date( account, ent_deffect );
		}
	}

	number = ofo_entry_get_number( entry );
	if( debit ){
		acc_num = ofo_account_get_deb_entry( account );
		if( number > acc_num ){
			account_set_deb_entry( account, number );
		}
	} else {
		acc_num = ofo_account_get_cre_entry( account );
		if( number > acc_num ){
			account_set_cre_entry( account, number );
		}
	}

	if( account_update_amounts( account, ofo_dossier_get_sgbd( dossier ))){
		g_signal_emit_by_name(
				G_OBJECT( dossier ),
				OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( account ), NULL );
	}
}

/**
 * ofo_account_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoAccount accounts, ordered by ascending
 * number. The returned list is owned by the #ofoAccount class, and
 * should not be freed by the caller.
 */
GList *
ofo_account_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	return( st_global->dataset );
}

/*
 * Loads/reloads the ordered list of accounts
 */
static GList *
account_load_dataset( void )
{
	return( ofo_base_load_dataset(
			st_boxed_defs,
			ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
			"OFA_T_ACCOUNTS ORDER BY ACC_NUMBER ASC",
			OFO_TYPE_ACCOUNT ));
}

/**
 * ofo_account_get_by_number:
 * @dossier: the currently opened #ofoDossier dossier.
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
ofo_account_get_by_number( const ofoDossier *dossier, const gchar *number )
{
	/*static const gchar *thisfn = "ofo_account_get_by_number";*/

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	/*g_debug( "%s: dossier=%p, number=%s", thisfn, ( void * ) dossier, number );*/

	if( !number || !g_utf8_strlen( number, -1 )){
		return( NULL );
	}

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	return( account_find_by_number( st_global->dataset, number ));
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
 * ofo_account_use_currency:
 * @dossier: the currently opened #ofoDossier dossier.
 * @currency: the currency ISO 3A code
 *
 * Returns: %TRUE if at least one recorded account makes use of the
 * specified currency.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
gboolean
ofo_account_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( currency && g_utf8_strlen( currency, -1 ), FALSE );

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	return( account_count_for_currency( ofo_dossier_get_sgbd( dossier ), currency ) > 0 );
}

static gint
account_count_for_currency( const ofoSgbd *sgbd, const gchar *currency )
{
	return( account_count_for( sgbd, "ACC_CURRENCY", currency ));
}

static gint
account_count_for( const ofoSgbd *sgbd, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	count = 0;
	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ACCOUNTS "
				"	WHERE %s='%s'",
					field, mnemo );

	result = ofo_sgbd_query_ex( sgbd, query, TRUE );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );
	}

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
	ofo_base_init_fields_list( st_boxed_defs, OFO_BASE( account ));

	return( account );
}

/**
 * ofo_account_dump_chart:
 */
void
ofo_account_dump_chart( GList *chart )
{
	static const gchar *thisfn = "ofo_account_dump_chart";
	ofoAccount *account;
	GList *ic;

	for( ic=chart ; ic ; ic=ic->next ){
		account = OFO_ACCOUNT( ic->data );
		g_debug( "%s: account %s - %s",
				thisfn,
				ofo_account_get_number( account ),
				ofo_account_get_label( account ));
	}
}

/**
 * ofo_account_archive_open_balances:
 * @dossier: the currently opened dossier.
 *
 * As part of the close of the exercice N, we archive the balances of
 * the accounts at the beginning of the exercice N+1. These balances
 * take into account the carried forward entries (fr: reports à
 * nouveau).
 *
 * At this time, all ledgers on exercice N should have been closed, and
 * no entries should have been recorded in ledgers for exercice N+1.
 */
void
ofo_account_archive_open_balances( const ofoDossier *dossier )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	g_list_foreach( st_global->dataset, ( GFunc ) archive_open_balances, NULL );
}

static void
archive_open_balances( ofoAccount *account, void *empty )
{
	static const gchar *thisfn = "ofo_account_archive_open_balances";
	gdouble amount;

	if( !ofo_account_is_root( account )){
		account_set_open_deb_entry( account, ofo_account_get_deb_entry( account ));
		account_set_open_deb_date( account, ofo_account_get_deb_date( account ));
		account_set_open_deb_amount( account, ofo_account_get_deb_amount( account ));
		account_set_open_cre_entry( account, ofo_account_get_cre_entry( account ));
		account_set_open_cre_date( account, ofo_account_get_cre_date( account ));
		account_set_open_cre_amount( account, ofo_account_get_cre_amount( account ));
		amount = ofo_account_get_day_deb_amount( account ) + ofo_account_get_day_cre_amount( account );
		if( amount != 0 ){
			g_warning( "%s: account=%s: daily balance is not zero",
					thisfn, ofo_account_get_number( account ));
		}
	}
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
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( ofo_account_get_class_from_number(
						ofo_account_get_number( account )));
	}

	return( 0 );
}

/**
 * ofo_account_get_class_from_number:
 * @account_number: an account number.
 *
 * Returns: the class number of this @account_number.
 *
 * TODO: make this UTF8-compliant....
 */
gint
ofo_account_get_class_from_number( const gchar *account_number )
{
	gchar *number;
	gint class;

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
 * TODO: make this UTF8-compliant....
 */
gint
ofo_account_get_level_from_number( const gchar *account_number )
{
	if( !account_number ){
		return( 0 );
	}
	return( g_utf8_strlen( account_number, -1 ));
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
 * ofo_account_get_type_account:
 * @account: the #ofoAccount account
 *
 * Returns: the type of the @account
 *
 * The type is identified by a letter which may be:
 * 'R': this is a root account, with or without children
 * 'D' (default): this is a detail account, on which entries may be
 *                written.
 */
const gchar *
ofo_account_get_type_account( const ofoAccount *account )
{
	account_get_string( ACC_TYPE );
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
 * ofo_account_get_deb_entry:
 * @account: the #ofoAccount account
 *
 * Returns: the greatest entry number responsible of this debit balance.
 */
gint
ofo_account_get_deb_entry( const ofoAccount *account )
{
	account_get_counter( ACC_DEB_ENTRY );
}

/**
 * ofo_account_get_deb_date:
 * @account: the #ofoAccount account
 *
 * Returns: the most recent effect date for this debit balance.
 */
const GDate *
ofo_account_get_deb_date( const ofoAccount *account )
{
	account_get_date( ACC_DEB_DATE );
}

/**
 * ofo_account_get_deb_amount:
 * @account: the #ofoAccount account
 *
 * Returns: the validated debit balance of the @account.
 */
gdouble
ofo_account_get_deb_amount( const ofoAccount *account )
{
	account_get_amount( ACC_DEB_AMOUNT );
}

/**
 * ofo_account_get_cre_entry:
 * @account: the #ofoAccount account
 *
 * Returns: the greatest entry number responsible of this credit balance.
 */
gint
ofo_account_get_cre_entry( const ofoAccount *account )
{
	account_get_counter( ACC_CRE_ENTRY );
}

/**
 * ofo_account_get_cre_date:
 * @account: the #ofoAccount account
 *
 * Returns: the most recent effect date for this credit balance.
 */
const GDate *
ofo_account_get_cre_date( const ofoAccount *account )
{
	account_get_date( ACC_CRE_DATE );
}

/**
 * ofo_account_get_cre_amount:
 * @account: the #ofoAccount account
 *
 * Returns: the validated credit balance of the @account.
 */
gdouble
ofo_account_get_cre_amount( const ofoAccount *account )
{
	account_get_amount( ACC_CRE_AMOUNT );
}

/**
 * ofo_account_get_day_deb_entry:
 * @account: the #ofoAccount account
 */
gint
ofo_account_get_day_deb_entry( const ofoAccount *account )
{
	account_get_counter( ACC_DAY_DEB_ENTRY );
}

/**
 * ofo_account_get_day_deb_date:
 * @account: the #ofoAccount account
 */
const GDate *
ofo_account_get_day_deb_date( const ofoAccount *account )
{
	account_get_date( ACC_DAY_DEB_DATE );
}

/**
 * ofo_account_get_day_deb_amount:
 * @account: the #ofoAccount account
 */
gdouble
ofo_account_get_day_deb_amount( const ofoAccount *account )
{
	account_get_amount( ACC_DAY_DEB_AMOUNT );
}

/**
 * ofo_account_get_day_cre_entry:
 * @account: the #ofoAccount account
 */
gint
ofo_account_get_day_cre_entry( const ofoAccount *account )
{
	account_get_counter( ACC_DAY_CRE_ENTRY);
}

/**
 * ofo_account_get_day_cre_date:
 * @account: the #ofoAccount account
 */
const GDate *
ofo_account_get_day_cre_date( const ofoAccount *account )
{
	account_get_date( ACC_DAY_CRE_DATE );
}

/**
 * ofo_account_get_day_cre_amount:
 * @account: the #ofoAccount account
 */
gdouble
ofo_account_get_day_cre_amount( const ofoAccount *account )
{
	account_get_amount( ACC_DAY_CRE_AMOUNT );
}

/**
 * ofo_account_get_open_deb_entry:
 * @account: the #ofoAccount account
 */
gint
ofo_account_get_open_deb_entry( const ofoAccount *account )
{
	account_get_counter( ACC_OPEN_DEB_ENTRY );
}

/**
 * ofo_account_get_open_deb_date:
 * @account: the #ofoAccount account
 */
const GDate *
ofo_account_get_open_deb_date( const ofoAccount *account )
{
	account_get_date( ACC_OPEN_DEB_DATE );
}

/**
 * ofo_account_get_open_deb_amount:
 * @account: the #ofoAccount account
 */
gdouble
ofo_account_get_open_deb_amount( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_DEB_AMOUNT );
}

/**
 * ofo_account_get_open_cre_entry:
 * @account: the #ofoAccount account
 */
gint
ofo_account_get_open_cre_entry( const ofoAccount *account )
{
	account_get_counter( ACC_OPEN_CRE_ENTRY);
}

/**
 * ofo_account_get_open_cre_date:
 * @account: the #ofoAccount account
 */
const GDate *
ofo_account_get_open_cre_date( const ofoAccount *account )
{
	account_get_date( ACC_OPEN_CRE_DATE );
}

/**
 * ofo_account_get_open_cre_amount:
 * @account: the #ofoAccount account
 */
gdouble
ofo_account_get_open_cre_amount( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_CRE_AMOUNT );
}

/**
 * ofo_account_is_deletable:
 * @account: the #ofoAccount account
 *
 * A account is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its entry numbers
 * must be nuls.
 *
 * Whether a root account with children is deletable is a user preference.
 * To be deletable, all children must also be deletable.
 */
gboolean
ofo_account_is_deletable( const ofoAccount *account )
{
	gboolean deletable;
	GList *children, *it;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		deletable = ofo_account_get_deb_entry( account ) == 0 &&
					ofo_account_get_cre_entry( account ) == 0 &&
					ofo_account_get_day_deb_entry( account ) == 0 &&
					ofo_account_get_day_cre_entry( account ) == 0;

		if( ofo_account_is_root( account ) && ofa_prefs_account_delete_root_with_children()){

			children = ofo_account_get_children( account );
			for( it=children ; it ; it=it->next ){
				deletable &= ofo_account_is_deletable( OFO_ACCOUNT( it->data ));
			}
			g_list_free( children );
		}

		return( deletable );
	}

	return( FALSE );
}

/**
 * ofo_account_is_root:
 * @account: the #ofoAccount account
 *
 * Default is 'detail' type.
 */
gboolean
ofo_account_is_root( const ofoAccount *account )
{
	gboolean is_root;
	const gchar *account_type;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	is_root = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_type = ofo_account_get_type_account( account );
		if( account_type && !g_utf8_collate( account_type, "R" )){
			is_root = TRUE;
		}
	}

	return( is_root );
}

/**
 * ofo_account_is_valid_data:
 */
gboolean
ofo_account_is_valid_data( const gchar *number, const gchar *label, const gchar *currency, const gchar *type )
{
	gunichar code;
	gint value;
	gboolean is_root;

	/* is account number valid ?
	 * must begin with a digit, and be at least two chars
	 */
	if( !number || g_utf8_strlen( number, -1 ) < 2 ){
		return( FALSE );
	}
	code = g_utf8_get_char( number );
	value = g_unichar_digit_value( code );
	/*g_debug( "ofo_account_is_valid_data: number=%s, code=%c, value=%d", number, code, value );*/
	if( value < 1 ){
		return( FALSE );
	}

	/* label */
	if( !label || !g_utf8_strlen( label, -1 )){
		return( FALSE );
	}

	/* is root account ? */
	is_root = ( type && !g_utf8_collate( type, "R" ));

	/* currency must be set for detail account */
	if( !is_root ){
		if( !currency || !g_utf8_strlen( currency, -1 )){
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_account_get_global_deffect:
 * @account: the #ofoAccount account
 *
 * Returns the most recent effect date of the account, keeping into
 * account both validated and rough entries. At last, even opening
 * balances may be considered.
 *
 * The returned value may be %NULL or not valid if no entry has ever
 * been recorded on this @account.
 */
const GDate *
ofo_account_get_global_deffect( const ofoAccount *account )
{
	const GDate *date, *dd;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	date = NULL;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		date = ofo_account_get_day_deb_date( account );
		dd = ofo_account_get_day_cre_date( account );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( dd )){
				if( my_date_compare( date, dd ) < 0 ){
					date = dd;
				}
			}
		} else if( my_date_is_valid( dd )){
			date = dd;
		}
		dd = ofo_account_get_deb_date( account );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( dd )){
				if( my_date_compare( date, dd ) < 0 ){
					date = dd;
				}
			}
		} else if( my_date_is_valid( dd )){
			date = dd;
		}
		dd = ofo_account_get_cre_date( account );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( dd )){
				if( my_date_compare( date, dd ) < 0 ){
					date = dd;
				}
			}
		} else if( my_date_is_valid( dd )){
			date = dd;
		}
		dd = ofo_account_get_open_deb_date( account );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( dd )){
				if( my_date_compare( date, dd ) < 0 ){
					date = dd;
				}
			}
		} else if( my_date_is_valid( dd )){
			date = dd;
		}
		dd = ofo_account_get_open_cre_date( account );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( dd )){
				if( my_date_compare( date, dd ) < 0 ){
					date = dd;
				}
			}
		} else if( my_date_is_valid( dd )){
			date = dd;
		}
	}

	return( date );
}

/**
 * ofo_account_get_global_solde:
 * @account: the #ofoAccount account
 *
 * Returns: the  global balance of the @account, taking into account
 * both validated and rough balances.
 *
 * The returned value is lesser than zero for a debit, or greater than
 * zero for a credit.
 */
gdouble
ofo_account_get_global_solde( const ofoAccount *account )
{
	gdouble amount;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0.0 );

	amount = 0.0;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		amount -= ofo_account_get_day_deb_amount( account );
		amount += ofo_account_get_day_cre_amount( account );
		amount -= ofo_account_get_deb_amount( account );
		amount += ofo_account_get_cre_amount( account );
	}

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

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_get_children( account, &child_str );
		g_list_free( child_str.children_list );

		return( child_str.children_count > 0 );
	}

	return( FALSE );
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

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_get_children( account, &child_str );

		return( child_str.children_list );
	}

	return( NULL );
}

static void
account_get_children( const ofoAccount *account, sChildren *child_str )
{
	memset( child_str, '\0' ,sizeof( sChildren ));
	child_str->number = ofo_account_get_number( account );
	child_str->children_count = 0;
	child_str->children_list = NULL;

	g_list_foreach( st_global->dataset, ( GFunc ) account_iter_children, child_str );
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

	is_child = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_number = ofo_account_get_number( account );
		candidate_number = ofo_account_get_number( candidate );
		is_child = g_str_has_prefix( candidate_number, account_number );
	}

	return( is_child );
}

/**
 * ofo_account_histo_valid_to_open:
 * @account: the #ofoAccount account
 *
 * Archive the validated balances of the @account to the opening
 * exercice ones.
 *
 * This function should only be called at the opening of the new
 * exercice, just after having closed the previous one, i.e. at a time
 * where:
 * - there is no rough entries
 * - all carried forward entries have been recorded.
 */
gboolean
ofo_account_histo_valid_to_open( ofoAccount *account )
{
	GString *query;
	gboolean has_comma;
	gboolean ok;
	gchar *sdate, *samount;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	ok = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );
		has_comma = FALSE;

		if( ofo_account_get_deb_entry( account ) > 0 ){

			account_set_open_deb_entry( account, ofo_account_get_deb_entry( account ));
			account_set_open_deb_date( account, ofo_account_get_deb_date( account ));
			account_set_open_deb_amount( account, ofo_account_get_deb_amount( account ));

			sdate = my_date_to_str( ofo_account_get_open_deb_date( account ), MY_DATE_SQL );
			samount = my_double_to_sql( ofo_account_get_open_deb_amount( account ));

			g_string_append_printf( query,
					"ACC_OPEN_DEB_ENTRY=%d,ACC_OPEN_DEB_DATE='%s',ACC_OPEN_DEB_AMOUNT=%s",
					ofo_account_get_open_deb_entry( account ), sdate, samount );

			has_comma = TRUE;
			g_free( sdate );
			g_free( samount );
		}

		if( ofo_account_get_cre_entry( account ) > 0 ){

			account_set_open_cre_entry( account, ofo_account_get_cre_entry( account ));
			account_set_open_cre_date( account, ofo_account_get_cre_date( account ));
			account_set_open_cre_amount( account, ofo_account_get_cre_amount( account ));

			sdate = my_date_to_str( ofo_account_get_open_cre_date( account ), MY_DATE_SQL );
			samount = my_double_to_sql( ofo_account_get_open_cre_amount( account ));

			if( !has_comma ){
				query = g_string_append_c( query, ',' );
			}
			g_string_append_printf( query,
					"ACC_OPEN_CRE_ENTRY=%d,ACC_OPEN_CRE_DATE='%s',ACC_OPEN_CRE_AMOUNT=%s",
					ofo_account_get_open_cre_entry( account ), sdate, samount );

			g_free( sdate );
			g_free( samount );
		}

		g_string_append_printf( query,
				" WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));

		ok = ofo_sgbd_query(
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )), query->str, TRUE );

		g_string_free( query, TRUE );
	}

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
 * ofo_account_set_type:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_type( ofoAccount *account, const gchar *type )
{
	account_set_string( ACC_TYPE, type );
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

/*
 * ofo_account_set_deb_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_deb_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_deb_date:
 * @account: the #ofoAccount account
 */
static void
account_set_deb_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_deb_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_deb_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/*
 * ofo_account_set_cre_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_cre_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_cre_date:
 * @account: the #ofoAccount account
 */
static void
account_set_cre_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_cre_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_cre_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/*
 * ofo_account_set_day_deb_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_day_deb_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_day_deb_date:
 * @account: the #ofoAccount account
 */
static void
account_set_day_deb_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_day_deb_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_day_deb_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/*
 * ofo_account_set_day_cre_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_day_cre_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_day_cre_date:
 * @account: the #ofoAccount account
 */
static void
account_set_day_cre_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_day_cre_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_day_cre_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/*
 * ofo_account_set_open_deb_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_open_deb_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_open_deb_date:
 * @account: the #ofoAccount account
 */
static void
account_set_open_deb_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_open_deb_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_open_deb_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/*
 * ofo_account_set_open_cre_entry:
 * @account: the #ofoAccount account
 */
static void
account_set_open_cre_entry( ofoAccount *account, gint number )
{
	account_set_counter( ACC_DEB_ENTRY, number );
}

/*
 * ofo_account_set_open_cre_date:
 * @account: the #ofoAccount account
 */
static void
account_set_open_cre_date( ofoAccount *account, const GDate *date )
{
	account_set_date( ACC_DEB_DATE, date );
}

/*
 * ofo_account_set_open_cre_amount:
 * @account: the #ofoAccount account
 */
static void
account_set_open_cre_amount( ofoAccount *account, gdouble amount )
{
	account_set_amount( ACC_DEB_AMOUNT, amount );
}

/**
 * ofo_account_insert:
 * @account: the new #ofoAccount account to be inserted.
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * This function is only of use when the user modifies the public
 * properties of an account. It is no worth to deal here with amounts
 * and/or debit/credit agregates.
 *
 * Returns: %TRUE if the insertion has been successful.
 */
gboolean
ofo_account_insert( ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_insert";

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p", thisfn, ( void * ) account );

		if( account_do_insert(
					account,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, account );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_insert( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ACCOUNTS" );

	g_string_append_printf( query,
			"	(ACC_NUMBER,ACC_LABEL,ACC_CURRENCY,ACC_NOTES,ACC_TYPE,"
			"	ACC_UPD_USER, ACC_UPD_STAMP)"
			"	VALUES ('%s','%s',",
					ofo_account_get_number( account ),
					label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "'%s',", ofo_account_get_currency( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s','%s')",
			ofo_account_get_type_account( account ),
			user, stamp_str );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		account_set_upd_user( account, user );
		account_set_upd_stamp( account, &stamp );
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
 *
 * we deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_account_update( ofoAccount *account, const gchar *prev_number )
{
	static const gchar *thisfn = "ofo_account_update";

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( prev_number && g_utf8_strlen( prev_number, -1 ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, prev_number=%s", thisfn, ( void * ) account, prev_number );

		if( account_do_update(
					account,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )),
					prev_number )){

			OFO_BASE_UPDATE_DATASET( st_global, account, prev_number );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_update( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_number )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	const gchar *new_number;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	if( g_utf8_collate( new_number, prev_number )){
		g_string_append_printf( query, "ACC_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query, "ACC_LABEL='%s',", label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "ACC_CURRENCY=NULL," );
	} else {
		g_string_append_printf( query, "ACC_CURRENCY='%s',", ofo_account_get_currency( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "ACC_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "ACC_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	ACC_TYPE='%s',",
					ofo_account_get_type_account( account ));

	g_string_append_printf( query,
			"	ACC_UPD_USER='%s',ACC_UPD_STAMP='%s'"
			"	WHERE ACC_NUMBER='%s'",
					user,
					stamp_str,
					prev_number );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		account_set_upd_user( account, user );
		account_set_upd_stamp( account, &stamp );

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );
	g_free( notes );
	g_free( label );

	return( ok );
}

static gboolean
account_update_amounts( ofoAccount *account, const ofoSgbd *sgbd )
{
	GString *query;
	gboolean ok;
	gchar *sdate, *samount;
	gint ecr_number;

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	/* validated debit */
	ecr_number = ofo_account_get_deb_entry( account );
	if( ecr_number ){
		sdate = my_date_to_str( ofo_account_get_deb_date( account ), MY_DATE_SQL );
		samount = my_double_to_sql( ofo_account_get_deb_amount( account ));
		g_string_append_printf( query,
				"ACC_DEB_ENTRY=%d,ACC_DEB_DATE='%s',ACC_DEB_AMOUNT=%s,",
					ecr_number, sdate, samount );
		g_free( sdate );
		g_free( samount );
	} else {
		query = g_string_append( query,
				"ACC_DEB_ENTRY=NULL,ACC_DEB_DATE=NULL,ACC_DEB_AMOUNT=NULL," );
	}

	/* validated credit */
	ecr_number = ofo_account_get_cre_entry( account );
	if( ecr_number ){
		sdate = my_date_to_str( ofo_account_get_cre_date( account ), MY_DATE_SQL );
		samount = my_double_to_sql( ofo_account_get_cre_amount( account ));
		g_string_append_printf( query,
				"ACC_CRE_ENTRY=%d,ACC_CRE_DATE='%s',ACC_CRE_AMOUNT=%s,",
					ecr_number, sdate, samount );
		g_free( sdate );
		g_free( samount );
	} else {
		query = g_string_append( query,
				"ACC_CRE_ENTRY=NULL,ACC_CRE_DATE=NULL,ACC_CRE_AMOUNT=NULL," );
	}

	/* brouillard debit */
	ecr_number = ofo_account_get_day_deb_entry( account );
	if( ecr_number ){
		sdate = my_date_to_str( ofo_account_get_day_deb_date( account ), MY_DATE_SQL );
		samount = my_double_to_sql( ofo_account_get_day_deb_amount( account ));
		g_string_append_printf( query,
				"ACC_DAY_DEB_ENTRY=%d,ACC_DAY_DEB_DATE='%s',ACC_DAY_DEB_AMOUNT=%s,",
					ecr_number, sdate, samount );
		g_free( sdate );
		g_free( samount );
	} else {
		query = g_string_append( query,
				"ACC_DAY_DEB_ENTRY=NULL,ACC_DAY_DEB_DATE=NULL,ACC_DAY_DEB_AMOUNT=NULL," );
	}

	/* brouillard credit */
	ecr_number = ofo_account_get_day_cre_entry( account );
	if( ecr_number ){
		sdate = my_date_to_str( ofo_account_get_day_cre_date( account ), MY_DATE_SQL );
		samount = my_double_to_sql( ofo_account_get_day_cre_amount( account ));
		g_string_append_printf( query,
				"ACC_DAY_CRE_ENTRY=%d,ACC_DAY_CRE_DATE='%s',ACC_DAY_CRE_AMOUNT=%s ",
					ecr_number, sdate, samount );
		g_free( sdate );
		g_free( samount );
	} else {
		query = g_string_append( query,
				"ACC_DAY_CRE_ENTRY=NULL,ACC_DAY_CRE_DATE=NULL,ACC_DAY_CRE_AMOUNT=NULL " );
	}

	g_string_append_printf( query,
				"	WHERE ACC_NUMBER='%s'",
						ofo_account_get_number( account ));

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

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

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_account_is_deletable( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p", thisfn, ( void * ) account );

		if( account_do_delete(
					account,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, account );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_delete( ofoAccount *account, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_ACCOUNTS"
			"	WHERE ACC_NUMBER='%s'",
					ofo_account_get_number( account ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

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

/**
 * ofo_account_get_csv:
 *
 * Returns: an export of the accounts
 */
GSList *
ofo_account_get_csv( const ofoDossier *dossier )
{
	/* these should be parameters of the function, along with the
	 * character code */
	static const gchar field_sep = ';';
	gboolean with_header = TRUE;
	gchar decimal_sep = '.';

	GSList *lines;
	gchar *str;
	GList *it;

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	lines = NULL;

	if( with_header ){
		str = ofa_boxed_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( lines, str );
	}

	for( it=st_global->dataset ; it ; it=it->next ){
		str = ofa_boxed_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, decimal_sep );
		lines = g_slist_prepend( lines, str );
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_account_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - account number
 * - label
 * - currency iso 3a code (mandatory for detail accounts, default to
 *   dossier currency)
 * - type (default to detail)
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 * All the balances are set to NULL.
 */
void
ofo_account_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_account_import_csv";
	ofoAccount *account;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	gint class;
	const gchar *str, *dev_code, *def_dev_code;
	gchar *type;
	ofoCurrency *currency;
	gchar *splitted;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	new_set = NULL;
	count = 0;
	errors = 0;
	def_dev_code = ofo_dossier_get_default_currency( dossier );

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			account = ofo_account_new();
			ico=ili->data;

			/* account number */
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty account number", thisfn, count );
				errors += 1;
				continue;
			}
			class = ofo_account_get_class_from_number( str );
			if( class < 1 || class > 9 ){
				g_warning( "%s: (line %d) invalid account number: %s", thisfn, count, str );
				errors += 1;
				continue;
			}
			ofo_account_set_number( account, str );

			/* account label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_account_set_label( account, str );

			/* currency code */
			ico = ico->next;
			dev_code = ( const gchar * ) ico->data;

			/* account type */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				type = g_strdup( "D" );
			} else {
				type = g_strdup( str );
			}
			ofo_account_set_type( account, type );

			if( !strcmp( type, "D" )){
				if( !dev_code || !g_utf8_strlen( str, -1 )){
					dev_code = def_dev_code;
				}
				currency = ofo_currency_get_by_code( dossier, dev_code );
				if( !currency ){
					g_warning( "%s: (line %d) invalid currency: '%s'", thisfn, count, dev_code );
					errors += 1;
					continue;
				}
				ofo_account_set_currency( account, dev_code );
			}

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					splitted = my_utils_import_multi_lines( str );
					ofo_account_set_notes( account, splitted );
					g_free( splitted );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, account );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		account_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			account_do_insert(
					OFO_ACCOUNT( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_ACCOUNT );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
account_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_ACCOUNTS", TRUE ));
}
