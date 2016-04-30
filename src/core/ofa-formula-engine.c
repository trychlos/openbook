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

#include "api/ofa-amount.h"
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

	/* arithmetic evaluation mode
	 */
	gboolean         auto_eval;

	/* the first recursive helper
	 */
	ofsFormulaHelper helper;
}
	ofaFormulaEnginePrivate;

#define DEBUG_FORMULA_EVAL                          1
#define DEBUG_EVAL_A_EXPRESSION                     0
#define DEBUG_EVAL_NAMES                            0
#define DEBUG_EVAL_NAME_CB                          0
#define DEBUG_EVAL_NAME_ARGS                        0
#define DEBUG_EVAL_NESTED                           0
#define DEBUG_EVAL_NESTED_CB                        0
#define DEBUG_EVAL_ARITHMETIC                       0
#define DEBUG_SPLIT_ARITHMETIC_EXPR                 0
#define DEBUG_EVAL_ARITHMETIC_OPS                   0
#define DEBUG_EVAL_IF                               0
#define DEBUG_PARSE_EXPR_REGEX                      0

/* A macro name or function name + args
 * Both regular expressions define one first subpattern as macro or
 *  function name (at index 1).
< * Function regular expression defines one more subpattern which is the
 * arguments list (at index 2). More, it makes sure there is no other
 * function call inside the arguments list.
 *
 * See: evaluate_names().
 */
static const gchar *st_functions_def               = "(?<!\\\\)%([a-zA-Z][a-zA-Z0-9_]*)\\(\\s*([^()]+)\\s*\\)";
static GRegex      *st_functions_regex             = NULL;

static const gchar *st_macros_def                  = "(?<!\\\\)%(\\w+)\\b(?!\\()";
static GRegex      *st_macros_regex                = NULL;

/* when we have replaced all macros and functions by their values, we
 * deal with nested parenthesis which may override the operator
 * precedences
 *
 * See: evaluate_nested().
 */
static const gchar *st_nested_def                  = "\\s*\\(\\s*([^()]+)\\s*\\)\\s*";
static GRegex      *st_nested_regex                = NULL;

static const gchar *st_fname_begin_def             = "%([a-zA-Z][a-zA-Z0-9_]*)\\(";
static GRegex      *st_fname_begin_regex           = NULL;

/* englobing parentheses
 */
static const gchar *st_englobing_def               = "^\\s*\\((.*)\\)\\s*$";
static GRegex      *st_englobing_regex             = NULL;

/* the operator regex
 */
static const gchar *st_arithmetic_op_def           = "\\s*(?<!\\\\)([+-/*])\\s*";
static GRegex      *st_arithmetic_op_regex         = NULL;

static const gchar *st_spaces_def                  = "\\s+";
static GRegex      *st_spaces_regex                = NULL;

static const gchar *st_minusminus_def              = "--";
static GRegex      *st_minusminus_regex            = NULL;

static const gchar *st_minusplus_def               = "-\\+|\\+-";
static GRegex      *st_minusplus_regex             = NULL;

/* the %EVAL(...) function
 * It is hardcoded in this formula engine, and does not have to be
 * provided by the caller.
 */
static const gchar *st_eval_begin_def              = "(?<!\\\\)%EVAL\\(";
static GRegex      *st_eval_begin_regex            = NULL;

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

/* a data structure used when evaluating nested parentheses
 */
typedef struct {
	GList            *eval_pos;
	GList            *fn_pos;
	ofsFormulaHelper *helper;
	gboolean          prev_arithmetics;
}
	sNested;

/* start and end position of a non-backslashed '%EVAL(' function call
 * end position is the position of the last closing parenthese in the
 * case of nested parentheses
 */
typedef struct {
	guint start_pos;
	guint end_pos;
}
	sStartEnd;

static void      regex_allocate( void );
static gboolean  check_for_formula( const gchar *formula, gchar **returned );
static gchar    *remove_backslashes( ofaFormulaEngine *self, const gchar *input );
static gchar    *do_evaluate_a_expression( ofsFormulaHelper *helper, const gchar *expression );
static gboolean  does_name_match( ofsFormulaHelper *helper, const gchar *expression );
static gchar    *evaluate_names( ofsFormulaHelper *helper, const gchar *expression );
static gboolean  evaluate_name_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper );
static gboolean  evaluate_name_args( ofsFormulaHelper *helper, gint fn_min_count, gint fn_max_count );
static gboolean  check_for_eval( ofsFormulaHelper *helper, const gchar *expression );
static GList    *parse_expression_for_regex( ofsFormulaHelper *helper, const gchar *expression, GRegex *regex );
static gboolean  does_nested_match( ofsFormulaHelper *helper, const gchar *expression );
static gchar    *evaluate_nested( ofsFormulaHelper *helper, const gchar *expression );
static gboolean  evaluate_nested_cb( const GMatchInfo *match_info, GString *result, sNested *nested );
static gboolean  is_opening_function_pos( gint start_pos, GList *fn_pos );
static gboolean  is_inside_eval_pos( gint start_pos, gint end_pos, GList *eval_pos );
static gboolean  does_arithmetic_op_match( ofsFormulaHelper *helper, const gchar *expression );
static gchar    *evaluate_arithmetic( ofsFormulaHelper *helper, const gchar *expression );
static GList    *split_a_expression( ofsFormulaHelper *helper, const gchar *expression );
static gboolean  is_opening_parenthese( gunichar ch );
static gboolean  is_closing_parenthese( gunichar ch );
static gboolean  is_arithmetic_op( gunichar ch );
static GList    *append_argument( GList *args, const gchar *pbegin, const gchar *p );
static gchar    *apply_arithmetic_op( ofsFormulaHelper *helper, const gchar *oper, const gchar *left, const gchar *right );
static gchar    *eval_eval( ofsFormulaHelper *helper );
static gchar    *eval_if( ofsFormulaHelper *helper );
static gboolean  eval_if_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper );
static gboolean  eval_if_true( ofaFormulaEngine *self, const gchar *left, const gchar *op, const gchar *right );
static void      eval_pos_free( sStartEnd *pos );

