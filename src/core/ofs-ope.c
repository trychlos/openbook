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
#include <math.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-formula-engine.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

/* sEvalDef:
 * @name: the name of the function.
 * @args_count: the expected count of arguments, -1 for do not check.
 * @eval: the evaluation function which provides the replacement string.
 *
 * Defines the evaluation callback functions.
 */
typedef struct {
	const gchar *name;
	gint         min_args;
	gint         max_args;
	gchar *   ( *eval )( ofsFormulaHelper * );
}
	sEvalDef;

/* an helper structure when computing formulas
 * @row: row number, counted from zero
 */
typedef struct {
	ofsOpe *ope;
	gint    row;
	gint    column;
}
	sOpeHelper;

/* an helper structure when checking operation
 */
typedef struct {
	const ofsOpe *ope;
	ofoLedger    *ledger;
	gchar        *message;
	GList        *currencies;
}
	sChecker;

static gboolean     st_debug                     = TRUE;
#define DEBUG                                    if( st_debug ) g_debug

/* operation template accepts:
 * - Ai, Li, Ci, Di as shortcuts to ACCOUNT(i), LABEL(i), DEBIT(i), CREDIT(i)
 * Defining here two subpatterns let this regex be evaluated with the
 * same one than 'std_function' above.
 */
static const gchar      *st_aldc_shortcuts_def   = "([ALDC])([0-9]+)";
static GRegex           *st_aldc_shortcuts_regex = NULL;

static ofaFormulaEngine *st_engine               = NULL;

static void             compute_simple_formulas( sOpeHelper *helper );
static void             compute_dates( sOpeHelper *helper );
static gchar           *compute_formula( const gchar *formula, sOpeHelper *helper );
static ofaFormulaEvalFn get_formula_eval_fn( const gchar *name, gint *min_count, gint *max_count, GMatchInfo *match_info, sOpeHelper *helper );
static gchar           *eval_aldc_shortcuts( ofsFormulaHelper *helper );
static gchar           *eval_a_shortcut( ofsFormulaHelper *helper, const gchar *rowstr );
static gchar           *eval_c_shortcut( ofsFormulaHelper *helper, const gchar *rowstr );
static gchar           *eval_d_shortcut( ofsFormulaHelper *helper, const gchar *rowstr );
static gchar           *eval_l_shortcut( ofsFormulaHelper *helper, const gchar *rowstr );
static gchar           *eval_account( ofsFormulaHelper *helper );
static gchar           *eval_accl( ofsFormulaHelper *helper );
static gchar           *eval_accu( ofsFormulaHelper *helper );
static gchar           *eval_acla( ofsFormulaHelper *helper );
static gchar           *eval_amount( ofsFormulaHelper *helper );
static gchar           *eval_code( ofsFormulaHelper *helper );
static gchar           *eval_credit( ofsFormulaHelper *helper );
static gchar           *eval_debit( ofsFormulaHelper *helper );
static gchar           *eval_deffect( ofsFormulaHelper *helper );
static gchar           *eval_dope( ofsFormulaHelper *helper );
static gchar           *eval_domy( ofsFormulaHelper *helper );
static gchar           *eval_eval( ofsFormulaHelper *helper );
static gchar           *eval_idem( ofsFormulaHelper *helper );
static gchar           *eval_label( ofsFormulaHelper *helper );
static gchar           *eval_lela( ofsFormulaHelper *helper );
static gchar           *eval_lemn( ofsFormulaHelper *helper );
static gchar           *eval_opmn( ofsFormulaHelper *helper );
static gchar           *eval_opla( ofsFormulaHelper *helper );
static gchar           *eval_rate( ofsFormulaHelper *helper );
static gchar           *eval_rate_by_name( ofsFormulaHelper *helper );
static gchar           *eval_ref( ofsFormulaHelper *helper );
static gchar           *eval_solde( ofsFormulaHelper *helper );
static ofsOpeDetail    *get_ope_detail( const gchar *row_str, ofsFormulaHelper *helper );
static gchar           *get_rate_by_name( const gchar *name, ofsFormulaHelper *helper );
static gboolean         check_for_ledger( sChecker *checker );
static gboolean         check_for_dates( sChecker *checker );
static gboolean         check_for_all_entries( sChecker *checker );
static gboolean         check_for_entry( sChecker *checker, ofsOpeDetail *detail, gint num );
static gboolean         check_for_currencies( sChecker *checker );
static ofsOpeDetail    *get_detail_from_cell_def( const ofsOpe *ope, const gchar *cell_def, gboolean *is_debit, gchar **msg );
static void             ope_dump_detail( ofsOpeDetail *detail, void *empty );
static void             ope_free_detail( ofsOpeDetail *detail );

static const sEvalDef st_formula_fns[] = {
		{ "ACCOUNT", 1, 1, eval_account },
		{ "ACCL",    1, 1, eval_accl },
		{ "ACCU",    1, 1, eval_accu },
		{ "ACLA",    1, 1, eval_acla },
		{ "AMOUNT",  1, 1, eval_amount },
		{ "CODE",    1, 1, eval_code },
		{ "CREDIT",  1, 1, eval_credit },
		{ "DEBIT",   1, 1, eval_debit },
		{ "DEFFECT", 0, 0, eval_deffect },
		{ "DOPE",    0, 0, eval_dope },
		{ "DOMY",    0, 0, eval_domy },
		{ "EVAL",    1, 1, eval_eval },
		{ "IDEM",    0, 0, eval_idem },
		{ "LABEL",   1, 1, eval_label },
		{ "LELA",    0, 0, eval_lela },
		{ "LEMN",    0, 0, eval_lemn },
		{ "OPMN",    0, 0, eval_opmn },
		{ "OPLA",    0, 0, eval_opla },
		{ "RATE",    1, 1, eval_rate },
		{ "REF",     0, 0, eval_ref },
		{ "SOLDE",   0, 0, eval_solde },
		{ 0 }
};


