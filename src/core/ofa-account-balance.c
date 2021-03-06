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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-account-balance.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* properties
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	gchar      *account_from;
	gchar      *account_to;
	GDate       from_date;
	GDate       to_date;

	GList      *accounts;				/* list of ofsAccountBalancePeriod structures */
	GList      *totals;					/* general total by currency (list of ofsAccountBalancePeriod structures) */
}
	ofaAccountBalancePrivate;

/* class properties
 */
enum {
	PROP_GETTER_ID = 1,
};

static const gchar *st_header_account             = N_( "Account" );
static const gchar *st_header_label               = N_( "Label" );
static const gchar *st_header_solde_at            = N_( "Solde at" );
static const gchar *st_header_total_debits        = N_( "Total debits" );
static const gchar *st_header_total_credits       = N_( "Total credits" );
static const gchar *st_header_sens_solde_begin    = N_( "SensSoldeBegin" );
static const gchar *st_header_sens_solde_end      = N_( "SensSoldeEnd" );
static const gchar *st_header_currency            = N_( "Currency" );

#define ACCOUNT_BALANCE_EXPORT_VERSION              1

static void                     compute_accounts_balance( ofaAccountBalance *self );
static gint                     cmp_entries( ofoEntry *a, ofoEntry *b );
static void                     complete_accounts_dataset( ofaAccountBalance *self );
static void                     compute_total_by_currency( ofaAccountBalance *self );
static void                     add_by_currency( ofaAccountBalance *self, ofsAccountBalancePeriod *sabp );
static gint                     cmp_currencies( const ofsAccountBalancePeriod *a, const ofsAccountBalancePeriod *b );
static ofsAccountBalancePeriod *find_account( ofaAccountBalance *self, ofoAccount *account );
static gint                     cmp_accounts( const ofsAccountBalancePeriod *a, const ofsAccountBalancePeriod *b );
static void                     free_accounts( ofaAccountBalance *self );
static void                     free_account( ofsAccountBalancePeriod *sabp );
static void                     free_totals( ofaAccountBalance *self );
static void                     iexportable_iface_init( ofaIExportableInterface *iface );
static guint                    iexportable_get_interface_version( void );
static gchar                   *iexportable_get_label( const ofaIExportable *instance );
static gboolean                 iexportable_export( ofaIExportable *instance, const gchar *format_id );
static gboolean                 iexportable_export_default( ofaIExportable *exportable );

G_DEFINE_TYPE_EXTENDED( ofaAccountBalance, ofa_account_balance, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaAccountBalance )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init ))

static void
account_balance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_balance_finalize";
	ofaAccountBalancePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE( instance ));

	/* free data members here */
	priv = ofa_account_balance_get_instance_private( OFA_ACCOUNT_BALANCE( instance ));

	g_free( priv->account_from );
	g_free( priv->account_to );

	free_accounts( OFA_ACCOUNT_BALANCE( instance ));
	free_totals( OFA_ACCOUNT_BALANCE( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_parent_class )->finalize( instance );
}

