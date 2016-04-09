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

#include "my/my-utils.h"

#include "api/ofa-formula.h"

#define DEBUG_REGEX_EVALUATION                      0
#define DEBUG_OPERATOR_EVALUATION                   0

/* a simple function: name+args without recursivity:
 * is replaced by its value
 */
static const gchar *st_std_function_def             = "(?<!\\\\)%([a-zA-Z][a-zA-Z0-9_]*)\\(\\s*([^()]+)\\s*\\)";
static GRegex      *st_std_function_regex           = NULL;

/* operation template accepts:
 * - Ai, Li, Ci, Di as shortcuts to ACCOUNT(i), LABEL(i), DEBIT(i), CREDIT(i)
 * Defining here two subpatterns let this regex be evaluated with the
 * same one than 'std_function' above.
 */
static const gchar *st_ope_template_shortcuts_def   = "(?<!\\\\)%([ALDC])([0-9]+)";
static GRegex      *st_ope_template_shortcuts_regex = NULL;

/* when we have replaced all macros and functions by their values, we
 * deal with nested parenthesis which may override the operator
 * precedences
 */
static const gchar *st_nested_def                  = "\\s*\\(\\s*([^()]+)\\s*\\)\\s*";
static GRegex      *st_nested_regex                = NULL;

/* the operator regex
 */
static const gchar *st_operator_def                = "\\s*(?<!\\\\)[+-/*]\\s*";
static GRegex      *st_operator_regex              = NULL;

/* at the end, we replace the backslasged characters by their nominal
 * equivalent
 */
static const gchar *st_backslashed_def             = "\\\\([+-/*%])";
static GRegex      *st_backslashed_regex           = NULL;

static void                regex_allocate( void );
static gchar              *formula_eval_names( const gchar *formula, ofsFormulaHelper *helper );
static gboolean            formula_eval_std_function_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper );
static gchar              *formula_eval_nested( const gchar *formula, ofsFormulaHelper *helper );
static gboolean            formula_eval_nested_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper );
static gchar              *eval_expression( const gchar *input, ofsFormulaHelper *helper );
static gchar              *eval_with_operators( const gchar *input, ofsFormulaHelper *helper );
static gchar              *apply_operator( const gchar *oper, const gchar *left, const gchar *right, ofsFormulaHelper *helper );
static gchar              *remove_backslashes( const gchar *input, ofsFormulaHelper *helper );
static gboolean            check_args_count( gchar **args_array, ofsFormulaHelper *helper, const gchar *caller );
static const ofsFormulaFn *get_formula_fn( const ofsFormulaFn *fns, const gchar *fname );
static gchar              *is_a_formula( const gchar *formula );

static void
regex_allocate( void )
{
	static const gchar *thisfn = "ofa_formula_regex_allocate";
	GError *error;

	if( !st_std_function_regex ){
		error = NULL;
		st_std_function_regex = g_regex_new( st_std_function_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_std_function_regex ){
			g_warning( "%s: std_function: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_ope_template_shortcuts_regex ){
		error = NULL;
		st_ope_template_shortcuts_regex = g_regex_new( st_ope_template_shortcuts_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_ope_template_shortcuts_regex ){
			g_warning( "%s: ope_template_shortcuts: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_nested_regex ){
		error = NULL;
		st_nested_regex = g_regex_new( st_nested_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_nested_regex ){
			g_warning( "%s: nested: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_operator_regex ){
		error = NULL;
		st_operator_regex = g_regex_new( st_operator_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_operator_regex ){
			g_warning( "%s: operator: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_backslashed_regex ){
		error = NULL;
		st_backslashed_regex = g_regex_new( st_backslashed_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_backslashed_regex ){
			g_warning( "%s: backslashed: %s", thisfn, error->message );
			g_error_free( error );
		}
	}
}

/**
 * ofa_formula_eval:
 * @formula: the string to be evaluated.
 * @fns: the list of evaluation functions.
 * @user_data: data to be passed to the evaluation functions.
 * @msg: [allow-none][out]: messages list.
 *
 * Returns: the evaluated string.
 */
gchar *
ofa_formula_eval( const gchar *formula, const ofsFormulaFn *fns, void *user_data, GList **msg )
{
	gchar *str, *res;
	const gchar *p;
	ofsFormulaHelper helper;

	if( msg ){
		*msg = NULL;
	}

	str = is_a_formula( formula );
	if( str ){
		return( str );
	}

	regex_allocate();

	helper.fns = fns;
	helper.user_data = user_data;
	helper.msg = msg;

	/* p is the formula without the '=' sign
	 * re-evaluate this formula while there is something to do
	 * function arguments are evaluated if they contain operators */
	p = g_utf8_next_char( formula );
	str = g_strdup( p );

	while( TRUE ){
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "ofa_formula_eval [1] str='%s'", str );
		}
		res = formula_eval_names( str, &helper );
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "ofa_formula_eval [1] res='%s'", res );
		}
		if( !my_collate( str, res )){
			break;
		}
		g_free( str );
		str = res;
	}

	/* when names have been evaluated, has to deal with parenthesis
	 */
	while( TRUE ){
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "ofa_formula_eval [2] str='%s'", str );
		}
		res = formula_eval_nested( str, &helper );
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "ofa_formula_eval [2] res='%s'", res );
		}
		if( !my_collate( str, res )){
			break;
		}
		g_free( str );
		str = res;
	}

	/* last, apply standard operators
	 * and remove backslash from backslashed characters
	 */
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "ofa_formula_eval [3] str='%s'", str );
	}
	res = eval_expression( str, &helper );
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "ofa_formula_eval [3] res='%s'", res );
	}

	g_free( str );
	str = res;
	res = remove_backslashes( str, &helper );
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "ofa_formula_eval [4] res='%s'", res );
	}

	g_free( str );

	return( res );
}