/**
 * ofs_ope_new:
 */
ofsOpe *
ofs_ope_new( const ofoOpeTemplate *template )
{
	ofsOpe *ope;
	ofsOpeDetail *detail;
	gint i, count;

	ope = g_new0( ofsOpe, 1 );
	ope->ope_template = g_object_ref(( gpointer ) template );

	count = ofo_ope_template_get_detail_count( template );
	for( i=0 ; i<count ; ++i ){
		detail = g_new0( ofsOpeDetail, 1 );
		ope->detail = g_list_prepend( ope->detail, detail );
	}

	return( ope );
}

/**
 * ofs_ope_apply_template:
 * @ope: [in]: the input operation.
 *
 * Update fields from @ope from formulas from @template.
 *
 * All formulas defined in operation template are computed and set
 * Have to scan all the fields of the operation template
 */
void
ofs_ope_apply_template( ofsOpe *ope )
{
	static const gchar *thisfn = "ofs_ope_apply_template";
	sOpeHelper *helper;

	g_debug( "%s: entering:", thisfn );
	ofs_ope_dump( ope );

	if( !st_engine ){
		st_engine = ofa_formula_engine_new();
		ofa_formula_engine_set_auto_eval( st_engine, FALSE );
	}

	helper = g_new0( sOpeHelper, 1 );
	helper->ope = ope;

	compute_simple_formulas( helper );

	g_free( helper );

	g_debug( "%s: returning:", thisfn );
	ofs_ope_dump( ope );
}

static void
compute_simple_formulas( sOpeHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_compute_simple_formulas";
	ofsOpe *ope;
	const ofoOpeTemplate *template;
	ofsOpeDetail *detail;
	gint i, count;
	gchar *str;

	ope = helper->ope;
	template = ope->ope_template;
	helper->row = -1;
	helper->column = -1;

	if( !ope->ledger_user_set ){
		g_free( ope->ledger );
		ope->ledger = compute_formula( ofo_ope_template_get_ledger( template ), helper );
	}

	if( !ope->ref_user_set ){
		g_free( ope->ref );
		ope->ref = compute_formula( ofo_ope_template_get_ref( template ), helper );
	}

	compute_dates( helper );

	count = ofo_ope_template_get_detail_count( template );
	for( i=0 ; i<count ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		helper->row = i;
		DEBUG( "%s: i=%d", thisfn, i );

		if( !detail->account_user_set ){
			g_free( detail->account );
			helper->column = OPE_COL_ACCOUNT;
			detail->account = compute_formula( ofo_ope_template_get_detail_account( template, i ), helper );
		}

		if( !detail->label_user_set ){
			g_free( detail->label );
			helper->column = OPE_COL_LABEL;
			detail->label = compute_formula( ofo_ope_template_get_detail_label( template, i ), helper );
		}

		if( !detail->debit_user_set ){
			helper->column = OPE_COL_DEBIT;
			str = compute_formula( ofo_ope_template_get_detail_debit( template, i ), helper );
			detail->debit = ofa_amount_from_str( str );
			g_free( str );
		}

		if( !detail->credit_user_set ){
			helper->column = OPE_COL_CREDIT;
			str = compute_formula( ofo_ope_template_get_detail_credit( template, i ), helper );
			detail->credit = ofa_amount_from_str( str );
			g_free( str );
		}
	}
}

static void
compute_dates( sOpeHelper *helper )
{
	ofsOpe *ope;
	ofaHub *hub;
	ofoDossier *dossier;
	GDate date;
	ofoLedger *ledger;

	ope = helper->ope;

	/* if dope is set, but not deffect:
	 * set minimal deffect depending of dossier and ledger
	 */
	if( ope->dope_user_set && !ope->deffect_user_set && my_date_is_valid( &ope->dope )){
		hub = ofo_base_get_hub( OFO_BASE( helper->ope->ope_template ));
		dossier = ofa_hub_get_dossier( hub );
		ledger = ofo_ledger_get_by_mnemo( hub, ope->ledger );
		if( ledger ){
			ofo_dossier_get_min_deffect( dossier, ledger, &date );
			if( my_date_is_valid( &date )){
				if( my_date_compare( &date, &ope->dope ) < 0 ){
					my_date_set_from_date( &ope->deffect, &ope->dope );
				} else {
					my_date_set_from_date( &ope->deffect, &date );
				}
			}
		}
	}

	/* if dope is not set, but deffect is:
	 * set dope = deffect
	 */
	if( !ope->dope_user_set && ope->deffect_user_set ){
		my_date_set_from_date( &ope->dope, &ope->deffect );
	}
}

/*
 * A formula is something like:
 *  blah blah %[A-Z]+ \(? ... \)?
 */