static void
account_balance_dispose( GObject *instance )
{
	ofaAccountBalancePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE( instance ));

	priv = ofa_account_balance_get_instance_private( OFA_ACCOUNT_BALANCE( instance ));

	if( !priv->dispose_has_run ){

		/* unref object members here */

		priv->dispose_has_run = TRUE;
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
account_balance_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaAccountBalancePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE( instance ));

	priv = ofa_account_balance_get_instance_private( OFA_ACCOUNT_BALANCE( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_GETTER_ID:
				g_value_set_pointer( value, priv->getter );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our instance datas
 */
static void
account_balance_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaAccountBalancePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE( instance ));

	priv = ofa_account_balance_get_instance_private( OFA_ACCOUNT_BALANCE( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_GETTER_ID:
				priv->getter = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofa_account_balance_init( ofaAccountBalance *self )
{
	static const gchar *thisfn = "ofa_account_balance_init";
	ofaAccountBalancePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BALANCE( self ));

	priv = ofa_account_balance_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->account_from = NULL;
	priv->account_to = NULL;
	my_date_clear( &priv->from_date );
	my_date_clear( &priv->to_date );
	priv->accounts = NULL;
	priv->totals = NULL;
}

static void
ofa_account_balance_class_init( ofaAccountBalanceClass *klass )
{
	static const gchar *thisfn = "ofa_account_balance_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = account_balance_get_property;
	G_OBJECT_CLASS( klass )->set_property = account_balance_set_property;
	G_OBJECT_CLASS( klass )->dispose = account_balance_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_balance_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_GETTER_ID,
			g_param_spec_pointer(
					"ofa-getter",
					"ofaIGetter instance",
					"ofaIGetter instance",
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));
}

/**
 * ofa_account_balance_new:
 * @getter: the #ofaIGetter of the application.
 *
 * Returns: a new #ofaAccountBalance object.
 */
ofaAccountBalance *
ofa_account_balance_new( ofaIGetter *getter )
{
	ofaAccountBalance *obj;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	obj = g_object_new( OFA_TYPE_ACCOUNT_BALANCE, "ofa-getter", getter, NULL );

	return( obj );
}

/**
 * ofa_account_balance_set_dates:
 * @balance: this #ofaAccountBalance object.
 * @account_from: [allow-none]: the start of the accounts.
 * @account_to: [allow-none]: the end of the accounts.
 * @date_from: the beginning effect date.
 * @date_to: the ending effect date.
 *
 * Compute the accounts balances between @date_from and @date_to effect dates.
 *
 * Returns: the list of all the detail accounts date_to be used as the
 * #ofaIRenderable dataset.
 */
GList *
ofa_account_balance_compute( ofaAccountBalance *balance, const gchar *account_from, const gchar *account_to, const GDate *date_from, const GDate *date_to )
{
	static const gchar *thisfn = "ofa_account_balance_compute";
	ofaAccountBalancePrivate *priv;

	g_debug( "%s: balance=%p, date_from=%p, date=%p",
			thisfn, ( void * ) balance, ( void * ) date_from, ( void * ) date_to );

	g_return_val_if_fail( balance && OFA_IS_ACCOUNT_BALANCE( balance ), NULL );

	priv = ofa_account_balance_get_instance_private( balance );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	g_free( priv->account_from );
	priv->account_from = g_strdup( account_from );
	g_free( priv->account_to );
	priv->account_to = g_strdup( account_to );

	my_date_set_from_date( &priv->from_date, date_from );
	my_date_set_from_date( &priv->to_date, date_to );

	compute_accounts_balance( balance );
	complete_accounts_dataset( balance );
	compute_total_by_currency( balance );

	return( priv->accounts );
}

/*
 * Compute the accounts balances once, before rendering
 */
static void
compute_accounts_balance( ofaAccountBalance *self )
{
	static const gchar *thisfn = "ofa_account_balance_compute_accounts_balance";
	ofaAccountBalancePrivate *priv;
	ofaHub *hub;
	ofoDossier *dossier;
	const GDate *exe_begin, *exe_end, *deffect;
	GList *entries, *sorted, *it;
	ofoEntry *entry;
	ofeEntryStatus status;
	ofeEntryPeriod period;
	ofeEntryRule rule;
	const gchar *acc_number;
	gchar *prev_number;
	ofsAccountBalancePeriod *sabp;
	ofoAccount *account;
	gint cmp;
	ofxAmount debit, credit;
	gboolean is_begin, is_end;

	priv = ofa_account_balance_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	exe_begin = ofo_dossier_get_exe_begin( dossier );
	exe_end = ofo_dossier_get_exe_end( dossier );

	/* whether the from_date is the beginning of the exercice */
	is_begin = my_date_is_valid( exe_begin ) && my_date_compare( &priv->from_date, exe_begin ) == 0;

	/* whether the to_date is the ending of the exercice */
	is_end = my_date_is_valid( exe_end ) && my_date_compare( &priv->to_date, exe_end ) == 0;

	/* get all entries (once) sorted by account, effect_date */
	entries = g_list_copy( ofo_entry_get_dataset( priv->getter ));
	sorted = g_list_sort( entries, ( GCompareFunc ) cmp_entries );
	prev_number = NULL;
	priv->accounts = NULL;
	sabp = NULL;

	for( it=sorted ; it ; it=it->next ){
		entry = ( ofoEntry * ) it->data;
		g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
		acc_number = ofo_entry_get_account( entry );
		if( my_strlen( priv->account_from ) && my_collate( priv->account_from, acc_number ) > 0 ){
			continue;
		}
		if( my_strlen( priv->account_to ) && my_collate( priv->account_to, acc_number ) < 0 ){
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
		/* on new account, initialize a new structure */
		if( my_collate( acc_number, prev_number ) != 0 ){
			g_free( prev_number );
			prev_number = g_strdup( acc_number );
			account = ofo_account_get_by_number( priv->getter, acc_number );
			g_return_if_fail( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account ));
			sabp = find_account( self, account );
		}
		g_return_if_fail( sabp != NULL );

		/* do not consider the effect date after the to_date */
		deffect = ofo_entry_get_deffect( entry );
		if( my_date_compare( &priv->to_date, deffect ) < 0 ){
			continue;
		}
		rule = ofo_entry_get_rule( entry );
		/* if end_date is the end of the exercice, then have to get rid
		 * of closing entries */
		if( is_end && rule == ENT_RULE_CLOSE ){
			continue;
		}
		/* if from_date is the beginning of the exercice, then begin_solde
		 * must take into account the forward entries at this date
		 * else begin_solde stops at the day-1 */
		debit = ofo_entry_get_debit( entry );
		credit = ofo_entry_get_credit( entry );
		if( is_begin ){
			cmp = my_date_compare( deffect, &priv->from_date );
			if( cmp < 0 ){
				g_warning(
						"%s: have entry number %lu before the beginning of the exercice, but is not marked as 'past'",
						thisfn, ofo_entry_get_number( entry ));
				debit = 0.0;
				credit = 0.0;
			} else if( cmp == 0 && rule == ENT_RULE_FORWARD ){
				sabp->begin_solde += credit - debit;
			} else {
				sabp->account_balance.debit += debit;
				sabp->account_balance.credit += credit;
			}
		} else {
			cmp = my_date_compare( deffect, &priv->from_date );
			if( cmp < 0 ){
				sabp->begin_solde += credit - debit;

			} else {
				sabp->account_balance.debit += debit;
				sabp->account_balance.credit += credit;
			}
		}
		sabp->end_solde += credit - debit;
	}

	g_free( prev_number );
	g_list_free( entries );
}

/*
 * sort the entries by account and effect_date
 */
static gint
cmp_entries( ofoEntry *a, ofoEntry *b )
{
	gint cmp;
	const gchar *acc_a, *acc_b;
	const GDate *effect_a, *effect_b;

	acc_a = ofo_entry_get_account( a );
	acc_b = ofo_entry_get_account( b );
	cmp = my_collate( acc_a, acc_b );

	if( cmp == 0 ){
		effect_a = ofo_entry_get_deffect( a );
		effect_b = ofo_entry_get_deffect( b );
		cmp = my_date_compare( effect_a, effect_b );
	}

	return( cmp );
}

/*
 * Complete the ofsAccountBalancePeriod list with the rest of all detail
 * accounts between @account_from and @account_to.
 */
static void
complete_accounts_dataset( ofaAccountBalance *self )
{
	ofaAccountBalancePrivate *priv;
	GList *dataset, *it;
	ofoAccount *account;
	const gchar *acc_number;

	priv = ofa_account_balance_get_instance_private( self );

	/* get only the detail accounts */
	dataset = ofo_account_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		account = ( ofoAccount * ) it->data;
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		acc_number = ofo_account_get_number( account );
		if( my_strlen( priv->account_from ) && my_collate( priv->account_from, acc_number ) > 0 ){
			continue;
		}
		if( my_strlen( priv->account_to ) && my_collate( priv->account_to, acc_number ) < 0 ){
			continue;
		}

		if( !ofo_account_is_root( account )){
			find_account( self, account );
		}
	}
}

