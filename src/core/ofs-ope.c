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

#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

/* operators in formula
 */
enum {
	OPE_MINUS = 1,
	OPE_PLUS,
	OPE_PROD,
	OPE_DIV
};

/* an helper structure when computing formulas
 */
typedef struct {
	ofsOpe               *ope;
	ofoDossier           *dossier;
	const ofoOpeTemplate *template;
	gint                  comp_row;			/* counted from zero */
	gint                  comp_column;
}
	sHelper;

/* an helper structure when checking operation
 */
typedef struct {
	const ofsOpe *ope;
	ofoDossier   *dossier;
	ofoLedger    *ledger;
	gchar        *message;
	GList        *currencies;
}
	sChecker;

static GRegex  *st_regex                = NULL;
static GRegex  *st_regex_detail_ref     = NULL;
static GRegex  *st_regex_global_ref     = NULL;
static GRegex  *st_regex_fn             = NULL;

static gboolean st_debug                = FALSE;

#define DEBUG                           if( st_debug ) g_debug

static void         ope_free_detail( ofsOpeDetail *detail );
static void         alloc_regex( void );
static void         compute_simple_formulas( sHelper *helper );
static void         compute_dates( sHelper *helper );
static gchar       *compute_formula( sHelper *helper, const gchar *formula, gint row, gint column );
static gboolean     eval_formula_cb( const GMatchInfo *match_info, GString *result, sHelper *helper );
static gboolean     is_detail_ref( const gchar *token, sHelper *helper, gchar **str );
static gboolean     is_global_ref( const gchar *token, sHelper *helper, gchar **str );
static gboolean     is_rate( const gchar *token, sHelper *helper, gchar **str );
static const gchar *get_ledger_label( sHelper *helper );
static gdouble      compute_solde( sHelper *helper );
static gchar       *get_prev( sHelper *helper );
static gboolean     eval_function_cb( const GMatchInfo *match_info, GString *result, sHelper *helper );
static gboolean     is_function( const gchar *token, sHelper *helper, gchar **str );
static const gchar *get_account_label( sHelper *helper, const gchar *content );
static const gchar *get_account_currency( sHelper *helper, const gchar *content );
static gdouble      eval( sHelper *helper, const gchar *content );
static gint         eval_parse_operator( const gchar *token );
static gdouble      rate( sHelper *helper, const gchar *content );
static gchar       *get_closing_account( sHelper *helper, const gchar *content );
static gboolean     check_for_ledger( sChecker *checker );
static gboolean     check_for_dates( sChecker *checker );
static gboolean     check_for_all_entries( sChecker *checker );
static gboolean     check_for_entry( sChecker *checker, ofsOpeDetail *detail );
static gboolean     check_for_currencies( sChecker *checker );
static void         ope_dump_detail( ofsOpeDetail *detail, void *empty );

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
	ope->ope_template = g_strdup( ofo_ope_template_get_mnemo( template ));

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
 * @dossier: the dossier
 * @template: [in]: the used operation template.
 *
 * Update fields from @ope from formulas from @template.
 *
 * All formulas defined in operation template are computed and set
 * Have to scan all the fields of the operation template
 */
void
ofs_ope_apply_template( ofsOpe *ope, ofoDossier *dossier, const ofoOpeTemplate *template )
{
	static const gchar *thisfn = "ofs_ope_apply_template";
	sHelper *helper;

	g_debug( "%s: entering:", thisfn );
	ofs_ope_dump( ope );

	helper = g_new0( sHelper, 1 );
	helper->ope = ope;
	helper->dossier = dossier;
	helper->template = template;

	alloc_regex();
	compute_simple_formulas( helper );

	g_free( helper );

	g_debug( "%s: returning:", thisfn );
	ofs_ope_dump( ope );
}

/*
 * A formula is something like:
 *  blah blah %[A-Z]+ \(? ... \)?
 */