static gchar *
compute_formula( const gchar *formula, sOpeHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_compute_formula";
	gchar *res;
	GList *msg, *it;

	res = NULL;

	if( my_strlen( formula )){
		res = ofa_formula_engine_eval( st_engine, formula, ( ofaFormulaFindFn ) get_formula_eval_fn, helper, &msg );
		g_debug( "%s: formula='%s', res='%s', msg_count=%d", thisfn, formula, res, g_list_length( msg ));

		for( it=msg ; it ; it=it->next ){
			g_info( "%s: %s", thisfn, ( const gchar * ) it->data );
		}

		g_list_free_full( msg, ( GDestroyNotify ) g_free );
	}

	return( res );
}

/*
 * this is a ofaFormula callback
 * Returns: the evaluation function for the name + expected args count
 */
static ofaFormulaEvalFn
get_formula_eval_fn( const gchar *name, gint *min_count, gint *max_count, GMatchInfo *match_info, sOpeHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_get_formula_eval_fn";
	gint i;
	ofaHub *hub;
	ofoRate *rate;
	GError *error;

	*min_count = 0;
	*max_count = -1;

	for( i=0 ; st_formula_fns[i].name ; ++i ){
		if( !my_collate( st_formula_fns[i].name, name )){
			*min_count = st_formula_fns[i].min_args;
			*max_count = st_formula_fns[i].max_args;
			DEBUG( "%s: found name=%s, min_count=%d, max_count=%d", thisfn, name, *min_count, *max_count );
			return(( ofaFormulaEvalFn ) st_formula_fns[i].eval );
		}
	}

	/* if not a predefined name, is it a rate ?
	 * because we do accept %TVAN as a shortcut to %RATE( TVAN )
	 */
	hub = ofo_base_get_hub( OFO_BASE( helper->ope->ope_template ));
	rate = ofo_rate_get_by_mnemo( hub, name );
	if( rate ){
		*min_count = 0;
		*max_count = 0;
		DEBUG( "%s: found rate for name=%s", thisfn, name );
		return(( ofaFormulaEvalFn ) eval_rate_by_name );
	}

	/* if not a predefined name nor a rate, is it an ALDC shortcut ?
	 */
	if( !st_aldc_shortcuts_regex ){
		error = NULL;
		st_aldc_shortcuts_regex = g_regex_new( st_aldc_shortcuts_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_aldc_shortcuts_regex ){
			g_warning( "%s: aldc_shortcuts: %s", thisfn, error->message );
			g_error_free( error );
		}
	}
	if( st_aldc_shortcuts_regex ){
		if( g_regex_match( st_aldc_shortcuts_regex, name, 0, NULL )){
			*min_count = 0;
			*max_count = 0;
			DEBUG( "%s: found aldc shortcut for name=%s", thisfn, name );
			return(( ofaFormulaEvalFn ) eval_aldc_shortcuts );
		}
	}

	DEBUG( "%s: name=%s: nothing found", thisfn, name );
	return( NULL );
}

/*
 * evaluate an ALDC shortcut
 */
static gchar *
eval_aldc_shortcuts( ofsFormulaHelper *helper )
{
	gchar *res;
	GMatchInfo *info;
	gchar *field, *number;
	GList *list;

	res = NULL;

	if( g_regex_match( st_aldc_shortcuts_regex, helper->match_zero, 0, &info )){

		field = g_match_info_fetch( info, 1 );
		number = g_match_info_fetch( info, 2 );
		list = g_list_prepend( NULL, number );
		helper->args_count = 1;
		helper->args_list = list;

		if( !my_collate( field, "A" )){
			res = eval_a_shortcut( helper, number );

		} else if( !my_collate( field, "L" )){
			res = eval_l_shortcut( helper, number );

		} else if( !my_collate( field, "D" )){
			res = eval_d_shortcut( helper, number );

		} else if( !my_collate( field, "C" )){
			res = eval_c_shortcut( helper, number );
		}

		g_list_free( helper->args_list );
		helper->args_list = NULL;
		helper->args_count = 0;
		g_free( field );
		g_free( number );
	}

	g_match_info_free( info );

	return( res );
}

/*
 * %Ai == %ACCOUNT(i)
 * Returns: the account id found on row <i>
 */
static gchar *
eval_a_shortcut( ofsFormulaHelper *helper, const gchar *rowstr )
{
	gchar *res;
	ofsOpeDetail *detail;

	detail = my_strlen( rowstr ) ? get_ope_detail( rowstr, helper ) : NULL;
	res = detail ? g_strdup( detail->account ) : NULL;

	return( res );
}

/*
 * %Ci == %CREDIT(i)
 * Returns: the credit found on row <i>
 */
static gchar *
eval_c_shortcut( ofsFormulaHelper *helper, const gchar *rowstr )
{
	gchar *res;
	ofsOpeDetail *detail;

	detail = my_strlen( rowstr ) ? get_ope_detail( rowstr, helper ) : NULL;
	res = detail ? ofa_amount_to_str( detail->credit, detail->currency ) : NULL;

	return( res );
}

/*
 * %Di == %DEBIT(i)
 * Returns: the debit found on row <i>
 */
static gchar *
eval_d_shortcut( ofsFormulaHelper *helper, const gchar *rowstr )
{
	gchar *res;
	ofsOpeDetail *detail;

	detail = my_strlen( rowstr ) ? get_ope_detail( rowstr, helper ) : NULL;
	res = detail ? ofa_amount_to_str( detail->debit, detail->currency ) : NULL;

	return( res );
}