/*
 * Compute the total per currency.
 * Get rid of round errors.
 */
static void
compute_total_by_currency( ofaAccountBalance *self )
{
	ofaAccountBalancePrivate *priv;
	GList *it;
	ofsAccountBalancePeriod *sabp;
	gint digits;

	priv = ofa_account_balance_get_instance_private( self );

	for( it=priv->accounts ; it ; it=it->next ){
		sabp = ( ofsAccountBalancePeriod * ) it->data;
		digits = ofo_currency_get_digits( sabp->account_balance.currency );
		if( my_double_is_zero( sabp->begin_solde, digits )){
			sabp->begin_solde = 0;
		}
		if( my_double_is_zero( sabp->end_solde, digits )){
			sabp->end_solde = 0;
		}
		add_by_currency( self, sabp );
	}
}

/*
 * We use the same #ofsAccountBalancePeriod for the total per currency
 * with a null account
 */
static void
add_by_currency( ofaAccountBalance *self, ofsAccountBalancePeriod *sabp )
{
	ofaAccountBalancePrivate *priv;
	GList *it;
	ofsAccountBalancePeriod *scur;
	const gchar *cur_code;
	gboolean found;

	priv = ofa_account_balance_get_instance_private( self );

	found = FALSE;
	cur_code = ofo_currency_get_code( sabp->account_balance.currency );

	for( it=priv->totals ; it ; it=it->next ){
		scur = ( ofsAccountBalancePeriod * ) it->data;
		if( !my_collate( cur_code, ofo_currency_get_code( scur->account_balance.currency ))){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		scur = g_new0( ofsAccountBalancePeriod, 1 );
		priv->totals = g_list_insert_sorted( priv->totals, scur, ( GCompareFunc ) cmp_currencies );

		scur->account_balance.currency = sabp->account_balance.currency;
		scur->account_balance.debit = 0.0;
		scur->account_balance.credit = 0.0;
		scur->begin_solde = 0.0;
		scur->end_solde = 0.0;
	}

	g_return_if_fail( scur != NULL );

	scur->account_balance.debit += sabp->account_balance.debit;
	scur->account_balance.credit += sabp->account_balance.credit;
	scur->begin_solde += scur->begin_solde;
	scur->end_solde += scur->end_solde;

	if( 0 ){
		g_debug( "adding begin=%lf, end=%lf - total begin=%lf, end=%lf",
				sabp->begin_solde, sabp->end_solde, scur->begin_solde, scur->end_solde );
	}
}

static gint
cmp_currencies( const ofsAccountBalancePeriod *a, const ofsAccountBalancePeriod *b )
{
	return( my_collate(
				ofo_currency_get_code( a->account_balance.currency ),
				ofo_currency_get_code( b->account_balance.currency )));
}

/**
 * ofa_account_balance_get_totals:
 * @balance: this #ofaAccountBalance object.
 *
 * Returns: the total by currency as a #GList of #ofsAccountBalanceCurrency.
 */
GList *
ofa_account_balance_get_totals( ofaAccountBalance *balance )
{
	static const gchar *thisfn = "ofa_account_balance_get_totals";
	ofaAccountBalancePrivate *priv;

	g_debug( "%s: balance=%p", thisfn, ( void * ) balance );

	g_return_val_if_fail( balance && OFA_IS_ACCOUNT_BALANCE( balance ), NULL );

	priv = ofa_account_balance_get_instance_private( balance );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->totals );
}