static void
alloc_regex( void )
{
	static const gchar *st_detail_ref = "%([ALRDC])([0-9]+)";
	static const gchar *st_global_ref = "%(OPMN|OPLA|LEMN|LELA|DOPE|DOMY|DEFFECT|SOLDE|IDEM)";
	static const gchar *st_function = "%(ACLA|ACCU|EVAL|RATE|ACCL)\\(\\s*([^()]+)\\s*\\)";
	/*static const gchar *st_function = "%(ACLA|ACCU|EVAL|RATE)\\(\\s*";*/

	if( !st_regex ){
		/* the general regex to identify simple formulas, either to
		 * global fields or to individual from details */
		st_regex = g_regex_new( "%[A-Z]+[0-9]*", G_REGEX_EXTENDED, 0, NULL );

		/* the regex to identify references to detail fields */
		st_regex_detail_ref = g_regex_new( st_detail_ref, G_REGEX_EXTENDED, 0, NULL );

		/* the regex to identify references to global fields */
		st_regex_global_ref = g_regex_new( st_global_ref, G_REGEX_EXTENDED, 0, NULL );

		/* a regex to identify references to functions */
		st_regex_fn = g_regex_new( st_function, G_REGEX_EXTENDED, 0, NULL );
	}
}

static void
compute_simple_formulas( sHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_compute_simple_formulas";
	ofsOpe *ope;
	const ofoOpeTemplate *template;
	ofsOpeDetail *detail;
	gint i, count;
	gchar *str;

	ope = helper->ope;
	template = helper->template;

	if( !ope->ledger_user_set ){
		g_free( ope->ledger );
		ope->ledger = compute_formula( helper, ofo_ope_template_get_ledger( template ), -1, -1 );
	}

	compute_dates( helper );

	count = ofo_ope_template_get_detail_count( template );
	for( i=0 ; i<count ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		DEBUG( "%s: i=%d", thisfn, i );

		if( !detail->ref_user_set ){
			g_free( detail->ref );
			detail->ref = compute_formula(
					helper, ofo_ope_template_get_detail_ref( template, i ), i, OPE_COL_REF );
		}

		if( !detail->account_user_set ){
			g_free( detail->account );
			detail->account = compute_formula(
					helper, ofo_ope_template_get_detail_account( template, i ), i, OPE_COL_ACCOUNT );
		}

		if( !detail->label_user_set ){
			g_free( detail->label );
			detail->label = compute_formula(
					helper, ofo_ope_template_get_detail_label( template, i ), i, OPE_COL_LABEL );
		}

		if( !detail->debit_user_set ){
			str = compute_formula(
					helper, ofo_ope_template_get_detail_debit( template, i ), i, OPE_COL_DEBIT );
			detail->debit = my_double_set_from_str( str );
			g_free( str );
		}

		if( !detail->credit_user_set ){
			str = compute_formula(
					helper, ofo_ope_template_get_detail_credit( template, i ), i, OPE_COL_CREDIT );
			detail->credit = my_double_set_from_str( str );
			g_free( str );
		}
	}
}