/*
 * %Li == %LABEL(i)
 * Returns: the label found on row <i>
 */
static gchar *
eval_l_shortcut( ofsFormulaHelper *helper, const gchar *rowstr )
{
	gchar *res;
	ofsOpeDetail *detail;

	detail = my_strlen( rowstr ) ? get_ope_detail( rowstr, helper ) : NULL;
	res = detail ? g_strdup( detail->label ) : NULL;

	return( res );
}

/*
 * %ACCOUNT(i)
 * Returns: the account id found on row <i>
 */
static gchar *
eval_account( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofsOpeDetail *detail;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	detail = my_strlen( cstr ) ? get_ope_detail( cstr, helper ) : NULL;
	res = detail ? g_strdup( detail->account ) : NULL;

	return( res );
}

/*
 * %ACCL( <account_id> )
 * Returns: the closing account for the currency of the account
 */
static gchar *
eval_accl( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofaHub *hub;
	ofoAccount *account;
	const gchar *currency, *solding;
	ofoDossier * dossier;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	hub = ofo_base_get_hub( OFO_BASE((( sOpeHelper * ) helper->user_data )->ope->ope_template ));
	account = ( hub && my_strlen( cstr )) ? ofo_account_get_by_number( hub, cstr ) : NULL;
	currency = ( account && !ofo_account_is_root( account )) ? ofo_account_get_currency( account ) : NULL;
	dossier = ofa_hub_get_dossier( hub );
	solding = currency ? ofo_dossier_get_sld_account( dossier, currency ) : NULL;
	res = g_strdup( solding ? solding : "" );

	return( res );
}

/*
 * %ACCU( <account_id> )
 * Returns: the currency of the account
 */
static gchar *
eval_accu( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofaHub *hub;
	ofoAccount *account;
	const gchar *currency;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	hub = ofo_base_get_hub( OFO_BASE((( sOpeHelper * ) helper->user_data )->ope->ope_template ));
	account = ( hub && my_strlen( cstr )) ? ofo_account_get_by_number( hub, cstr ) : NULL;
	currency = ( account && !ofo_account_is_root( account )) ? ofo_account_get_currency( account ) : NULL;
	res = g_strdup( currency ? currency : "" );

	return( res );
}

/*
 * %ACLA( <account_id> )
 * Returns: the label of the account
 */
static gchar *
eval_acla( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofaHub *hub;
	ofoAccount *account;
	const gchar *label;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	hub = ofo_base_get_hub( OFO_BASE((( sOpeHelper * ) helper->user_data )->ope->ope_template ));
	account = ( hub && my_strlen( cstr )) ? ofo_account_get_by_number( hub, cstr ) : NULL;
	label = account ? ofo_account_get_label( account ) : NULL;
	res = g_strdup( label ? label : "" );

	return( res );
}

static gchar *
eval_amount( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	gdouble a;

	res = NULL;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		a = g_strtod( cstr, NULL );
		res = g_strdup_printf( "%lf", 1.1 * a );
	}

	return( res );
}

static gchar *
eval_code( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;

	res = NULL;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		res = g_strdup_printf( "%s", cstr );
	}

	return( res );
}

/*
 * %Ci is a shortcut to %CREDIT(i)
 * Returns: the credit found on row <i>
 */
static gchar *
eval_credit( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofsOpeDetail *detail;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	detail = my_strlen( cstr ) ? get_ope_detail( cstr, helper ) : NULL;
	res = detail ? ofa_amount_to_str( detail->credit, detail->currency ) : NULL;

	return( res );
}

/*
 * %DEBIT( <row_number> ), where row_number is counted from 1
 */
static gchar *
eval_debit( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofsOpeDetail *detail;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	detail = my_strlen( cstr ) ? get_ope_detail( cstr, helper ) : NULL;
	res = detail ? ofa_amount_to_str( detail->debit, detail->currency ) : NULL;

	return( res );
}

/*
 * %DOMY: operation date as mmm yyyy
 */
static gchar *
eval_domy( ofsFormulaHelper *helper )
{
	return( my_date_to_str( &(( sOpeHelper * ) helper->user_data )->ope->dope, MY_DATE_MMYY ));
}

/*
 * %DOPE: operation date user_prefs format
 */
static gchar *
eval_dope( ofsFormulaHelper *helper )
{
	return( my_date_to_str( &(( sOpeHelper * ) helper->user_data )->ope->dope, ofa_prefs_date_display()));
}

/*
 * %DEFFECT: effect date user_prefs format
 */
static gchar *
eval_deffect( ofsFormulaHelper *helper )
{
	return( my_date_to_str( &(( sOpeHelper * ) helper->user_data )->ope->deffect, ofa_prefs_date_display()));
}

/*
 * %EVAL( expression ): just returns the expression
 */
static gchar *
eval_eval( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	res = g_strdup( cstr ? cstr : "" );

	return( res );
}

/*
 * %IDEM: returns the value of same column, previous row
 */
