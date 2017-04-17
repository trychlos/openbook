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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-prefs.h"
#include "api/ofa-igetter.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;

	/* account
	 */
	gboolean           account_delete_with_children;
	gboolean           account_settle_warns;
	gboolean           account_settle_ctrl;
	gboolean           account_reconcil_warns;
	gboolean           account_reconcil_ctrl;

	/* amount
	 */
	gchar             *amount_decimal_sep;
	gchar             *amount_thousand_sep;
	gboolean           amount_accept_dot;
	gboolean           amount_accept_comma;

	/* application
	 */
	gboolean           appli_confirm_on_altf4;
	gboolean           appli_confirm_on_quit;

	/* assistant
	 */
	gboolean           assistant_quit_on_escape;
	gboolean           assistant_confirm_on_escape;
	gboolean           assistant_confirm_on_cancel;

	/* check dbms integrity
	 */
	gboolean           check_integrity_display_all;

	/* date
	 */
	myDateFormat       date_display_format;
	myDateFormat       date_check_format;
	gboolean           date_overwrite;

	/* export
	 */
	gchar             *export_default_folder;

	/* main window / main notebook
	 */
	ofeMainbookStartup mainbook_startup_mode;
	ofeMainbookOpen    mainbook_open_mode;
	ofeMainbookTabs    mainbook_tabs_mode;
	gboolean           mainbook_with_detach_pin;
	ofeMainbookClose   mainbook_close_mode;
}
	ofaPrefsPrivate;

static const gchar *st_account          = "ofaPreferences-Account";
static const gchar *st_amount           = "ofaPreferences-Amount";
static const gchar *st_application      = "ofaPreferences-Application";
static const gchar *st_assistant        = "ofaPreferences-Assistant";
static const gchar *st_check_integrity  = "ofaPreferences-CheckIntegrity";
static const gchar *st_date             = "ofaPreferences-Date";
static const gchar *st_export           = "ofaPreferences-Export";
static const gchar *st_mainbook         = "ofaPreferences-MainNotebook";

/* indicator are managed as enum's in the code (easier)
 * but as unlocalized letters in the user settings (more maintainable)
 */
typedef struct {
	gint         id;
	const gchar *code;
}
	sEnum;

static const sEnum st_mainbook_startup_mode[] = {
		{ MAINBOOK_STARTNORMAL, "N" },
		{ MAINBOOK_STARTMINI,   "M" },
		{ 0 }
};

static const sEnum st_mainbook_open_mode[] = {
		{ MAINBOOK_OPENKEEP,    "K" },
		{ MAINBOOK_OPENNATURAL, "N" },
		{ 0 }
};

static const sEnum st_mainbook_tabs_mode[] = {
		{ MAINBOOK_TABDETACH,  "D" },
		{ MAINBOOK_TABREORDER, "R" },
		{ 0 }
};

static const sEnum st_mainbook_close_mode[] = {
		{ MAINBOOK_CLOSEKEEP,  "K" },
		{ MAINBOOK_CLOSERESET, "R" },
		{ 0 }
};

static void         account_read_settings( ofaPrefs *self );
static void         account_write_settings( ofaPrefs *self );
static void         amount_read_settings( ofaPrefs *self );
static void         amount_write_settings( ofaPrefs *self );
static void         appli_read_settings( ofaPrefs *self );
static void         appli_write_settings( ofaPrefs *self );
static gboolean     is_willing_to_quit( ofaPrefs *self );
static void         assistant_read_settings( ofaPrefs *self );
static void         assistant_write_settings( ofaPrefs *self );
static void         check_integrity_read_settings( ofaPrefs *self );
static void         check_integrity_write_settings( ofaPrefs *self );
static void         date_read_settings( ofaPrefs *self );
static void         date_write_settings( ofaPrefs *self );
static void         export_read_settings( ofaPrefs *self );
static void         export_write_settings( ofaPrefs *self );
static void         mainbook_read_settings( ofaPrefs *self );
static void         mainbook_write_settings( ofaPrefs *self );
static gint         enum_code_to_enum( const sEnum *table, const gchar *code, gint def_value );
static const gchar *enum_enum_to_code( const sEnum *table, gint value, gint def_value );
static const gchar *enum_find_enum( const sEnum *table, gint value );