static void
compute_dates( sHelper *helper )
{
	ofsOpe *ope;
	GDate date;
	ofoLedger *ledger;

	ope = helper->ope;

	/* if dope is set, but not deffect:
	 * set minimal deffect depending of dossier and ledger
	 */
	if( ope->dope_user_set && !ope->deffect_user_set ){
		ledger = ofo_ledger_get_by_mnemo( helper->dossier, ope->ledger );
		if( ledger ){
			ofo_dossier_get_min_deffect( &date, helper->dossier, ledger );
			if( my_date_compare( &date, &ope->dope ) < 0 ){
				my_date_set_from_date( &ope->deffect, &ope->dope );
			} else {
				my_date_set_from_date( &ope->deffect, &date );
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
compute_formula( sHelper *helper, const gchar *formula, gint row, gint column )
{
	static const gchar *thisfn = "ofs_ope_compute_formula";
	gchar *str1, *str2, *str3;

	str3 = NULL;
	helper->comp_row = row;
	helper->comp_column = column;

	if( formula ){
		str1 = g_regex_replace_eval( st_regex,
					formula, -1, 0, 0, ( GRegexEvalCallback ) eval_formula_cb, helper, NULL );

		str2 = g_regex_replace_eval( st_regex_fn,
					str1, -1, 0, 0, ( GRegexEvalCallback ) eval_function_cb, helper, NULL );

		if( column == OPE_COL_DEBIT || column == OPE_COL_CREDIT ){
			str3 = my_double_to_str( eval( helper, str2 ));
		} else {
			str3 = g_strdup( str2 );
		}

		g_free( str1 );
		g_free( str2 );

		DEBUG( "%s: formula=%s, row=%d, column=%d, str3=%s", thisfn, formula, row, column, str3 );
	}

	return( str3 );
}

static gboolean
eval_formula_cb( const GMatchInfo *match_info, GString *result, sHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_eval_formula_cb";
	gchar *match;
	gchar *str;

	str = NULL;
	match = g_match_info_fetch( match_info, 0 );
	DEBUG( "%s: match=%s", thisfn, match );

	if( is_detail_ref( match, helper, &str )){
		if( str ){
			g_string_append( result, str );
			g_free( str );
		}

	} else if( is_global_ref( match, helper, &str )){
		if( str ){
			g_string_append( result, str );
			g_free( str );
		}

	} else if( is_rate( match, helper, &str )){
		if( str ){
			g_string_append( result, str );
			g_free( str );
		}

	} else {
		g_string_append( result, match );
	}

	g_free( match );

	return( FALSE );
}

static gboolean
is_detail_ref( const gchar *token, sHelper *helper, gchar **str )
{
	static const gchar *thisfn = "ofs_ope_is_detail_ref";
	gboolean ok;
	GMatchInfo *info;
	gchar *field, *number;
	gint num;
	ofsOpeDetail *detail;

	field = NULL;
	number = NULL;

	ok = g_regex_match( st_regex_detail_ref, token, 0, &info );
	if( ok ){
		ok = g_match_info_matches( info );
	}
	if( ok ){
		field = g_match_info_fetch( info, 1 );
		number = g_match_info_fetch( info, 2 );
		num = atoi( number );
		ok = ( num >= 1 && num <= g_list_length( helper->ope->detail ));
	}
	if( ok ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( helper->ope->detail, num-1 );

		if( !g_utf8_collate( field, "A" )){
			*str = g_strdup( detail->account );

		} else if( !g_utf8_collate( field, "L" )){
			*str = g_strdup( detail->label );

		} else if( !g_utf8_collate( field, "R" )){
			*str = g_strdup( detail->ref );

		} else if( !g_utf8_collate( field, "D" )){
			*str = my_double_to_str( detail->debit );

		} else if( !g_utf8_collate( field, "C" )){
			*str = my_double_to_str( detail->credit );

		} else {
			ok = FALSE;
		}
	}

	g_free( field );
	g_free( number );
	g_match_info_free( info );

	DEBUG( "%s: token=%s, str=%s, ok=%s", thisfn, token, *str, ok ? "True":"False" );
	return( ok );
}

static gboolean
is_global_ref( const gchar *token, sHelper *helper, gchar **str )
{
	static const gchar *thisfn = "ofs_ope_is_global_ref";
	gboolean ok;
	GMatchInfo *info;
	gchar *field;

	field = NULL;

	ok = g_regex_match( st_regex_global_ref, token, 0, &info );
	if( ok ){
		ok = g_match_info_matches( info );
	}
	if( ok ){
		field = g_match_info_fetch( info, 1 );

		if( !g_utf8_collate( field, "OPMN" )){
			*str = g_strdup( helper->ope->ope_template );

		} else if( !g_utf8_collate( field, "OPLA" )){
			*str = g_strdup( ofo_ope_template_get_label( helper->template ));

		} else if( !g_utf8_collate( field, "LEMN" )){
			*str = g_strdup( helper->ope->ledger );

		} else if( !g_utf8_collate( field, "LELA" )){
			*str = g_strdup( get_ledger_label( helper ));

		} else if( !g_utf8_collate( field, "DOPE" )){
			*str = my_date_to_str( &helper->ope->dope, MY_DATE_DMYY );

		} else if( !g_utf8_collate( field, "DOMY" )){
			*str = my_date_to_str( &helper->ope->dope, MY_DATE_MMYY );

		} else if( !g_utf8_collate( field, "DEFFECT" )){
			*str = my_date_to_str( &helper->ope->deffect, MY_DATE_DMYY );

		} else if( !g_utf8_collate( field, "SOLDE" )){
			*str = my_double_to_str( compute_solde( helper ));

		} else if( !g_utf8_collate( field, "IDEM" )){
			*str = get_prev( helper );

		} else {
			ok = FALSE;
		}
	}

	g_free( field );
	g_match_info_free( info );

	DEBUG( "%s: token=%s, str=%s, ok=%s", thisfn, token, *str, ok ? "True":"False" );
	return( ok );
}

static gboolean
is_rate( const gchar *token, sHelper *helper, gchar **str )
{
	static const gchar *thisfn = "ofs_ope_is_rate";
	gboolean ok;
	ofoRate *rate;
	ofxAmount amount;

	ok = FALSE;
	rate = ofo_rate_get_by_mnemo( helper->dossier, token+1 );
	if( rate ){
		ok = TRUE;
		if( my_date_is_valid( &helper->ope->dope )){
			amount = ofo_rate_get_rate_at_date( rate, &helper->ope->dope )/100;
			*str = my_double_to_str_ex( amount, 5 );
		}
	}

	DEBUG( "%s: token=%s, str=%s, ok=%s", thisfn, token, *str, ok ? "True":"False" );
	return( ok );
}

static const gchar *
get_ledger_label( sHelper *helper )
{
	ofoLedger *ledger;
	const gchar *label;

	label = NULL;

	ledger = ofo_ledger_get_by_mnemo( helper->dossier, helper->ope->ledger );
	if( ledger ){
		label = ofo_ledger_get_label( ledger );
	}

	return( label );
}

/*
 * compute solde without taking into account the currently computed
 * debit/credit field
 */
static gdouble
compute_solde( sHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_compute_solde";
	gdouble dsold, csold, solde;
	ofsOpeDetail *detail;
	gint i;

	csold = 0.0;
	dsold = 0.0;
	for( i=0 ; i<g_list_length( helper->ope->detail ) ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( helper->ope->detail, i );
		if( helper->comp_row != i || helper->comp_column != OPE_COL_DEBIT ){
			dsold += detail->debit;
		}
		if( helper->comp_row != i || helper->comp_column != OPE_COL_CREDIT ){
			csold += detail->credit;
		}
	}

	solde = fabs( csold-dsold );
	DEBUG( "%s: solde=%.5lf", thisfn, solde );

	return( solde );
}

static gchar *
get_prev( sHelper *helper )
{
	ofsOpeDetail *prev;
	gchar *str;

	str = NULL;

	if( helper->comp_row > 0 ){
		prev = ( ofsOpeDetail * ) g_list_nth_data( helper->ope->detail, helper->comp_row-1 );
		switch( helper->comp_column ){
			case OPE_COL_REF:
				str = g_strdup( prev->ref );
				break;
			case OPE_COL_ACCOUNT:
				str = g_strdup( prev->account );
				break;
			case OPE_COL_LABEL:
				str = g_strdup( prev->label );
				break;
			case OPE_COL_DEBIT:
				str = my_double_to_str( prev->debit );
				break;
			case OPE_COL_CREDIT:
				str = my_double_to_str( prev->credit );;
				break;
		}
	}

	return( str );
}

static gboolean
eval_function_cb( const GMatchInfo *match_info, GString *result, sHelper *helper )
{
	static const gchar *thisfn = "ofs_ope_eval_function_cb";
	gchar *match;
	gchar *str;

	str = NULL;
	match = g_match_info_fetch( match_info, 0 );
	DEBUG( "%s: match=%s", thisfn, match );

	if( is_function( match, helper, &str )){
		if( str ){
			g_string_append( result, str );
			g_free( str );
		}

	} else {
		g_string_append( result, match );
	}

	g_free( match );

	return( FALSE );
}

static gboolean
is_function( const gchar *token, sHelper *helper, gchar **str )
{
	static const gchar *thisfn = "ofs_ope_is_function";
	gboolean ok;
	GMatchInfo *info;
	gchar *field, *content;

	field = NULL;

	ok = g_regex_match( st_regex_fn, token, 0, &info );
	DEBUG( "%s: token=%s, g_regex_match=%s", thisfn, token, ok ? "True":"False" );
	if( ok ){
		ok = g_match_info_matches( info );
		DEBUG( "%s: g_match_info_matches=%s", thisfn, ok ? "True":"False" );
	}
	if( ok ){
		field = g_match_info_fetch( info, 1 );
		content = g_match_info_fetch( info, 2 );
		DEBUG( "%s: field=%s, content=%s", thisfn, field, content );

		if( !g_utf8_collate( field, "ACLA" )){
			*str = g_strdup( get_account_label( helper, content ));

		} else if( !g_utf8_collate( field, "ACCU" )){
			*str = g_strdup( get_account_currency( helper, content ));

		} else if( !g_utf8_collate( field, "EVAL" )){
			*str = my_double_to_str( eval( helper, content ));

		} else if( !g_utf8_collate( field, "RATE" )){
			*str = my_double_to_str_ex( rate( helper, content ), 5 );

		} else if( !g_utf8_collate( field, "ACCL" )){
			*str = get_closing_account( helper, content );

		} else {
			ok = FALSE;
		}
	}

	g_free( field );
	g_match_info_free( info );

	DEBUG( "%s: token=%s, str=%s, ok=%s", thisfn, token, *str, ok ? "True":"False" );
	return( ok );
}

static const gchar *
get_account_label( sHelper *helper, const gchar *content )
{
	ofoAccount *account;
	const gchar *label;

	label = NULL;

	if( content ){
		account = ofo_account_get_by_number( helper->dossier, content );
		if( account ){
			label = ofo_account_get_label( account );
		}
	}

	return( label );
}

static const gchar *
get_account_currency( sHelper *helper, const gchar *content )
{
	ofoAccount *account;
	const gchar *currency;

	currency = NULL;

	if( content ){
		account = ofo_account_get_by_number( helper->dossier, content );
		if( account ){
			currency = ofo_account_get_currency( account );
		}
	}

	return( currency );
}

static gdouble
eval( sHelper *helper, const gchar *content )
{
	static const gchar *thisfn = "ofs_ope_eval";
	gchar **tokens, **iter;
	gdouble amount, amount_iter;
	gboolean first_iter, expect_operator;
	gint operator;

	tokens = g_regex_split_simple( "([-+*/])", content, 0, 0 );
	iter = tokens;
	amount = 0;
	first_iter = TRUE;
	expect_operator = TRUE;
	operator = 0;

	while( *iter ){
		if( expect_operator ){
			operator = eval_parse_operator( *iter );
			if( !operator ){
				if( first_iter ){
					operator = OPE_PLUS;
					expect_operator = FALSE;
				} else {
					g_warning( "%s: formula='%s': found token='%s' while an operator was expected",
							thisfn, content, *iter );
					break;
				}
			}
		}
		if( !expect_operator ){
			amount_iter = my_double_set_from_str( *iter );
			switch( operator ){
				case OPE_MINUS:
					amount -= amount_iter;
					break;
				case OPE_PLUS:
					amount += amount_iter;
					break;
				case OPE_PROD:
					amount *= amount_iter;
					break;
				case OPE_DIV:
					amount /= amount_iter;
					break;
			}
			amount_iter = 0;
		}
		first_iter = FALSE;
		expect_operator = !expect_operator;
		iter++;
	}

	g_strfreev( tokens );

	return( amount );
}

static gint
eval_parse_operator( const gchar *token )
{
	gint oper;

	oper = 0;

	if( !g_utf8_collate( token, "-" )){
		oper = OPE_MINUS;
	} else if( !g_utf8_collate( token, "+" )){
		oper = OPE_PLUS;
	} else if( !g_utf8_collate( token, "*" )){
		oper = OPE_PROD;
	} else if( !g_utf8_collate( token, "/" )){
		oper = OPE_DIV;
	}

	return( oper );
}

static gdouble
rate( sHelper *helper, const gchar *content )
{
	ofoRate *rate;
	gdouble amount;

	amount = 0;
	if( my_date_is_valid( &helper->ope->dope )){
		rate = ofo_rate_get_by_mnemo( helper->dossier, content );
		if( rate ){
			amount = ofo_rate_get_rate_at_date( rate, &helper->ope->dope )/100;
		}
	}

	return( amount );
}

static gchar *
get_closing_account( sHelper *helper, const gchar *content )
{
	ofoAccount *account;
	const gchar *currency;
	gchar *str;

	str = NULL;

	if( content ){
		account = ofo_account_get_by_number( helper->dossier, content );
		if( account && !ofo_account_is_root( account )){
			currency = ofo_account_get_currency( account );
			str = g_strdup( ofo_dossier_get_sld_account( helper->dossier, currency ));
		}
	}

	g_debug( "ofs_ope_get_closing_account: content=%s, str=%s", content, str );

	return( str );
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
 * Returns: %TRUE if the entries have been successfully generated and
 *  all chcecks are ok.
 */
gboolean
ofs_ope_is_valid( const ofsOpe *ope, ofoDossier *dossier, gchar **message, GList **currencies )
{
	sChecker *checker;
	gboolean ok, oki;

	checker = g_new0( sChecker, 1 );
	checker->ope = ope;
	checker->dossier = dossier;
	ok = TRUE;

	/* check for a valid ledger */
	oki = check_for_ledger( checker );
	ok &= oki;

	/* check for valid operation and effect dates */
	oki = check_for_dates( checker );
	ok &= oki;

	/* check for non empty accounts and labels, updating the currencies */
	oki = check_for_all_entries( checker );
	ok &= oki;

	/* check for balance by currency */
	oki = check_for_currencies( checker );
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
	ofoLedger *ledger;

	ok = FALSE;
	ope = checker->ope;

	if( !ope->ledger || !g_utf8_strlen( ope->ledger, -1 )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Ledger is empty" ));

	} else {
		ledger = ofo_ledger_get_by_mnemo( checker->dossier, ope->ledger );
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
	gboolean ok;
	GDate dmin;
	gint cmp;
	gchar *str;

	ope = checker->ope;
	ok = FALSE;

	if( !my_date_is_valid( &ope->dope )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Invalid operation date" ));

	} else if( !my_date_is_valid( &ope->deffect )){
		g_free( checker->message );
		checker->message = g_strdup( _( "Invalid effect date" ));

	} else if( checker->ledger ){
		ofo_dossier_get_min_deffect( &dmin, checker->dossier, checker->ledger );
		if( my_date_is_valid( &dmin )){
			cmp = my_date_compare( &dmin, &ope->deffect );
			if( cmp >= 0 ){
				str = my_date_to_str( &dmin, MY_DATE_DMYY );
				g_free( checker->message );
				checker->message = g_strdup_printf(
						_( "Effect date less or equal to minimum allowed on this ledger: %s" ), str );
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

	ok = TRUE;
	ope = checker->ope;

	for( it=ope->detail ; it ; it=it->next ){
		detail = ( ofsOpeDetail * ) it->data;
		ok &= check_for_entry( checker, detail );
	}

	return( ok );
}

/*
 * empty account is not an error if both debit and credit are zero
 */
static gboolean
check_for_entry( sChecker *checker, ofsOpeDetail *detail )
{
	ofoAccount *account;
	const gchar *currency;
	gboolean ok;

	ok = FALSE;
	currency = NULL;

	if( !detail->account || !g_utf8_strlen( detail->account, -1 )){
		if( detail->debit || detail->credit ){
			g_free( checker->message );
			checker->message = g_strdup( _( "Empty account" ));
		} else {
			/* entry is not generated */
			ok = TRUE;
		}
	} else {
		account = ofo_account_get_by_number( checker->dossier, detail->account );
		if( !account || !OFO_IS_ACCOUNT( account )){
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Unknown account: %s" ), detail->account );

		} else if( ofo_account_is_root( account )){
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Account is root: %s" ), detail->account );

		} else if( !detail->label || !g_utf8_strlen( detail->label, -1 )){
			g_free( checker->message );
			checker->message = g_strdup( _( "Empty label" ));

		} else if(( detail->debit && detail->credit ) || ( !detail->debit && !detail->credit )){
			g_free( checker->message );
			checker->message = g_strdup( _( "Invalid amounts" ));

		} else {
			currency = ofo_account_get_currency( account );
			ofs_currency_add_currency( &checker->currencies, currency, detail->debit, detail->credit );
			ok = TRUE;
		}
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
	gdouble precision;

	errors = 0;
	precision = 1/PRECISION;

	for( it=checker->currencies ; it ; it=it->next ){
		scur = ( ofsCurrency * ) it->data;
		if( scur->debit == scur->credit && !scur->debit ){
			errors += 1;
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Empty currency balance: %s" ), scur->currency );

		} else if( abs( scur->debit - scur->credit ) > precision ){
			errors += 1;
			g_free( checker->message );
			checker->message = g_strdup_printf( _( "Unbalanced currency: %s" ), scur->currency );
		}

		/* debit = credit and are not nuls */
	}

	return( errors == 0 );
}

/**
 * ofs_ope_generate_entries:
 */
GList *
ofs_ope_generate_entries( const ofsOpe *ope, ofoDossier *dossier )
{
	GList *entries;
	ofsOpeDetail *detail;
	gint i, count;
	ofoAccount *account;
	const gchar *currency;

	entries = NULL;
	count = g_list_length( ope->detail );

	for( i=0 ; i<count ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		if( detail->account && ( detail->debit || detail->credit )){
			account = ofo_account_get_by_number( dossier, detail->account );
			if( account ){
				currency = ofo_account_get_currency( account );
				entries = g_list_append(
						entries,
						ofo_entry_new_with_data(
								dossier,
								&ope->deffect, &ope->dope, detail->label,
								detail->ref, detail->account,
								currency,
								ope->ledger,
								ope->ope_template,
								detail->debit, detail->credit ));
			}
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

	sdope = my_date_to_str( &ope->dope, MY_DATE_DMYY );
	sdeffect = my_date_to_str( &ope->deffect, MY_DATE_DMYY );

	g_debug( "%s: ope=%p, template=%s, ledger=%s, ledger_user_set=%s,"
			" dope=%s, dope_user_set=%s, deffect=%s, deffect_user_set=%s",
				thisfn, ( void * ) ope, ope->ope_template,
				ope->ledger, ope->ledger_user_set ? "True":"False",
				sdope, ope->dope_user_set ? "True":"False",
				sdeffect, ope->deffect_user_set ? "True":"False" );

	g_free( sdope );
	g_free( sdeffect );

	g_list_foreach( ope->detail, ( GFunc ) ope_dump_detail, NULL );
}

static void
ope_dump_detail( ofsOpeDetail *detail, void *empty )
{
	static const gchar *thisfn = "ofs_ope_dump";

	g_debug( "%s: detail=%p, ref=%s, ref_user_set=%s, account=%s, account_user_set=%s,"
			" label=%s, label_user_set=%s, debit=%.5lf, debit_user_set=%s, credit=%.5lf, credit_user_set=%s",
				thisfn, ( void * ) detail,
				detail->ref, detail->ref_user_set ? "True":"False",
				detail->account, detail->account_user_set ? "True":"False",
				detail->label, detail->label_user_set ? "True":"False",
				detail->debit, detail->debit_user_set ? "True":"False",
				detail->credit, detail->credit_user_set ? "True":"False" );
}

/**
 * ofs_ope_free:
 */
void
ofs_ope_free( ofsOpe *ope )
{
	if( ope ){
		g_free( ope->ope_template );
		g_free( ope->ledger );
		g_list_free_full( ope->detail, ( GDestroyNotify ) ope_free_detail );
		g_free( ope );
	}
}

static void
ope_free_detail( ofsOpeDetail *detail )
{
	g_free( detail->ref );
	g_free( detail->account );
	g_free( detail->label );
	g_free( detail );
}