static gchar *
eval_idem( ofsFormulaHelper *helper )
{
	sOpeHelper *ope_helper;
	ofsOpeDetail *prev;
	gchar *res;

	res = NULL;
	ope_helper = ( sOpeHelper * ) helper->user_data;

	if( ope_helper->row > 0 ){
		prev = ( ofsOpeDetail * ) g_list_nth_data( ope_helper->ope->detail, ope_helper->row-1 );
		switch( ope_helper->column ){
			case OPE_COL_ACCOUNT:
				res = g_strdup( prev->account );
				break;
			case OPE_COL_LABEL:
				res = g_strdup( prev->label );
				break;
			case OPE_COL_DEBIT:
				res = ofa_amount_to_str( prev->debit, prev->currency );
				break;
			case OPE_COL_CREDIT:
				res = ofa_amount_to_str( prev->credit, prev->currency );
				break;
		}
	}

	g_debug( "eval_idem: col=%d, row=%d, returns '%s'", ope_helper->column, ope_helper->row, res );

	return( res );
}

/*
 * %LABEL(i)
 * Returns: the label found on row <i>
 */
static gchar *
eval_label( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofsOpeDetail *detail;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	detail = my_strlen( cstr ) ? get_ope_detail( cstr, helper ) : NULL;
	res = detail ? g_strdup( detail->label ) : NULL;

	return( res );
}

/*
 * %LELA
 * Returns: the ledger label
 */
static gchar *
eval_lela( ofsFormulaHelper *helper )
{
	gchar *res;
	ofaHub *hub;
	ofoLedger *ledger;

	hub = ofo_base_get_hub( OFO_BASE((( sOpeHelper * ) helper->user_data )->ope->ope_template ));
	ledger = ofo_ledger_get_by_mnemo( hub, (( sOpeHelper * ) helper->user_data )->ope->ledger );
	res = ledger ? g_strdup( ofo_ledger_get_label( ledger )) : NULL;

	return( res );
}

/*
 * %LEMN
 * Returns: the ledger mnemonic
 */
static gchar *
eval_lemn( ofsFormulaHelper *helper )
{
	return( g_strdup((( sOpeHelper * ) helper->user_data )->ope->ledger ));
}

/*
 * %OPLA
 * Returns: the operation template label
 */
static gchar *
eval_opla( ofsFormulaHelper *helper )
{
	return( g_strdup( ofo_ope_template_get_label((( sOpeHelper * ) helper->user_data )->ope->ope_template )));
}

/*
 * %OPMN
 * Returns: the operation template mnemonic
 */
static gchar *
eval_opmn( ofsFormulaHelper *helper )
{
	return( g_strdup( ofo_ope_template_get_mnemo((( sOpeHelper * ) helper->user_data )->ope->ope_template )));
}

/*
 * %RATE( <rate_id> )
 * returns the rate_id rate at DOPE date
 */
static gchar *
eval_rate( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	res = my_strlen( cstr ) ? get_rate_by_name( cstr, helper ) : NULL;

	return( res );
}

/*
 * %<rate_id>
 * e.g. %TVAN as a shortcut to %RATE( TVAN )
 */
static gchar *
eval_rate_by_name( ofsFormulaHelper *helper )
{
	return( get_rate_by_name( helper->match_name, helper ));
}

/*
 * %REF
 * Returns: the operation reference
 */
static gchar *
eval_ref( ofsFormulaHelper *helper )
{
	return( g_strdup((( sOpeHelper * ) helper->user_data )->ope->ref ));
}

/*
 * %SOLDE
 * Returns: the solde of the operation as a user-displayable string
 */
static gchar *
eval_solde( ofsFormulaHelper *helper )
{
	gchar *res;
	sOpeHelper *ope_helper;
	gdouble dsold, csold, solde;
	ofsOpeDetail *detail;
	gint i;

	csold = 0.0;
	dsold = 0.0;
	ope_helper = ( sOpeHelper * ) helper->user_data;

	for( i=0 ; i<g_list_length( ope_helper->ope->detail ) ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope_helper->ope->detail, i );
		if( ope_helper->row != i || ope_helper->column != OPE_COL_DEBIT ){
			dsold += detail->debit;
		}
		if( ope_helper->row != i || ope_helper->column != OPE_COL_CREDIT ){
			csold += detail->credit;
		}
	}

	solde = fabs( csold-dsold );
	res = ofa_amount_to_str( solde, NULL );

	return( res );
}

static ofsOpeDetail *
get_ope_detail( const gchar *row_str, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_get_ope_detail";
	gint num;
	sOpeHelper *ope_helper;
	gchar *str;

	num = atoi( row_str );
	ope_helper = ( sOpeHelper * ) helper->user_data;

	if( num >= 1 && num <= g_list_length( ope_helper->ope->detail )){
		return(( ofsOpeDetail * ) g_list_nth_data( ope_helper->ope->detail, num-1 ));

	} else {
		str = g_strdup_printf( _( "%s: unable to find a valid operation detail for row=%s" ), thisfn, row_str );
		helper->msg = g_list_append( helper->msg, str );
	}

	return( NULL );
}

