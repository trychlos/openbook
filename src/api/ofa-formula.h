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

#ifndef __OPENBOOK_API_OFA_FORMULA_H__
#define __OPENBOOK_API_OFA_FORMULA_H__

/**
 * SECTION: ofa_formula
 * @short_description: Formula functions
 * @include: openbook/ofa-formula.h
 *
 * The formula engine is able to evaluate formulas which are found in
 * operation template, VAT forms and so on.
 *
 * It is based on a sample macro language, and acts recursively.
 *
 * The evaluation of a formula gives a string, which may or may not be
 * evaluated depending of whether we have been able to get all needed
 * datas.
 *
 * A formula begins with equal ('=') sign. To begin any string with an
 * equal sign without tranforming it in a formula, just prefix it with
 * a single quote (e.g. "'=this is not a formula").
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
 * Formula evaluation may return with a set of error messages.
 * This set must be released by the caller with #g_list_free_full( list, ( GDestroyNotify ) g_free ).
 *
 * v54 known functions
 * -------------------
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
 *
 * BNF formulation
 * ---------------
 *
 * OPERATOR OP ::= "+" | "-" | "/" | "*"
 *    addition | substraction | division | product
 *    precedences are:    "*" "/"
 *               then:    "+" "-"
 *
 * FORMULA ::= "=" <content> [ OP <content [ ... ]]
 */

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ofsFormulaHelper ofsFormulaHelper;

/**
 * ofsFormulaFn:
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
	ofsFormulaFn;

/**
 * ofsFormulaHelper:
 *
 * => a copy of #ofa_formula_eval() original arguments
 *
 * @fns: the list of callbacks.
 * @user_data: the user data pointer.
 * @msg: the output #GList of messages.
 *
 * => the arguments for the current match
 *
 * @match_info: the current #GMatchInfo instance.
 * @match_str: the current #GMatchInfo match at index 0.
 * @match_fn: the found #ofsFormulaFn struct.
 * @args_list: the current arguments list.
 * @args_count: the count of found arguments in the @args_list.
 *
 * A data structure which is passed to each callback function.
 */
struct _ofsFormulaHelper {

	const ofsFormulaFn *fns;
	void               *user_data;
	GList             **msg;

	const GMatchInfo   *match_info;
	gchar              *match_str;
	const ofsFormulaFn *match_fn;
	GList              *args_list;
	guint               args_count;
};

#define OFA_FORMULA_ARG_SEP             ";"

gchar *ofa_formula_eval( const gchar *formula, const ofsFormulaFn *fns, void *user_data, GList **msg );

void   ofa_formula_test( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_FORMULA_H__ */
