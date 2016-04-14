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

#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-formula-engine.h"
#include "api/ofa-preferences.h"
#include "api/ofo-currency.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* amount interpretation format
	 */
	gunichar         thousand_sep;
	gunichar         decimal_sep;
	guint            digits;

	/* initialization
	 */
	gchar           *formula;
	ofaFormulaFindFn finder;
	void            *user_data;
	GList           *msg;
}
	ofaFormulaEnginePrivate;

#define DEBUG_REGEX_EVALUATION                      1
#define DEBUG_OPERATOR_EVALUATION                   1

/* A macro name or function name + args
 */
//static const gchar *st_functions_def               = "(?<!\\\\)%(?:([a-zA-Z][a-zA-Z0-9_]*)(?:\\(\\s*([^()]+)\\s*\\)))|(?:([a-zA-Z][a-zA-Z0-9_]*)\b(?!\\())";
static const gchar *st_functions_def               = "(?<!\\\\)%([a-zA-Z][a-zA-Z0-9_]*)\\(\\s*([^()]+)\\s*\\)";
static GRegex      *st_functions_regex             = NULL;

static const gchar *st_macros_def                  = "(?<!\\\\)%(\\w+)\\b(?!\\()";
static GRegex      *st_macros_regex                = NULL;

/* when we have replaced all macros and functions by their values, we
 * deal with nested parenthesis which may override the operator
 * precedences
 */
static const gchar *st_nested_def                  = "\\s*\\(\\s*([^()]+)\\s*\\)\\s*";
static GRegex      *st_nested_regex                = NULL;

/* the operator regex
 */
//static const gchar *st_operator_def                = "\\s*(?<!\\\\)[+-/*]\\s*";
static const gchar *st_operator_def                = "\\s*([^+-/*]*)(?<!\\\\)([+-/*])\\s*(.*)";
static GRegex      *st_operator_regex              = NULL;

static const gchar *st_minusminus_def              = "--";
static GRegex      *st_minusminus_regex            = NULL;

static const gchar *st_minusplus_def               = "-+";
static GRegex      *st_minusplus_regex             = NULL;

/* the %IF(...) function
 * It is hardcoded in this formula engine, and does not have to be
 * provided by the caller.
 */
static const gchar *st_if_def                      = "(.*)([<=>!]+)(.*)";
static GRegex      *st_if_regex                    = NULL;

/* at the end, we replace the backslasged characters by their nominal
 * equivalent
 */
static const gchar *st_backslashed_def             = "\\\\([+-/*%])";
static GRegex      *st_backslashed_regex           = NULL;

static void      regex_allocate( void );
static gchar    *do_evaluate_names( ofaFormulaEngine *self, const gchar *formula );
static gboolean  evaluate_name_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self );
static gboolean  evaluate_args( ofaFormulaEngine *self, ofsFormulaHelper *helper, gint fn_count );
static gchar    *do_evaluate_nested( ofaFormulaEngine *self, const gchar *formula );
static gboolean  evaluate_nested_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self );
static gchar    *evaluate_expression( ofaFormulaEngine *self, const gchar *input );
static gchar    *eval_with_cmp_op( ofaFormulaEngine *self, const gchar *input );
static gboolean  eval_with_cmp_op_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self );
static gchar    *eval_with_oper_op( ofaFormulaEngine *self, const gchar *input );
static gchar    *apply_oper_op( ofaFormulaEngine *self, const gchar *oper, const gchar *left, const gchar *right );
static gchar    *eval_if( ofsFormulaHelper *helper );
static gboolean  eval_if_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper );
static gboolean  eval_if_true( ofaFormulaEngine *self, const gchar *left, const gchar *op, const gchar *right );
static gchar    *remove_backslashes( ofaFormulaEngine *self, const gchar *input );
static gboolean  is_a_formula( ofaFormulaEngine *self, gchar **returned );

G_DEFINE_TYPE_EXTENDED( ofaFormulaEngine, ofa_formula_engine, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaFormulaEngine ))

