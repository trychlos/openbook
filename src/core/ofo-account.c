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
struct _ofoAccountPrivate {

	/* sgbd data
	 */
	gchar     *number;
	gchar     *label;
	gchar     *currency;
	gchar     *notes;
	gchar     *type;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
	gint       deb_entry;
	GDate      deb_date;
	gdouble    deb_amount;
	gint       cre_entry;
	GDate      cre_date;
	gdouble    cre_amount;
	gint       day_deb_entry;
	GDate      day_deb_date;
	gdouble    day_deb_amount;
	gint       day_cre_entry;
	GDate      day_cre_date;
	gdouble    day_cre_amount;
};

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
static void        account_set_upd_user( ofoAccount *account, const gchar *upd_user );
static void        account_set_upd_stamp( ofoAccount *account, const GTimeVal *upd_stamp );
static gboolean    account_do_insert( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user );
static gboolean    account_do_update( ofoAccount *account, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_number );
static gboolean    account_update_amounts( ofoAccount *account, const ofoSgbd *sgbd );
static gboolean    account_do_delete( ofoAccount *account, const ofoSgbd *sgbd );
static gint        account_cmp_by_number( const ofoAccount *a, const gchar *number );
static gboolean    account_do_drop_content( const ofoSgbd *sgbd );

static void
account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";
	ofoAccountPrivate *priv;

	priv = OFO_ACCOUNT( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->number, priv->label );

	g_return_if_fail( instance && OFO_IS_ACCOUNT( instance ));

	/* free data members here */
	g_free( priv->number );
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->type );
	g_free( priv->notes );
	g_free( priv->upd_user );

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

	my_date_clear( &self->priv->deb_date );
	my_date_clear( &self->priv->cre_date );
	my_date_clear( &self->priv->day_deb_date );
	my_date_clear( &self->priv->day_cre_date );
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
	ofoAccountPrivate *priv;
	gdouble debit, credit;
	gint number;
	const GDate *deffect, *prev_deffect;
	gdouble prev;
	gint prev_ecr;

	if( ofo_entry_get_status( entry ) == ENT_STATUS_ROUGH ){

		account = ofo_account_get_by_number( dossier, ofo_entry_get_account( entry ));
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
		priv = account->priv;

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
				priv->day_deb_entry = number;
			}
			prev_deffect = ofo_account_get_day_deb_date( account );
			if( !my_date_is_valid( prev_deffect ) || my_date_compare( prev_deffect, deffect ) < 0 ){
				my_date_set_from_date( &priv->day_deb_date, deffect );
			}
			prev = ofo_account_get_day_deb_amount( account );
			priv->day_deb_amount = prev+debit;

		} else {
			prev_ecr = ofo_account_get_day_cre_entry( account );
			if( prev_ecr >= number ){
				g_warning(
						"%s: new entry (%u) has a number less or equal to last entry (%u) imputed on the account",
						thisfn, number, prev_ecr );
			} else {
				priv->day_cre_entry = number;
			}
			prev_deffect = ofo_account_get_day_cre_date( account );
			if( !my_date_is_valid( prev_deffect ) || my_date_compare( prev_deffect, deffect ) < 0 ){
				my_date_set_from_date( &priv->day_cre_date, deffect );
			}
			prev = ofo_account_get_day_cre_amount( account );
			priv->day_cre_amount = prev+credit;
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
	ofoAccountPrivate *priv;
	gdouble debit, credit, amount;
	const GDate *ent_deffect, *acc_date;
	gint number, acc_num;

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	acc_number = ofo_entry_get_account( entry );
	account = ofo_account_get_by_number( dossier, acc_number );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
	priv = account->priv;

	debit = ofo_entry_get_debit( entry );
	if( debit ){
		amount = ofo_account_get_day_deb_amount( account );
		priv->day_deb_amount = amount-debit;
		amount = ofo_account_get_deb_amount( account );
		priv->deb_amount = amount+debit;
	}

	credit = ofo_entry_get_credit( entry );
	if( credit ){
		amount = ofo_account_get_day_cre_amount( account );
		priv->day_cre_amount = amount-credit;
		amount = ofo_account_get_cre_amount( account );
		priv->cre_amount = amount+credit;
	}

	ent_deffect = ofo_entry_get_deffect( entry );
	if( debit ){
		acc_date = ofo_account_get_deb_date( account );
		if( !my_date_is_valid( acc_date ) || my_date_compare( acc_date, ent_deffect ) < 0 ){
			my_date_set_from_date( &priv->deb_date, ent_deffect );
		}
	} else {
		acc_date = ofo_account_get_cre_date( account );
		if( !my_date_is_valid( acc_date ) || my_date_compare( acc_date, ent_deffect ) < 0 ){
			my_date_set_from_date( &priv->cre_date, ent_deffect );
		}
	}

	number = ofo_entry_get_number( entry );
	if( debit ){
		acc_num = ofo_account_get_deb_entry( account );
		if( number > acc_num ){
			priv->deb_entry = number;
		}
	} else {
		acc_num = ofo_account_get_cre_entry( account );
		if( number > acc_num ){
			priv->cre_entry = number;
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

/**
 * ofo_account_load_chart:
 *
 * Loads/reloads the ordered list of accounts
 */
static GList *
account_load_dataset( void )
{
	const ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoAccount *account;
	ofoAccountPrivate *priv;
	GList *dataset;
	GTimeVal timeval;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT ACC_NUMBER,ACC_LABEL,ACC_CURRENCY,ACC_NOTES,ACC_TYPE,"
			"	ACC_UPD_USER,ACC_UPD_STAMP,"
			"	ACC_DEB_ENTRY,ACC_DEB_DATE,ACC_DEB_AMOUNT,"
			"	ACC_CRE_ENTRY,ACC_CRE_DATE,ACC_CRE_AMOUNT,"
			"	ACC_DAY_DEB_ENTRY,ACC_DAY_DEB_DATE,ACC_DAY_DEB_AMOUNT,"
			"	ACC_DAY_CRE_ENTRY,ACC_DAY_CRE_DATE,ACC_DAY_CRE_AMOUNT "
			"	FROM OFA_T_ACCOUNTS "
			"	ORDER BY ACC_NUMBER ASC", TRUE );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		account = ofo_account_new();
		priv = account->priv;

		ofo_account_set_number( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_label( account, ( gchar * ) icol->data );
		icol = icol->next;
		/*g_debug( "account_load_dataset: %s - %s",
				ofo_account_get_number( account ), ofo_account_get_label( account ));*/
		/* currency may be left unset for root accounts, though is
		 * mandatory for detail ones */
		if( icol->data ){
			ofo_account_set_currency( account, ( gchar * ) icol->data );
		}
		icol = icol->next;
		ofo_account_set_notes( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_type( account, ( gchar * ) icol->data );
		icol = icol->next;
		account_set_upd_user( account, ( gchar * ) icol->data );
		icol = icol->next;
		account_set_upd_stamp( account,
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			priv->deb_entry = atoi(( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &priv->deb_date, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->deb_amount = my_double_set_from_sql(( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->cre_entry = atoi(( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &priv->cre_date, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->cre_amount = my_double_set_from_sql(( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->day_deb_entry = atoi(( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &priv->day_deb_date, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->day_deb_amount = my_double_set_from_sql(( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->day_cre_entry = atoi(( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			my_date_set_from_sql( &priv->day_cre_date, ( const gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			priv->day_cre_amount = my_double_set_from_sql(( const gchar * ) icol->data );
		}

		dataset = g_list_prepend( dataset, account );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_account_get_by_number:
 *
 * Returns: the searched account, or %NULL.
 *
 * The returned object is owned by the #ofoAccount class, and should
 * not be unreffed by the caller.
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
 *
 * Returns: %TRUE if a recorded account makes use of the specified currency.
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
 */
ofoAccount *
ofo_account_new( void )
{
	ofoAccount *account;

	account = g_object_new( OFO_TYPE_ACCOUNT, NULL );

	return( account );
}

/**
 * ofo_account_dump_chart:
 */
void
ofo_account_dump_chart( GList *chart )
{
	static const gchar *thisfn = "ofo_account_dump_chart";
	ofoAccountPrivate *priv;
	GList *ic;

	for( ic=chart ; ic ; ic=ic->next ){
		priv = OFO_ACCOUNT( ic->data )->priv;
		g_debug( "%s: account %s - %s", thisfn, priv->number, priv->label );
	}
}

/**
 * ofo_account_get_class:
 */
gint
ofo_account_get_class( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( ofo_account_get_class_from_number( account->priv->number ));
	}

	return( 0 );
}

/**
 * ofo_account_get_class_from_number:
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
 */
const gchar *
ofo_account_get_number( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->number );
	}

	return( NULL );
}

/**
 * ofo_account_get_label:
 */
const gchar *
ofo_account_get_label( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->label );
	}

	return( NULL );
}

/**
 * ofo_account_get_currency:
 */
const gchar *
ofo_account_get_currency( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const gchar * ) account->priv->currency );
	}

	return( NULL );
}

/**
 * ofo_account_get_notes:
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->notes );
	}

	return( NULL );
}

/**
 * ofo_account_get_type_account:
 */
const gchar *
ofo_account_get_type_account( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->type );
	}

	return( NULL );
}

/**
 * ofo_account_get_upd_user:
 */
const gchar *
ofo_account_get_upd_user( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const gchar * ) account->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_account_get_upd_stamp:
 */
const GTimeVal *
ofo_account_get_upd_stamp( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &account->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_account_get_deb_entry:
 */
gint
ofo_account_get_deb_entry( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->deb_entry );
	}

	return( 0 );
}

/**
 * ofo_account_get_deb_date:
 */
const GDate *
ofo_account_get_deb_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->deb_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_deb_amount:
 */
gdouble
ofo_account_get_deb_amount( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->deb_amount );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_cre_entry:
 */
gint
ofo_account_get_cre_entry( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->cre_entry );
	}

	return( 0 );
}

/**
 * ofo_account_get_cre_date:
 */
const GDate *
ofo_account_get_cre_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->cre_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_cre_amount:
 */
gdouble
ofo_account_get_cre_amount( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->cre_amount );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_day_deb_entry:
 */
gint
ofo_account_get_day_deb_entry( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->day_deb_entry );
	}

	return( 0 );
}

/**
 * ofo_account_get_day_deb_date:
 */
const GDate *
ofo_account_get_day_deb_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->day_deb_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_day_deb_amount:
 */
gdouble
ofo_account_get_day_deb_amount( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->day_deb_amount );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_day_cre_entry:
 */
gint
ofo_account_get_day_cre_entry( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->day_cre_entry );
	}

	return( 0 );
}

/**
 * ofo_account_get_day_cre_date:
 */
const GDate *
ofo_account_get_day_cre_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->day_cre_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_day_cre_amount:
 */
gdouble
ofo_account_get_day_cre_amount( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->day_cre_amount );
	}

	return( 0.0 );
}

/**
 * ofo_account_is_deletable:
 *
 * A account is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls.
 *
 * Whether a root account with children is deletable is a user preference
 * (todo #411).
 */
gboolean
ofo_account_is_deletable( const ofoAccount *account )
{
	gboolean deletable;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		deletable = !ofo_account_get_deb_amount( account ) &&
					!ofo_account_get_cre_amount( account ) &&
					!ofo_account_get_day_deb_amount( account ) &&
					!ofo_account_get_day_cre_amount( account );

		if( ofo_account_is_root( account ) && ofo_account_has_children( account )){
			deletable &= ofa_prefs_account_delete_root_with_children();
		}

		return( deletable );
	}

	return( FALSE );
}

/**
 * ofo_account_is_root:
 */
gboolean
ofo_account_is_root( const ofoAccount *account )
{
	gboolean is_root;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	is_root = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		if( account->priv->type && !g_utf8_collate( account->priv->type, "R" )){
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
 *
 * Returns the most recent effect date of the account.
 * May be %NULL if no entry has never been recorded in this account.
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
	}

	return( date );
}

/**
 * ofo_account_get_global_solde:
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
 *
 * Whether an account has children is only relevant for a root account
 * (but this is not checked here).
 */
gboolean
ofo_account_has_children( const ofoAccount *account )
{
	gchar *query;
	GSList *result, *icol;
	gint count;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		query = g_strdup_printf(
					"SELECT COUNT(*) FROM OFA_T_ACCOUNTS "
					"	WHERE ACC_NUMBER LIKE '%s%%'", ofo_account_get_number( account ));

		count = 0;
		result = ofo_sgbd_query_ex(
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )), query, TRUE );
		if( result ){
			icol = ( GSList * ) result->data;
			count = atoi( icol->data );
		}
		ofo_sgbd_free_result( result );

		return( count > 1 );
	}

	return( FALSE );
}

