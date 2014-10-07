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

#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-devise.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoLedgerPrivate {

	/* sgbd data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
	GList     *exes;					/* exercices */
	GList     *amounts;					/* balances per currency */
};

typedef struct {
	gint       exe_id;
	gchar     *currency;
	gdouble    clo_deb;
	gdouble    clo_cre;
	gdouble    deb;
	myDate    *deb_date;
	gdouble    cre;
	myDate    *cre_date;
}
	sDetailCur;

typedef struct {
	gint       exe_id;
	myDate    *last_clo;
}
	sDetailExe;

G_DEFINE_TYPE( ofoLedger, ofo_ledger, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, ledger )

static void        on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data );
static void        on_new_ledger_entry( ofoDossier *dossier, ofoEntry *entry );
static void        on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( const ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void        on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data );
static GList      *ledger_load_dataset( void );
static ofoLedger  *ledger_find_by_mnemo( GList *set, const gchar *mnemo );
static gint        ledger_count_for_currency( const ofoSgbd *sgbd, const gchar *currency );
static gint        ledger_count_for( const ofoSgbd *sgbd, const gchar *field, const gchar *mnemo );
static sDetailCur *ledger_find_cur_by_code( const ofoLedger *ledger, gint exe_id, const gchar *currency );
static sDetailExe *ledger_find_exe_by_id( const ofoLedger *ledger, gint exe_id );
static void        ledger_set_upd_user( ofoLedger *ledger, const gchar *upd_user );
static void        ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *upd_stamp );
static sDetailCur *ledger_new_cur_with_code( ofoLedger *ledger, gint exe_id, const gchar *currency );
static gboolean    ledger_do_insert( ofoLedger *ledger, const ofoSgbd *sgbd, const gchar *user );
static gboolean    ledger_insert_main( ofoLedger *ledger, const ofoSgbd *sgbd, const gchar *user );
static gboolean    ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user );
static gboolean    ledger_do_update_detail_cur( const ofoLedger *ledger, sDetailCur *detail, const ofoSgbd *sgbd );
static gboolean    ledger_do_update_detail_exe( const ofoLedger *ledger, sDetailExe *detail, const ofoSgbd *sgbd );
static gboolean    ledger_do_delete( ofoLedger *ledger, const ofoSgbd *sgbd );
static gint        ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo );
static gboolean    ledger_do_drop_content( const ofoSgbd *sgbd );

static void
free_detail_cur( sDetailCur *detail )
{
	g_free( detail->currency );
	g_object_unref( detail->deb_date );
	g_object_unref( detail->cre_date );
	g_free( detail );
}

static void
free_detail_exe( sDetailExe *detail )
{
	g_object_unref( detail->last_clo );
	g_free( detail );
}

static void
ledger_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ledger_finalize";
	ofoLedgerPrivate *priv;

	priv = OFO_LEDGER( instance )->private;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	g_return_if_fail( instance && OFO_IS_LEDGER( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->upd_user );
	g_list_free_full( priv->exes, ( GDestroyNotify ) free_detail_exe );
	g_list_free_full( priv->amounts, ( GDestroyNotify ) free_detail_cur );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ledger_parent_class )->finalize( instance );
}

static void
ledger_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_LEDGER( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ledger_parent_class )->dispose( instance );
}

static void
ofo_ledger_init( ofoLedger *self )
{
	static const gchar *thisfn = "ofo_ledger_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoLedgerPrivate, 1 );
}

static void
ofo_ledger_class_init( ofoLedgerClass *klass )
{
	static const gchar *thisfn = "ofo_ledger_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoLedgerPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ledger_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_finalize;
}

/**
 * ofo_ledger_connect_handlers:
 *
 * This function is called once, when opening the dossier.
 */
void
ofo_ledger_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ledger_connect_handlers";

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
on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data )
{
	if( OFO_IS_ENTRY( object )){
		on_new_ledger_entry( dossier, OFO_ENTRY( object ));
	}
}

/*
 * we are recording a new entry (so necessarily on the current exercice)
 * thus update the balances
 */