/*
 * First phase of formula evaluation.
 * We deal here with all %XXX macros and %XXX(...) functions, and their
 * calls are replaced with the evaluated values.
 *
 * This evaluation is iterated until no more changes are available.
 */
static gchar *
formula_eval_names( const gchar *formula, ofsFormulaHelper *helper )
{
	gchar *str1, *str2;

	/* replace ope template shortcuts
	 */
	str1 = g_regex_replace_eval(
					st_ope_template_shortcuts_regex,
					formula, -1, 0, 0,
					( GRegexEvalCallback ) formula_eval_std_function_cb, helper, NULL );

	/* replace function calls %XXX(...) by their individual evaluation
	 */
	str2 = g_regex_replace_eval(
					st_std_function_regex,
					str1, -1, 0, 0,
					( GRegexEvalCallback ) formula_eval_std_function_cb, helper, NULL );

	g_free( str1 );

	return( str2 );
}

/*
 * This is called for each occurrence of the pattern in the string passed
 * to g_regex_replace_eval(), and it should append the replacement to result.
 *
 * 'std_function' regex defines two subpatterns:
 * - index=1: the function name (without the percent sign)
 * - index=2: the parenthesis content (without the parenthesis);
 *            this content is semi-colon-splitted to get the list of arguments;
 *            each argument is operator-evaluated;
 *            the final args list is then passed to the evaluation function
 */
static gboolean
formula_eval_std_function_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_eval_std_function_cb";
	gint count;
	gchar *match, *str, *fname, *args;
	const ofsFormulaFn *sfn;
	gchar **args_array;

	match = g_match_info_fetch( match_info, 0 );
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: entering for match='%s'", thisfn, match );
	}

	count = g_match_info_get_match_count( match_info );
	if( count != 3 ){
		str = g_strdup_printf(
					_( "%s [error] match count=%d, match='%s'" ),
					thisfn, count, match );
		*( helper->msg ) = g_list_append( *( helper->msg ), str );
		g_free( str );
	} else {
		fname = g_match_info_fetch( match_info, 1 );
		if( !my_strlen( fname )){
			g_free( fname );
			str = g_strdup_printf(
						_( "%s [error] match='%s': empty function name" ),
						thisfn, match );
			*( helper->msg ) = g_list_append( *( helper->msg ), str );
			g_free( str );
		} else {
			sfn = get_formula_fn( helper->fns, fname );
			if( sfn ){
				args = g_match_info_fetch( match_info, 2 );
				args_array = g_strsplit( args, OFA_FORMULA_ARG_SEP, -1 );
				helper->match_info = match_info;
				helper->match_str = match;
				helper->match_fn = sfn;
				if( check_args_count( args_array, helper, thisfn )){
					str = ( *sfn->eval )( helper );
					if( str ){
						g_string_append( result, str );
						g_free( str );
					}
					g_list_free_full( helper->args_list, ( GDestroyNotify ) g_free );
					helper->args_list = NULL;
					helper->args_count = 0;
				}
				helper->match_info = NULL;
				helper->match_str = NULL;
				helper->match_fn = NULL;
				g_strfreev( args_array );
				g_free( args );
			} else {
				str = g_strdup_printf(
							_( "%s [error] match='%s': unknown function name: %s" ),
							thisfn, match, fname );
				*( helper->msg ) = g_list_append( *( helper->msg ), str );
				g_free( str );
			}
			g_free( fname );
		}
	}
	g_free( match );

	/* Returns FALSE to continue the replacement process, TRUE to stop it */
	return( FALSE );
}