G_DEFINE_TYPE_EXTENDED( ofaPrefs, ofa_prefs, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaPrefs ))

static void
prefs_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_prefs_finalize";
	ofaPrefsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PREFS( instance ));

	/* free data members here */
	priv = ofa_prefs_get_instance_private( OFA_PREFS( instance ));

	g_free( priv->amount_decimal_sep );
	g_free( priv->amount_thousand_sep );
	g_free( priv->export_default_folder );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_prefs_parent_class )->finalize( instance );
}

static void
prefs_dispose( GObject *instance )
{
	ofaPrefsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PREFS( instance ));

	priv = ofa_prefs_get_instance_private( OFA_PREFS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here,
		 * ending with collection, ofaExtenderCollection at very last */
	}

	G_OBJECT_CLASS( ofa_prefs_parent_class )->dispose( instance );
}

static void
ofa_prefs_init( ofaPrefs *self )
{
	static const gchar *thisfn = "ofa_prefs_init";
	ofaPrefsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PREFS( self ));

	priv = ofa_prefs_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->account_delete_with_children = FALSE;
	priv->account_settle_warns = TRUE;
	priv->account_settle_ctrl = TRUE;
	priv->account_reconcil_warns = TRUE;
	priv->account_reconcil_ctrl = TRUE;

	priv->amount_decimal_sep = g_strdup( "." );
	priv->amount_thousand_sep = g_strdup( "," );
	priv->amount_accept_dot = TRUE;
	priv->amount_accept_comma = FALSE;

	priv->appli_confirm_on_altf4 = TRUE;
	priv->appli_confirm_on_quit = TRUE;

	priv->assistant_quit_on_escape = TRUE;
	priv->assistant_confirm_on_escape = TRUE;
	priv->assistant_confirm_on_cancel = FALSE;

	priv->check_integrity_display_all = FALSE;

	priv->date_display_format = MY_DATE_YYMD;
	priv->date_check_format = MY_DATE_YYMD;
	priv->date_overwrite = FALSE;

	priv->export_default_folder = g_strdup( "/tmp" );

	priv->mainbook_startup_mode = MAINBOOK_STARTNORMAL;
	priv->mainbook_open_mode = MAINBOOK_OPENNATURAL;
	priv->mainbook_tabs_mode = MAINBOOK_TABREORDER;
	priv->mainbook_with_detach_pin = FALSE;
	priv->mainbook_close_mode = MAINBOOK_CLOSEKEEP;
}

static void
ofa_prefs_class_init( ofaPrefsClass *klass )
{
	static const gchar *thisfn = "ofa_prefs_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = prefs_dispose;
	G_OBJECT_CLASS( klass )->finalize = prefs_finalize;
}

/**
 * ofa_prefs_new:
 * @getter: the #ofaIGetter of the application.
 *
 * Allocates and initializes the #ofaPrefs object of the application.
 *
 * Returns: a new #ofaPrefs object.
 */
ofaPrefs *
ofa_prefs_new( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	prefs = g_object_new( OFA_TYPE_PREFS, NULL );

	priv = ofa_prefs_get_instance_private( prefs );

	priv->getter = getter;

	account_read_settings( prefs );
	amount_read_settings( prefs );
	appli_read_settings( prefs );
	assistant_read_settings( prefs );
	check_integrity_read_settings( prefs );
	date_read_settings( prefs );
	export_read_settings( prefs );
	mainbook_read_settings( prefs );

	return( prefs );
}

/**
 * ofa_prefs_account_delete_root_with_child:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if deleting a root account also deletes its children.
 */
gboolean
ofa_prefs_account_get_delete_with_children( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->account_delete_with_children );
}

/**
 * ofa_prefs_account_settle_warns_if_unbalanced:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user should be warned when he tries to settle
 * an unbalanced group of entries.
 */
gboolean
ofa_prefs_account_settle_warns_if_unbalanced( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->account_settle_warns );
}

/**
 * ofa_prefs_account_settle_warns_unless_ctrl:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user can get rid of the previous warning by
 * hitting the 'Ctrl' key.
 */
gboolean
ofa_prefs_account_settle_warns_unless_ctrl( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->account_settle_ctrl );
}