static void
on_new_ledger_entry( ofoDossier *dossier, ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_ledger_on_new_ledger_entry";
	gint current;
	const gchar *mnemo, *currency;
	ofoLedger *ledger;
	sDetailCur *detail;
	const myDate *deffect;
	gdouble debit;

	current = ofo_dossier_get_current_exe_id( dossier );
	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );

	if( ledger ){
		currency = ofo_entry_get_currency( entry );
		detail = ledger_new_cur_with_code( ledger, current, currency );
		g_return_if_fail( detail );

		debit = ofo_entry_get_debit( entry );
		deffect = ofo_entry_get_deffect( entry );

		if( debit ){
			detail->deb += debit;
			if( !my_date_is_valid( detail->deb_date ) ||
					my_date_compare( detail->deb_date, deffect ) < 0 ){
				ofo_ledger_set_deb_date( ledger, detail->exe_id, detail->currency, deffect );
			}

		} else {
			detail->cre += ofo_entry_get_credit( entry );
			if( !my_date_is_valid( detail->cre_date ) ||
					my_date_compare( detail->cre_date, deffect ) < 0 ){
				ofo_ledger_set_cre_date( ledger, detail->exe_id, detail->currency, deffect );
			}
		}

		if( ledger_do_update_detail_cur( ledger, detail, ofo_dossier_get_sgbd( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( ledger ), NULL );
		}

	} else {
		g_warning( "%s: ledger not found: %s", thisfn, mnemo );
	}

}

static void
on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_ledger_on_updated_object";
	const gchar *code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_DEVISE( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_devise_get_code( OFO_DEVISE( object ));
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
	gint exe_id;

	exe_id = ofo_dossier_get_current_exe_id( dossier );

	query = g_strdup_printf(
					"UPDATE OFA_T_LEDGERS_CUR "
					"	SET LED_CUR_CODE='%s' WHERE LED_CUR_CODE='%s' AND LED_EXE_ID=%d",
						code, prev_id, exe_id );

	ofo_sgbd_query( ofo_dossier_get_sgbd( dossier ), query, TRUE );

	g_free( query );

	g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
	st_global->dataset = NULL;
	g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_LEDGER );
}

/*
 * an entry is validated, either individually or as the result of the
 * closing of a ledger
 */
static void
on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data )
{
	static const gchar *thisfn = "ofo_ledger_on_validated_entry";
	gint exe_id;
	const gchar *currency, *mnemo;
	ofoLedger *ledger;
	sDetailCur *detail;
	gdouble debit, credit;
	gchar *str;
	GDate deffect2;
	const myDate *deffect;

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
	if( ledger ){

		deffect = ofo_entry_get_deffect( entry );
		str = my_date_to_str( deffect, MY_DATE_SQL );
		my_date_set_from_sql( &deffect2, str );
		exe_id = ofo_dossier_get_exe_by_date( dossier, &deffect2 );
		g_free( str );
		currency = ofo_entry_get_currency( entry );
		detail = ledger_find_cur_by_code( ledger, exe_id, currency );
		/* the entry has necessarily be already recorded while in rough
		 * status */
		g_return_if_fail( detail );

		debit = ofo_entry_get_debit( entry );
		detail->clo_deb += debit;
		detail->deb -= debit;
		credit = ofo_entry_get_credit( entry );
		detail->clo_cre += credit;
		detail->cre -= credit;

		if( ledger_do_update_detail_cur( ledger, detail, ofo_dossier_get_sgbd( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( ledger ), mnemo );
		}

	} else {
		g_warning( "%s: ledger not found: %s", thisfn, mnemo );
	}
}

/**
 * ofo_ledger_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoLedger ledgers, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoLedger class, and
 * should not be freed by the caller.
 *
 * Notes:
 * - the list is not sorted
 * - we are loading all the content of the entity, i.e. the list of
 *   ledgers plus all the list of detail rows
 */
GList *
ofo_ledger_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ledger_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, ledger );

	return( st_global->dataset );
}