/*
 * Second phase of formula evaluation.
 * We deal here with an expression between parentheses (and no more
 * parentheses inside of the expression)
 *
 * This evaluation is iterated until no more changes are available.
 */
static gchar *
formula_eval_nested( const gchar *formula, ofsFormulaHelper *helper )
{
	gchar *str1;

	/* replace insider parenthesis
	 */
	str1 = g_regex_replace_eval(
					st_nested_regex,
					formula, -1, 0, 0,
					( GRegexEvalCallback ) formula_eval_nested_cb, helper, NULL );

	return( str1 );
}

/*
 *
 */
static gboolean
formula_eval_nested_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_eval_nested_cb";
	gchar *match, *res;
	gint count;

	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: entering for match='%s', count=%d", thisfn, match, count );
	}

	g_free( match );
	match = g_match_info_fetch( match_info, 1 );

	res = eval_expression( match, helper );
	result = g_string_append( result, res );

	g_free( res );
	g_free( match );

	/* Returns FALSE to continue the replacement process, TRUE to stop it */
	return( FALSE );
}

/*
 * evaluate a string which may (or not) contain simple operations
 */
static gchar *
eval_expression( const gchar *input, ofsFormulaHelper *helper )
{
	gchar *res;

	res = g_strdup( input );

	if( g_regex_match( st_operator_regex, input, 0, NULL )){
		g_free( res );
		res = eval_with_operators( input, helper );
	}

	return( res );
}

static gchar *
eval_with_operators( const gchar *input, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_eval_with_operators";
	const gchar *begin, *p;
	gchar *str, *res;
	gunichar ch;
	GList *list0, *it, *itnext, *itprev;
	const gchar *left, *right;

	/* extract all the arguments */
	list0 = NULL;
	begin = input;
	p = begin;
	while( p ){
		ch = g_utf8_get_char( p );
		if( ch == '\0' ){
			str = ( p == begin ) ? g_strdup( "" ) : g_strstrip( g_strndup( begin, p-begin ));
			list0 = g_list_append( list0, str );
			break;

		} else if( ch == '+' || ch == '-' || ch == '/' || ch == '*' ){
			str = ( p == begin ) ? g_strdup( "" ) : g_strstrip( g_strndup( begin, p-begin ));
			list0 = g_list_append( list0, str );
			str = g_strdup_printf( "%c", ch );
			list0 = g_list_append( list0, str );
			p = g_utf8_next_char( p );
			begin = p;

		} else {
			p = g_utf8_next_char( p );
		}
	}

	if( DEBUG_OPERATOR_EVALUATION ){
		for( it=list0 ; it ; it=it->next ){
			g_debug( "%s [1] '%s'", thisfn, ( const gchar * ) it->data );
		}
	}

	/* first apply product/quotient precedence
	 * deal with one operator per iteration
	 * iterate until no more * nor / operators */
	it = list0;
	while( it ){
		if( !my_collate( it->data, "*" ) || !my_collate( it->data, "/" )){
			itprev = it->prev;
			left = itprev ? ( const gchar * ) itprev->data : NULL;
			itnext = it->next;
			right = itnext ? ( const gchar * ) itnext->data : NULL;
			res = apply_operator( it->data, left, right, helper );
			g_free( it->data );
			it->data = res;
			list0 = g_list_remove_link( list0, itprev );
			g_list_free_full( itprev, ( GDestroyNotify ) g_free );
			list0 = g_list_remove_link( list0, itnext );
			g_list_free_full( itnext, ( GDestroyNotify ) g_free );
			it = list0;

		} else {
			it = it->next;
		}
	}

	if( DEBUG_OPERATOR_EVALUATION ){
		for( it=list0 ; it ; it=it->next ){
			g_debug( "%s [2] '%s'", thisfn, ( const gchar * ) it->data );
		}
	}

	/* and apply addition/substraction last */
	it = list0;
	while( it ){
		if( !my_collate( it->data, "+" ) || !my_collate( it->data, "-" )){
			itprev = it->prev;
			left = itprev ? ( const gchar * ) itprev->data : NULL;
			itnext = it->next;
			right = itnext ? ( const gchar * ) itnext->data : NULL;
			res = apply_operator( it->data, left, right, helper );
			g_free( it->data );
			it->data = res;
			list0 = g_list_remove_link( list0, itprev );
			g_list_free_full( itprev, ( GDestroyNotify ) g_free );
			list0 = g_list_remove_link( list0, itnext );
			g_list_free_full( itnext, ( GDestroyNotify ) g_free );
			it = list0;

		} else {
			it = it->next;
		}
	}

	if( DEBUG_OPERATOR_EVALUATION ){
		for( it=list0 ; it ; it=it->next ){
			g_debug( "%s [3] '%s'", thisfn, ( const gchar * ) it->data );
		}
	}

	res = g_strdup_printf( "%s", ( const gchar * ) list0->data );
	g_list_free_full( list0, ( GDestroyNotify ) g_free );

	/* Returns FALSE to continue the replacement process, TRUE to stop it */
	return( res );
}

