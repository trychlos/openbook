/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-accounts-filter-vv-bin.h"
#include "ui/ofa-dates-filter-hv-bin.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-balance-bin.h"

/* private instance data
 */
struct _ofaBalanceBinPrivate {
	gboolean                dispose_has_run;
	ofaMainWindow          *main_window;

	/* UI
	 */
	ofaAccountsFilterVVBin *accounts_filter;
	GtkWidget              *per_class_btn;		/* subtotal per class */
	GtkWidget              *new_page_btn;
	ofaDatesFilterHVBin    *dates_filter;
	GtkWidget              *accounts_balance_btn;
	GtkWidget              *from_prompt;
	GtkWidget              *from_entry;

	/* internals
	 */
	gboolean                per_class;
	gboolean                new_page;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-balance-bin.ui";
static const gchar *st_ui_id            = "BalanceBin";
static const gchar *st_settings         = "RenderBalances";

G_DEFINE_TYPE( ofaBalanceBin, ofa_balance_bin, GTK_TYPE_BIN )

static GtkWidget *load_dialog( ofaBalanceBin *bin );
static void       setup_account_selection( ofaBalanceBin *self, GtkContainer *parent );
static void       setup_date_selection( ofaBalanceBin *self, GtkContainer *parent );
static void       setup_others( ofaBalanceBin *self, GtkContainer *parent );
static void       on_accounts_filter_changed( ofaIAccountsFilter *filter, ofaBalanceBin *self );
static void       on_per_class_toggled( GtkToggleButton *button, ofaBalanceBin *self );
static void       on_new_page_toggled( GtkToggleButton *button, ofaBalanceBin *self );
static void       on_accounts_balance_toggled( GtkToggleButton *button, ofaBalanceBin *self );
static void       on_dates_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaBalanceBin *self );
static void       load_settings( ofaBalanceBin *bin );
static void       set_settings( ofaBalanceBin *bin );

static void
balance_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_balance_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BALANCE_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_bin_parent_class )->finalize( instance );
}

static void
balance_bin_dispose( GObject *instance )
{
	ofaBalanceBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BALANCE_BIN( instance ));

	priv = OFA_BALANCE_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_balance_bin_parent_class )->dispose( instance );
}

static void
ofa_balance_bin_init( ofaBalanceBin *self )
{
	static const gchar *thisfn = "ofa_balance_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BALANCE_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_BALANCE_BIN, ofaBalanceBinPrivate );
}

static void
ofa_balance_bin_class_init( ofaBalanceBinClass *klass )
{
	static const gchar *thisfn = "ofa_balance_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = balance_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = balance_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaBalanceBinPrivate ));

	/**
	 * ofaBalanceBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBalanceBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_BALANCE_BIN,
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
 * ofa_balance_bin_new:
 * @main_window:
 *
 * Returns: a newly allocated #ofaBalanceBin object.
 */
ofaBalanceBin *
ofa_balance_bin_new( ofaMainWindow *main_window )
{
	ofaBalanceBin *self;
	GtkWidget *parent;

	self = g_object_new( OFA_TYPE_BALANCE_BIN, NULL );

	self->priv->main_window = main_window;

	parent = load_dialog( self );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), NULL );

	setup_account_selection( self, GTK_CONTAINER( parent ));
	setup_date_selection( self, GTK_CONTAINER( parent ));
	setup_others( self, GTK_CONTAINER( parent ));

	load_settings( self );

	return( self );
}

/*
 * returns: the GtkContainer parent
 */
static GtkWidget *
load_dialog( ofaBalanceBin *bin )
{
	GtkWidget *top_widget;

	top_widget = my_utils_container_attach_from_ui( GTK_CONTAINER( bin ), st_ui_xml, st_ui_id, "top" );
	g_return_val_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ), NULL );

	return( top_widget );
}

static void
setup_account_selection( ofaBalanceBin *self, GtkContainer *parent )
{
	ofaBalanceBinPrivate *priv;
	GtkWidget *alignment;
	ofaAccountsFilterVVBin *bin;

	priv = self->priv;

	alignment = my_utils_container_get_child_by_name( parent, "accounts-filter" );
	g_return_if_fail( alignment && GTK_IS_CONTAINER( alignment ));

	bin = ofa_accounts_filter_vv_bin_new( priv->main_window );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( bin ));

	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_accounts_filter_changed ), self );

	priv->accounts_filter = bin;
}