static void
formula_engine_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_formula_engine_finalize";
	ofaFormulaEnginePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FORMULA_ENGINE( instance ));

	/* free data members here */
	priv = ofa_formula_engine_get_instance_private( OFA_FORMULA_ENGINE( instance ));

	g_free( priv->formula );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_formula_engine_parent_class )->finalize( instance );
}

static void
formula_engine_dispose( GObject *instance )
{
	ofaFormulaEnginePrivate *priv;

	g_return_if_fail( instance && OFA_IS_FORMULA_ENGINE( instance ));

	priv = ofa_formula_engine_get_instance_private( OFA_FORMULA_ENGINE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_formula_engine_parent_class )->dispose( instance );
}

static void
ofa_formula_engine_init( ofaFormulaEngine *self )
{
	static const gchar *thisfn = "ofa_formula_engine_init";
	ofaFormulaEnginePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_FORMULA_ENGINE( self ));

	priv = ofa_formula_engine_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->thousand_sep = g_utf8_get_char( ofa_prefs_amount_thousand_sep());
	priv->decimal_sep = g_utf8_get_char( ofa_prefs_amount_decimal_sep());
	priv->digits = CUR_DEFAULT_DIGITS;
}

static void
ofa_formula_engine_class_init( ofaFormulaEngineClass *klass )
{
	static const gchar *thisfn = "ofa_formula_engine_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = formula_engine_dispose;
	G_OBJECT_CLASS( klass )->finalize = formula_engine_finalize;

	regex_allocate();
}

