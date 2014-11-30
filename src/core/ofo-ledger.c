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
#include "api/ofa-dbms.h"
#include "api/ofa-idataset.h"
#include "api/ofa-iexportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* priv instance data
 */
struct _ofoLedgerPrivate {

	/* dbms data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
	GDate      last_clo;
	GList     *amounts;					/* balances per currency */
};

typedef struct {
	gchar     *currency;
	ofxAmount  clo_deb;
	ofxAmount  clo_cre;
	ofxAmount  deb;
	ofxAmount  cre;
}
	sDetailCur;

static void        on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data );
static void        on_new_ledger_entry( ofoDossier *dossier, ofoEntry *entry );
static void        on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void        on_updated_object_currency_code( ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void        on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data );
static GList      *ledger_load_dataset( ofoDossier *dossier, GType type );
static ofoLedger  *ledger_find_by_mnemo( GList *set, const gchar *mnemo );
static gint        ledger_count_for_currency( const ofaDbms *dbms, const gchar *currency );
static gint        ledger_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo );
static sDetailCur *ledger_find_cur_by_code( const ofoLedger *ledger, const gchar *currency );
static void        ledger_set_upd_user( ofoLedger *ledger, const gchar *upd_user );
static void        ledger_set_upd_stamp( ofoLedger *ledger, const GTimeVal *upd_stamp );
static void        ledger_set_last_clo( ofoLedger *ledger, const GDate *date );
static sDetailCur *ledger_new_cur_with_code( ofoLedger *ledger, const gchar *currency );
static gboolean    ledger_do_insert( ofoLedger *ledger, const ofaDbms *dbms, const gchar *user );
static gboolean    ledger_insert_main( ofoLedger *ledger, const ofaDbms *dbms, const gchar *user );
static gboolean    ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user );
static gboolean    ledger_do_update_detail_cur( const ofoLedger *ledger, sDetailCur *detail, const ofaDbms *dbms );
static gboolean    ledger_do_delete( ofoLedger *ledger, const ofaDbms *dbms );
static gint        ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo );
static gint        ledger_cmp_by_ptr( const ofoLedger *a, const ofoLedger *b );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier );
static gboolean    ledger_do_drop_content( const ofaDbms *dbms );

G_DEFINE_TYPE_EXTENDED( ofoLedger, ofo_ledger, OFO_TYPE_BASE, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IEXPORTABLE, iexportable_iface_init ));

OFA_IDATASET_LOAD( LEDGER, ledger );

static void
free_detail_cur( sDetailCur *detail )
{
	g_free( detail->currency );
	g_free( detail );
}

static void
ledger_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ledger_finalize";
	ofoLedgerPrivate *priv;

	priv = OFO_LEDGER( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	g_return_if_fail( instance && OFO_IS_LEDGER( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->upd_user );
	g_list_free_full( priv->amounts, ( GDestroyNotify ) free_detail_cur );

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_LEDGER, ofoLedgerPrivate );
}

static void
ofo_ledger_class_init( ofoLedgerClass *klass )
{
	static const gchar *thisfn = "ofo_ledger_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_finalize;

	g_type_class_add_private( klass, sizeof( ofoLedgerPrivate ));
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
				SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_VALIDATED_ENTRY, G_CALLBACK( on_validated_entry ), NULL );
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
	const gchar *mnemo, *currency;
	ofoLedger *ledger;
	sDetailCur *detail;
	ofxAmount debit;

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );

	if( ledger ){
		currency = ofo_entry_get_currency( entry );
		detail = ledger_new_cur_with_code( ledger, currency );
		g_return_if_fail( detail );

		debit = ofo_entry_get_debit( entry );
		if( debit ){
			detail->deb += debit;

		} else {
			detail->cre += ofo_entry_get_credit( entry );
		}

		if( ledger_do_update_detail_cur( ledger, detail, ofo_dossier_get_dbms( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( ledger ), NULL );
		}

	} else {
		g_warning( "%s: ledger not found: %s", thisfn, mnemo );
	}

}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_ledger_on_updated_object";
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