static GList *
ledger_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoLedger *ledger;
	const ofoSgbd *sgbd;
	GList *dataset, *iset;
	gchar *query;
	sDetailCur *balance;
	sDetailExe *exercice;
	GTimeVal timeval;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT LED_MNEMO,LED_LABEL,LED_NOTES,"
			"	LED_UPD_USER,LED_UPD_STAMP "
			"	FROM OFA_T_LEDGERS", TRUE );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		ledger = ofo_ledger_new();
		ofo_ledger_set_mnemo( ledger, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_ledger_set_label( ledger, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_ledger_set_notes( ledger, ( gchar * ) icol->data );
		icol = icol->next;
		ledger_set_upd_user( ledger, ( gchar * ) icol->data );
		icol = icol->next;
		ledger_set_upd_stamp( ledger,
				my_utils_stamp_from_sql( &timeval, ( const gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, ledger );
	}

	ofo_sgbd_free_result( result );

	/* then load the details
	 */
	for( iset=dataset ; iset ; iset=iset->next ){

		ledger = OFO_LEDGER( iset->data );

		query = g_strdup_printf(
				"SELECT LED_EXE_ID,LED_CUR_CODE,"
				"	LED_CUR_CLO_DEB,LED_CUR_CLO_CRE,"
				"	LED_CUR_DEB,LED_CUR_DEB_DATE,LED_CUR_CRE,LED_CUR_CRE_DATE "
				"	FROM OFA_T_LEDGERS_CUR "
				"	WHERE LED_MNEMO='%s'",
						ofo_ledger_get_mnemo( ledger ));

		result = ofo_sgbd_query_ex( sgbd, query, TRUE );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			balance = g_new0( sDetailCur, 1 );
			balance->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			balance->currency = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			balance->clo_deb = my_double_from_sql(( const gchar * ) icol->data );
			/*g_debug( "clo_deb=%lf", balance->clo_deb );*/
			icol = icol->next;
			balance->clo_cre = my_double_from_sql(( const gchar * ) icol->data );
			/*g_debug( "clo_cre=%lf", balance->clo_cre );*/
			icol = icol->next;
			balance->deb = my_double_from_sql(( const gchar * ) icol->data );
			/*g_debug( "deb=%lf", balance->deb );*/
			icol = icol->next;
			balance->deb_date = my_date_new_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			balance->cre = my_double_from_sql(( const gchar * ) icol->data );
			/*g_debug( "cre=%lf", balance->cre );*/
			icol = icol->next;
			balance->cre_date = my_date_new_from_sql(( const gchar * ) icol->data );

			g_debug( "ledger_load_dataset: adding ledger=%s, exe_id=%d, currency=%s",
					ofo_ledger_get_mnemo( ledger ), balance->exe_id, balance->currency );

			ledger->private->amounts = g_list_prepend( ledger->private->amounts, balance );
		}

		ofo_sgbd_free_result( result );

		query = g_strdup_printf(
				"SELECT LED_EXE_ID,LED_EXE_LAST_CLO "
				"	FROM OFA_T_LEDGERS_EXE "
				"	WHERE LED_MNEMO='%s'",
						ofo_ledger_get_mnemo( ledger ));

		result = ofo_sgbd_query_ex( sgbd, query, TRUE );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			exercice = g_new0( sDetailExe, 1 );
			exercice->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			exercice->last_clo = my_date_new_from_sql(( const gchar * ) icol->data );

			ledger->private->exes = g_list_prepend( ledger->private->exes, exercice );
		}

		ofo_sgbd_free_result( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_ledger_get_by_mnemo:
 *
 * Returns: the searched ledger, or %NULL.
 *
 * The returned object is owned by the #ofoLedger class, and should
 * not be unreffed by the caller.
 */
ofoLedger *
ofo_ledger_get_by_mnemo( const ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, ledger );

	return( ledger_find_by_mnemo( st_global->dataset, mnemo ));
}

static ofoLedger *
ledger_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) ledger_cmp_by_mnemo );
	if( found ){
		return( OFO_LEDGER( found->data ));
	}

	return( NULL );
}

/**
 * ofo_ledger_use_currency:
 *
 * Returns: %TRUE if a recorded ledger makes use of the specified
 * currency.
 */
gboolean
ofo_ledger_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	OFO_BASE_SET_GLOBAL( st_global, dossier, ledger );

	return( ledger_count_for_currency( ofo_dossier_get_sgbd( dossier ), currency ) > 0 );
}

static gint
ledger_count_for_currency( const ofoSgbd *sgbd, const gchar *currency )
{
	return( ledger_count_for( sgbd, "LED_CUR_CODE", currency ));
}

static gint
ledger_count_for( const ofoSgbd *sgbd, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_LEDGERS_CUR WHERE %s='%s'",
				field, mnemo );

	count = 0;
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
 * ofo_ledger_new:
 */
ofoLedger *
ofo_ledger_new( void )
{
	ofoLedger *ledger;

	ledger = g_object_new( OFO_TYPE_LEDGER, NULL );

	return( ledger );
}

/**
 * ofo_ledger_get_mnemo:
 */
const gchar *
ofo_ledger_get_mnemo( const ofoLedger *ledger )
{
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const gchar * ) ledger->private->mnemo );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_label:
 */
const gchar *
ofo_ledger_get_label( const ofoLedger *ledger )
{
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const gchar * ) ledger->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_notes:
 */
const gchar *
ofo_ledger_get_notes( const ofoLedger *ledger )
{
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const gchar * ) ledger->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_upd_user:
 */