/**
 * ofa_prefs_account_reconcil_warns_if_unbalanced:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user should be warned when he tries to reconcil
 * an unbalanced group of entries.
 */
gboolean
ofa_prefs_account_reconcil_warns_if_unbalanced( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->account_reconcil_warns );
}

/**
 * ofa_prefs_account_reconcil_warns_unless_ctrl:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user can get rid of the previous warning by
 * hitting the 'Ctrl' key.
 */
gboolean
ofa_prefs_account_reconcil_warns_unless_ctrl( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->account_reconcil_ctrl );
}

/**
 * ofa_prefs_account_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @delete: whether deleting a root account also deletes the children.
 * @settle_warns: whether the user should be warned when settlement
 *  group is unbalanced.
 * @settle_ctrl: whether the 'Ctrl' key get rid of the previous warning.
 * @reconcil_warns: whether the user should be warned when conciliation
 *  group is unbalanced.
 * @reconcil_ctrl: whether the 'Ctrl' key get rid of the previous warning.
 *
 * Set the user settings.
 */
void
ofa_prefs_account_set_user_settings( ofaIGetter *getter,
							gboolean delete,
							gboolean settle_warns, gboolean settle_ctrl,
							gboolean reconcil_warns, gboolean reconcil_ctrl )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->account_delete_with_children = delete;
	priv->account_settle_warns = settle_warns;
	priv->account_settle_ctrl = settle_ctrl;
	priv->account_reconcil_warns = reconcil_warns;
	priv->account_reconcil_ctrl = reconcil_ctrl;

	account_write_settings( prefs );
}

/*
 * Account settings: delete_with_children;settle_warns;settle_ctrl;reconcil_warns;reconcil_ctrl;
 */
static void
account_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_account );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->account_delete_with_children = my_utils_boolean_from_str( cstr );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->account_settle_warns = my_utils_boolean_from_str( cstr );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->account_settle_ctrl = my_utils_boolean_from_str( cstr );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->account_reconcil_warns = my_utils_boolean_from_str( cstr );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->account_reconcil_ctrl = my_utils_boolean_from_str( cstr );

	my_isettings_free_string_list( settings, strlist );
}

static void
account_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;%s;%s;%s;%s;",
			priv->account_delete_with_children ? "True":"False",
			priv->account_settle_warns ? "True":"False",
			priv->account_settle_ctrl ? "True":"False",
			priv->account_reconcil_warns ? "True":"False",
			priv->account_reconcil_ctrl ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_account, str );

	g_free( str );
}

/**
 * ofa_prefs_amount_get_decimal_sep:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered decimal separator (for display).
 *
 * The returned string is owned by #ofaPrefs, and should not be released
 * by the caller.
 */
const gchar *
ofa_prefs_amount_get_decimal_sep( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->amount_decimal_sep );
}

/**
 * ofa_prefs_amount_get_thousand_sep:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered thousand separator (for display).
 *
 * The returned string is owned by #ofaPrefs, and should not be released
 * by the caller.
 */
const gchar *
ofa_prefs_amount_get_thousand_sep( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->amount_thousand_sep );
}

/**
 * ofa_prefs_amount_get_accept_dot:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the user accepts dot as a decimal separator.
 */
gboolean
ofa_prefs_amount_get_accept_dot( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->amount_accept_dot );
}

/**
 * ofa_prefs_amount_get_accept_comma:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the user accepts comma as a decimal separator.
 */
gboolean
ofa_prefs_amount_get_accept_comma( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->amount_accept_comma );
}

/**
 * ofa_prefs_amount_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @decimal_sep: the decimal separator.
 * @thousand_sep: the thousand separator.
 * @accept_dot: whether accept dot as a decimal separator on entry.
 * @accept_comma: whether accept comma as a decimal separator on entry.
 *
 * Set the user settings.
 */
void
ofa_prefs_amount_set_user_settings( ofaIGetter *getter,
										const gchar *decimal_sep, const gchar *thousand_sep,
										gboolean accept_dot, gboolean accept_comma )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->amount_decimal_sep );
	priv->amount_decimal_sep = g_strdup( decimal_sep );
	g_free( priv->amount_thousand_sep );
	priv->amount_thousand_sep = g_strdup( thousand_sep );
	priv->amount_accept_dot = accept_dot;
	priv->amount_accept_comma = accept_comma;

	amount_write_settings( prefs );
}