/**
 * ofa_account_balance_clear:
 * @balance: this #ofaAccountBalance object.
 *
 * Clear the internal resources associated to the @balance object.
 *
 * This will invalidate the list previously returned by
 * ofa_account_balance_compute().
 */
void
ofa_account_balance_clear( ofaAccountBalance *balance )
{
	static const gchar *thisfn = "ofa_account_balance_clear";
	ofaAccountBalancePrivate *priv;

	g_debug( "%s: balance=%p", thisfn, ( void * ) balance );

	g_return_if_fail( balance && OFA_IS_ACCOUNT_BALANCE( balance ));

	priv = ofa_account_balance_get_instance_private( balance );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->account_from );
	priv->account_from = NULL;
	g_free( priv->account_to );
	priv->account_to = NULL;

	free_accounts( balance );
	free_totals( balance );
}

/*
 * Search for a ofsAccountBalancePeriod structure.
 * If not found, allocates a new one, and inserts it in the list
 */
static ofsAccountBalancePeriod *
find_account( ofaAccountBalance *self, ofoAccount *account )
{
	ofaAccountBalancePrivate *priv;
	ofsAccountBalancePeriod *sabp;
	GList *it;
	const gchar *cur_code;
	gboolean found;

	priv = ofa_account_balance_get_instance_private( self );

	found = FALSE;
	for( it=priv->accounts ; it ; it=it->next ){
		sabp = ( ofsAccountBalancePeriod * ) it->data;
		if( sabp->account_balance.account == account ){
			found = TRUE;
			break;
		}
	}

	if( !found ){
		sabp = g_new0( ofsAccountBalancePeriod, 1 );
		sabp->account_balance.account = account;
		cur_code = ofo_account_get_currency( account );
		g_return_val_if_fail( my_strlen( cur_code ), NULL );
		sabp->account_balance.currency = ofo_currency_get_by_code( priv->getter, cur_code );
		g_return_val_if_fail( sabp->account_balance.currency && OFO_IS_CURRENCY( sabp->account_balance.currency ), NULL );
		sabp->account_balance.debit = 0.0;
		sabp->account_balance.credit = 0.0;
		sabp->begin_solde = 0.0;
		sabp->end_solde = 0.0;
		priv->accounts = g_list_insert_sorted( priv->accounts, sabp, ( GCompareFunc ) cmp_accounts );
	}

	return( sabp );
}