static gchar *
apply_operator( const gchar *oper, const gchar *left, const gchar *right, ofsFormulaHelper *helper )
{
	gchar *res;
	gdouble a, b;

	res = NULL;
	a = g_strtod( left ? left : "0", NULL );
	b = g_strtod( right ? right : "0", NULL );

	if( !my_collate( oper, "+" )){
		res = g_strdup_printf( "%.lf", a + b );
	} else if( !my_collate( oper, "-" )){
		res = g_strdup_printf( "%.lf", a - b );
	} else if( !my_collate( oper, "/" ) && b != 0 ){
		res = g_strdup_printf( "%.lf", a / b );
	} else if( !my_collate( oper, "*" )){
		res = g_strdup_printf( "%.lf", a * b );
	} else if( !my_collate( oper, "." )){
		res = g_strdup_printf( "%s%s", left, right );
	}

	return( res );
}

static gchar *
remove_backslashes( const gchar *input, ofsFormulaHelper *helper )
{
	gchar *res;

	res = g_regex_replace( st_backslashed_regex, input, -1, 0, "\\1", 0, NULL );

	return( res );
}

/*
 * check args count
 * set:
 * - a new args_array after operator evaluation
 * - args count
 *
 * Returns: %TRUE if no error.
 */
static gboolean
check_args_count( gchar **args_array, ofsFormulaHelper *helper, const gchar *caller )
{
	gboolean ok;
	gchar **it;
	guint count;
	gchar *str;

	ok = TRUE;
	it = args_array;
	count = 0;
	while( *it ){
		count += 1;
		it++;
	}

	if( helper->match_fn->args_count >= 0 ){
		if( count != helper->match_fn->args_count ){
			ok = FALSE;
			str = g_strdup_printf( _( "%s [error] match='%s': expected %u arguments, found %u" ),
					caller, helper->match_str, helper->match_fn->args_count, count );
			*( helper->msg ) = g_list_append( *( helper->msg ), str );
			g_free( str );
		}
	}

	if( ok ){
		helper->args_count = count;
		helper->args_list = NULL;
		it = args_array;
		while( *it ){
			str = eval_expression( *it, helper );
			helper->args_list = g_list_append( helper->args_list, str );
			it++;
		}
	}

	return( ok );
}

static const ofsFormulaFn *
get_formula_fn( const ofsFormulaFn *fns, const gchar *fname )
{
	gint i;

	for( i=0 ; fns[i].name ; ++i ){
		if( !my_collate( fns[i].name, fname )){
			return( &fns[i] );
		}
	}

	return( NULL );
}

/*
 * returns the to-be-returned string if not a formula
 */
static gchar *
is_a_formula( const gchar *formula )
{
	const gchar *p;

	/* returns the same if not UTF-8 valide */
	if( !g_utf8_validate( formula, -1, NULL )){
		return( g_strdup( formula ));
	}

	/* remove the leading quote if this is not a formula */
	if( g_str_has_prefix( formula, "'=" )){
		p = g_utf8_next_char( formula );
		p = g_utf8_next_char( p );
		return( g_strdup( p ));
	}

	/* returns the same if not a formula */
	if( !g_str_has_prefix( formula, "=" )){
		return( g_strdup( formula ));
	}

	return( NULL );
}

/*
 * TESTS
 */