/*
 * Amount settings: decimal_char;thousand_char;accept_dot;accept_comma;
 */
static void
amount_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_amount );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->amount_decimal_sep );
		priv->amount_decimal_sep = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->amount_thousand_sep );
		priv->amount_thousand_sep = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->amount_accept_dot = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->amount_accept_comma = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
amount_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;%s;%s;%s;",
			priv->amount_decimal_sep,
			priv->amount_thousand_sep,
			priv->amount_accept_dot ? "True":"False",
			priv->amount_accept_comma ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_amount, str );

	g_free( str );
}

/**
 * ofa_prefs_appli_confirm_on_altf4:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a confirmation is required when quitting the
 * application on Alt+F4 key.
 */
gboolean
ofa_prefs_appli_confirm_on_altf4( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->appli_confirm_on_altf4 );
}

/**
 * ofa_prefs_appli_confirm_on_quit:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if a confirmation is required when quitting the
 * application.
 */
gboolean
ofa_prefs_appli_confirm_on_quit( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->appli_confirm_on_quit );
}

/**
 * ofa_prefs_appli_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @confirm_on_altf4: whether quitting by 'Alt+F4' key requires a user
 *  confirmation.
 * @confirm_on_quit: whether quitting by the menu requires a user
 *  confirmation.
 *
 * Set the user settings.
 */
void
ofa_prefs_appli_set_user_settings( ofaIGetter *getter, gboolean confirm_on_altf4, gboolean confirm_on_quit )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->appli_confirm_on_altf4 = confirm_on_altf4;
	priv->appli_confirm_on_quit = confirm_on_quit;

	appli_write_settings( prefs );
}

/*
 * Application settings: confirm_on_altf4; confirm_on_quit;
 */
static void
appli_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_application );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->appli_confirm_on_altf4 = my_utils_boolean_from_str( cstr );

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->appli_confirm_on_quit = my_utils_boolean_from_str( cstr );

	my_isettings_free_string_list( settings, strlist );
}

static void
appli_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;%s;",
			priv->appli_confirm_on_altf4 ? "True":"False",
			priv->appli_confirm_on_quit ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_application, str );

	g_free( str );
}

/**
 * ofa_prefs_assistant_quit_on_escape:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if assistant can be quit on Escape key.
 */
gboolean
ofa_prefs_assistant_quit_on_escape( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->assistant_quit_on_escape );
}

/**
 * ofa_prefs_assistant_confirm_on_escape:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if confirmation is required when quitting an assistant
 * on Escape key.
 */
gboolean
ofa_prefs_assistant_confirm_on_escape( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->assistant_confirm_on_escape );
}

/**
 * ofa_prefs_assistant_confirm_on_cancel:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if confirmation is required when quitting an assistant
 * on Cancel key.
 */
gboolean
ofa_prefs_assistant_confirm_on_cancel( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->assistant_confirm_on_cancel );
}

/**
 * ofa_prefs_assistant_is_willing_to_quit:
 * @getter: a #ofaIGetter instance.
 * @keyval: the hit key.
 *
 * Returns: %TRUE if the assistant can quit.
 */
gboolean
ofa_prefs_assistant_is_willing_to_quit( ofaIGetter *getter, guint keyval )
{
	static const gchar *thisfn = "ofa_prefs_assistant_is_willing_to_quit";
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;
	gboolean ok_escape, ok_cancel;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok_escape = ( keyval == GDK_KEY_Escape &&
					priv->assistant_quit_on_escape &&
					(!priv->assistant_confirm_on_escape || is_willing_to_quit( prefs )));
	g_debug( "%s: ok_escape=%s", thisfn, ok_escape ? "True":"False" );

	ok_cancel = ( keyval == GDK_KEY_Cancel &&
					(!priv->assistant_confirm_on_cancel || is_willing_to_quit( prefs )));
	g_debug( "%s: ok_cancel=%s", thisfn, ok_cancel ? "True":"False" );

	return( ok_escape || ok_cancel );
}