/*
 * a currency iso code has been modified (this should be very rare)
 * so update our ledger records
 */
static void
on_updated_object_currency_code( ofoDossier *dossier, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_LEDGERS_CUR "
					"	SET LED_CUR_CODE='%s' WHERE LED_CUR_CODE='%s'", code, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );

	g_free( query );

	ofa_idataset_free_dataset( dossier, OFO_TYPE_LEDGER );

	g_signal_emit_by_name( G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_LEDGER );
}

/*
 * an entry is validated, either individually or as the result of the
 * closing of a ledger
 */
static void
on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data )
{
	static const gchar *thisfn = "ofo_ledger_on_validated_entry";
	const gchar *currency, *mnemo;
	ofoLedger *ledger;
	sDetailCur *detail;
	ofxAmount debit, credit;

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	mnemo = ofo_entry_get_ledger( entry );
	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
	if( ledger ){

		currency = ofo_entry_get_currency( entry );
		detail = ledger_find_cur_by_code( ledger, currency );
		/* the entry has necessarily be already recorded while in rough * status */
		g_return_if_fail( detail );

		debit = ofo_entry_get_debit( entry );
		detail->clo_deb += debit;
		detail->deb -= debit;
		credit = ofo_entry_get_credit( entry );
		detail->clo_cre += credit;
		detail->cre -= credit;

		if( ledger_do_update_detail_cur( ledger, detail, ofo_dossier_get_dbms( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( ledger ), mnemo );
		}

	} else {
		g_warning( "%s: ledger not found: %s", thisfn, mnemo );
	}
}

static GList *
ledger_load_dataset( ofoDossier *dossier, GType type )
{
	GSList *result, *irow, *icol;
	ofoLedger *ledger;
	const ofaDbms *dbms;
	GList *dataset, *iset;
	gchar *query;
	sDetailCur *balance;
	GTimeVal timeval;
	GDate date;

	dbms = ofo_dossier_get_dbms( dossier );

	result = ofa_dbms_query_ex( dbms,
			"SELECT LED_MNEMO,LED_LABEL,LED_NOTES,"
			"	LED_UPD_USER,LED_UPD_STAMP,LED_LAST_CLO "
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
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));
		icol = icol->next;
		ledger_set_last_clo( ledger, my_date_set_from_sql( &date, ( const gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, ledger );
	}

	ofa_dbms_free_results( result );

	/* then load the details
	 */
	for( iset=dataset ; iset ; iset=iset->next ){

		ledger = OFO_LEDGER( iset->data );

		query = g_strdup_printf(
				"SELECT LED_CUR_CODE,"
				"	LED_CUR_CLO_DEB,LED_CUR_CLO_CRE,"
				"	LED_CUR_DEB,LED_CUR_CRE "
				"FROM OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'",
						ofo_ledger_get_mnemo( ledger ));

		result = ofa_dbms_query_ex( dbms, query, TRUE );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			balance = g_new0( sDetailCur, 1 );
			balance->currency = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			balance->clo_deb = my_double_set_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			balance->clo_cre = my_double_set_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			balance->deb = my_double_set_from_sql(( const gchar * ) icol->data );
			icol = icol->next;
			balance->cre = my_double_set_from_sql(( const gchar * ) icol->data );

			g_debug( "ledger_load_dataset: adding ledger=%s, currency=%s",
					ofo_ledger_get_mnemo( ledger ), balance->currency );

			ledger->priv->amounts = g_list_prepend( ledger->priv->amounts, balance );
		}

		ofa_dbms_free_results( result );
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
ofo_ledger_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	OFA_IDATASET_GET( dossier, LEDGER, ledger );

	return( ledger_find_by_mnemo( ledger_dataset, mnemo ));
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
ofo_ledger_use_currency( ofoDossier *dossier, const gchar *currency )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	OFA_IDATASET_GET( dossier, LEDGER, ledger );

	return( ledger_count_for_currency( ofo_dossier_get_dbms( dossier ), currency ) > 0 );
}

static gint
ledger_count_for_currency( const ofaDbms *dbms, const gchar *currency )
{
	return( ledger_count_for( dbms, "LED_CUR_CODE", currency ));
}

static gint
ledger_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_LEDGERS_CUR WHERE %s='%s'",
				field, mnemo );

	count = 0;
	result = ofa_dbms_query_ex( dbms, query, TRUE );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofa_dbms_free_results( result );
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

		return(( const gchar * ) ledger->priv->mnemo );
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

		return(( const gchar * ) ledger->priv->label );
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

		return(( const gchar * ) ledger->priv->notes );
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

		return(( const gchar * ) ledger->priv->upd_user );
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

		return(( const GTimeVal * ) &ledger->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ledger_get_last_close:
 *
 * Returns the last closing date for this ledger.
 * The returned date is not %NULL, but may be invalid if the ledger
 * has not been closed yet during the exercice.
 */
const GDate *
ofo_ledger_get_last_close( const ofoLedger *ledger )
{
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		return(( const GDate * ) &ledger->priv->last_clo );
	}

	return( NULL );
}