const gchar *
ofo_ledger_get_upd_user( const ofoLedger *ledger )
{
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const gchar * ) ledger->private->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_upd_stamp:
 */
const GTimeVal *
ofo_ledger_get_upd_stamp( const ofoLedger *ledger )
{
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &ledger->private->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_last_entry:
 *
 * Returns the effect date of the most recent entry written in this
 * ledger as a newly allocated #myDate object which should be
 * g_object_unref() by the caller.
 *
 * If no entry has been found for this ledger, a cleared (thus invalid)
 * date is returned (but a not-null #myDate object).
 */
myDate *
ofo_ledger_get_last_entry( const ofoLedger *ledger )
{
	myDate *deffet;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	deffet = my_date_new();

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		query = g_strdup_printf(
				"SELECT MAX(ECR_DEFFET) FROM OFA_T_ECRITURES "
				"	WHERE ECR_LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ));

		result = ofo_sgbd_query_ex(
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )), query, TRUE );
		g_free( query );

		if( result ){
			icol = ( GSList * ) result->data;
			my_date_set_from_str( deffet, ( const gchar * ) icol->data, MY_DATE_SQL );
			ofo_sgbd_free_result( result );
		}
	}

	return( deffet );
}

/**
 * ofo_ledger_get_last_closing:
 *
 * Returns the most recent closing date, all exercices considered, for
 * this ledger as a newly allocated #myDate object which should be
 * g_object_unref() by the caller.
 *
 * If the ledger has never been closed, a cleared (thus invalid) date
 * is returned (but a not-null #myDate object).
 */
myDate *
ofo_ledger_get_last_closing( const ofoLedger *ledger )
{
	myDate *dlast;
	sDetailExe *sdetail;
	GList *idet;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	dlast = my_date_new();

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		for( idet=ledger->private->exes ; idet ; idet=idet->next ){
			sdetail = ( sDetailExe * ) idet->data;
			if( !my_date_is_valid( dlast )){
				if( my_date_is_valid( sdetail->last_clo )){
					my_date_set_from_date( dlast, sdetail->last_clo );
				}
			} else if( my_date_is_valid( sdetail->last_clo ) &&
						my_date_compare( sdetail->last_clo, dlast ) > 0 ){

				my_date_set_from_date( dlast, sdetail->last_clo );
			}
		}
	}

	return( dlast );
}

/**
 * ofo_ledger_get_clo_deb:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the debit balance of this ledger at the last closing for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_ledger_get_clo_deb( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return( sdev->clo_deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_clo_cre:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the credit balance of this ledger at the last closing for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_ledger_get_clo_cre( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return( sdev->clo_cre );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_deb:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the current debit balance of this ledger for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_ledger_get_deb( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return( sdev->deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_deb_date:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the most recent entry effect date written at the debit of
 * this ledger
 */
const myDate *
ofo_ledger_get_deb_date( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return(( const myDate * ) sdev->deb_date );
		}
	}

	return( NULL );
}

/**
 * ofo_ledger_get_cre:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_ledger_get_cre( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return( sdev->cre );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_cre_date:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Returns the most recent entry effect date written at the credit of
 * this ledger, or NULL.
 */
const myDate *
ofo_ledger_get_cre_date( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, exe_id, currency );
		if( sdev ){
			return(( const myDate * ) sdev->cre_date );
		}
	}

	return( NULL );
}

/**
 * ofo_ledger_get_exe_list:
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
ofo_ledger_get_exe_list( const ofoLedger *ledger )
{
	GList *list;
	GList *iam;
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	list = NULL;

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		for( iam=ledger->private->amounts ; iam ; iam=iam->next ){
			sdev = ( sDetailCur * ) iam->data;
			list = g_list_prepend( list, GINT_TO_POINTER( sdev->exe_id ));
		}
	}

	return( list );
}

/**
 * ofo_ledger_get_last_closing:
 */
const myDate *
ofo_ledger_get_exe_closing( const ofoLedger *ledger, gint exe_id )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sexe = ledger_find_exe_by_id( ledger, exe_id );
		if( sexe ){
			return(( const myDate * ) sexe->last_clo );
		}
	}

	return( NULL );
}

static sDetailCur *
ledger_find_cur_by_code( const ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	static const gchar *thisfn = "ofo_ledger_ledger_find_cur_by_code";
	GList *idet;
	sDetailCur *sdet;

	for( idet=ledger->private->amounts ; idet ; idet=idet->next ){
		sdet = ( sDetailCur * ) idet->data;
		if( sdet->exe_id == exe_id && !g_utf8_collate( sdet->currency, currency )){
			return( sdet );
		}
	}

	g_debug( "%s: ledger=%s, exe_id=%d, currency=%s not found",
			thisfn, ofo_ledger_get_mnemo( ledger ), exe_id, currency );

	return( NULL );
}