static void
setup_date_selection( ofaBalanceBin *self, GtkContainer *parent )
{
	ofaBalanceBinPrivate *priv;
	GtkWidget *alignment, *label;
	ofaDatesFilterHVBin *bin;
	GtkWidget *check;

	priv = self->priv;

	alignment = my_utils_container_get_child_by_name( parent, "dates-filter" );
	g_return_if_fail( alignment && GTK_IS_CONTAINER( alignment ));

	bin = ofa_dates_filter_hv_bin_new();
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( bin ));

	/* instead of "effect dates filter" */
	label = ofa_idates_filter_get_frame_label( OFA_IDATES_FILTER( bin ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	check = gtk_check_button_new_with_mnemonic( _( "Acc_ounts balance" ));
	ofa_idates_filter_add_widget( OFA_IDATES_FILTER( bin ), check, IDATES_FILTER_BEFORE );
	g_signal_connect( check, "toggled", G_CALLBACK( on_accounts_balance_toggled ), self );
	priv->accounts_balance_btn = check;

	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_dates_filter_changed ), self );

	priv->from_prompt =
			ofa_idates_filter_get_prompt( OFA_IDATES_FILTER( bin ), IDATES_FILTER_FROM );
	priv->from_entry =
			ofa_idates_filter_get_entry( OFA_IDATES_FILTER( bin ), IDATES_FILTER_FROM );

	priv->dates_filter = bin;
}

static void
setup_others( ofaBalanceBin *self, GtkContainer *parent )
{
	ofaBalanceBinPrivate *priv;
	GtkWidget *toggle;

	priv = self->priv;

	/* setup the new_page btn before the per_class one in order to be
	 * safely updated when setting the later preference */
	toggle = my_utils_container_get_child_by_name( parent, "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = toggle;

	toggle = my_utils_container_get_child_by_name( parent, "p3-per-class" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_per_class_toggled ), self );
	priv->per_class_btn = toggle;
}

static void
on_accounts_filter_changed( ofaIAccountsFilter *filter, ofaBalanceBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_per_class_toggled( GtkToggleButton *button, ofaBalanceBin *self )
{
	ofaBalanceBinPrivate *priv;
	gboolean bvalue;

	priv = self->priv;

	bvalue = gtk_toggle_button_get_active( button );
	gtk_widget_set_sensitive( priv->new_page_btn, bvalue );

	priv->per_class = bvalue;

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaBalanceBin *self )
{
	ofaBalanceBinPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_accounts_balance_toggled( GtkToggleButton *button, ofaBalanceBin *self )
{
	ofaBalanceBinPrivate *priv;
	gboolean active;
	const GDate *begin;
	ofoDossier *dossier;

	priv = self->priv;

	active = gtk_toggle_button_get_active( button );
	if( active ){
		dossier = ofa_main_window_get_dossier( priv->main_window );
		g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

		begin = ofo_dossier_get_exe_begin( dossier );
		ofa_idates_filter_set_date(
				OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, begin );
	}
	gtk_widget_set_sensitive( priv->from_prompt, !active );
	gtk_widget_set_sensitive( priv->from_entry, !active );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_dates_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaBalanceBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_balance_bin_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_balance_bin_is_valid( ofaBalanceBin *bin, gchar **message )
{
	ofaBalanceBinPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), FALSE );

	priv = bin->priv;
	valid = FALSE;
	if( message ){
		*message = NULL;
	}

	if( !priv->dispose_has_run ){

		valid = ofa_idates_filter_is_valid(
						OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, message ) &&
				ofa_idates_filter_is_valid(
						OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO, message );

		if( valid ){
			set_settings( bin );
		}
	}

	return( valid );
}

/**
 * ofa_balance_bin_get_accounts_filter:
 */
ofaIAccountsFilter *
ofa_balance_bin_get_accounts_filter( const ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	ofaIAccountsFilter *filter;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), NULL );

	priv = bin->priv;
	filter = NULL;

	if( !priv->dispose_has_run ){

		filter = OFA_IACCOUNTS_FILTER( priv->accounts_filter );
	}

	return( filter );
}