static gint
cmp_accounts( const ofsAccountBalancePeriod *a, const ofsAccountBalancePeriod *b )
{
	return( my_collate(
					ofo_account_get_number( a->account_balance.account ),
					ofo_account_get_number( b->account_balance.account )));
}

static void
free_accounts( ofaAccountBalance *self )
{
	ofaAccountBalancePrivate *priv;

	priv = ofa_account_balance_get_instance_private( self );

	g_list_free_full( priv->accounts, ( GDestroyNotify ) free_account );
	priv->accounts = NULL;
}

static void
free_account( ofsAccountBalancePeriod *sabp )
{
	g_free( sabp );
}

static void
free_totals( ofaAccountBalance *self )
{
	ofaAccountBalancePrivate *priv;

	priv = ofa_account_balance_get_instance_private( self );

	g_list_free_full( priv->totals, ( GDestroyNotify ) free_account );
	priv->totals = NULL;
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_balance_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
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
	return( g_strdup( _( "Current account balances" )));
}

static gboolean
iexportable_export( ofaIExportable *instance, const gchar *format_id )
{
	static const gchar *thisfn = "ofa_account_balance_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTER_DEFAULT_FORMAT_ID )){
		return( iexportable_export_default( instance ));
	}

	g_warning( "%s: format=%s is not managed here", thisfn, format_id );

	return( FALSE );
}