static sDetailExe *
ledger_find_exe_by_id( const ofoLedger *ledger, gint exe_id )
{
	GList *idet;
	sDetailExe *sdet;

	for( idet=ledger->private->exes ; idet ; idet=idet->next ){
		sdet = ( sDetailExe * ) idet->data;
		if( sdet->exe_id == exe_id ){
			return( sdet );
		}
	}

	return( NULL );
}

/**
 * ofo_ledger_has_entries:
 */
gboolean
ofo_ledger_has_entries( const ofoLedger *ledger )
{
	gboolean ok;
	const gchar *mnemo;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		mnemo = ofo_ledger_get_mnemo( ledger );
		ok = ofo_entry_use_ledger( OFO_DOSSIER( st_global->dossier ), mnemo );
		return( ok );
	}

	return( FALSE );
}

/**
 * ofo_ledger_is_deletable:
 *
 * A ledger is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls for all currencies.
 *
 * There is no need to test for the last closing date as this is not
 * relevant here: even if set, they does not mean that there has been
 * any entries recorded on the ledger.
 *
 * More: a ledger should not be deleted while it is referenced by a
 * model or an entry.
 */
gboolean
ofo_ledger_is_deletable( const ofoLedger *ledger, const ofoDossier *dossier )
{
	gboolean ok;
	GList *ic;
	gint exe_id;
	sDetailCur *detail;
	const gchar *mnemo;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		ok = TRUE;
		exe_id = ofo_dossier_get_current_exe_id( dossier );

		for( ic=ledger->private->amounts ; ic && ok ; ic=ic->next ){
			detail = ( sDetailCur * ) ic->data;
			if( detail->exe_id == exe_id ){
				ok &= detail->clo_deb == 0.0 && detail->clo_cre == 0.0 &&
						detail->deb == 0.0 && detail->cre == 0.0;
			}
		}

		mnemo = ofo_ledger_get_mnemo( ledger );

		ok &= !ofo_entry_use_ledger( dossier, mnemo ) &&
				!ofo_ope_template_use_ledger( dossier, mnemo );

		return( ok );
	}

	return( FALSE );
}

/**
 * ofo_ledger_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoLedger a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_ledger_is_valid( const gchar *mnemo, const gchar *label )
{
	return( mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 ));
}

/**
 * ofo_ledger_set_mnemo:
 */
void
ofo_ledger_set_mnemo( ofoLedger *ledger, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_free( ledger->private->mnemo );
		ledger->private->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_ledger_set_label:
 */
void
ofo_ledger_set_label( ofoLedger *ledger, const gchar *label )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_free( ledger->private->label );
		ledger->private->label = g_strdup( label );
	}
}

/**
 * ofo_ledger_set_notes:
 */
void
ofo_ledger_set_notes( ofoLedger *ledger, const gchar *notes )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_free( ledger->private->notes );
		ledger->private->notes = g_strdup( notes );
	}
}

/*
 * ledger_set_upd_user:
 */
static void
ledger_set_upd_user( ofoLedger *ledger, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_free( ledger->private->upd_user );
		ledger->private->upd_user = g_strdup( upd_user );
	}
}

/*
 * ledger_set_upd_stamp:
 */
static void
ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		memcpy( &ledger->private->upd_stamp, upd_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_ledger_set_clo_deb:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the debit balance of this ledger at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_clo_deb( ofoLedger *ledger, gint exe_id, const gchar *currency, gdouble amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		sdev->clo_deb += amount;
	}
}

