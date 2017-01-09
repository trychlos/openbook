/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_FORMULA_ENGINE_H__
#define __OPENBOOK_API_OFA_FORMULA_ENGINE_H__

/**
 * SECTION: ofa_formula_engine
 * @short_description: ofaFormulaEngine class definition
 * @include: openbook/ofa-formula-engine.h
 *
 * The formula engine is able to evaluate formulas which are found in
 * operation template, VAT forms and so on.
 *
 * It is based on a sample macro language, and acts recursively.
 *
 * The evaluation of a formula gives a string, which may or may not be
 * evaluated depending of whether we have been able to get all needed
 * datas. Dates and amounts are evaluated as displayable strings, using
 * set user preferences.
 *
 * A formula begins with equal ('=') sign. To begin any string with an
 * equal sign without tranforming it in a formula, just prefix it with
 * a single quote (e.g. "'=this is not a formula").
 *
 * Arithmetic operators + - / and * are honored inside of the %EVAL()
 * function. This function may have nested parentheses. Most of the
 * time, it is easier to have just one %EVAL() outside of the whole
 * formula, but this is in case mandatory.
 *
 * Leading and terminating spaces are silently ignored.
 *
 * A formula is build around functions and arithmetic signs, where:
 * - exammples of functions may be:
 *   %RATE(...)
 *   %ACCOUNT(...)
 *   etc.
 * - arithmetic signs
 *
 * The caller must provide the function with the set of known functions
 * it is willing to deal with. In this set, function names may be
 * abbreviated as long as they stay unique.
 *
 * Functions (resp. macros) are identified as the '%' sign, followed by
 * the function (resp. macro) name. Functions must immediately follows
 * with an opening parenthesis.
 *
 * A macro is the same as a function, but does not take any argument.
 * It is expected to be evaluable in the global scope of the current
 * call. Macro are just special functions with zero argument.
 *
 * The string returned by each function is reinjected into the original
 * formula and re-evaluated. It may thus be a shortcut to a more complex
 * formula.
 *
 * Formula evaluation may return with a list of error messages.
 * This list must be released by the caller with
 *  #g_list_free_full( list, ( GDestroyNotify ) g_free ).
 *
 * BNF formulation
 * ---------------
 *
 * FORMULA ::= "=" a_expression
 *
 * a_expression ::= arithmetic expression
 * a_expression ::= [ "(" ] content [ AOP content [ ... ]] [ ")" ]
 *
 * AOP ::= arithmetic operator
 * AOP ::= "+" | "-" | "/" | "*"
 *    aka: addition | substraction | division | product
 *    Precedences are:    "*" "/"
 *               then:    "+" "-"
 *    AOP is evaluated:
 *    - inside of a %EVAL function
 *    - or if the automatic arithmetic evaluation mode is %TRUE.
 *
 *    Example:
 *    a) "a * b + c" is evaluated as "a * b = res1", then "res1 + c = res2"
 *    b) "a * ( b + c )" is evaluated as "b + c = res1", then "a * res1 = res2"
 *
 * "(" and ")" are parentheses which modify the precedence of arithmetic
 * operators during arithmetic evaluation.
 * As such, they are only taken into account:
 *    - inside of a %EVAL function
 *    - or if the automatic arithmetic evaluation mode is %TRUE.
 * When auto evaluation mode is %FALSE and outside of an %EVAL(...)
 * function, parentheses are just rendered as-is.
 *
 * content ::= %MACRO | %FN( arg1 [ ; arg2 [ ... ]] )
 *
 * MACRO ::= a macro function which evaluates without argument.
 *
 * %FN() ::= a function which evaluates its argument(s).
 *
 * argi ::= a_expression | c_expression
 *
 * c_expression ::= comparison expression
 * c_expression ::= [ "(" ] content CMP content [ ")" ]
 *
 * CMP ::= comparison operator
 * CMP ::= ( "<" | ">" | "!" | "=" ){1,3}
 *    Comparison operator is used in %IF() first argument.
 *    It evaluates to "1" (comparison is true) or "0" (comparison is false).
 *
 * v54 known functions
 * -------------------
 *
 * Formula engine
 *
 *    The formula engine itself provides some standard functions, which
 *    are so available to all callers:
 *
 *    - %EVAL( a op b [ op c [...]] ): evaluates the arithmetic expression
 *
 *    - %IF( condition; if_true; if_false ): evaluates the condition,
 *      returning 'if_true' or 'if_false' strings.
 *
 *    Auto Eval:
 *
 *    When auto evaluation is set to TRUE, which is the default,
 *    formulas do not need to use %EVAL(...) function to evaluate
 *    arithmetic expressions. Instead they are auto-evaluated anywhere
 *    in the formula.
 *    In this mode, the %EVAL(...) function is just ignored.
 *    As a side effect, arithmetic operators which do not have to be
 *    evaluated have to be backslashed (e.g. when returning a label
 *    which embeds a minus sign '-' as typo separator).
 *
 *    When auto evaluation is set to FALSE, the arithmetic operators
 *    are only evaluated inside of a %EVAL(...) function.
 *
 * Operation template
 *
 *    A reference to another field of the operation;
 *    these tokens are single-uppercase-letter, immediately followed
 *    by the number of the row, counted from 1:
 *       - 'Ai': account number
 *       - 'Li': label
 *       - 'Di': debit
 *       - 'Ci': credit
 *       E.g.: the '%A1' string (without the quotes) will be substituted
 *             with the account number from row n° 1.
 *
 *    2/ a reference to a global field of the operation
 *       - 'OPMN':    operation template mnemo
 *       - 'OPLA':    operation template label
 *       - 'LEMN':    ledger mnemo
 *       - 'LELA':    ledger label
 *       - 'REF':     the operation piece reference
 *       - 'DOPE':    operation date (format from user preferences)
 *       - 'DOMY':    operation date as mmm yyyy
 *       - 'DEFFECT': effect date (format from user preferences)
 *       - 'SOLDE':   the solde of the entries debit and credit to balance
 *                    the operation
 *       - 'IDEM':    the content of the same field from the previous row
 *
 *       Other keywords are searched as rate mnemonics (as a convenient
 *       shortcut to RATE() function).
 *
 *    3/ a function, arguments being passed between parenthesis:
 *       - 'ACLA()': returns account label
 *       - 'ACCU()': returns account currency
 *       - 'EVAL()': evaluate the result of the operation
 *                   may be omitted when in an amount entry
 *       - 'RATE()': evaluates the rate value at the operation date
 *                   may be omitted, only using %<rate> if the
 *                   'rate' mnemonic is not ambiguous
 *       - 'ACCL()': the closing account for the currency of the specified
 *                   account number
 *
 * VAT forms
 *
 *    1/ a function, arguments being passed between parenthesis:
 *       - 'CODE()':    identifies the rows whose 'Code' column holds the value
 *       - 'ACCOUNT()': returns the rough+validated balance of the account
 *       - 'AMOUNT():
 *       - 'BASE()':
 */