G_DEFINE_TYPE_EXTENDED( ofaFormulaEngine, ofa_formula_engine, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaFormulaEngine ))

static void
formula_engine_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_formula_engine_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FORMULA_ENGINE( instance ));

	/* free data members here */

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

	priv->auto_eval = TRUE;
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

	if( !st_fname_begin_regex ){
		error = NULL;
		st_fname_begin_regex = g_regex_new( st_fname_begin_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_fname_begin_regex ){
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

	if( !st_englobing_regex ){
		error = NULL;
		st_englobing_regex = g_regex_new( st_englobing_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_englobing_regex ){
			g_warning( "%s: englobing: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_arithmetic_op_regex ){
		error = NULL;
		st_arithmetic_op_regex = g_regex_new( st_arithmetic_op_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_arithmetic_op_regex ){
			g_warning( "%s: operator: %s", thisfn, error->message );
			g_error_free( error );
		}
	}

	if( !st_spaces_regex ){
		error = NULL;
		st_spaces_regex = g_regex_new( st_spaces_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_spaces_regex ){
			g_warning( "%s: spaces: %s", thisfn, error->message );
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

	if( !st_eval_begin_regex ){
		error = NULL;
		st_eval_begin_regex = g_regex_new( st_eval_begin_def, G_REGEX_EXTENDED, 0, &error );
		if( !st_eval_begin_regex ){
			g_warning( "%s: if: %s", thisfn, error->message );
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
 * ofa_formula_engine_set_auto_eval:
 * @engine: this #ofaFormulaEngine instance.
 * @auto_eval: whether the expressions must be automatically evaluated.
 *
 * Set the arithmetic evaluation mode.
 */
void
ofa_formula_engine_set_auto_eval( ofaFormulaEngine *engine, gboolean auto_eval )
{
	static const gchar *thisfn = "ofa_formula_engine_set_auto_eval";
	ofaFormulaEnginePrivate *priv;

	g_return_if_fail( engine && OFA_IS_FORMULA_ENGINE( engine ));

	priv = ofa_formula_engine_get_instance_private( engine );

	g_return_if_fail( !priv->dispose_has_run );

	g_debug( "%s: set auto_eval to %s", thisfn, auto_eval ? "True":"False" );
	priv->auto_eval = auto_eval;
}

/**
 * ofa_formula_engine_set_amount_format:
 * @engine: this #ofaFormulaEngine instance.
 * @thousand_sep: the thousand separator.
 * @decimal_sep: the decimal separator.
 * @digits: default count of digits.
 *
 * Set the amount interpretation format.
 */
void
ofa_formula_engine_set_amount_format( ofaFormulaEngine *engine, gunichar thousand_sep, gunichar decimal_sep, guint digits )
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
 *
 * ofa_formula_engine_eval( formula )
 * where 'formula' is anything - returned unchanged if not a formula
 *  |
 *  +-> do_evaluate_a_expression( expr )
 *  |   where 'expr' is any arithmetic expression (BNF 'a_expression')
 *  |    |
 *  |    +-> if macro or function name(s) are detected
 *  |    |    |
 *  |    |    +-> evaluate_names
 *  |    |        foreach found name, do
 *  |    |         |
 *  |    |         +-> evaluate_name_cb
 *  |    |         |   + extract arguments of the function
 *  |    |         |     foreach function argument, do
 *  |    |         |      |
 *  |    |         |      +-> evaluate_name_args
 *  |    |         |          where argument is either a 'a_expression' or a 'c_expression'
 *  |    |         |           |
 *  |    |         |           +-> do_evaluate_a_expression( arg )
 *  |    |         |
 *  |    |         +-> execute named macro or function, returning a result
 *  |    |             this result comes in replacement of the detected name
 *  |    |
 *  |    +-> if arithmetic nested expression is detected
 *  |    |    |
 *  |    |    +-> evaluate_most_intern_expression
 *  |    |         |
 *  |    |         +-> do_evaluate_a_expression( expr )
 *  |    |
 *  |    +-> if arithmetic operator(s) are detected
 *  |         |
 *  |         +-> list = split_a_expression( expr )
 *  |             split to operands and arithmetic operators (maybe with only one operand)
 *  |             with operand is a BNF 'content'
 *  |             foreach( operand ), do
 *  |              |
 *  |              +-> res = eval_arithmetic_ops
 *  |
 *  +-> remove_backslashes
 */
gchar *
ofa_formula_engine_eval( ofaFormulaEngine *engine, const gchar *formula, ofaFormulaFindFn finder, void *user_data, GList **msg )
{
	static const gchar *thisfn = "ofa_formula_engine_eval";
	ofaFormulaEnginePrivate *priv;
	gchar *str, *res;
	const gchar *p;

	g_debug( "%s: engine=%p, formula='%s', finder=%p, user_data=%p, msg=%p",
			thisfn, ( void * ) engine, formula, ( void * ) finder, user_data, ( void * ) msg );

	g_return_val_if_fail( engine && OFA_IS_FORMULA_ENGINE( engine ), NULL );

	priv = ofa_formula_engine_get_instance_private( engine );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( msg ){
		*msg = NULL;
	}

	/* str is resturned as a newly allocated copy of formula */
	if( !check_for_formula( formula, &str )){
		return( str );
	}

	priv->helper.engine = engine;
	priv->helper.finder = finder;
	priv->helper.user_data = user_data;
	priv->helper.msg = NULL;
	priv->helper.eval_arithmetics = priv->auto_eval;

	/* p is the formula without the '=' sign
	 * re-evaluate this formula while there is something to do
	 * function arguments may be evaluated if they contain operators */
	g_return_val_if_fail( g_utf8_get_char( str ) == '=', NULL );
	p = g_utf8_next_char( str );
	res = do_evaluate_a_expression( &priv->helper, p );
	g_free( str );

	str = res;
	res = remove_backslashes( engine, str );
	g_free( str );

	if( msg ){
		*msg = priv->helper.msg;
	}

	if( DEBUG_FORMULA_EVAL ){
		g_debug( "%s: formula='%s' -> res='%s'", thisfn, formula, res );
	}

	return( res );
}

/*
 * returns %TRUE if the provided formula is actually a to-be-evaluated
 * formula
 *
 * @returned: [out]: is set either to the string to be returned, or the
 * stripped formula
 */
static gboolean
check_for_formula( const gchar *formula, gchar **returned )
{
	gboolean is_formula;
	gchar *p, *formula_str;

	/* if empty, return empty */
	if( !my_strlen( formula )){
		*returned = g_strdup( "" );
		is_formula = FALSE;

	/* cannot be evaluated if not UTF-8 valid */
	} else if( !g_utf8_validate( formula, -1, NULL )){
		*returned = g_strdup( formula );
		is_formula = FALSE;

	} else {
		formula_str = g_strstrip( g_strdup( formula ));

		/* remove the leading quote if this is not a formula */
		if( g_str_has_prefix( formula_str, "'=" )){
			p = g_utf8_next_char( formula_str );
			*returned = g_strdup( p );
			g_free( formula_str );
			is_formula = FALSE;

		/* it is a valid utf-8 string
		 * + first non-space character is an equal sign '='
		 * => then this is so a formula to be evaluated */
		} else {
			*returned = formula_str;
			is_formula = g_str_has_prefix( formula_str, "=" );
		}
	}

	return( is_formula );
}

static gchar *
remove_backslashes( ofaFormulaEngine *self, const gchar *input )
{
	gchar *res;

	res = g_regex_replace( st_backslashed_regex, input, -1, 0, "\\1", 0, NULL );

	return( res );
}

/*
 * @expression: an arithmetic expression.
 * a_expression ::= [ "(" ] content [ AOP content [ ... ]] [ ")" ]
 *
 * Evaluates an arithmetic expression:
 *
 * Result depends on whether auto evaluation mode is TRUE, or we are
 * inside of a %EVAL() function.
 *
 * In both cases, a comparison expression (BNF 'c_expression') is
 * returned unchanged.
 */
static gchar *
do_evaluate_a_expression( ofsFormulaHelper *helper, const gchar *expression )
{
	static const gchar *thisfn = "ofa_formula_engine_do_evaluate_a_expression";
	gchar *res, *str;

	if( DEBUG_EVAL_A_EXPRESSION ){
		g_debug( "%s: expression='%s', eval_arithmetics=%s",
				thisfn, expression, helper->eval_arithmetics ? "True":"False" );
	}

	res = g_strdup( expression );

	if( does_name_match( helper, res )){
		str = res;
		res = evaluate_names( helper, str );
		g_free( str );
	}

	/* if arithmetic evaluation is ON, then convert the (maybe)
	 *  arithmetic expression into a list of (content [, op [, content [...]]] )
	 * else this is just a suite of macro/functions to be evaluated and
	 *  the returned list only contains one element
	 */
	if( helper->eval_arithmetics || check_for_eval( helper, res )){
		if( does_nested_match( helper, res )){
			str = res;
			res = evaluate_nested( helper, str );
			g_free( str );
		}
	}

	/* nested parentheses and %EVAL() are expected to have been evaluated
	 */
	if( helper->eval_arithmetics ){
		if( does_arithmetic_op_match( helper, res )){
			str = res;
			res = evaluate_arithmetic( helper, str );
			g_free( str );
		}
	}

	if( DEBUG_EVAL_A_EXPRESSION ){
		g_debug( "%s: expression='%s' -> res='%s'", thisfn, expression, res );
	}

	return( res );
}

/*
 * Whether the expression may match a name ?
 */
static gboolean
does_name_match( ofsFormulaHelper *helper, const gchar *expression )
{
	return( g_regex_match( st_functions_regex, expression, 0, NULL ) ||
				g_regex_match( st_macros_regex, expression, 0, NULL ));
}

/*
 * @expression: a member of an arithmetic/comparison expression.
 * content ::= %MACRO | %FN( arg1 [ ; arg2 [ ... ]] )
 *
 * Evaluates the macro or function names.
 */
static gchar *
evaluate_names( ofsFormulaHelper *helper, const gchar *expression )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_names";
	gchar *begin, *res, *str;

	if( DEBUG_EVAL_NAMES ){
		g_debug( "%s: expression='%s'", thisfn, expression );
	}

	begin = NULL;
	res = g_strdup( expression );

	while( does_name_match( helper, res )){
		g_free( begin );
		begin = g_strdup( res );

		/* evaluate macros */
		str = res;
		res = g_regex_replace_eval(
						st_macros_regex, str, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_name_cb, helper, NULL );
		g_free( str );

		/* evaluate functions */
		str = res;
		res = g_regex_replace_eval(
						st_functions_regex, str, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_name_cb, helper, NULL );
		g_free( str );

		/* if no more macro/function substitution, then break the loop */
		if( !my_collate( res, begin )){
			break;
		}
	}

	g_free( begin );

	if( DEBUG_EVAL_NAMES ){
		g_debug( "%s: expression='%s' -> res='%s'", thisfn, expression, res );
	}

	return( res );
}

/*
 * This is called for each occurrence of the pattern in the string passed
 * to g_regex_replace_eval(), and it should append the replacement to result.
 *
 * This callback is used with two patters: macro and function.
 *
 * Macro pattern does not define any more subpattern than the macro name
 * itself at index 1 (so count=2).
 *
 * Function pattern defines two subpatterns: the function name itself and
 * its arguments list, the second one being optional.
 *
 * Examples:
 * - match='%IDEM'
 *   count=2
 *     i=1, str='IDEM'
 *
 * - match='%EVAL(%D1*%TVAN)'
 *   count=3
 *     i=1, str='EVAL'
 *     i=2, str='%D1*%TVAN'
 *
 * Note: the two patterns make sure that there is no other macro nor
 * function embedded inside of the arguments list.
 *
 * Returns FALSE to continue the replacement process, TRUE to stop it
 */
static gboolean
evaluate_name_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_name_cb";
	gint count, fn_min_count, fn_max_count;
	ofaFormulaEvalFn fn_ptr;
	gchar *match, *str, *name, *res;
	ofsFormulaHelper *helper_cb;

	res = NULL;
	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );

	if( DEBUG_EVAL_NAME_CB ){
		g_debug( "%s: entering for match='%s', count=%d", thisfn, match, count );
		for( gint i=1 ; i<count ; ++i ){
			gchar *str = g_match_info_fetch( match_info, i );
			g_debug( "%s: i=%d, str='%s'", thisfn, i, str );
			g_free( str );
		}
	}

	if( count != 2 && count != 3 ){
		str = g_strdup_printf( _( "%s [error] match='%s', count=%d" ), thisfn, match, count );
		helper->msg = g_list_append( helper->msg, str );

	} else {
		name = g_match_info_fetch( match_info, 1 );
		g_return_val_if_fail( my_strlen( name ), FALSE );

		if( !my_collate( name, "EVAL" )){
			fn_ptr = eval_eval;
			fn_min_count = 1;
			fn_max_count = 1;

		} else if( !my_collate( name, "IF" )){
			fn_ptr = eval_if;
			fn_min_count = 3;
			fn_max_count = 3;

		} else {
			fn_ptr = ( *helper->finder )( name, &fn_min_count, &fn_max_count, match_info, helper->user_data );
		}

		if( fn_ptr ){
			helper_cb = g_new0( ofsFormulaHelper, 1 );
			helper_cb->engine = helper->engine;
			helper_cb->finder = helper->finder;
			helper_cb->user_data = helper->user_data;
			helper_cb->msg = NULL;
			helper_cb->eval_arithmetics = helper->eval_arithmetics;
			helper_cb->match_info = match_info;
			helper_cb->match_zero = match;
			helper_cb->match_name = name;

			if( count == 2 || evaluate_name_args( helper_cb, fn_min_count, fn_max_count )){
				res = ( *fn_ptr )( helper_cb );
			}

			if( helper_cb->msg ){
				helper->msg = g_list_concat( helper->msg, helper_cb->msg );
			}
			g_free( helper_cb );

		} else {
			str = g_strdup_printf( _( "%s [error] match='%s': unknown function name: %s" ), thisfn, match, name );
			helper->msg = g_list_append( helper->msg, str );
		}

		g_free( name );
	}

	if( !res ){
		res = g_strdup( "" );
	}

	if( DEBUG_EVAL_NAME_CB ){
		g_debug( "%s: match='%s' -> res='%s'", thisfn, match, res );
	}

	g_string_append( result, res );
	g_free( res );

	g_free( match );

	/* continue regex replacement */
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
evaluate_name_args( ofsFormulaHelper *helper, gint fn_min_count, gint fn_max_count )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_name_args";
	gboolean ok;
	gchar *args_str;
	gchar **args_array, **it;
	guint count;
	gchar *str;

	ok = TRUE;
	args_str = g_match_info_fetch( helper->match_info, 2 );
	args_array = g_strsplit( args_str, OFA_FORMULA_ARG_SEP, -1 );

	/* get the actual count of arguments */
	it = args_array;
	count = 0;
	while( *it ){
		count += 1;
		it++;
	}

	if( count < fn_min_count || ( count > fn_max_count && fn_max_count != -1 )){
		ok = FALSE;
		str = g_strdup_printf( _( "%s [error] match='%s': expected %u-%u arguments, found %u" ),
				thisfn, helper->match_zero, fn_min_count, fn_max_count, count );
		helper->msg = g_list_append( helper->msg, str );
	}

	if( ok ){
		helper->args_count = count;
		helper->args_list = NULL;
		it = args_array;

		while( *it ){
			str = g_strstrip( do_evaluate_a_expression( helper, *it ));
			helper->args_list = g_list_append( helper->args_list, str );
			it++;
		}
	}

	g_strfreev( args_array );
	g_free( args_str );

	return( ok );
}

/*
 * This function is called when about to try to evaluate nested
 * parentheses and auto_eval is False.
 * The regex is non-backslashed '%EVAL('.
 */
static gboolean
check_for_eval( ofsFormulaHelper *helper, const gchar *expression )
{
	GList *eval_pos;
	gboolean has_eval;

	eval_pos = parse_expression_for_regex( helper, expression, st_eval_begin_regex );
	has_eval = ( g_list_length( eval_pos ) > 0 );
	g_list_free_full( eval_pos, ( GDestroyNotify ) eval_pos_free );

	return( has_eval );
}

/*
 * @regex: the regex to match
 * @list: the list to setup
 *
 * Parse the expression to detect the provided @regex.
 * Setup the provider @list.
 *
 * Returns: the #GList of start/end positions.
 */
static GList *
parse_expression_for_regex( ofsFormulaHelper *helper, const gchar *expression, GRegex *regex )
{
	static const gchar *thisfn = "ofa_formula_engine_parse_expression_for_regex";
	GList *list;
	GMatchInfo *match_info;
	sStartEnd *pos;
	gint start_pos, end_pos;
	guint opened_par;
	gunichar ch;
	const gchar *p;

	list = NULL;

	if( g_regex_match( regex, expression, 0, &match_info )){
		while( TRUE ){
			g_match_info_fetch_pos( match_info, 0, &start_pos, &end_pos );
			pos = g_new0( sStartEnd, 1 );
			pos->start_pos = end_pos-1;		/* position of the opening parenthese */
			opened_par = 1;
			p = expression + end_pos;
			while( opened_par > 0 ){
				p = g_utf8_next_char( p );
				ch = g_utf8_get_char( p );
				if( ch == '\0' ){
					break;
				}
				if( is_opening_parenthese( ch )){
					opened_par += 1;
				} else if( is_closing_parenthese( ch )){
					opened_par -= 1;
				}
			}
			pos->end_pos = p - expression;
			if( DEBUG_PARSE_EXPR_REGEX ){
				g_debug( "%s: found start_pos=%u, end_pos=%u", thisfn, pos->start_pos, pos->end_pos );
			}
			list = g_list_prepend( list, pos );
			if( !g_match_info_next( match_info, NULL )){
				break;
			}
		}
	}

	return( g_list_reverse( list ));
}

/*
 * Whether the expression may match a nested arithmetic expression ?
 * This function is called when auto_eval is True, or we have detected
 * some %EVAL() functions in the expression
 */
static gboolean
does_nested_match( ofsFormulaHelper *helper, const gchar *expression )
{
	return( g_regex_match( st_nested_regex, expression, 0, NULL ));
}

/*
 * After having substituted macros and functions, deals here with
 * parentheses which modify the operators precedence.
 *
 * This evaluation is iterated until no more changes are available.
 *
 * Nested parentheses are evaluated either because auto_eval is True,
 * or because they are inside of a %EVAL() function.
 */
static gchar *
evaluate_nested( ofsFormulaHelper *helper, const gchar *expression )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_nested";
	gchar *res, *str;
	sNested nested;
	gboolean unchanged;

	res = g_strdup( expression );

	while( g_regex_match( st_nested_regex, res, 0, NULL )){
		if( DEBUG_EVAL_NESTED ){
			g_debug( "%s: res='%s'", thisfn, res );
		}

		if( DEBUG_PARSE_EXPR_REGEX ){
			g_debug( "%s: evaluating %%EVAL() pos", thisfn );
		}
		nested.eval_pos = parse_expression_for_regex( helper, res, st_eval_begin_regex );
		if( DEBUG_PARSE_EXPR_REGEX ){
			g_debug( "%s: evaluating %%FN() pos", thisfn );
		}
		nested.fn_pos = parse_expression_for_regex( helper, res, st_fname_begin_regex );
		nested.helper = helper;
		nested.prev_arithmetics = helper->eval_arithmetics;

		str = res;
		res = g_regex_replace_eval(
						st_nested_regex, str, -1, 0, 0,
						( GRegexEvalCallback ) evaluate_nested_cb, &nested, NULL );
		unchanged = ( my_collate( str, res ) == 0 );
		g_free( str );

		helper->eval_arithmetics = nested.prev_arithmetics;

		if( unchanged && g_list_length( nested.eval_pos ) > 0 ){
			str = res;
			res = evaluate_names( helper, str );
			g_free( str );
		}

		g_list_free_full( nested.eval_pos, ( GDestroyNotify ) eval_pos_free );
		g_list_free_full( nested.fn_pos, ( GDestroyNotify ) eval_pos_free );

		if( unchanged ){
			break;
		}
	}

	if( DEBUG_EVAL_NESTED ){
		g_debug( "%s: expression='%s' -> res='%s'", thisfn, expression, res );
	}

	return( res );
}

/*
 * we evaluate here nested parentheses
 * BUT we try to avoid to evaluate the parentheses which come with a
 * function
 *
 * -> List of %FN() functions:
 *    when evaluating nested parentheses, do not 'eat' the parentheses
 *    which delimit the function arguments.
 *
 * -> List of %EVAL() functions:
 *    evaluate expression even when auto_eval is False when inside of a
 *    %EVAL() function.
 *
 * Note: end_pos as returned by g_match_info_fetch_pos() is actually the
 * position of the character which follows the end of the match
 */
static gboolean
evaluate_nested_cb( const GMatchInfo *match_info, GString *result, sNested *nested )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_nested_cb";
	gchar *match, *res;
	gint count, start_pos, end_pos;

	match = g_match_info_fetch( match_info, 0 );
	count = g_match_info_get_match_count( match_info );
	g_match_info_fetch_pos( match_info, 0, &start_pos, &end_pos );

	if( DEBUG_EVAL_NESTED_CB ){
		g_debug( "%s: entering for match='%s', count=%d at start_pos=%d, end_pos=%d",
				thisfn, match, count, start_pos, end_pos );
	}

	/* if our opening parentheses matches those of an identifier function,
	 * then do not interpret it, and stop the replacement
	 */
	if( is_opening_function_pos( start_pos, nested->fn_pos )){
		if( DEBUG_EVAL_NESTED_CB ){
			g_debug( "%s: is_opening_function_pos=True", thisfn );
		}
		g_string_append( result, match );
		g_free( match );
		return( TRUE );
	}

	/* if our match is inside of an eval, then evaluate it
	 * set the arithmetic evaluation mode to True
	 */
	if( is_inside_eval_pos( start_pos, end_pos-1, nested->eval_pos )){
		if( DEBUG_EVAL_NESTED_CB ){
			g_debug( "%s: is_inside_eval_pos=True", thisfn );
		}
		nested->helper->eval_arithmetics = TRUE;
	} else {
		if( DEBUG_EVAL_NESTED_CB ){
			g_debug( "%s: is_inside_eval_pos=False, reset eval_arithmetics to %s",
					thisfn, nested->prev_arithmetics ? "True":"False" );
		}
		nested->helper->eval_arithmetics = nested->prev_arithmetics;
	}

	g_free( match );
	match = g_match_info_fetch( match_info, 1 );

	res = do_evaluate_a_expression( nested->helper, match );
	g_string_append( result, res );

	g_free( res );
	g_free( match );

	/* Returns FALSE to continue the replacement process, TRUE to stop it */
	return( FALSE );
}

/*
 * does this current match beginning at start_pos correspond to the
 * opening parenthese of a function ?
 */
static gboolean
is_opening_function_pos( gint start_pos, GList *fn_pos )
{
	GList *it;
	sStartEnd *pos;

	for( it=fn_pos ; it ; it=it->next ){
		pos = ( sStartEnd * ) it->data;
		if( start_pos == pos->start_pos ){
			return( TRUE );
		}
	}

	return( FALSE );
}

/*
 * does this current match beginning at start_pos is inside of a %EVAL() ?
 */
static gboolean
is_inside_eval_pos( gint start_pos, gint end_pos, GList *eval_pos )
{
	GList *it;
	sStartEnd *pos;

	for( it=eval_pos ; it ; it=it->next ){
		pos = ( sStartEnd * ) it->data;
		if( start_pos > pos->start_pos && end_pos < pos->end_pos ){
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
does_arithmetic_op_match( ofsFormulaHelper *helper, const gchar *expression )
{
	return( g_regex_match( st_arithmetic_op_regex, expression, 0, NULL ));
}

/*
 * @expression: an arithmetic expression.
 * a_expression ::= [ "(" ] content [ AOP content [ ... ]] [ ")" ]
 *
 * Evaluates an arithmetic expression:
 *
 * Result depends on whether auto evaluation mode is TRUE, or we are
 * inside of a %EVAL() function.
 *
 * In both cases, a comparison expression (BNF 'c_expression') is
 * returned unchanged.
 */
static gchar *
evaluate_arithmetic( ofsFormulaHelper *helper, const gchar *expression )
{
	static const gchar *thisfn = "ofa_formula_engine_evaluate_arithmetic";
	gchar *res, *str;
	GList *args, *it, *itprev, *itnext;
	const gchar *left, *right;

	g_return_val_if_fail( helper->eval_arithmetics, NULL );

	if( DEBUG_EVAL_ARITHMETIC ){
		g_debug( "%s: expression='%s'", thisfn, expression );
	}

	res = g_strdup( expression );

	/* remove the englobing parentheses (if any) */
	str = res;
	res = g_regex_replace_literal( st_englobing_regex, str, -1, 0, "", 0, NULL );
	g_free( str );

	/* remove spaces from the expression (if any) */
	str = res;
	res = g_regex_replace_literal( st_spaces_regex, str, -1, 0, "", 0, NULL );
	g_free( str );

	/* replace '--' by '+' and '-+' or '+-' by '-' */
	str = res;
	res = g_regex_replace_literal( st_minusminus_regex, str, -1, 0, "+", 0, NULL );
	g_free( str );
	str = res;
	res = g_regex_replace_literal( st_minusplus_regex, str, -1, 0, "-", 0, NULL );
	g_free( str );

	/* spit the expression by operator */
	args = split_a_expression( helper, res );
	g_free( res );

	if( g_list_length( args ) > 1 ){

		/* apply product/quotient first precedence
		 * deal with one operator per iteration
		 * iterate until no more * nor / operators */
		it = args;
		while( it ){
			if( !my_collate( it->data, "*" ) || !my_collate( it->data, "/" )){
				itprev = it->prev;
				left = itprev ? ( const gchar * ) itprev->data : NULL;
				itnext = it->next;
				right = itnext ? ( const gchar * ) itnext->data : NULL;
				res = apply_arithmetic_op( helper, it->data, left, right );
				g_free( it->data );
				it->data = res;
				args = g_list_remove_link( args, itprev );
				g_list_free_full( itprev, ( GDestroyNotify ) g_free );
				args = g_list_remove_link( args, itnext );
				g_list_free_full( itnext, ( GDestroyNotify ) g_free );
				it = args;

			} else {
				it = it->next;
			}
		}

		if( DEBUG_EVAL_ARITHMETIC_OPS ){
			for( it=args ; it ; it=it->next ){
				g_debug( "%s [2] '%s'", thisfn, ( const gchar * ) it->data );
			}
		}

		/* and apply addition/substraction last */
		it = args;
		while( it ){
			if( !my_collate( it->data, "+" ) || !my_collate( it->data, "-" )){
				itprev = it->prev;
				left = itprev ? ( const gchar * ) itprev->data : NULL;
				itnext = it->next;
				right = itnext ? ( const gchar * ) itnext->data : NULL;
				res = apply_arithmetic_op( helper, it->data, left, right );
				g_free( it->data );
				it->data = res;
				args = g_list_remove_link( args, itprev );
				g_list_free_full( itprev, ( GDestroyNotify ) g_free );
				args = g_list_remove_link( args, itnext );
				g_list_free_full( itnext, ( GDestroyNotify ) g_free );
				it = args;

			} else {
				it = it->next;
			}
		}
	}

	res = g_strdup(( const gchar * ) args->data );
	g_list_free_full( args, ( GDestroyNotify ) g_free );

	if( DEBUG_EVAL_ARITHMETIC ){
		g_debug( "%s: expression='%s' -> res='%s'", thisfn, expression, res );
	}

	return( res );
}

/*
 * split the provided @expression into a GList of
 * ( arg1 [, op, arg2 [, op, arg3 [...]]] )
 */
static GList *
split_a_expression( ofsFormulaHelper *helper, const gchar *expression )
{
	static const gchar *thisfn = "ofa_formula_engine_split_a_expression";
	gchar *str, *p, *p_begin, *op;
	guint opened_pars;
	gunichar ch, prev_ch, next_ch;
	GList *args, *it;
	gboolean last_is_operand;

	if( DEBUG_SPLIT_ARITHMETIC_EXPR ){
		g_debug( "%s: expression='%s'", thisfn, expression );
	}

	/* if arithmetic evaluation is ON, then convert the (maybe)
	 *  arithmetic expression into a list of (content [, op [, content [...]]] )
	 *
	 * else this is just a suite of macro/functions to be evaluated
	 */
	args = NULL;
	str = g_strdup( expression );
	prev_ch = '\0';
	last_is_operand = FALSE;
	opened_pars = 0;
	p = str;
	p_begin = p;

	while(( ch = g_utf8_get_char( p )) != '\0' ){
		if( is_opening_parenthese( ch )){
			opened_pars += 1;

		} else if( is_closing_parenthese( ch )){
			opened_pars -= 1;

		} else if( opened_pars == 0 && is_arithmetic_op( ch ) && prev_ch != '\\' ){
			if( p > p_begin ){
				args = append_argument( args, p_begin, p );
				last_is_operand = TRUE;
			}
			next_ch = g_utf8_get_char( p+1 );
			if( ch == '-' && g_unichar_isdigit( next_ch )){
				if( last_is_operand ){
					args = g_list_append( args, g_strdup( "+" ));
					last_is_operand = FALSE;
				}
				p_begin = p;

			} else {
				op = g_strdup_printf( "%c", ch );
				args = g_list_append( args, op );
				last_is_operand = FALSE;
				p_begin = g_utf8_next_char( p );
			}
		}

		p = g_utf8_next_char( p );
		prev_ch = ch;
	}

	args = append_argument( args, p_begin, p );
	g_free( str );

	if( DEBUG_SPLIT_ARITHMETIC_EXPR ){
		for( it=args ; it ; it=it->next ){
			g_debug( "%s: it='%s'", thisfn, ( const gchar * ) it->data );
		}
	}

	return( args );
}

static gboolean
is_opening_parenthese( gunichar ch )
{
	return( ch == '(' );
}

static gboolean
is_closing_parenthese( gunichar ch )
{
	return( ch == ')' );
}

static gboolean
is_arithmetic_op( gunichar ch )
{
	return( ch == '+' || ch == '-' || ch == '/' || ch == '*' );
}

static GList *
append_argument( GList *args, const gchar *pbegin, const gchar *p )
{
	if( pbegin == p ){
		args = g_list_append( args, g_strdup( "0" ));
	} else {
		args = g_list_append( args, g_strstrip( g_strndup( pbegin, p-pbegin )));
	}

	return( args );
}

/*
 * apply arithmetic operator to left and right operands
 * which are expected to be numbers
 */
static gchar *
apply_arithmetic_op( ofsFormulaHelper *helper, const gchar *oper, const gchar *left, const gchar *right )
{
	static const gchar *thisfn = "ofa_formula_engine_apply_arithmetic_op";
	ofaFormulaEnginePrivate *priv;
	gchar *res, *str;
	gdouble a, b, c;

	if( DEBUG_EVAL_ARITHMETIC_OPS ){
		g_debug( "%s: helper=%p, left='%s', oper='%s', right='%s'", thisfn, ( void * ) helper, left, oper, right );
	}

	priv = ofa_formula_engine_get_instance_private( helper->engine );

	res = NULL;
	a = my_double_set_from_str( left, priv->thousand_sep, priv->decimal_sep );
	b = my_double_set_from_str( right, priv->thousand_sep, priv->decimal_sep );
	c = 0;

	if( !my_collate( oper, "+" )){
		c = a + b;
	} else if( !my_collate( oper, "-" )){
		c = a - b;
	} else if( !my_collate( oper, "/" )){
		if( b == 0 ){
			str = g_strdup_printf( _( "%s [error] division by zero: left='%s', op='%s', right='%s'" ),
					thisfn, left, oper, right );
			helper->msg = g_list_append( helper->msg, str );
		} else {
			c = a / b;
		}
	} else if( !my_collate( oper, "*" )){
		c = a * b;
	}

	res = my_double_to_str( c, priv->thousand_sep, priv->decimal_sep, priv->digits );

	if( DEBUG_EVAL_ARITHMETIC_OPS ){
		g_debug( "%s: res='%s'", thisfn, res );
	}

	return( res );
}

/*
 * %EVAL( a op b [ op c [...]] )
 * Expects one argument.
 * Possible nested parentheses are expected to have been already interpreted.
 */
static gchar *
eval_eval( ofsFormulaHelper *helper )
{
	gchar *res;
	GList *it;
	const gchar *cstr;
	gboolean prev_eval;

	res = NULL;

	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		prev_eval = helper->eval_arithmetics;
		helper->eval_arithmetics = TRUE;
		res = do_evaluate_a_expression( helper, cstr );
		helper->eval_arithmetics = prev_eval;
	}

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

	/* condition is expected to be a BNF 'c_expression' */
	res = g_regex_replace_eval(
					st_if_regex, condition, -1, 0, 0,
					( GRegexEvalCallback ) eval_if_cb, helper, NULL );

	bres = my_utils_boolean_from_str( res );
	g_free( res );
	res = g_strdup( bres ? iftrue : iffalse );

	if( DEBUG_EVAL_IF ){
		g_debug( "%s: condition=%s -> res=%s", thisfn, condition, res );
	}

	return( res );
}

/*
 * Parse the condition
 * Expects a BNF 'c_expression':
 *
 * c_expression ::= [ "(" ] content CMP content [ ")" ]
 *
 * CMP ::= comparison operator
 * CMP ::= ( "<" | ">" | "!" | "=" ){1,3}
 *    Comparison operator is used in %IF() first argument.
 *    It evaluates to "1" (comparison is true) or "0" (comparison is false).
 *
 * Returns: "1" if true, "0" if false.
 */
static gboolean
eval_if_cb( const GMatchInfo *match_info, GString *result, ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_formula_engine_eval_if_cb";
	guint count;
	gchar *match, *str, *res, *left, *op, *right, *leftres, *rightres;

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
			leftres = do_evaluate_a_expression( helper, left );
			rightres = do_evaluate_a_expression( helper, right );

			if( !my_strlen( leftres ) || !my_strlen( rightres )){
				str = g_strdup_printf( _( "%s [error] leftres='%s', op='%s', rightres='%s'" ), thisfn, leftres, op, rightres );
				helper->msg = g_list_append( helper->msg, str );

			} else {
				if( DEBUG_EVAL_IF ){
					g_debug( "%s left='%s', op='%s', right='%s'", thisfn, left, op, right );
				}
				if( eval_if_true( helper->engine, left, op, right )){
					g_free( res );
					res = g_strdup( "1" );
				}
			}

			g_free( leftres );
			g_free( rightres );
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

	if( DEBUG_EVAL_IF ){
		g_debug( "%s: left=%s, op=%s, right=%s, is_true=%s", thisfn, left, op, right, is_true ? "True":"False" );
	}

	return( is_true );
}

static void
eval_pos_free( sStartEnd *pos )
{
	g_free( pos );
}

/*
 * TESTS
 *
 * Macros:
 * - %Ai is same than %ACCOUNT( i )
 * - %Di is same than %DEBIT( i )
 * - %TVAN is same the %RATE( TVAN )
 *
 * Functions:
 * - %ACCOUNT( i ) returns: iiiii
 * - %ACLA( i )    returns: label iiiii
 * - %DEBIT( i )   returns: -3,3 * i
 * - %RATE( TVAN ) returns: 0,196
 * - %CODE( i )    returns: i
 * - %AMOUNT( i )  returns:  5,55 * i
 */
static const gchar *st_formulas[] = {
		"= %DEBIT( 1 ) * %RATE( TVAN )",
		"=%EVAL( %D1 * %TVAN )",
		"=%AMOUNT(%CODE(08)+21) + %AMOUNT(%CODE(10))",
		"=%DEBIT(3) - %DEBIT(09) + ( %DEBIT(5) / ( %DEBIT(10) + %DEBIT(2) ) - %DEBIT(3) ) + %DEBIT(4) * %DEBIT(5) / %DEBIT(3)",
		"NOT A FORMULA %A1 - %ACLA(%A1)",
		"'=NOT A FORMULA %A1 - %ACLA(%A1)",
		"=SOLDE DE CLOTURE DE L'EXERCICE %A1 \\- %ACLA(%A1)",
		"=SOLDE DE CLOTURE DE L'EXERCICE %A1 - %ACLA(%A1)",
		"= 1 + %EVAL( 2*( 14+(3*3)-5 )) + 2 + %EVAL( 3* (2+5))",
		"=%IF( %DEBIT(%CODE(5)) > 0 ; %DEBIT(3); %AMOUNT(5))",
		NULL
};

static ofaFormulaEvalFn get_formula_eval_fn( const gchar *fname, gint *count, GMatchInfo *match_info, void *user_data );

static gchar *eval_a( ofsFormulaHelper *helper );
static gchar *eval_account( ofsFormulaHelper *helper );
static gchar *eval_acla( ofsFormulaHelper *helper );
static gchar *eval_amount( ofsFormulaHelper *helper );
static gchar *eval_code( ofsFormulaHelper *helper );
static gchar *eval_d( ofsFormulaHelper *helper );
static gchar *eval_debit( ofsFormulaHelper *helper );
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
		{ "D", 1, eval_d },
		{ "ACCOUNT", 1, eval_account },
		{ "ACLA", 1, eval_acla },
		{ "RATE", 1, eval_rate },
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

	ofa_formula_engine_set_auto_eval( engine, TRUE );

	for( i=0 ; st_formulas[i] ; ++i ){
		g_debug( "%s: formula='%s'", thisfn, st_formulas[i] );
		result = ofa_formula_engine_eval( engine, st_formulas[i], ( ofaFormulaFindFn ) get_formula_eval_fn, NULL, &msg );
		g_debug( "%s: result='%s'", thisfn, result );
		g_free( result );
	}

	ofa_formula_engine_set_auto_eval( engine, FALSE );

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
	//static const gchar *thisfn = "get_formula_eval_fn";
	gint i;

	*count = -1;

	for( i=0 ; st_formula_fns[i].name ; ++i ){
		if( !my_collate( st_formula_fns[i].name, fname )){
			*count = st_formula_fns[i].args_count;
			return(( ofaFormulaEvalFn ) st_formula_fns[i].eval );
		}
	}

	if( !my_collate( fname, "A1" )){
		return( eval_a );

	} else if( !my_collate( fname, "D1" )){
		return( eval_d );

	} else if( g_str_has_prefix( fname, "TVA" )){
		return( eval_rate );
	}

	return( NULL );
}

/*
 * %Ai is a shortcut to %ACCOUNT(i)
 */
static gchar *
eval_a( ofsFormulaHelper *helper )
{
	int row;
	gchar *res;

	row = atoi( helper->match_name+1 );
	res = g_strdup_printf( "%%ACCOUNT(%d)", row );

	g_debug( "eval_a: returns res='%s'", res );

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

/*
 * %ACLA(i)
 * Returns: the account label
 */
static gchar *
eval_acla( ofsFormulaHelper *helper )
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
		res = g_strdup_printf( "Label « %%ACCOUNT(%d) » ®", row );
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
		a = ofa_amount_from_str( cstr );
		res = ofa_amount_to_str( 5.55 * a, NULL );
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
 * %Di is a shortcut to %DEBIT(i)
 */
static gchar *
eval_d( ofsFormulaHelper *helper )
{
	int row;
	gchar *res;

	row = atoi( helper->match_name+1 );
	res = g_strdup_printf( "%%DEBIT(%d)", row );

	g_debug( "eval_d: returns res='%s'", res );

	return( res );
}

/*
 * %DEBIT( i ), where row_number is counted from 1
 * -> return -11.05 * i;
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
		res = ofa_amount_to_str( -3.33 * ( gdouble ) row, NULL );
	}

	g_debug( "eval_debit: returns res='%s'", res );

	return( res );
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

	res = NULL;
	cstr = NULL;

	if( !my_collate( helper->match_name, "TVAN" )){
		res = g_strdup( "0,196" );

	} else {
		it = helper->args_list;
		cstr = it ? ( const gchar * ) it->data : NULL;
		if( my_strlen( cstr )){
			if( !my_collate( cstr, "TVAN" )){
				res = g_strdup( "0,196" );
			}
		}
	}

	g_debug( "eval_rate: name=%s, cstr=%s, returns res='%s'", helper->match_name, cstr, res );

	return( res );
}