/**
 * ofo_ledger_set_clo_cre:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the credit balance of this ledger at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_clo_cre( ofoLedger *ledger, gint exe_id, const gchar *currency, gdouble amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		sdev->clo_cre += amount;
	}
}

/**
 * ofo_ledger_set_deb:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the current debit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_deb( ofoLedger *ledger, gint exe_id, const gchar *currency, gdouble amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		sdev->deb += amount;
	}
}

/**
 * ofo_ledger_set_deb_date:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the most recent entry effect date at the debit of the ledger
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_deb_date( ofoLedger *ledger, gint exe_id, const gchar *currency, const myDate *date )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		my_date_set_from_date( sdev->deb_date, date );
	}
}

/**
 * ofo_ledger_set_cre:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the current credit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_cre( ofoLedger *ledger, gint exe_id, const gchar *currency, gdouble amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		sdev->cre += amount;
	}
}

/**
 * ofo_ledger_set_cre_date:
 * @ledger:
 * @exe_id:
 * @currency:
 *
 * Set the most recent entry effect date at the debit of the ledger
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_cre_date( ofoLedger *ledger, gint exe_id, const gchar *currency, const myDate *date )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, exe_id, currency );
		g_return_if_fail( sdev );

		my_date_set_from_date( sdev->cre_date, date );
	}
}

static sDetailCur *
ledger_new_cur_with_code( ofoLedger *ledger, gint exe_id, const gchar *currency )
{
	sDetailCur *sdet;

	sdet = ledger_find_cur_by_code( ledger, exe_id, currency );

	if( !sdet ){
		sdet = g_new0( sDetailCur, 1 );

		sdet->clo_deb = 0.0;
		sdet->clo_cre = 0.0;
		sdet->deb = 0.0;
		sdet->deb_date = my_date_new();
		sdet->cre = 0.0;
		sdet->cre_date = my_date_new();
		sdet->exe_id = exe_id;
		sdet->currency = g_strdup( currency );

		ledger->private->amounts = g_list_prepend( ledger->private->amounts, sdet );
	}

	return( sdet );
}

/**
 * ofo_ledger_close:
 *
 * - all entries in rough status and whose effect date in less or equal
 *   to the closing date, and which are written in this ledger, are
 *   validated
 */
gboolean
ofo_ledger_close( ofoLedger *ledger, const myDate *closing )
{
	static const gchar *thisfn = "ofo_ledger_close";
	gboolean ok;
	gint exe_id;
	sDetailExe *sexe;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( closing && my_date_is_valid( closing ), FALSE );

	g_debug( "%s: ledger=%p, closing=%p", thisfn, ( void * ) ledger, ( void * ) closing );

	ok = FALSE;

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		if( ofo_entry_validate_by_ledger(
						OFO_DOSSIER( st_global->dossier ),
						ofo_ledger_get_mnemo( ledger ),
						closing )){

			exe_id = ofo_dossier_get_current_exe_id( OFO_DOSSIER( st_global->dossier ));
			sexe = ledger_find_exe_by_id( ledger, exe_id );

			if( !sexe ){
				sexe = g_new0( sDetailExe, 1 );
				sexe->exe_id = exe_id;
				sexe->last_clo = my_date_new();
				ledger->private->exes = g_list_prepend( ledger->private->exes, sexe );
			}

			my_date_set_from_date( sexe->last_clo, closing );

			if( ledger_do_update_detail_exe(
						ledger,
						sexe,
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

				g_signal_emit_by_name(
						G_OBJECT( st_global->dossier ),
						OFA_SIGNAL_UPDATED_OBJECT,
						g_object_ref( ledger ),
						NULL );

				ok = TRUE;
			}
		}
	}

	return( ok );
}

/**
 * ofo_ledger_insert:
 *
 * Only insert here a new ledger, so only the main properties
 */