static gchar *
get_rate_by_name( const gchar *name, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_get_rate_by_name";
	gchar *res;
	sOpeHelper *ope_helper;
	ofaHub *hub;
	ofoRate *rate;
	ofxAmount amount;
	gchar *str;

	res = NULL;
	g_debug( "%s: rate=%s", thisfn, name );
	ope_helper = ( sOpeHelper * ) helper->user_data;
	hub = ofo_base_get_hub( OFO_BASE( ope_helper->ope->ope_template ));
	rate = ofo_rate_get_by_mnemo( hub, name );
	if( rate ){
		if( my_date_is_valid( &ope_helper->ope->dope )){
			amount = ofo_rate_get_rate_at_date( rate, &ope_helper->ope->dope )/( gdouble ) 100;
			res = my_double_to_str( amount,
						g_utf8_get_char( ofa_prefs_amount_thousand_sep()),
						g_utf8_get_char( ofa_prefs_amount_decimal_sep()), 3 );

		} else {
			str = g_strdup_printf( _( "%s: unable to get a rate value while operation date is invalid" ), thisfn );
			helper->msg = g_list_append( helper->msg, str );
		}

	} else {
		str = g_strdup_printf( _( "%s: unknown rate: %s" ), thisfn, name );
		helper->msg = g_list_append( helper->msg, str );
	}

	return( res );
}

/**
 * ofs_ope_is_valid:
 * @ope: [in]: the input operation.
 * @dossier: the dossier.
 * @message: [out][allow-none]: a place to set the output error messsage.
 * @currencies: [out][allow-none]: a place to set the list of balances
 *  per currency. If set, the returned list should be #ofs_currency_list_free()
 *  by the caller.
 *
 * Returns: %TRUE if the operation is valid enough to generate balanced
 * entries on existing accounts with compatible operation and effect
 * dates.
 */
gboolean
ofs_ope_is_valid( const ofsOpe *ope, gchar **message, GList **currencies )
{
	sChecker *checker;
	gboolean ok, oki;

	checker = g_new0( sChecker, 1 );
	checker->ope = ope;
	ok = TRUE;

	/* check for non empty accounts and labels, updating the currencies */
	oki = check_for_all_entries( checker );
	ok &= oki;

	/* check for balance by currency */
	oki = check_for_currencies( checker );
	ok &= oki;

	/* check for a valid ledger */
	oki = check_for_ledger( checker );
	ok &= oki;

	/* check for valid operation and effect dates */
	oki = check_for_dates( checker );
	ok &= oki;

	if( message ){
		*message = checker->message;
	} else {
		g_free( checker->message );
	}
	if( currencies ){
		*currencies = checker->currencies;
	} else {
		ofs_currency_list_free( &checker->currencies );
	}

	return( ok );
}

/*
 * Returns TRUE if a ledger is set and valid
 */
static gboolean
check_for_ledger( sChecker *checker )
{
	const ofsOpe *ope;
	gboolean ok;
	ofaHub *hub;
	ofoLedger *ledger;

	ok = FALSE;
	ope = checker->ope;

	if( !my_strlen( ope->ledger )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Ledger is empty" ));

	} else {
		hub = ofo_base_get_hub( OFO_BASE( checker->ope->ope_template ));
		ledger = ofo_ledger_get_by_mnemo( hub, ope->ledger );
		if( !ledger || !OFO_IS_LEDGER( ledger )){
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Unknown ledger: %s" ), ope->ledger );

		} else {
			checker->ledger = ledger;
			ok = TRUE;
		}
	}

	return( ok );
}

/*
 * Returns TRUE if the dates are set and valid
 *
 * The first valid effect date is older than:
 * - the last exercice closing date of the dossier (if set)
 * - the last closing date of the ledger (if set)
 */
static gboolean
check_for_dates( sChecker *checker )
{
	const ofsOpe *ope;
	ofaHub *hub;
	ofoDossier *dossier;
	gboolean ok;
	GDate dmin;
	gint cmp;
	gchar *str;

	ope = checker->ope;
	hub = ofo_base_get_hub( OFO_BASE( ope->ope_template ));
	dossier = ofa_hub_get_dossier( hub );
	ok = FALSE;

	if( !my_date_is_valid( &ope->dope )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Invalid operation date" ));

	} else if( !my_date_is_valid( &ope->deffect )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Invalid effect date" ));

	} else if( checker->ledger ){
		ofo_dossier_get_min_deffect( dossier, checker->ledger, &dmin );
		if( my_date_is_valid( &dmin )){
			cmp = my_date_compare( &dmin, &ope->deffect );
			if( cmp > 0 ){
				str = my_date_to_str( &dmin, ofa_prefs_date_display());
				g_free( checker->message );
				checker->message = g_strdup_printf(
						_( "Effect date less than the minimum allowed on this ledger: %s" ), str );
				g_free( str );

			} else {
				ok = TRUE;
			}
		}
	}

	return( ok );
}

/*
 * Returns TRUE if the entries are valid:
 * - for entries which have a not-nul balance:
 *   > account is valid
 *   > label is set
 * - totals are the same (no diff) and not nuls
 *
 * Note that we have to check *all* entries in order to be able to
 * visually highlight the erroneous fields
 */
static gboolean
check_for_all_entries( sChecker *checker )
{
	const ofsOpe *ope;
	ofsOpeDetail *detail;
	gboolean ok;
	GList *it;
	gint num, count;

	ok = TRUE;
	ope = checker->ope;

	for( it=ope->detail, num=1 ; it ; it=it->next, ++num ){
		detail = ( ofsOpeDetail * ) it->data;
		ok &= check_for_entry( checker, detail, num );
	}

	/* if all is correct, also check that we would be able to generate
	 * at least one entry */
	if( ok ){
		for( it=ope->detail, count=0 ; it ; it=it->next ){
			detail = ( ofsOpeDetail * ) it->data;
			if( detail->account_is_valid && detail->label_is_valid && detail->amounts_are_valid ){
				count += 1;
			}
		}
		if( count <= 1 ){
			g_free( checker->message );
			checker->message = g_strdup(
					_( "No entry would be generated (may amounts be all zero ?)" ));
			ok = FALSE;
		}
	}

	return( ok );
}