/**
 * ofo_ledger_get_last_entry:
 *
 * Returns the effect date of the most recent entry written in this
 * ledger as a newly allocated #GDate structure which should be
 * g_free() by the caller,
 * or %NULL if no entry has been found for this ledger.
 */
GDate *
ofo_ledger_get_last_entry( const ofoLedger *ledger )
{
	GDate *deffect;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	deffect = NULL;

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		query = g_strdup_printf(
				"SELECT MAX(ENT_DEFFECT) FROM OFA_T_ENTRIES "
				"	WHERE ENT_LEDGER='%s'", ofo_ledger_get_mnemo( ledger ));

		result = ofa_dbms_query_ex(
						ofo_dossier_get_dbms( BASE_GET_DOSSIER( ledger )), query, TRUE );
		g_free( query );

		if( result ){
			icol = ( GSList * ) result->data;
			deffect = g_new0( GDate, 1 );
			my_date_set_from_sql( deffect, ( const gchar * ) icol->data );
			ofa_dbms_free_results( result );
		}
	}

	return( deffect );
}

/**
 * ofo_ledger_get_clo_deb:
 * @ledger:
 * @currency:
 *
 * Returns the debit balance of this ledger at the last closing for
 * the currency specified, or zero if not found.
 */
ofxAmount
ofo_ledger_get_clo_deb( const ofoLedger *ledger, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, currency );
		if( sdev ){
			return( sdev->clo_deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_clo_cre:
 * @ledger:
 * @currency:
 *
 * Returns the credit balance of this ledger at the last closing for
 * the currency specified, or zero if not found.
 */
ofxAmount
ofo_ledger_get_clo_cre( const ofoLedger *ledger, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, currency );
		if( sdev ){
			return( sdev->clo_cre );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_deb:
 * @ledger:
 * @currency:
 *
 * Returns the current debit balance of this ledger for
 * the currency specified, or zero if not found.
 */
ofxAmount
ofo_ledger_get_deb( const ofoLedger *ledger, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, currency );
		if( sdev ){
			return( sdev->deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_ledger_get_cre:
 * @ledger:
 * @currency:
 *
 * Returns the current credit balance of this ledger for
 * the currency specified, or zero if not found.
 */
ofxAmount
ofo_ledger_get_cre( const ofoLedger *ledger, const gchar *currency )
{
	sDetailCur *sdev;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), 0.0 );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_find_cur_by_code( ledger, currency );
		if( sdev ){
			return( sdev->cre );
		}
	}

	return( 0.0 );
}

static sDetailCur *
ledger_find_cur_by_code( const ofoLedger *ledger, const gchar *currency )
{
	static const gchar *thisfn = "ofo_ledger_ledger_find_cur_by_code";
	GList *idet;
	sDetailCur *sdet;

	for( idet=ledger->priv->amounts ; idet ; idet=idet->next ){
		sdet = ( sDetailCur * ) idet->data;
		if( !g_utf8_collate( sdet->currency, currency )){
			return( sdet );
		}
	}

	g_debug( "%s: ledger=%s, currency=%s not found",
			thisfn, ofo_ledger_get_mnemo( ledger ), currency );

	return( NULL );
}

static sDetailCur *
ledger_new_cur_with_code( ofoLedger *ledger, const gchar *currency )
{
	sDetailCur *sdet;

	sdet = ledger_find_cur_by_code( ledger, currency );

	if( !sdet ){
		sdet = g_new0( sDetailCur, 1 );

		sdet->clo_deb = 0.0;
		sdet->clo_cre = 0.0;
		sdet->deb = 0.0;
		sdet->cre = 0.0;
		sdet->currency = g_strdup( currency );

		ledger->priv->amounts = g_list_prepend( ledger->priv->amounts, sdet );
	}

	return( sdet );
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
		ok = ofo_entry_use_ledger( BASE_GET_DOSSIER( ledger ), mnemo );
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
	sDetailCur *detail;
	const gchar *mnemo;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		ok = TRUE;

		for( ic=ledger->priv->amounts ; ic && ok ; ic=ic->next ){
			detail = ( sDetailCur * ) ic->data;
				ok &= detail->clo_deb == 0.0 && detail->clo_cre == 0.0 &&
						detail->deb == 0.0 && detail->cre == 0.0;
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

		g_free( ledger->priv->mnemo );
		ledger->priv->mnemo = g_strdup( mnemo );
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

		g_free( ledger->priv->label );
		ledger->priv->label = g_strdup( label );
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

		g_free( ledger->priv->notes );
		ledger->priv->notes = g_strdup( notes );
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

		g_free( ledger->priv->upd_user );
		ledger->priv->upd_user = g_strdup( upd_user );
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

		my_utils_stamp_set_from_stamp( &ledger->priv->upd_stamp, upd_stamp );
	}
}

/*
 * ledger_set_last_clo:
 * @ledger:
 *
 * Set the closing date for the ledger.
 */
static void
ledger_set_last_clo( ofoLedger *ledger, const GDate *date )
{
	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		my_date_set_from_date( &ledger->priv->last_clo, date );
	}
}

/**
 * ofo_ledger_set_clo_deb:
 * @ledger:
 * @currency:
 *
 * Set the debit balance of this ledger at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_clo_deb( ofoLedger *ledger, const gchar *currency, ofxAmount amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, currency );
		g_return_if_fail( sdev );

		sdev->clo_deb += amount;
	}
}

/**
 * ofo_ledger_set_clo_cre:
 * @ledger:
 * @currency:
 *
 * Set the credit balance of this ledger at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_clo_cre( ofoLedger *ledger, const gchar *currency, ofxAmount amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, currency );
		g_return_if_fail( sdev );

		sdev->clo_cre += amount;
	}
}

/**
 * ofo_ledger_set_deb:
 * @ledger:
 * @currency:
 *
 * Set the current debit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_deb( ofoLedger *ledger, const gchar *currency, ofxAmount amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, currency );
		g_return_if_fail( sdev );

		sdev->deb += amount;
	}
}

/**
 * ofo_ledger_set_cre:
 * @ledger:
 * @currency:
 *
 * Set the current credit balance of this ledger for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_ledger_set_cre( ofoLedger *ledger, const gchar *currency, ofxAmount amount )
{
	sDetailCur *sdev;

	g_return_if_fail( OFO_IS_LEDGER( ledger ));

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		sdev = ledger_new_cur_with_code( ledger, currency );
		g_return_if_fail( sdev );

		sdev->cre += amount;
	}
}

/**
 * ofo_ledger_close:
 *
 * - all entries in rough status and whose effect date in less or equal
 *   to the closing date, and which are written in this ledger, are
 *   validated
 */
gboolean
ofo_ledger_close( ofoLedger *ledger, ofoDossier *dossier, const GDate *closing )
{
	static const gchar *thisfn = "ofo_ledger_close";
	gboolean ok;

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( closing && my_date_is_valid( closing ), FALSE );

	g_debug( "%s: ledger=%p, closing=%p", thisfn, ( void * ) ledger, ( void * ) closing );

	ok = FALSE;

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		if( ofo_entry_validate_by_ledger(
						dossier,
						ofo_ledger_get_mnemo( ledger ),
						closing )){

			ledger_set_last_clo( ledger, closing );

			if( ofo_ledger_update( ledger, dossier, ofo_ledger_get_mnemo( ledger ))){

				g_signal_emit_by_name(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( ledger ), NULL );

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
ofo_ledger_insert( ofoLedger *ledger, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ledger_insert";

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

		if( ledger_do_insert(
					ledger,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, LEDGER, ledger );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
ledger_do_insert( ofoLedger *ledger, const ofaDbms *dbms, const gchar *user )
{
	return( ledger_insert_main( ledger, dbms, user ));
}

static gboolean
ledger_insert_main( ofoLedger *ledger, const ofaDbms *dbms, const gchar *user )
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

	if( ofa_dbms_query( dbms, query->str, TRUE )){

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
ofo_ledger_update( ofoLedger *ledger, ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ledger_update";

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p, prev_mnemo=%s", thisfn, ( void * ) ledger, prev_mnemo );

		if( ledger_do_update(
					ledger,
					prev_mnemo,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_UPDATE( dossier, LEDGER, ledger, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
ledger_do_update( ofoLedger *ledger, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str, *sdate;
	GTimeVal stamp;
	const GDate *last_clo;

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

	last_clo = ofo_ledger_get_last_close( ledger );
	if( my_date_is_valid( last_clo )){
		sdate = my_date_to_str( last_clo, MY_DATE_SQL );
		g_string_append_printf( query, "LED_LAST_CLO='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "LED_LAST_CLO=NULL," );
	}

	g_string_append_printf( query,
			"	LED_UPD_USER='%s',LED_UPD_STAMP='%s'"
			"	WHERE LED_MNEMO='%s'", user, stamp_str, prev_mnemo );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		ledger_set_upd_user( ledger, user );
		ledger_set_upd_stamp( ledger, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	if( ok && g_utf8_collate( prev_mnemo, ofo_ledger_get_mnemo( ledger ))){
		query = g_string_new( "UPDATE OFA_T_LEDGERS_CUR SET " );
		g_string_append_printf( query,
				"LED_MNEMO='%s' WHERE LED_MNEMO='%s'", ofo_ledger_get_mnemo( ledger ), prev_mnemo );
		ok &= ofa_dbms_query( dbms, query->str, TRUE );
		g_string_free( query, TRUE );
	}

	return( ok );
}

static gboolean
ledger_do_update_detail_cur( const ofoLedger *ledger, sDetailCur *detail, const ofaDbms *dbms )
{
	gchar *query;
	gchar *deb, *cre, *clo_deb, *clo_cre;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR "
			"	WHERE LED_MNEMO='%s' AND LED_CUR_CODE='%s'",
					ofo_ledger_get_mnemo( ledger ),
					detail->currency );

	ofa_dbms_query( dbms, query, FALSE );
	g_free( query );

	deb = my_double_to_sql( ofo_ledger_get_deb( ledger, detail->currency ));
	cre = my_double_to_sql( ofo_ledger_get_cre( ledger, detail->currency ));
	clo_deb = my_double_to_sql( ofo_ledger_get_clo_deb( ledger, detail->currency ));
	clo_cre = my_double_to_sql( ofo_ledger_get_clo_cre( ledger, detail->currency ));

	query = g_strdup_printf(
					"INSERT INTO OFA_T_LEDGERS_CUR "
					"	(LED_MNEMO,LED_CUR_CODE,"
					"	LED_CUR_CLO_DEB,LED_CUR_CLO_CRE,"
					"	LED_CUR_DEB,LED_CUR_CRE) VALUES "
					"	('%s','%s',%s,%s,%s,%s)",
							ofo_ledger_get_mnemo( ledger ),
							detail->currency,
							clo_deb,
							clo_cre,
							deb,
							cre );

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( deb );
	g_free( cre );
	g_free( clo_deb );
	g_free( clo_cre );
	g_free( query );

	return( ok );
}

/**
 * ofo_ledger_delete:
 *
 * Take care of deleting both main and detail records.
 */
gboolean
ofo_ledger_delete( ofoLedger *ledger, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ledger_delete";

	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_ledger_is_deletable( ledger, dossier ), FALSE );

	if( !OFO_BASE( ledger )->prot->dispose_has_run ){

		g_debug( "%s: ledger=%p", thisfn, ( void * ) ledger );

		if( ledger_do_delete(
					ledger,
					ofo_dossier_get_dbms( dossier ))){

			OFA_IDATASET_REMOVE( dossier, LEDGER, ledger );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
ledger_do_delete( ofoLedger *ledger, const ofaDbms *dbms )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_LEDGERS_CUR WHERE LED_MNEMO='%s'",
					ofo_ledger_get_mnemo( ledger ));

	ok &= ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
ledger_cmp_by_mnemo( const ofoLedger *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_ledger_get_mnemo( a ), mnemo ));
}

static gint
ledger_cmp_by_ptr( const ofoLedger *a, const ofoLedger *b )
{
	return( ledger_cmp_by_mnemo( a, ofo_ledger_get_mnemo( b )));
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_ledger_iexportable_iface_init";

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
iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, ofoDossier *dossier )
{
	GList *it, *amount;
	GSList *lines;
	gchar *str, *notes, *stamp, *sdclo;
	ofoLedger *ledger;
	sDetailCur *sdev;
	const gchar *muser;
	gboolean ok, with_headers;
	gulong count;

	OFA_IDATASET_GET( dossier, LEDGER, ledger );

	with_headers = ofa_export_settings_get_headers( settings );

	count = ( gulong ) g_list_length( ledger_dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=ledger_dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		count += g_list_length( ledger->priv->amounts );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "1;Mnemo;Label;Notes;MajUser;MajStamp;LastClo" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = g_strdup_printf( "2;Mnemo;Currency;CloDeb;CloCre;Deb;Cre" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=ledger_dataset ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );

		notes = my_utils_export_multi_lines( ofo_ledger_get_notes( ledger ));
		muser = ofo_ledger_get_upd_user( ledger );
		stamp = my_utils_stamp_to_str( ofo_ledger_get_upd_stamp( ledger ), MY_STAMP_YYMDHMS );
		sdclo = my_date_to_str( ofo_ledger_get_last_close( ledger ), MY_DATE_SQL );

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s;%s",
				ofo_ledger_get_mnemo( ledger ),
				ofo_ledger_get_label( ledger ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "",
				sdclo );

		g_free( sdclo );
		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		for( amount=ledger->priv->amounts ; amount ; amount=amount->next ){
			sdev = ( sDetailCur * ) amount->data;

			str = g_strdup_printf( "2;%s;%s;%.5lf;%.5lf;%.5lf;%.5lf",
					ofo_ledger_get_mnemo( ledger ),
					sdev->currency,
					ofo_ledger_get_clo_deb( ledger, sdev->currency ),
					ofo_ledger_get_clo_cre( ledger, sdev->currency ),
					ofo_ledger_get_deb( ledger, sdev->currency ),
					ofo_ledger_get_cre( ledger, sdev->currency ));

			lines = g_slist_prepend( NULL, str );
			ok = ofa_iexportable_export_lines( exportable, lines );
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
			if( !ok ){
				return( FALSE );
			}
		}
	}

	return( TRUE );
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
ofo_ledger_import_csv( ofoDossier *dossier, GSList *lines, gboolean with_header )
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
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_LEDGER, FALSE );

		ledger_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			ledger_do_insert(
					OFO_LEDGER( ise->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		ofa_idataset_free_dataset( dossier, OFO_TYPE_LEDGER );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_LEDGER );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_LEDGER, TRUE );
	}
}

static gboolean
ledger_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_LEDGERS", TRUE ) &&
			ofa_dbms_query( dbms, "DELETE FROM OFA_T_LEDGERS_CUR", TRUE ));
}