static void
regex_allocate( void )
{
	static const gchar *thisfn = "ofa_formula_engine_regex_allocate";
	GError *error;

	if( !st_functions_regex ){
		error = NULL;
		st_functions_regex = g_regex_new( st_functions_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_functions_regex ){
			g_warning( "%s: std_name: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_macros_regex ){
		error = NULL;
		st_macros_regex = g_regex_new( st_macros_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_macros_regex ){
			g_warning( "%s: std_name: %s", thisfn, error->message );
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

	if( !st_minusminus_regex ){
		error = NULL;
		st_minusminus_regex = g_regex_new( st_minusminus_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_minusminus_regex ){
			g_warning( "%s: minusminus: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_minusplus_regex ){
		error = NULL;
		st_minusplus_regex = g_regex_new( st_minusplus_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_minusplus_regex ){
			g_warning( "%s: minusplus: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_if_regex ){
		error = NULL;
		st_if_regex = g_regex_new( st_if_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_if_regex ){
			g_warning( "%s: if: %s", thisfn, error->message );
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
 * ofa_formula_engine_new:
 *
 * Returns: a new #ofaFormulaEngine object.
 */
ofaFormulaEngine *
ofa_formula_engine_new( void )
{
	ofaFormulaEngine *engine;

	engine = g_object_new( OFA_TYPE_FORMULA_ENGINE, NULL );

	return( engine );
}

/**
 * ofa_formula_engine_set_format:
 * @engine: this #ofaFormulaEngine instance.
 * @thousand_sep: the thousand separator.
 * @decimal_sep: the decimal separator.
 * @digits: default count of digits.
 *
 * Set the amount interpretation format.
 */
void
ofa_formula_engine_set_format( ofaFormulaEngine *engine, gunichar thousand_sep, gunichar decimal_sep, guint digits )
{
	ofaFormulaEnginePrivate *priv;

	g_return_if_fail( engine && OFA_IS_FORMULA_ENGINE( engine ));

	priv = ofa_formula_engine_get_instance_private( engine );

	g_return_if_fail( !priv->dispose_has_run );

	priv->thousand_sep = thousand_sep;
	priv->decimal_sep = decimal_sep;
	priv->digits = digits;
}

/**
 * ofa_formula_engine_eval:
 * @engine: this #ofaFormulaEngine instance.
 * @formula: the string to be evaluated.
 * @fns: the list of evaluation functions.
 * @user_data: data to be passed to the evaluation functions.
 * @msg: [allow-none][out]: messages list.
 *
 * Returns: the evaluated string.
 */
gchar *
ofa_formula_engine_eval( ofaFormulaEngine *engine, const gchar *formula, ofaFormulaFindFn finder, void *user_data, GList **msg )
{
	ofaFormulaEnginePrivate *priv;
	gchar *str, *res;
	const gchar *p;

	g_return_val_if_fail( engine && OFA_IS_FORMULA_ENGINE( engine ), NULL );

	priv = ofa_formula_engine_get_instance_private( engine );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( msg ){
		*msg = NULL;
	}

	if( !my_strlen( formula )){
		return( g_strdup( "" ));
	}

	priv->formula = g_strdup( formula );
	priv->finder = finder;
	priv->user_data = user_data;
	if( msg ){
		priv->msg = *msg;
	}

	if( !is_a_formula( engine, &str )){
		return( str );
	}

	/* p is the formula without the '=' sign
	 * re-evaluate this formula while there is something to do
	 * function arguments are evaluated if they contain operators */
	p = g_utf8_next_char( formula );
	res = do_evaluate_names( engine, p );
	str = res;
	res = do_evaluate_nested( engine, str );
	g_free( str );
	str = res;
	res = evaluate_expression( engine, str );
	g_free( str );
	str = res;
	res = remove_backslashes( engine, str );
	g_free( str );

	if( msg ){
		*msg = priv->msg;
	}

	return( res );
}

/*
 * First phase of formula evaluation.
 * We deal here with all %XXX names which may be either a macro or a
 * function, and their calls are replaced with the evaluated values.
 *
 * This evaluation is iterated until no more changes are available.
 */
static gchar *
do_evaluate_names( ofaFormulaEngine *self, const gchar *formula )
{
	static const gchar *thisfn = "ofa_formula_engine_do_evaluate_names";
	gchar *res, *str;

	str = g_strdup( formula );

	while( TRUE ){
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "%s: str='%s'", thisfn, str );
		}
		res = g_regex_replace_eval(
						st_macros_regex, str, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_name_cb, self, NULL );
		g_free( str );
		str = res;
		res = g_regex_replace_eval(
						st_functions_regex, str, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_name_cb, self, NULL );
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "%s: res='%s'", thisfn, res );
		}
		if( !my_collate( str, res )){
			break;
		}
		g_free( str );
		str = res;
	}

	g_free( str );

	return( res );
}

/*
 * This is called for each occurrence of the pattern in the string passed
 * to g_regex_replace_eval(), and it should append the replacement to result.
 *
 * 'std_name' regex defines two subpatterns, the second one being optional:
 * - index=1: the function name (without the percent sign)
 * - index=2: the arguments if any
 *
 * match='%IDEM', count=2
 *   i=1, str='IDEM'
 *
 * match='%EVAL(%D1*%TVAN)', count=3
 *   i=1, str='EVAL'
 *   i=2, str='%D1*%TVAN'
 *
 * Returns FALSE to continue the replacement process, TRUE to stop it
 */
static gboolean
evaluate_name_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_name_cb";
	ofaFormulaEnginePrivate *priv;
	gint count, fn_count;
	ofaFormulaEvalFn fn_ptr;
	gchar *match, *str, *name, *res;
	ofsFormulaHelper *helper;

	priv = ofa_formula_engine_get_instance_private( self );

	res = NULL;
	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );

	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: entering for match='%s', count=%d", thisfn, match, count );
		for( gint i=1 ; i<count ; ++i ){
			gchar *str = g_match_info_fetch( match_info, i );
			g_debug( "%s: i=%d, str='%s'", thisfn, i, str );
			g_free( str );
		}
	}

	if( count != 2 && count != 3 ){
		str = g_strdup_printf( _( "%s [error] match='%s', count=%d" ), thisfn, match, count );
		priv->msg = g_list_append( priv->msg, str );

	} else {
		name = g_match_info_fetch( match_info, 1 );
		g_return_val_if_fail( my_strlen( name ), FALSE );

		if( !my_collate( name, "IF" )){
			fn_ptr = eval_if;
			fn_count = 3;
		} else {
			fn_ptr = ( *priv->finder )( name, &fn_count, match_info, priv->user_data );
		}

		if( fn_ptr ){
			helper = g_new0( ofsFormulaHelper, 1 );
			helper->engine = self;
			helper->user_data = priv->user_data;
			helper->match_info = match_info;
			helper->match_zero = match;
			helper->match_name = name;

			if( count == 2 || evaluate_args( self, helper, fn_count )){
				res = ( *fn_ptr )( helper );
			}

			if( helper->msg ){
				priv->msg = g_list_concat( priv->msg, helper->msg );
			}
			g_free( helper );

		} else {
			str = g_strdup_printf( _( "%s [error] match='%s': unknown function name: %s" ), thisfn, match, name );
			priv->msg = g_list_append( priv->msg, str );
		}

		g_free( name );
	}

	if( !res ){
		res = g_strdup( "" );
	}
	g_string_append( result, res );
	g_free( res );

	g_free( match );

	return( FALSE );
}
/*
 * check args count
 * evaluate args
 * set:
 * - the helper args list
 * - the helper args count
 *
 * Returns: %TRUE if no error.
 */
static gboolean
evaluate_args( ofaFormulaEngine *self, ofsFormulaHelper *helper, gint fn_count )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_args";
	ofaFormulaEnginePrivate *priv;
	gboolean ok;
	gchar *args_str;
	gchar **args_array, **it;
	guint count;
	gchar *str, *str1, *str2;

	priv = ofa_formula_engine_get_instance_private( self );

	ok = TRUE;
	args_str = g_match_info_fetch( helper->match_info, 2 );
	args_array = g_strsplit( args_str, OFA_FORMULA_ARG_SEP, -1 );

	it = args_array;
	count = 0;
	while( *it ){
		count += 1;
		it++;
	}

	if( fn_count >= 0 && fn_count != count ){
		ok = FALSE;
		str = g_strdup_printf( _( "%s [error] match='%s': expected %u arguments, found %u" ),
				thisfn, helper->match_zero, fn_count, count );
		priv->msg = g_list_append( priv->msg, str );
	}

	if( ok ){
		helper->args_count = count;
		helper->args_list = NULL;
		it = args_array;

		while( *it ){
			str1 = do_evaluate_names( self, *it );
			str2 = evaluate_expression( self, str1 );
			g_free( str1 );
			helper->args_list = g_list_append( helper->args_list, str2 );
			it++;
		}
	}

	g_strfreev( args_array );
	g_free( args_str );

	return( ok );
}

/*
 * Second phase of formula evaluation.
 * After having substituted all macros and functions, deals here with
 * parentheses which modify the operators precedence.
 *
 * This evaluation is iterated until no more changes are available.
 */
static gchar *
do_evaluate_nested( ofaFormulaEngine *self, const gchar *formula )
{
	static const gchar *thisfn = "ofa_formula_engine_do_evaluate_nested";
	gchar *res, *str;

	str = g_strdup( formula );

	while( TRUE ){
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "%s: str='%s'", thisfn, str );
		}
		res = g_regex_replace_eval(
						st_nested_regex, formula, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_nested_cb, self, NULL );
		if( DEBUG_REGEX_EVALUATION ){
			g_debug( "%s: res='%s'", thisfn, res );
		}
		if( !my_collate( str, res )){
			break;
		}
		g_free( str );
		str = res;
	}

	g_free( str );

	return( res );
}

static gboolean
evaluate_nested_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self )
{
	static const gchar *thisfn = "ofa_evaluate_nested_cb";
	gchar *match, *res;
	gint count;

	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );
	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: entering for match='%s', count=%d", thisfn, match, count );
	}

	g_free( match );
	match = g_match_info_fetch( match_info, 1 );

	res = evaluate_expression( self, match );
	result = g_string_append( result, res );

	g_free( res );
	g_free( match );

	/* Returns FALSE to continue the replacement process, TRUE to stop it */
	return( FALSE );
}