gboolean
ofo_ledger_insert( ofoLedger *ledger )
{
	static const gchar *thisfn = "ofo_ledger_insert";

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

		if( ledger_do_insert(
					ledger,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, ledger );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
ledger_do_insert( ofoLedger *ledger, const ofoSgbd *sgbd, const gchar *user )
{
	return( ledger_insert_main( ledger, sgbd, user ));
}

static gboolean
ledger_insert_main( ofoLedger *ledger, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;
	label = my_utils_quote( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote( ofo_ledger_get_notes( ledger ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_LEDGERS" );

	g_string_append_printf( query,
			"	(LED_MNEMO,LED_LABEL,LED_NOTES,"
			"	LED_UPD_USER, LED_UPD_STAMP) VALUES ('%s','%s',",
			ofo_ledger_get_mnemo( ledger ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			user, stamp_str );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){

		ledger_set_upd_user( ledger, user );
		ledger_set_upd_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_ledger_update:
 *
 * We only update here the user properties, so do not care with the
 * details of balances per currency.
 */
gboolean
ofo_ledger_update( ofoLedger *ledger, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ledger_update";

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p, prev_mnemo=%s", thisfn, ( void * ) ledger, prev_mnemo );

		if( ledger_do_update(
					ledger,
					prev_mnemo,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_UPDATE_DATASET( st_global, ledger, prev_mnemo );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;
	label = my_utils_quote( ofo_ledger_get_label( ledger ));
	notes = my_utils_quote( ofo_ledger_get_notes( ledger ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_LEDGERS SET " );

	g_string_append_printf( query, "LED_MNEMO='%s',", ofo_ledger_get_mnemo( ledger ));
	g_string_append_printf( query, "LED_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "LED_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "LED_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	LED_UPD_USER='%s',LED_UPD_STAMP='%s'"
			"	WHERE LED_MNEMO='%s'", user, stamp_str, prev_mnemo );

	if( ofo_sgbd_query( sgbd, query->str, TRUE )){
		ledger_set_upd_user( ledger, user );
		ledger_set_upd_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	if( ok ){
		query = g_string_new( "UPDATE OFA_T_LEDGERS_CUR SET " );
		g_string_append_printf( query,
				"LED_MNEMO='%s' WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ), prev_mnemo );
		ok &= ofo_sgbd_query( sgbd, query->str, TRUE );
		g_string_free( query, TRUE );
	}

	if( ok ){
		query = g_string_new( "UPDATE OFA_T_LEDGERS_EXE SET " );
		g_string_append_printf( query,
				"LED_MNEMO='%s' WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ), prev_mnemo );
		ok &= ofo_sgbd_query( sgbd, query->str, TRUE );
		g_string_free( query, TRUE );
	}

	return( ok );
}

static gboolean
ledger_do_update_detail_cur( const ofoLedger *ledger, sDetailCur *detail, const ofoSgbd *sgbd )
{
	gchar *query;
	gchar *deb, *cre, *clo_deb, *clo_cre;
	gchar *sdebd, *scred;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR "
			"	WHERE LED_MNEMO='%s' AND LED_EXE_ID=%d AND LED_CUR_CODE='%s'",
					ofo_ledger_get_mnemo( ledger ),
					detail->exe_id,
					detail->currency );

	ofo_sgbd_query( sgbd, query, FALSE );
	g_free( query );

	deb = my_double_to_sql( ofo_ledger_get_deb( ledger, detail->exe_id, detail->currency ));
	cre = my_double_to_sql( ofo_ledger_get_cre( ledger, detail->exe_id, detail->currency ));
	clo_deb = my_double_to_sql( ofo_ledger_get_clo_deb( ledger, detail->exe_id, detail->currency ));
	clo_cre = my_double_to_sql( ofo_ledger_get_clo_cre( ledger, detail->exe_id, detail->currency ));
	sdebd = my_date_to_str(
					ofo_ledger_get_deb_date( ledger, detail->exe_id, detail->currency ), MY_DATE_SQL );
	scred = my_date_to_str(
					ofo_ledger_get_cre_date( ledger, detail->exe_id, detail->currency ), MY_DATE_SQL );

	query = g_strdup_printf(
					"INSERT INTO OFA_T_LEDGERS_CUR "
					"	(LED_MNEMO,LED_EXE_ID,LED_CUR_CODE,"
					"	LED_CUR_CLO_DEB,LED_CUR_CLO_CRE,"
					"	LED_CUR_DEB,LED_CUR_DEB_DATE,LED_CUR_CRE,LED_CUR_CRE_DATE) VALUES "
					"	('%s',%d,'%s',%s,%s,%s,'%s',%s,'%s')",
							ofo_ledger_get_mnemo( ledger ),
							detail->exe_id,
							detail->currency,
							clo_deb,
							clo_cre,
							deb,
							sdebd,
							cre,
							scred );

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( scred );
	g_free( sdebd );
	g_free( deb );
	g_free( cre );
	g_free( clo_deb );
	g_free( clo_cre );
	g_free( query );

	return( ok );
}

static gboolean
ledger_do_update_detail_exe( const ofoLedger *ledger, sDetailExe *detail, const ofoSgbd *sgbd )
{
	gchar *query;
	gchar *sdate;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_EXE "
			"	WHERE LED_MNEMO='%s' AND LED_EXE_ID=%d",
					ofo_ledger_get_mnemo( ledger ),
					detail->exe_id );

	ofo_sgbd_query( sgbd, query, FALSE );
	g_free( query );

	sdate = my_date_to_str( detail->last_clo, MY_DATE_SQL );

	query = g_strdup_printf(
					"INSERT INTO OFA_T_LEDGERS_EXE "
					"	(LED_MNEMO,LED_EXE_ID,LED_EXE_LAST_CLO) "
					"	VALUES "
					"	('%s',%d,'%s')",
							ofo_ledger_get_mnemo( ledger ),
							detail->exe_id,
							sdate );

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( sdate );
	g_free( query );

	return( ok );
}

/**
 * ofo_ledger_delete:
 *
 * Take care of deleting both main and detail records.
 */
gboolean
ofo_ledger_delete( ofoLedger *ledger )
{
	static const gchar *thisfn = "ofo_ledger_delete";

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_ledger_is_deletable( ledger, OFO_DOSSIER( st_global->dossier )), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

		if( ledger_do_delete(
					ledger,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, ledger );

			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
ledger_do_delete( ofoLedger *ledger, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok &= ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_EXE WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok &= ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_ledger_get_mnemo( a ), mnemo ));
}

/**
 * ofo_ledger_get_csv:
 */
GSList *
ofo_ledger_get_csv( const ofoDossier *dossier )
{
	GList *set, *exe, *amount;
	GSList *lines;
	gchar *str, *notes, *stamp, *sdfin, *sdclo;
	ofoLedger *ledger;
	sDetailExe *sexe;
	sDetailCur *sdev;
	const gchar *muser;
	gchar *sdebd, *scred;

	OFO_BASE_SET_GLOBAL( st_global, dossier, ledger );

	lines = NULL;

	str = g_strdup_printf( "1;Mnemo;Label;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;Mnemo;Exe;Closed" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "3;Mnemo;Exe;Currency;CloDeb;CloCre;Deb;DebDate;Cre;CreDate" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		ledger = OFO_LEDGER( set->data );

		notes = my_utils_export_multi_lines( ofo_ledger_get_notes( ledger ));
		muser = ofo_ledger_get_upd_user( ledger );
		stamp = my_utils_stamp_to_str( ofo_ledger_get_upd_stamp( ledger ), MY_STAMP_YYMDHMS );

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s",
				ofo_ledger_get_mnemo( ledger ),
				ofo_ledger_get_label( ledger ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( lines, str );

		for( exe=ledger->private->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;

			sdfin = my_date2_to_str( ofo_dossier_get_exe_fin( dossier, sexe->exe_id ), MY_DATE_SQL );
			sdclo = my_date_to_str( sexe->last_clo, MY_DATE_SQL );

			str = g_strdup_printf( "2;%s;%s;%s",
					ofo_ledger_get_mnemo( ledger ),
					sdfin,
					sdclo );

			g_free( sdfin );
			g_free( sdclo );

			lines = g_slist_prepend( lines, str );
		}

		for( amount=ledger->private->amounts ; amount ; amount=amount->next ){
			sdev = ( sDetailCur * ) amount->data;

			sdfin = my_date2_to_str(
							ofo_dossier_get_exe_fin( dossier, sdev->exe_id ),
							MY_DATE_SQL );
			sdebd = my_date_to_str(
							ofo_ledger_get_deb_date( ledger, sdev->exe_id, sdev->currency ),
							MY_DATE_SQL );
			scred = my_date_to_str(
							ofo_ledger_get_cre_date( ledger, sdev->exe_id, sdev->currency ),
							MY_DATE_SQL );

			str = g_strdup_printf( "3;%s;%s;%s;%.2lf;%.2lf;%.2lf;%s;%.2lf;%s",
					ofo_ledger_get_mnemo( ledger ),
					sdfin,
					sdev->currency,
					ofo_ledger_get_clo_deb( ledger, sdev->exe_id, sdev->currency ),
					ofo_ledger_get_clo_cre( ledger, sdev->exe_id, sdev->currency ),
					ofo_ledger_get_deb( ledger, sdev->exe_id, sdev->currency ),
					sdebd,
					ofo_ledger_get_cre( ledger, sdev->exe_id, sdev->currency ),
					scred );

			g_free( sdfin );

			lines = g_slist_prepend( lines, str );
		}
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_ledger_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - ledger mnemo
 * - label
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_ledger_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_ledger_import_csv";
	ofoLedger *ledger;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;
	gchar *splitted;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, ledger );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			ledger = ofo_ledger_new();
			ico=ili->data;

			/* ledger mnemo */
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty mnemo", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_ledger_set_mnemo( ledger, str );

			/* ledger label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_ledger_set_label( ledger, str );

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					splitted = my_utils_import_multi_lines( str );
					ofo_ledger_set_notes( ledger, splitted );
					g_free( splitted );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, ledger );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		ledger_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			ledger_do_insert(
					OFO_LEDGER( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_LEDGER );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
ledger_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_LEDGERS", TRUE ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_LEDGERS_CUR", TRUE ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_LEDGERS_EXE", TRUE ));
}