/*
 * empty account or empty label is not error
 *  debit and credit of such a line will not be counted in the currency
 *  balance
 *
 * @num is counted from 1 and refers to the row of GuidedInput grid.
 *
 * OK here means that the operation may be validated, generating valid
 * entries - this doesn't mean that all rows will generate an entry.
 */
static gboolean
check_for_entry( sChecker *checker, ofsOpeDetail *detail, gint num )
{
	ofaHub *hub;
	ofoAccount *account;
	const gchar *currency;
	gboolean ok;

	ok = TRUE;
	account = NULL;
	currency = NULL;
	detail->account_is_valid = FALSE;
	detail->label_is_valid = FALSE;
	detail->amounts_are_valid = FALSE;
	detail->currency = NULL;
	hub = ofo_base_get_hub( OFO_BASE( checker->ope->ope_template ));

	if( my_strlen( detail->label )){
		detail->label_is_valid = TRUE;
	}

	if( my_strlen( detail->account )){
		account = ofo_account_get_by_number( hub, detail->account );
		if( !account || !OFO_IS_ACCOUNT( account )){
			g_free( checker->message );
			checker->message = g_strdup_printf(
					_( "(row %d) unknown account: %s" ), num, detail->account );
			ok = FALSE;

		} else if( ofo_account_is_root( account )){
			g_free( checker->message );
			checker->message = g_strdup_printf(
					_( "(row %d) account is root: %s" ), num, detail->account );
			ok = FALSE;

		} else if( ofo_account_is_closed( account )){
			g_free( checker->message );
			checker->message = g_strdup_printf(
					_( "(row %d) account is closed: %s" ), num, detail->account );
			ok = FALSE;

		} else {
			currency = ofo_account_get_currency( account );
			if( !my_strlen( currency )){
				g_free( checker->message );
				checker->message = g_strdup_printf(
						_( "(row %d) empty currency for %s account" ), num, detail->account );
				ok = FALSE;

			} else {
				detail->account_is_valid = TRUE;
				detail->currency = ofo_currency_get_by_code( hub, currency );
				g_return_val_if_fail( detail->currency && OFO_IS_CURRENCY( detail->currency ), FALSE );
			}
		}
	}

	if(( detail->debit && !detail->credit ) || ( !detail->debit && detail->credit )){
		detail->amounts_are_valid = TRUE;

	/* only an error if both amounts are set */
	} else if( detail->debit && detail->credit ){
		g_free( checker->message );
		checker->message = g_strdup_printf(
				_( "(row %d) invalid amounts" ), num );
		ok = FALSE;
	}

	if( detail->account_is_valid && detail->label_is_valid && detail->amounts_are_valid ){
		g_debug( "check_for_entry: currency=%s, debit=%lf, credit=%lf", currency, detail->debit, detail->credit );
		ofs_currency_add_by_object( &checker->currencies, detail->currency, detail->debit, detail->credit );
	}

	return( ok );
}

/*
 * check for balance by currency
 */
static gboolean
check_for_currencies( sChecker *checker )
{
	GList *it;
	ofsCurrency *scur;
	gint errors;

	errors = 0;

	for( it=checker->currencies ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;
		if( ofs_currency_is_zero( scur )){
			errors += 1;
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Empty currency balance: %s" ), ofo_currency_get_code( scur->currency ));

		} else if( !ofs_currency_is_balanced( scur )){
			errors += 1;
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Unbalanced currency: %s" ), ofo_currency_get_code( scur->currency ));
		}

		/* debit = credit and are not nuls */
	}

	return( errors == 0 );
}

/**
 * ofs_ope_get_amount:
 * @ope: this #ofsOpe instance.
 * @cell_def: the definition of the cell.
 * @message: [allow-none][out]: error message placeholder.
 *
 * Returns: the amount of the specified @cell_def.
 *
 * @cell_def is something like 'D1' or 'C2' or so on.
 */
ofxAmount
ofs_ope_get_amount( const ofsOpe *ope, const gchar *cell_def, gchar **message )
{
	ofxAmount amount;
	ofsOpeDetail *detail;
	gboolean is_debit;
	gchar *msg;

	amount = 0.0;
	if( message ){
		*message = NULL;
	}

	detail = get_detail_from_cell_def( ope, cell_def, &is_debit, &msg );

	if( detail ){
		if( is_debit ){
			amount = detail->debit;
		} else {
			amount = detail->credit;
		}

	} else if( message ){
		*message = msg;
	}

	return( amount );
}

/**
 * ofs_ope_set_amount:
 * @ope: this #ofsOpe instance.
 * @cell_def: the definition of the cell.
 * @amount: the amount to be set.
 *
 * Set the @amount on the specified @cell_def cell.
 */
void
ofs_ope_set_amount( ofsOpe *ope, const gchar *cell_def, ofxAmount amount )
{
	ofsOpeDetail *detail;
	gboolean is_debit;

	detail = get_detail_from_cell_def( ope, cell_def, &is_debit, NULL );

	if( detail ){
		if( is_debit ){
			detail->debit = amount;
			detail->debit_user_set = TRUE;

		} else {
			detail->credit = amount;
			detail->credit_user_set = TRUE;
		}
	}
}