/*
 * evaluate a string which may (or not) contain simple operations
 * The string may be a %IF condition, so extract the operators
 */
static gchar *
evaluate_expression( ofaFormulaEngine *self, const gchar *input )
{
	gchar *res, *tmp;

	res = g_strdup( input );

	/* resolve comparison operator
	 */
	if( g_regex_match( st_if_regex, res, 0, NULL )){
		tmp = eval_with_cmp_op( self, res );
		g_free( res );
		res = tmp;

	/* or resolve operation operator
	 */
	} else if( g_regex_match( st_operator_regex, res, 0, NULL )){
		g_debug( "match operator: %s", input );

		/* remove double-minus */
		if( g_regex_match( st_minusminus_regex, res, 0, NULL )){
			tmp = g_regex_replace_literal( st_minusminus_regex, res, -1, 0, "", 0, NULL );
			g_free( res );
			res = tmp;
		}

		/* replace -+ by - */
		if( g_regex_match( st_minusplus_regex, res, 0, NULL )){
			tmp = g_regex_replace_literal( st_minusplus_regex, res, -1, 0, "-", 0, NULL );
			g_free( res );
			res = tmp;
		}

		tmp = eval_with_oper_op( self, res );
		g_free( res );
		res = tmp;
	}

	return( res );
}