/**
 * ofo_account_set_number:
 */
void
ofo_account_set_number( ofoAccount *account, const gchar *number )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->number );
		account->priv->number = g_strdup( number );
	}
}

/**
 * ofo_account_set_label:
 */
void
ofo_account_set_label( ofoAccount *account, const gchar *label )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->label );
		account->priv->label = g_strdup( label );
	}
}

/**
 * ofo_account_set_currency:
 */
void
ofo_account_set_currency( ofoAccount *account, const gchar *currency )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->currency );
		account->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_account_set_notes:
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->notes );
		account->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_account_set_type:
 */
void
ofo_account_set_type( ofoAccount *account, const gchar *type )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->type );
		account->priv->type = g_strdup( type );
	}
}

/*
 * ofo_account_set_upd_user:
 */
static void
account_set_upd_user( ofoAccount *account, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->upd_user );
		account->priv->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_account_set_upd_stamp:
 */
static void
account_set_upd_stamp( ofoAccount *account, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &account->priv->upd_stamp, upd_stamp );
	}
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

/**
 * ofo_account_get_csv:
 */
GSList *
ofo_account_get_csv( const ofoDossier *dossier )
{
	GList *set;
	GSList *lines;
	gchar *str, *stamp;
	ofoAccount *account;
	gchar *notes, *sdeb, *scre, *sbrodeb, *sbrocre;
	const gchar *currency, *atype, *muser;

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	lines = NULL;

	str = g_strdup_printf( "Number;Label;Currency;Type;Notes;MajUser;MajStamp;"
			"DebEntry;DebDate;DebAmount;CreEntry;CreDate;CreAmount;"
			"BroDebEntry;BroDebDate;BroDebAmount;BroCreEntry;BroCreDate;BroCreAmount" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		account = OFO_ACCOUNT( set->data );

		currency = ofo_account_get_currency( account );
		atype = ofo_account_get_type_account( account );
		notes = my_utils_export_multi_lines( ofo_account_get_notes( account ));
		muser = ofo_account_get_upd_user( account );
		stamp = my_utils_stamp_to_str( ofo_account_get_upd_stamp( account ), MY_STAMP_YYMDHMS );

		sdeb = my_date_to_str( ofo_account_get_deb_date( account ), MY_DATE_SQL );
		scre = my_date_to_str( ofo_account_get_cre_date( account ), MY_DATE_SQL );
		sbrodeb = my_date_to_str( ofo_account_get_day_deb_date( account ), MY_DATE_SQL );
		sbrocre = my_date_to_str( ofo_account_get_day_cre_date( account ), MY_DATE_SQL );

		str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;%s;%d;%s;%.2lf;%d;%s;%.2lf;%d;%s;%.2lf;%d;%s;%.2lf",
				ofo_account_get_number( account ),
				ofo_account_get_label( account ),
				currency ? currency : "",
				atype ? atype : "",
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "",
				ofo_account_get_deb_entry( account ),
				sdeb,
				ofo_account_get_deb_amount( account ),
				ofo_account_get_cre_entry( account ),
				scre,
				ofo_account_get_cre_amount( account ),
				ofo_account_get_day_deb_entry( account ),
				sbrodeb,
				ofo_account_get_day_deb_amount( account ),
				ofo_account_get_day_cre_entry( account ),
				sbrocre,
				ofo_account_get_day_cre_amount( account ));

		g_free( notes );
		g_free( stamp );
		g_free( sdeb );
		g_free( scre );
		g_free( sbrodeb );
		g_free( sbrocre );

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