#include <glib.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_FORMULA_ENGINE                ( ofa_formula_engine_get_type())
#define OFA_FORMULA_ENGINE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_FORMULA_ENGINE, ofaFormulaEngine ))
#define OFA_FORMULA_ENGINE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_FORMULA_ENGINE, ofaFormulaEngineClass ))
#define OFA_IS_FORMULA_ENGINE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_FORMULA_ENGINE ))
#define OFA_IS_FORMULA_ENGINE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_FORMULA_ENGINE ))
#define OFA_FORMULA_ENGINE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_FORMULA_ENGINE, ofaFormulaEngineClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
 	 ofaFormulaEngine;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaFormulaEngineClass;

/**
 * OFA_FORMULA_ARG_SEP:
 *
 * The arguments separator when a function takes several arguments.
 * It has been chosen to be compatible wich potential thousand and
 * decimal separators.
 */
#define OFA_FORMULA_ARG_SEP             ";"

typedef struct _ofsFormulaHelper ofsFormulaHelper;

/**
 * ofaFormulaEvalFn:
 *
 * The prototype for formula evaluation functions.
 *
 * The evaluation functions receive a #ofsFormulaHelper data structure
 * which describe the current match, and must return the string to be
 * substituted, as a newly allocated string which will be g_free() by
 * the ofaFormula code.
 */
typedef gchar * ( *ofaFormulaEvalFn )( ofsFormulaHelper * );

/**
 * ofaFormulaFindFn:
 * @name: the found name.
 * @min_count: [out]: should be set to the minimal expected arguments
 *  count.
 * @max_count: [out]: should be set to the maximal expected arguments
 *  count, or -1 for no limit.
 * @match_info: the current #GMatchInfo instance.
 * @user_data: the user data.
 *
 * A callback which should return a pointer to the function which will
 * provide the @name evaluation.
 */
typedef ofaFormulaEvalFn ( *ofaFormulaFindFn )( const gchar *, gint *, gint *, const GMatchInfo *, void * );

/**
 * ofsFormulaHelper:
 *
 * => a copy of #ofa_formula_engine_eval() original arguments
 *
 * @engine: the #ofaFormulaEngine instance itself.
 * @finder: the #ofaFormulaFindFn usere-provided callback.
 * @user_data: the user data user-provided pointer.
 * @msg: the output #GList of messages.
 *
 * => runtime data
 *
 * @eval_arithmetics: whether arithmetic expression must be evaluated.
 *
 * => the arguments for the current match
 *
 * @match_info: the current #GMatchInfo instance.
 * @match_zero: the match at index 0.
 * @match_name: the macro or function name (match at index 1).
 * @args_list: the current arguments list.
 * @args_count: the count of found arguments in the @args_list.
 *
 * A data structure which is passed to each callback function.
 */
struct _ofsFormulaHelper {

	ofaFormulaEngine   *engine;
	ofaFormulaFindFn    finder;
	void               *user_data;
	GList              *msg;

	gboolean            eval_arithmetics;

	const GMatchInfo   *match_info;
	gchar              *match_zero;
	gchar              *match_name;
	GList              *args_list;
	guint               args_count;
};

GType             ofa_formula_engine_get_type         ( void ) G_GNUC_CONST;

ofaFormulaEngine *ofa_formula_engine_new              ( ofaHub *hub );

void              ofa_formula_engine_set_auto_eval    ( ofaFormulaEngine *engine,
															gboolean auto_eval );

void              ofa_formula_engine_set_amount_format( ofaFormulaEngine *engine,
															gunichar thousand_sep,
															gunichar decimal_sep,
															guint digits );

gchar            *ofa_formula_engine_eval             ( ofaFormulaEngine *engine,
															const gchar *formula,
															ofaFormulaFindFn finder,
															void *user_data,
															GList **msg );

void              ofa_formula_test                    ( ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_FORMULA_ENGINE_H__ */