/*
 * Enter here because the @input expression contains a comparison
 *  operator: evaluate the two members independantly
 *  e.g.: " 133+24 > 54-67*2" returns "157 > -80"
 */
static gchar *
eval_with_cmp_op( ofaFormulaEngine *self, const gchar *input )
{
	gchar *res;

	res = g_regex_replace_eval(
					st_if_regex, input, -1, 0, 0,
					( GRegexEvalCallback ) eval_with_cmp_op_cb, self, NULL );

	return( res );
}

/*
 * parse the condition
 * expects <something> <op> <something>
 * returns: "1" if true, "0" if false.
 */
static gboolean
eval_with_cmp_op_cb( const GMatchInfo *match_info, GString *result, ofaFormulaEngine *self )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_with_cmp_op_cb";
	ofaFormulaEnginePrivate *priv;
	guint count;
	gchar *match, *str, *res, *left, *op, *right;
	gchar *leftres, *rightres;

	priv = ofa_formula_engine_get_instance_private( self );

	res = g_strdup( "0" );
	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );

	if( count != 4 ){
		str = g_strdup_printf( _( "%s [error] match='%s', count=%d" ), thisfn, match, count );
		priv->msg = g_list_append( priv->msg, str );

	} else {
		leftres = NULL;
		rightres = NULL;
		left = g_match_info_fetch( match_info, 1 );
		op = g_match_info_fetch( match_info, 2 );
		right = g_match_info_fetch( match_info, 3 );

		if( !my_strlen( left ) || !my_strlen( op ) || !my_strlen( right )){
			str = g_strdup_printf( _( "%s [error] left='%s', op='%s', right='%s'" ), thisfn, left, op, right );
			priv->msg = g_list_append( priv->msg, str );

		} else {
			leftres = evaluate_expression( self, left );
			rightres = evaluate_expression( self, right );
			res = g_strdup_printf( "%s%s%s", leftres, op, rightres );
		}

		g_free( right );
		g_free( op );
		g_free( left );
		g_free( rightres );
		g_free( leftres );
	}

	g_string_append( result, res );
	g_free( res );
	g_free( match );

	return( FALSE );
}