static gboolean
iexportable_export_default( ofaIExportable *exportable )
{
	ofaAccountBalancePrivate *priv;
	ofaStreamFormat *stformat;
	gchar field_sep, *str, *sdate;
	GString *gstr;
	gboolean ok;
	GList *it;
	ofsAccountBalancePeriod *sabp;
	ofxAmount amount;
	const gchar *cstr;
	gulong count;

	priv = ofa_account_balance_get_instance_private( OFA_ACCOUNT_BALANCE( exportable ));

	stformat = ofa_iexportable_get_stream_format( exportable );
	g_return_val_if_fail( stformat && OFA_IS_STREAM_FORMAT( stformat ), FALSE );

	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = g_list_length( priv->accounts );
	count += 2;
	if( ofa_stream_format_get_with_headers( stformat )){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	/* add version lines at the very beginning of the file */
	str = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str );
	g_free( str );
	if( ok ){
		str = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, ACCOUNT_BALANCE_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str );
		g_free( str );
	}

	/* export headers */
	gstr = g_string_new( "0" );							// header
	g_string_append_printf( gstr, "%c1", field_sep );	// first subtable (there is only one here)

	str = my_utils_str_funny_capitalized( gettext( st_header_account ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	str = my_utils_str_funny_capitalized( gettext( st_header_label ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	sdate = my_date_to_str( &priv->from_date, ofa_prefs_date_get_display_format( priv->getter ));
	str = g_strdup_printf( "%s %s" , gettext( st_header_solde_at ), sdate );
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );
	g_free( sdate );

	str = my_utils_str_funny_capitalized( gettext( st_header_sens_solde_begin ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	str = my_utils_str_funny_capitalized( gettext( st_header_total_debits ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	str = my_utils_str_funny_capitalized( gettext( st_header_total_credits ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	sdate = my_date_to_str( &priv->to_date, ofa_prefs_date_get_display_format( priv->getter ));
	str = g_strdup_printf( "%s %s" , gettext( st_header_solde_at ), sdate );
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );
	g_free( sdate );

	str = my_utils_str_funny_capitalized( gettext( st_header_sens_solde_end ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	str = my_utils_str_funny_capitalized( gettext( st_header_currency ));
	g_string_append_printf( gstr, "%c%s", field_sep, str );
	g_free( str );

	ok = ofa_iexportable_append_line( exportable, gstr->str );
	g_string_free( gstr, TRUE );


	/* export dataset */
	for( it=priv->accounts ; it && ok ; it=it->next ){
		sabp = ( ofsAccountBalancePeriod * ) it->data;

		gstr = g_string_new( "1" );							// data line
		g_string_append_printf( gstr, "%c1", field_sep );	// first subtable (there is only one here)

		g_string_append_printf( gstr, "%c%s", field_sep, ofo_account_get_number( sabp->account_balance.account ));
		g_string_append_printf( gstr, "%c%s", field_sep, ofo_account_get_label( sabp->account_balance.account ));

		amount = sabp->begin_solde;
		if( amount < 0 ){
			amount *= -1.0;
		}
		str = ofa_amount_to_csv( amount, sabp->account_balance.currency, stformat );
		g_string_append_printf( gstr, "%c%s", field_sep, str );
		g_free( str );

		cstr = amount > 0 ? ( sabp->begin_solde > 0 ? _( "CR" ) : _( "DB" )) : "";
		g_string_append_printf( gstr, "%c%s", field_sep, cstr );

		str = ofa_amount_to_csv( sabp->account_balance.debit, sabp->account_balance.currency, stformat );
		g_string_append_printf( gstr, "%c%s", field_sep, str );
		g_free( str );

		str = ofa_amount_to_csv( sabp->account_balance.credit, sabp->account_balance.currency, stformat );
		g_string_append_printf( gstr, "%c%s", field_sep, str );
		g_free( str );

		amount = sabp->end_solde;
		if( amount < 0 ){
			amount *= -1.0;
		}
		str = ofa_amount_to_csv( amount, sabp->account_balance.currency, stformat );
		g_string_append_printf( gstr, "%c%s", field_sep, str );
		g_free( str );

		cstr = amount > 0 ? ( sabp->end_solde > 0 ? _( "CR" ) : _( "DB" )) : "";
		g_string_append_printf( gstr, "%c%s", field_sep, cstr );

		g_string_append_printf( gstr, "%c%s", field_sep, ofo_currency_get_code( sabp->account_balance.currency ));

		ok = ofa_iexportable_append_line( exportable, gstr->str );
		g_string_free( gstr, TRUE );
	}

	return( ok );
}