static gboolean
is_willing_to_quit( ofaPrefs *self )
{
	gboolean ok;

	ok = my_utils_dialog_question(
			NULL,
			_( "Are you sure you want to quit this assistant ?" ),
			_( "_Quit" ));

	return( ok );
}

/**
 * ofa_prefs_assistant_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @quit_on_escape: whether the assistant may be quitted by hitting the
 *  'Escape' key.
 * @confirm_on_escape: whether quitting by 'Escape' key requires a user
 *  confirmation.
 * @confirm_on_cancel: whether quitting by 'Cancel' button requires a user
 *  confirmation.
 *
 * Set the user settings.
 */
void
ofa_prefs_assistant_set_user_settings( ofaIGetter *getter,
											gboolean quit_on_escape, gboolean confirm_on_escape, gboolean confirm_on_cancel )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->assistant_quit_on_escape = quit_on_escape;
	priv->assistant_confirm_on_escape = confirm_on_escape;
	priv->assistant_confirm_on_cancel = confirm_on_cancel;

	assistant_write_settings( prefs );
}

/*
 * Assistant settings: quit_on_escape; confirm_on_escape; confirm_on_cancel;
 */
static void
assistant_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_assistant );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->assistant_quit_on_escape = my_utils_boolean_from_str( cstr );

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->assistant_confirm_on_escape = my_utils_boolean_from_str( cstr );

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->assistant_confirm_on_cancel = my_utils_boolean_from_str( cstr );

	my_isettings_free_string_list( settings, strlist );
}

static void
assistant_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;%s;%s;",
			priv->assistant_quit_on_escape ? "True":"False",
			priv->assistant_confirm_on_escape ? "True":"False",
			priv->assistant_confirm_on_cancel ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_assistant, str );

	g_free( str );
}

/**
 * ofa_prefs_check_integrity_get_display_all:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if we have to display all messages, %FALSE to display
 * only errors.
 */
gboolean
ofa_prefs_check_integrity_get_display_all( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->check_integrity_display_all );
}

/**
 * ofa_prefs_check_integrity_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @display: whether the CheckDBMSIntegrity dialog should display all
 *  messages; alternative is to display only error messages.
 *
 * Set the user settings.
 */
void
ofa_prefs_check_integrity_set_user_settings( ofaIGetter *getter, gboolean display )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->check_integrity_display_all = display;

	check_integrity_write_settings( prefs );
}

/*
 * CheckIntegrity settings: display_all;
 */
static void
check_integrity_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_check_integrity );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->check_integrity_display_all = my_utils_boolean_from_str( cstr );

	my_isettings_free_string_list( settings, strlist );
}

static void
check_integrity_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;",
			priv->check_integrity_display_all ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_check_integrity, str );

	g_free( str );
}

/**
 * ofa_prefs_date_get_display_format:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered format for displaying the dates
 */
myDateFormat
ofa_prefs_date_get_display_format( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), MY_DATE_YYMD );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), MY_DATE_YYMD );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, MY_DATE_YYMD );

	return( priv->date_display_format );
}

/**
 * ofa_prefs_date_get_check_format:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the prefered format for visually checking the dates
 */
myDateFormat
ofa_prefs_date_get_check_format( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), MY_DATE_YYMD );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), MY_DATE_YYMD );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, MY_DATE_YYMD );

	return( priv->date_check_format );
}

/**
 * ofa_prefs_date_get_overwrite:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: whether the edition should start in overwrite mode.
 */
gboolean
ofa_prefs_date_get_overwrite( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->date_overwrite );
}

/**
 * ofa_prefs_date_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @display: the display format (e.g. on a #GtkEntry).
 * @check: the check format (e.g. on a #GtkLabel).
 * @overwrite: whether the #GtkEntry should be in overwrite mode.
 *
 * Set the user settings.
 */
void
ofa_prefs_date_set_user_settings( ofaIGetter *getter, myDateFormat display, myDateFormat check, gboolean overwrite )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->date_display_format = display;
	priv->date_check_format = check;
	priv->date_overwrite = overwrite;

	date_write_settings( prefs );
}

/*
 * Date settings: display_format(i); check_format(i); overwrite(b);
 */
