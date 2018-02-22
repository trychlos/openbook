/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-account-balance-args.h"
#include "ui/ofa-account-filter-vv-bin.h"

/* private instance data
 */
typedef struct {
	gboolean               dispose_has_run;

	/* initialization
	 */
	ofaIGetter            *getter;
	gchar                 *settings_prefix;

	/* runtime
	 */
	myISettings           *settings;
	gboolean               per_class;
	gboolean               new_page;

	/* UI
	 */
	ofaAccountFilterVVBin *account_filter;
	GtkWidget             *per_class_btn;			/* subtotal per class */
	GtkWidget             *new_page_btn;
	ofaDateFilterHVBin    *date_filter;
	GtkWidget             *accounts_balance_btn;
}
	ofaAccountBalanceArgsPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-account-balance-args.ui";

static void setup_runtime( ofaAccountBalanceArgs *self );
static void setup_bin( ofaAccountBalanceArgs *self );
static void setup_account_selection( ofaAccountBalanceArgs *self );
static void setup_date_selection( ofaAccountBalanceArgs *self );
static void setup_others( ofaAccountBalanceArgs *self );
static void on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBalanceArgs *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBalanceArgs *self );
static void on_per_class_toggled( GtkToggleButton *button, ofaAccountBalanceArgs *self );
static void on_new_page_toggled( GtkToggleButton *button, ofaAccountBalanceArgs *self );
static void read_settings( ofaAccountBalanceArgs *self );
static void write_settings( ofaAccountBalanceArgs *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountBalanceArgs, ofa_account_balance_args, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaAccountBalanceArgs ))

static void
account_balance_args_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_balance_args_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE_ARGS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_args_parent_class )->finalize( instance );
}

static void
account_balance_args_dispose( GObject *instance )
{
	ofaAccountBalanceArgsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BALANCE_ARGS( instance ));

	priv = ofa_account_balance_args_get_instance_private( OFA_ACCOUNT_BALANCE_ARGS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_balance_args_parent_class )->dispose( instance );
}

static void
ofa_account_balance_args_init( ofaAccountBalanceArgs *self )
{
	static const gchar *thisfn = "ofa_account_balance_args_init";
	ofaAccountBalanceArgsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BALANCE_ARGS( self ));

	priv = ofa_account_balance_args_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_account_balance_args_class_init( ofaAccountBalanceArgsClass *klass )
{
	static const gchar *thisfn = "ofa_account_balance_args_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_balance_args_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_balance_args_finalize;

	/**
	 * ofaAccountBalanceArgs::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountBalanceArgs *bin,
	 * 						gpointer             user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ACCOUNT_BALANCE_ARGS,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_account_balance_args_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a newly allocated #ofaAccountBalanceArgs object.
 */
ofaAccountBalanceArgs *
ofa_account_balance_args_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaAccountBalanceArgs *bin;
	ofaAccountBalanceArgsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_ACCOUNT_BALANCE_ARGS, NULL );

	priv = ofa_account_balance_args_get_instance_private( bin );

	priv->getter = getter;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_runtime( bin );
	setup_bin( bin );
	setup_account_selection( bin );
	setup_date_selection( bin );
	setup_others( bin );

	read_settings( bin );

	return( bin );
}

static void
setup_runtime( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;

	priv = ofa_account_balance_args_get_instance_private( self );

	priv->settings = ofa_igetter_get_user_settings( priv->getter );
}

static void
setup_bin( ofaAccountBalanceArgs *self )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "bb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_account_selection( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;
	GtkWidget *parent;
	ofaAccountFilterVVBin *filter;

	priv = ofa_account_balance_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_account_filter_vv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_account_filter_changed ), self );

	priv->account_filter = filter;
}

static void
setup_date_selection( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_account_balance_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));
	ofa_idate_filter_set_mandatory( OFA_IDATE_FILTER( filter ), IDATE_FILTER_FROM, TRUE );
	ofa_idate_filter_set_mandatory( OFA_IDATE_FILTER( filter ), IDATE_FILTER_TO, TRUE );

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->date_filter = filter;
}

static void
setup_others( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;
	GtkWidget *toggle;

	priv = ofa_account_balance_args_get_instance_private( self );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-per-class" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_per_class_toggled ), self );
	priv->per_class_btn = toggle;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = toggle;
}