/**
 * ofa_balance_bin_get_accounts_balance:
 */
gboolean
ofa_balance_bin_get_accounts_balance( const ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	gboolean acc_balance;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), FALSE );

	priv = bin->priv;
	acc_balance = FALSE;

	if( !priv->dispose_has_run ){

		acc_balance = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->accounts_balance_btn ));
	}

	return( acc_balance );
}

/**
 * ofa_balance_bin_get_subtotal_per_class:
 */
gboolean
ofa_balance_bin_get_subtotal_per_class( const ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	gboolean subtotal;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), FALSE );

	priv = bin->priv;
	subtotal = FALSE;

	if( !priv->dispose_has_run ){

		subtotal = priv->per_class;
	}

	return( subtotal );
}

/**
 * ofa_balance_bin_get_new_page_per_class:
 */
gboolean
ofa_balance_bin_get_new_page_per_class( const ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), FALSE );

	priv = bin->priv;
	new_page = FALSE;

	if( !priv->dispose_has_run ){

		new_page = priv->new_page;
	}

	return( new_page );
}

/**
 * ofa_balance_bin_get_date_filter:
 */
ofaIDatesFilter *
ofa_balance_bin_get_dates_filter( const ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	ofaIDatesFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_BALANCE_BIN( bin ), NULL );

	priv = bin->priv;
	date_filter = NULL;

	if( !priv->dispose_has_run ){

		date_filter = OFA_IDATES_FILTER( priv->dates_filter );
	}

	return( date_filter );
}

/*
 * setttings:
 * account_from;account_to;all_accounts;effect_from;effect_to;subtotal_per_class;new_page_per_class;accounts_balance;
 */
static void
load_settings( ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = bin->priv;
	list = ofa_settings_get_string_list( st_settings );

	it = list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccounts_filter_set_account(
				OFA_IACCOUNTS_FILTER( priv->accounts_filter ), IACCOUNTS_FILTER_FROM, cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccounts_filter_set_account(
				OFA_IACCOUNTS_FILTER( priv->accounts_filter ), IACCOUNTS_FILTER_TO, cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		ofa_iaccounts_filter_set_all_accounts(
				OFA_IACCOUNTS_FILTER( priv->accounts_filter ), my_utils_boolean_from_str( cstr ));
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idates_filter_set_date(
				OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idates_filter_set_date(
				OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->per_class_btn ), my_utils_boolean_from_str( cstr ));
		on_per_class_toggled( GTK_TOGGLE_BUTTON( priv->per_class_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->new_page_btn ), my_utils_boolean_from_str( cstr ));
		on_new_page_toggled( GTK_TOGGLE_BUTTON( priv->new_page_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->accounts_balance_btn ), my_utils_boolean_from_str( cstr ));
		on_accounts_balance_toggled( GTK_TOGGLE_BUTTON( priv->accounts_balance_btn ), bin );
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaBalanceBin *bin )
{
	ofaBalanceBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;
	const gchar *from_account, *to_account;
	gboolean all_accounts, acc_balance;

	priv = bin->priv;

	from_account = ofa_iaccounts_filter_get_account(
			OFA_IACCOUNTS_FILTER( priv->accounts_filter ), IACCOUNTS_FILTER_FROM );
	to_account = ofa_iaccounts_filter_get_account(
			OFA_IACCOUNTS_FILTER( priv->accounts_filter ), IACCOUNTS_FILTER_TO );
	all_accounts = ofa_iaccounts_filter_get_all_accounts(
			OFA_IACCOUNTS_FILTER( priv->accounts_filter ));

	acc_balance = ofa_balance_bin_get_accounts_balance( bin );

	sdfrom = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;%s;%s;",
			from_account ? from_account : "",
			to_account ? to_account : "",
			all_accounts ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			priv->per_class ? "True":"False",
			priv->new_page ? "True":"False",
			acc_balance ? "True":"False" );

	ofa_settings_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