static void
date_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_date );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->date_display_format = my_strlen( cstr ) ? atoi( cstr ) : MY_DATE_YYMD;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->date_check_format = my_strlen( cstr ) ? atoi( cstr ) : MY_DATE_YYMD;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->date_overwrite = my_utils_boolean_from_str( cstr );

	my_isettings_free_string_list( settings, strlist );
}

static void
date_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%u;%u;%s;",
			priv->date_display_format,
			priv->date_check_format,
			priv->date_overwrite ? "True":"False" );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_date, str );

	g_free( str );
}

/**
 * ofa_prefs_export_get_default_folder:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the default export folder.
 *
 * The returned string is owned by #ofaPrefs, and should not be released
 * by the caller.
 */
const gchar *
ofa_prefs_export_get_default_folder( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->export_default_folder );
}

/**
 * ofa_prefs_export_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @folder: the default export folder.
 *
 * Set the user settings.
 */
void
ofa_prefs_export_set_user_settings( ofaIGetter *getter, const gchar *folder )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->export_default_folder );
	priv->export_default_folder = g_strdup( folder );

	export_write_settings( prefs );
}

/*
 * Export settings: default_folder;
 */
static void
export_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_export );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->export_default_folder );
		priv->export_default_folder = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
export_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;",
			priv->export_default_folder );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_export, str );

	g_free( str );
}

/**
 * ofa_prefs_mainbook_get_startup_mode:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the startup mmode of the main window.
 *
 * This determines how the main window is created at the application
 * startup:
 * - either with a natural size,
 * - or with its minimal size: only the menubar is visible.
 *
 * In other words, we manage two display modes at application startup:
 * - mini  : startup_mode=mini
 * - normal: startup_mode=normal
 */
ofeMainbookStartup
ofa_prefs_mainbook_get_startup_mode( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->mainbook_startup_mode );
}

/**
 * ofa_prefs_mainbook_get_open_mode:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the sizing mode of the main window when opening a dossier.
 *
 * This may be:
 * - either keep its startup time,
 * - or let it go with a normal size,
 *
 * Giving the startup mode above, the main window may have two display
 * modes when a dossier is opened:
 * - mini  : startup_mode=mini   and open_mode=keep
 * - normal: startup_mode=normal or  open_mode=natural
 */
ofeMainbookOpen
ofa_prefs_mainbook_get_open_mode( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->mainbook_open_mode );
}

/**
 * ofa_prefs_mainbook_get_tabs_mode:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the display mode of the main notebook tabs.
 *
 * Either the tabs are reorderable via the standard mechanism of the
 * #GtkNotebook (which happens to internally be a Drag-and-drop
 * implementation), or the tabs are detachable via our own DnD
 * implementation.
 *
 * This option is only relevant when the main window is normally
 * displayed (i.e. with a child area large enough to contain the
 * main notebook).
 */
ofeMainbookTabs
ofa_prefs_mainbook_get_tabs_mode( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->mainbook_tabs_mode );
}

/**
 * ofa_prefs_mainbook_get_with_detach_pin:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: %TRUE if the user can detach the main tabs via a pin button.
 *
 * This option is only valid when the user has also chosen to be able to
 * reorder the tabs instead of DnD them.
 *
 * Defaults is %FALSE.
 *
 * This option is only relevant when the main window is normally
 * displayed (i.e. with a child area large enough to contain the
 * main notebook).
 */
gboolean
ofa_prefs_mainbook_get_with_detach_pin( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->mainbook_with_detach_pin );
}

/**
 * ofa_prefs_mainbook_get_close_mode:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the behavior of the main window when a dossier is closed.
 *
 * Either the main window keep its current size, or it is reset to its
 * startup configuration.
 */
ofeMainbookClose
ofa_prefs_mainbook_get_close_mode( ofaIGetter *getter )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_val_if_fail( prefs && OFA_IS_PREFS( prefs ), FALSE );

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->mainbook_close_mode );
}

/**
 * ofa_prefs_mainbook_set_user_settings:
 * @getter: a #ofaIGetter instance.
 * @startup_mode: the startup mode.
 * @pages_size: the main window behavior on dossier opening.
 * @tabs_mode: whether the tabs of the main notebook are reorderable.
 * @with_detach_pin: whether the tabs of the main notebook display a detach pin.
 * @close_mode: the main window behavior on dossier closing.
 *
 * Set the user settings.
 */