static gchar *
eval_with_oper_op( ofaFormulaEngine *self, const gchar *input )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_with_oper_op";
	const gchar *begin, *p;
	gchar *str, *res;
	gunichar ch, prev_ch;
	GList *list0, *it, *itnext, *itprev;
	const gchar *left, *right;

	/* extract all the arguments */
	list0 = NULL;
	prev_ch = '\0';
	begin = input;
	p = begin;
	while( p ){
		ch = g_utf8_get_char( p );
		if( ch == '\0' ){
			str = ( p == begin ) ? g_strdup( "" ) : g_strstrip( g_strndup( begin, p-begin ));
			list0 = g_list_append( list0, str );
			break;

		} else if(( ch == '+' || ch == '-' || ch == '/' || ch == '*' ) && prev_ch != '\\' ){
			str = ( p == begin ) ? g_strdup( "" ) : g_strstrip( g_strndup( begin, p-begin ));
			list0 = g_list_append( list0, str );
			str = g_strdup_printf( "%c", ch );
			list0 = g_list_append( list0, str );
			p = g_utf8_next_char( p );
			begin = p;

		} else {
			p = g_utf8_next_char( p );
		}
		prev_ch = ch;
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
			res = apply_oper_op( self, it->data, left, right );
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
			res = apply_oper_op( self, it->data, left, right );
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
apply_oper_op( ofaFormulaEngine *self, const gchar *oper, const gchar *left, const gchar *right )
{
	ofaFormulaEnginePrivate *priv;
	gchar *res;
	gdouble a, b, c;

	priv = ofa_formula_engine_get_instance_private( self );

	res = NULL;
	a = my_double_set_from_str( left ? left : "0", priv->thousand_sep, priv->decimal_sep );
	b = my_double_set_from_str( right ? right : "0", priv->thousand_sep, priv->decimal_sep );
	c = 0;

	if( !my_collate( oper, "+" )){
		c = a + b;
	} else if( !my_collate( oper, "-" )){
		c = a - b;
	} else if( !my_collate( oper, "/" ) && b != 0 ){
		c = a / b;
	} else if( !my_collate( oper, "*" )){
		c = a * b;
	}

	res = my_double_to_str( c, priv->thousand_sep, priv->decimal_sep, priv->digits );

	return( res );
}

/*
 * A standard function '%IF(...) embedded in the formula engine
 * Syntax: =%IF( <condition> ; <result_if_true> ; <result_if_false> )
 */
static gchar *
eval_if( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_if";
	gchar *res;
	const gchar *condition, *iftrue, *iffalse;
	GList *it;
	gboolean bres;

	it = helper->args_list;
	condition = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	iftrue = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	iffalse = it ? ( const gchar * ) it->data : NULL;

	res = g_regex_replace_eval(
					st_if_regex, condition, -1, 0, 0,
					( GRegexEvalCallback ) eval_if_cb, helper, NULL );

	bres = my_utils_boolean_from_str( res );
	g_free( res );
	res = g_strdup( bres ? iftrue : iffalse );

	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: condition=%s, res=%s", thisfn, condition, res );
	}

	return( res );
}

/*
 * parse the condition
 * expects <something> <op> <something>
 * returns: "1" if true, "0" if false.
 */
static gboolean
eval_if_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_if_cb";
	guint count;
	gchar *match, *str, *res, *left, *op, *right;

	res = g_strdup( "0" );
	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );

	if( count != 4 ){
		str = g_strdup_printf( _( "%s [error] match='%s', count=%d" ), thisfn, match, count );
		helper->msg = g_list_append( helper->msg, str );

	} else {
		left = g_match_info_fetch( match_info, 1 );
		op = g_match_info_fetch( match_info, 2 );
		right = g_match_info_fetch( match_info, 3 );
		if( !my_strlen( left ) || !my_strlen( op ) || !my_strlen( right )){
			str = g_strdup_printf( _( "%s [error] left='%s', op='%s', right='%s'" ), thisfn, left, op, right );
			helper->msg = g_list_append( helper->msg, str );

		} else {
			if( DEBUG_REGEX_EVALUATION ){
				g_debug( "%s left='%s', op='%s', right='%s'", thisfn, left, op, right );
			}
			if( eval_if_true( helper->engine, left, op, right )){
				g_free( res );
				res = g_strdup( "1" );
			}
		}

		g_free( right );
		g_free( op );
		g_free( left );
	}

	g_string_append( result, res );
	g_free( res );
	g_free( match );

	return( FALSE );
}