static void
on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBalanceArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBalanceArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_per_class_toggled( GtkToggleButton *button, ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;

	priv = ofa_account_balance_args_get_instance_private( self );

	priv->per_class = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;

	priv = ofa_account_balance_args_get_instance_private( self );

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_account_balance_args_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 *
 * We request that the two dates be set and valid.
 */
gboolean
ofa_account_balance_args_is_valid( ofaAccountBalanceArgs *bin, gchar **message )
{
	ofaAccountBalanceArgsPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BALANCE_ARGS( bin ), FALSE );

	priv = ofa_account_balance_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( message ){
		*message = NULL;
	}

	valid = ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, message ) &&
			ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, message );

	if( valid ){
		write_settings( bin );
	}

	return( valid );
}

/**
 * ofa_account_balance_args_get_account_filter:
 * @bin: this #ofaAccountBalanceArgs widget.
 *
 * Returns: the #ofaIAccountFilter instance.
 */
ofaIAccountFilter *
ofa_account_balance_args_get_account_filter( ofaAccountBalanceArgs *bin )
{
	ofaAccountBalanceArgsPrivate *priv;
	ofaIAccountFilter *filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BALANCE_ARGS( bin ), NULL );

	priv = ofa_account_balance_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	filter = OFA_IACCOUNT_FILTER( priv->account_filter );

	return( filter );
}

/**
 * ofa_account_balance_args_get_date_filter:
 * @bin: this #ofaAccountBalanceArgs widget.
 *
 * Returns: the #ofaIDateFilter instance.
 */
ofaIDateFilter *
ofa_account_balance_args_get_date_filter( ofaAccountBalanceArgs *bin )
{
	ofaAccountBalanceArgsPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BALANCE_ARGS( bin ), NULL );

	priv = ofa_account_balance_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/**
 * ofa_account_balance_args_get_subtotal_per_class:
 * @bin: this #ofaAccountBalanceArgs widget.
 *
 * Returns: %TRUE if the user wants a subtotal per class.
 */
gboolean
ofa_account_balance_args_get_subtotal_per_class( ofaAccountBalanceArgs *bin )
{
	ofaAccountBalanceArgsPrivate *priv;
	gboolean subtotal;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BALANCE_ARGS( bin ), FALSE );

	priv = ofa_account_balance_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	subtotal = priv->per_class;

	return( subtotal );
}

/**
 * ofa_account_balance_args_get_new_page_per_class:
 * @bin: this #ofaAccountBalanceArgs widget.
 *
 * Returns: %TRUE if the user wants a new page per class.
 */
gboolean
ofa_account_balance_args_get_new_page_per_class( ofaAccountBalanceArgs *bin )
{
	ofaAccountBalanceArgsPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BALANCE_ARGS( bin ), FALSE );

	priv = ofa_account_balance_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	new_page = priv->new_page;

	return( new_page );
}

/*
 * setttings:
 * 		effect_from;effect_to;account_from;account_to;all_accounts;subtotal_per_class;new_page_per_class;
 */
static void
read_settings( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;
	gchar *key;

	priv = ofa_account_balance_args_get_instance_private( self );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	strlist = my_isettings_get_string_list( priv->settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccount_filter_set_account(
				OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_FROM, cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccount_filter_set_account(
				OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_TO, cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccount_filter_set_all_accounts(
				OFA_IACCOUNT_FILTER( priv->account_filter ), my_utils_boolean_from_str( cstr ));
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->per_class_btn ), my_utils_boolean_from_str( cstr ));
		on_per_class_toggled( GTK_TOGGLE_BUTTON( priv->per_class_btn ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->new_page_btn ), my_utils_boolean_from_str( cstr ));
		on_new_page_toggled( GTK_TOGGLE_BUTTON( priv->new_page_btn ), self );
	}

	my_isettings_free_string_list( priv->settings, strlist );
	g_free( key );
}

static void
write_settings( ofaAccountBalanceArgs *self )
{
	ofaAccountBalanceArgsPrivate *priv;
	gchar *str, *sdfrom, *sdto, *key;
	const gchar *from_account, *to_account;
	gboolean all_accounts;

	priv = ofa_account_balance_args_get_instance_private( self );

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	from_account = ofa_iaccount_filter_get_account(
			OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_FROM );
	to_account = ofa_iaccount_filter_get_account(
			OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_TO );
	all_accounts = ofa_iaccount_filter_get_all_accounts(
			OFA_IACCOUNT_FILTER( priv->account_filter ));

	str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;%s;",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			from_account ? from_account : "",
			to_account ? to_account : "",
			all_accounts ? "True":"False",
			priv->per_class ? "True":"False",
			priv->new_page ? "True":"False" );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	my_isettings_set_string( priv->settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