void
ofa_prefs_mainbook_set_user_settings( ofaIGetter *getter,
						ofeMainbookStartup startup_mode,
						ofeMainbookOpen pages_size, ofeMainbookTabs tabs_mode, gboolean with_detach_pin,
						ofeMainbookClose close_mode )
{
	ofaPrefs *prefs;
	ofaPrefsPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	prefs = ofa_igetter_get_user_prefs( getter );
	g_return_if_fail( prefs && OFA_IS_PREFS( prefs ));

	priv = ofa_prefs_get_instance_private( prefs );
	g_return_if_fail( !priv->dispose_has_run );

	priv->mainbook_startup_mode = startup_mode;
	priv->mainbook_open_mode = pages_size;
	priv->mainbook_tabs_mode = tabs_mode;
	priv->mainbook_with_detach_pin = with_detach_pin;
	priv->mainbook_close_mode = close_mode;

	mainbook_write_settings( prefs );
}

/*
 * To make the user interface clearer, booleans are often displayed as
 * a two-radio-buttons group.
 * Because we are not really sure to not add an option in a future time,
 * code is written to use an enumeration.
 * And because this enumeration is subject to changes with the code, it
 * is written in user settings as alpha codes, which are expected to be
 * invariant.
 *
 * MainNotebook settings: startup_mode, pages_size; tabs_mode; with_pin; close_mode;
 */
static void
mainbook_read_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, st_mainbook );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->mainbook_startup_mode = enum_code_to_enum( st_mainbook_startup_mode, cstr, priv->mainbook_startup_mode );
	}

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->mainbook_open_mode = enum_code_to_enum( st_mainbook_open_mode, cstr, priv->mainbook_open_mode );
	}

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->mainbook_tabs_mode = enum_code_to_enum( st_mainbook_tabs_mode, cstr, priv->mainbook_tabs_mode );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->mainbook_with_detach_pin = my_utils_boolean_from_str( cstr );

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->mainbook_close_mode = enum_code_to_enum( st_mainbook_close_mode, cstr, priv->mainbook_close_mode );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
mainbook_write_settings( ofaPrefs *self )
{
	ofaPrefsPrivate *priv;
	myISettings *settings;
	gchar *str;

	priv = ofa_prefs_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));

	str = g_strdup_printf( "%s;%s;%s;%s;%s;",
			enum_enum_to_code( st_mainbook_startup_mode, priv->mainbook_tabs_mode, MAINBOOK_STARTNORMAL ),
			enum_enum_to_code( st_mainbook_open_mode, priv->mainbook_open_mode, MAINBOOK_OPENNATURAL ),
			enum_enum_to_code( st_mainbook_tabs_mode, priv->mainbook_tabs_mode, MAINBOOK_TABREORDER ),
			priv->mainbook_with_detach_pin ? "True":"False",
			enum_enum_to_code( st_mainbook_close_mode, priv->mainbook_close_mode, MAINBOOK_CLOSEKEEP ));

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, st_mainbook, str );

	g_free( str );
}

/*
 * Convert the unlocalized code found in user settings to a suitable
 * value from the @table of #sEnum structures.
 */
static gint
enum_code_to_enum( const sEnum *table, const gchar *code, gint def_value )
{
	guint i;

	for( i=0 ; table[i].id ; ++i ){
		if( !my_collate( code, table[i].code )){
			return( table[i].id );
		}
	}

	g_warning( "code=%s: unknown or invalid code, returning default value=%d", code, def_value );

	return( def_value );
}

/*
 * Convert an enum value to the unlocalized code to be written in user
 * settings.
 * Set @def_value to -1 if you do not want any default value.
 */
static const gchar *
enum_enum_to_code( const sEnum *table, gint value, gint def_value )
{
	const gchar *code;

	code = enum_find_enum( table, value );

	if( my_strlen( code )){
		return( code );
	}

	g_warning( "value=%d: unknown or invalid enum, returning code for default value=%d", value, def_value );

	return( enum_find_enum( table, def_value ));
}

static const gchar *
enum_find_enum( const sEnum *table, gint value )
{
	guint i;

	for( i=0 ; table[i].id ; ++i ){
		if( value == table[i].id ){
			return( table[i].code );
		}
	}

	return( NULL );
}