static gboolean
eval_if_true( ofaFormulaEngine *self, const gchar *left, const gchar *op, const gchar *right )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_if_true";
	ofaFormulaEnginePrivate *priv;
	gboolean is_true;
	gdouble a, b;

	priv = ofa_formula_engine_get_instance_private( self );

	a = my_double_set_from_str( left, priv->thousand_sep, priv->decimal_sep );
	b = my_double_set_from_str( right, priv->thousand_sep, priv->decimal_sep );

	is_true = TRUE;

	if( !my_collate( op, "<>" )){
		is_true = ( a != b );
	} else if( !my_collate( op, "<" )){
		is_true = ( a < b );
	} else if( !my_collate( op, "<=" )){
		is_true = ( a <= b );
	} else if( !my_collate( op, ">" )){
		is_true = ( a > b );
	} else if( !my_collate( op, ">=" )){
		is_true = ( a >= b );
	} else if( !my_collate( op, "=" )){
		is_true = ( a == b );
	}
	if( g_strrstr( op, "!" )){
		is_true = !is_true;
	}

	if( DEBUG_REGEX_EVALUATION ){
		g_debug( "%s: left=%s, op=%s, right=%s, is_true=%s", thisfn, left, op, right, is_true ? "True":"False" );
	}

	return( is_true );
}

static gchar *
remove_backslashes( ofaFormulaEngine *self, const gchar *input )
{
	gchar *res;

	res = g_regex_replace( st_backslashed_regex, input, -1, 0, "\\1", 0, NULL );

	return( res );
}

/*
 * returns %TRUE if the provided formula is actually a to-be-evaluated
 * formula
 */
static gboolean
is_a_formula( ofaFormulaEngine *self, gchar **returned )
{
	ofaFormulaEnginePrivate *priv;
	const gchar *p;

	priv = ofa_formula_engine_get_instance_private( self );

	*returned = NULL;

	/* cannot be evaluated if not UTF-8 valide */
	if( !g_utf8_validate( priv->formula, -1, NULL )){
		*returned = g_strdup( priv->formula );
		return( FALSE );
	}

	/* remove the leading quote if this is not a formula */
	if( g_str_has_prefix( priv->formula, "'=" )){
		p = g_utf8_next_char( priv->formula );
		p = g_utf8_next_char( p );
		*returned = g_strdup( p );
		return( FALSE );
	}

	/* returns the same if not a formula */
	if( !g_str_has_prefix( priv->formula, "=" )){
		*returned = g_strdup( priv->formula );
		return( FALSE );
	}

	/* it is a valid utf-8 string
	 * + which begins with an equal '=' sign
	 * this is so a formula to be evaluated
	 */
	return( TRUE );
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

static ofaFormulaEvalFn get_formula_eval_fn( const gchar *fname, gint *count, GMatchInfo *match_info, void *user_data );

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

/* sEvalDef:
 * @name: the name of the function.
 * @args_count: the expected count of arguments, -1 for do not check.
 * @eval: the evaluation function which provides the replacement string.
 *
 * Defines the evaluation callback functions.
 */
typedef struct {
	const gchar *name;
	gint         args_count;
	gchar *   ( *eval )( ofsFormulaHelper * );
}
	sEvalDef;

static const sEvalDef st_formula_fns[] = {
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
	ofaFormulaEngine *engine;
	guint i;
	GList *msg;
	gchar *result;

	engine = ofa_formula_engine_new();

	for( i=0 ; st_formulas[i] ; ++i ){
		g_debug( "%s: formula='%s'", thisfn, st_formulas[i] );
		result = ofa_formula_engine_eval( engine, st_formulas[i], ( ofaFormulaFindFn ) get_formula_eval_fn, NULL, &msg );
		g_debug( "%s: result='%s'", thisfn, result );
		g_free( result );
	}

	g_object_unref( engine );
}

/*
 * this is a ofaFormula callback
 * Returns: the evaluation function for the name + expected args count
 */
static ofaFormulaEvalFn
get_formula_eval_fn( const gchar *fname, gint *count, GMatchInfo *match_info, void *user_data )
{
	gint i;

	*count = -1;

	for( i=0 ; st_formula_fns[i].name ; ++i ){
		if( !my_collate( st_formula_fns[i].name, fname )){
			*count = st_formula_fns[i].args_count;
			return(( ofaFormulaEvalFn ) st_formula_fns[i].eval );
		}
	}

	return( NULL );
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