static ofsOpeDetail *
get_detail_from_cell_def( const ofsOpe *ope, const gchar *cell_def, gboolean *is_debit, gchar **msg )
{
	ofsOpeDetail *detail;
	gint row, count;

	detail = NULL;
	if( msg ){
		*msg = NULL;
	}

	count = g_list_length( ope->detail );
	row = atoi( cell_def + 1 ) - 1;

	/* row must be in [0, count-1] interval */
	if( row < 0 || row >= count ){
		if( msg ){
			*msg = g_strdup_printf( _( "invalid row number: %s" ), cell_def+1 );
		}

	} else {
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, row );
		if( g_str_has_prefix( cell_def, "D" )){
			*is_debit = TRUE;

		} else if( g_str_has_prefix( cell_def, "C" )){
			*is_debit = FALSE;

		} else {
			if( msg ){
				*msg = g_strdup_printf( _( "invalid column specification: %c" ), cell_def[0] );
			}
			detail = NULL;
		}
	}

	return( detail );
}

/**
 * ofs_ope_generate_entries:
 *
 * This generate valid entries.
 * The function heavily relies on the result of the #ofs_ope_is_valid()
 * one, which is so called before trying to do the actual generation
 * (this prevent us to have to check that is has been actually called
 *  before this one by the caller).
 */
GList *
ofs_ope_generate_entries( const ofsOpe *ope )
{
	static const gchar *thisfn = "ofs_ope_generate_entries";
	ofaHub *hub;
	GList *entries;
	ofsOpeDetail *detail;
	gint i, count;
	ofoAccount *account;
	const gchar *currency;
	gchar *message;

	if( !ofs_ope_is_valid( ope, &message, NULL )){
		g_warning( "%s: %s", thisfn, message );
		g_free( message );
		return( NULL );
	}

	entries = NULL;
	count = g_list_length( ope->detail );
	hub = ofo_base_get_hub( OFO_BASE( ope->ope_template ));

	for( i=0 ; i<count ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		if( detail->account_is_valid &&
				detail->label_is_valid &&
				detail->amounts_are_valid ){

			account = ofo_account_get_by_number( hub, detail->account );
			g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

			currency = ofo_account_get_currency( account );
			g_return_val_if_fail( my_strlen( currency ), NULL );

			entries = g_list_append( entries,
					ofo_entry_new_with_data(
							hub,
							&ope->deffect, &ope->dope, detail->label,
							ope->ref, detail->account,
							currency,
							ope->ledger,
							ofo_ope_template_get_mnemo( ope->ope_template ),
							detail->debit, detail->credit ));
		}
	}

	return( entries );
}

/**
 * ofs_ope_dump:
 */
void
ofs_ope_dump( const ofsOpe *ope )
{
	static const gchar *thisfn = "ofs_ope_dump";
	gchar *sdope, *sdeffect;

	sdope = my_date_to_str( &ope->dope, ofa_prefs_date_display());
	sdeffect = my_date_to_str( &ope->deffect, ofa_prefs_date_display());

	g_debug( "%s: ope=%p, template=%s, ledger=%s, ledger_user_set=%s,"
			" dope=%s, dope_user_set=%s, deffect=%s, deffect_user_set=%s,"
			" ref=%s, ref_user_set=%s",
				thisfn, ( void * ) ope, ofo_ope_template_get_mnemo( ope->ope_template ),
				ope->ledger, ope->ledger_user_set ? "True":"False",
				sdope, ope->dope_user_set ? "True":"False",
				sdeffect, ope->deffect_user_set ? "True":"False",
				ope->ref, ope->ref_user_set ? "True":"False" );

	g_free( sdope );
	g_free( sdeffect );

	g_list_foreach( ope->detail, ( GFunc ) ope_dump_detail, NULL );
}

static void
ope_dump_detail( ofsOpeDetail *detail, void *empty )
{
	static const gchar *thisfn = "ofs_ope_dump";

	g_debug( "%s: detail=%p, "
			"account=%s, account_user_set=%s, account_is_valid=%s, "
			"label=%s, label_user_set=%s, label_is_valid=%s, "
			"debit=%.5lf, debit_user_set=%s, credit=%.5lf, credit_user_set=%s, "
			"amounts_are_valid=%s",
				thisfn, ( void * ) detail,
				detail->account, detail->account_user_set ? "True":"False", detail->account_is_valid ? "True":"False",
				detail->label, detail->label_user_set ? "True":"False", detail->label_is_valid ? "True":"False",
				detail->debit, detail->debit_user_set ? "True":"False",
				detail->credit, detail->credit_user_set ? "True":"False",
				detail->amounts_are_valid ? "True":"False" );
}

/**
 * ofs_ope_free:
 */
void
ofs_ope_free( ofsOpe *ope )
{
	if( ope ){
		g_object_unref(( gpointer ) ope->ope_template );
		g_free( ope->ledger );
		g_free( ope->ref );
		g_list_free_full( ope->detail, ( GDestroyNotify ) ope_free_detail );
		g_free( ope );
	}
}

static void
ope_free_detail( ofsOpeDetail *detail )
{
	g_free( detail->account );
	g_free( detail->label );
	g_free( detail );
}