static const gchar *st_formulas[] = {
		"= %DEBIT( 1 ) * %RATE( TVAN )",							/* %EVAL(%D1*%TVAN) */
		"=%AMOUNT(%CODE(08)+21) + %AMOUNT(%CODE(09)) + %AMOUNT(\\%CODE(09B)) + %AMOUNT(%CODE(10)) + %AMOUNT(%CODE(11)) + %AMOUNT(%CODE(13)) + %AMOUNT(%CODE(14)) + %AMOUNT(%CODE(15)) + %AMOUNT(%CODE(5B))",
		"=%AMOUNT(%CODE(08)) * %AMOUNT(%CODE(09)) + ( %AMOUNT(%CODE(09B)) / ( %AMOUNT(%CODE(10)) + %AMOUNT(%CODE(11)) ) + %AMOUNT(%CODE(13)) ) + %AMOUNT(%CODE(14)) * %AMOUNT(%CODE(15)) + %AMOUNT(%CODE(5B))",
		"SOLDE DE CLOTURE DE L'EXERCICE %A1 - %ACLA(%A1)",
		"'=SOLDE DE CLOTURE DE L'EXERCICE %A1 - %ACLA(%A1)",
		"=SOLDE DE CLOTURE DE L'EXERCICE %A1 \\- %ACLA(%A1)",
		NULL
};

static gchar *eval_a( ofsFormulaHelper *helper );
static gchar *eval_account( ofsFormulaHelper *helper );
static gchar *eval_amount( ofsFormulaHelper *helper );
static gchar *eval_c( ofsFormulaHelper *helper );
static gchar *eval_code( ofsFormulaHelper *helper );
static gchar *eval_credit( ofsFormulaHelper *helper );
static gchar *eval_d( ofsFormulaHelper *helper );
static gchar *eval_debit( ofsFormulaHelper *helper );
static gchar *eval_l( ofsFormulaHelper *helper );
static gchar *eval_label( ofsFormulaHelper *helper );
static gchar *eval_rate( ofsFormulaHelper *helper );

static const ofsFormulaFn st_formula_fns[] = {
		{ "A", 1, eval_a },
		{ "C", 1, eval_c },
		{ "D", 1, eval_d },
		{ "L", 1, eval_l },
		{ "RATE", 1, eval_rate },				/* should be 2 */
		{ "DEBIT", 1, eval_debit },
		{ "CODE", 1, eval_code },
		{ "AMOUNT", 1, eval_amount },
		{ 0 }
};

/**
 * ofa_formula_test:
 *
 * Test the formula evaluation.
 */
void
ofa_formula_test( void )
{
	static const gchar *thisfn = "ofa_formula_test";
	guint i;
	GList *msg;
	gchar *result;

	for( i=0 ; st_formulas[i] ; ++i ){
		g_debug( "%s: formula='%s'", thisfn, st_formulas[i] );
		result = ofa_formula_eval( st_formulas[i], st_formula_fns, NULL, &msg );
		g_debug( "%s: result='%s'", thisfn, result );
		g_free( result );
	}
}

/*
 * %Ai is a shortcut to %ACCOUNT(i)
 */
static gchar *
eval_a( ofsFormulaHelper *helper )
{
	return( eval_account( helper ));
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
	gint row;

	res = NULL;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		row = atoi( cstr );
		res = g_strdup_printf( "ACC%6.6d", row );
	}

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

/*
 * %Ci is a shortcut to %CREDIT(i)
 */
static gchar *
eval_c( ofsFormulaHelper *helper )
{
	return( eval_credit( helper ));
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
 * %Ai is a shortcut to %ACCOUNT(i)
 * Returns: the account id found on row <i>
 */
static gchar *
eval_credit( ofsFormulaHelper *helper )
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
		res = g_strdup_printf( "%lf", 2.2 * a );
	}

	return( res );
}

/*
 * %Di is a shortcut to %DEBIT(i)
 */
static gchar *
eval_d( ofsFormulaHelper *helper )
{
	return( eval_debit( helper ));
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
	gint row;

	res = NULL;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		row = atoi( cstr );
		res = g_strdup_printf( "DEBIT_%3.3d", row );
	}

	return( res );
}

/*
 * %Li is a shortcut to %LABEL(i)
 */
static gchar *
eval_l( ofsFormulaHelper *helper )
{
	return( eval_label( helper ));
}

/*
 * %LABEL(i)
 * Returns: the label found on row <i>
 */
static gchar *
eval_label( ofsFormulaHelper *helper )
{
	return( g_strdup( "label" ));
}

/*
 * %RATE( <rate_id> )
 * returns the rate_id rate at DOPE date
 */
static gchar *
eval_rate( ofsFormulaHelper *helper )
{
	return( g_strdup( "0" ));
}
