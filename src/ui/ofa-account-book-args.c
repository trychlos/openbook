/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "api/ofo-account.h"

#include "ui/ofa-account-filter-vv-bin.h"
#include "ui/ofa-account-book-args.h"

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
	ofaHub                *hub;
	myISettings           *settings;
	gboolean               new_page;

	/* UI
	 */
	ofaAccountFilterVVBin *account_filter;
	ofaDateFilterHVBin    *date_filter;
	GtkWidget             *new_page_btn;
}
	ofaAccountBookArgsPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-account-book-args.ui";

static void setup_runtime( ofaAccountBookArgs *self );
static void setup_bin( ofaAccountBookArgs *self );
static void setup_account_selection( ofaAccountBookArgs *self );
static void setup_date_selection( ofaAccountBookArgs *self );
static void setup_others( ofaAccountBookArgs *self );
static void on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBookArgs *self );
static void on_new_page_toggled( GtkToggleButton *button, ofaAccountBookArgs *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBookArgs *self );
static void read_settings( ofaAccountBookArgs *self );
static void write_settings( ofaAccountBookArgs *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountBookArgs, ofa_account_book_args, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaAccountBookArgs ))

static void
accounts_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_book_args_finalize";
	ofaAccountBookArgsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_ARGS( instance ));

	/* free data members here */
	priv = ofa_account_book_args_get_instance_private( OFA_ACCOUNT_BOOK_ARGS( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_args_parent_class )->finalize( instance );
}

static void
accounts_book_bin_dispose( GObject *instance )
{
	ofaAccountBookArgsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_ARGS( instance ));

	priv = ofa_account_book_args_get_instance_private( OFA_ACCOUNT_BOOK_ARGS( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_ACCOUNT_BOOK_ARGS( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_args_parent_class )->dispose( instance );
}

static void
ofa_account_book_args_init( ofaAccountBookArgs *self )
{
	static const gchar *thisfn = "ofa_account_book_args_init";
	ofaAccountBookArgsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BOOK_ARGS( self ));

	priv = ofa_account_book_args_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_account_book_args_class_init( ofaAccountBookArgsClass *klass )
{
	static const gchar *thisfn = "ofa_account_book_args_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_book_bin_finalize;

	/**
	 * ofaAccountBookArgs::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountBookArgs *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ACCOUNT_BOOK_ARGS,
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
 * ofa_account_book_args_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a newly allocated #ofaAccountBookArgs object.
 */
ofaAccountBookArgs *
ofa_account_book_args_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaAccountBookArgs *bin;
	ofaAccountBookArgsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_ACCOUNT_BOOK_ARGS, NULL );

	priv = ofa_account_book_args_get_instance_private( bin );

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
setup_runtime( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;

	priv = ofa_account_book_args_get_instance_private( self );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->settings = ofa_hub_get_user_settings( priv->hub );
}

static void
setup_bin( ofaAccountBookArgs *self )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "abb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_account_selection( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;
	GtkWidget *parent;
	ofaAccountFilterVVBin *filter;

	priv = ofa_account_book_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_account_filter_vv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));
	priv->account_filter = filter;

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_account_filter_changed ), self );
}

static void
setup_date_selection( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_account_book_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new( ofa_igetter_get_hub( priv->getter ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));
	priv->date_filter = filter;

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );
}

static void
setup_others( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;
	GtkWidget *toggle;

	priv = ofa_account_book_args_get_instance_private( self );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-one-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	priv->new_page_btn = toggle;

	g_signal_connect( toggle, "toggled", G_CALLBACK( on_new_page_toggled ), self );
}

static void
on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBookArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;

	priv = ofa_account_book_args_get_instance_private( self );

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBookArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_account_book_args_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_account_book_args_is_valid( ofaAccountBookArgs *bin, gchar **message )
{
	ofaAccountBookArgsPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_ARGS( bin ), FALSE );

	priv = ofa_account_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( message ){
		*message = NULL;
	}

	valid = ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, message ) &&
			ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, message );

	return( valid );
}

/**
 * ofa_account_book_args_get_account_filter:
 */
ofaIAccountFilter *
ofa_account_book_args_get_account_filter( ofaAccountBookArgs *bin )
{
	ofaAccountBookArgsPrivate *priv;
	ofaIAccountFilter *filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_ARGS( bin ), NULL );

	priv = ofa_account_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	filter = OFA_IACCOUNT_FILTER( priv->account_filter );

	return( filter );
}

/**
 * ofa_account_book_args_get_new_page_per_account:
 */
gboolean
ofa_account_book_args_get_new_page_per_account( ofaAccountBookArgs *bin )
{
	ofaAccountBookArgsPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_ARGS( bin ), FALSE );

	priv = ofa_account_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	new_page = priv->new_page;
return( new_page );
}

/**
 * ofa_account_book_args_get_date_filter:
 */
ofaIDateFilter *
ofa_account_book_args_get_date_filter( ofaAccountBookArgs *bin )
{
	ofaAccountBookArgsPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_ARGS( bin ), NULL );

	priv = ofa_account_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/*
 * setttings:
 * account_from;account_to;all_accounts;effect_from;effect_to;new_page_per_account;
 */
static void
read_settings( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;
	gchar *key;

	priv = ofa_account_book_args_get_instance_private( self );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	strlist = my_isettings_get_string_list( priv->settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
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
write_settings( ofaAccountBookArgs *self )
{
	ofaAccountBookArgsPrivate *priv;
	gchar *str, *sdfrom, *sdto, *key;
	const gchar *from_account, *to_account;
	gboolean all_accounts;

	priv = ofa_account_book_args_get_instance_private( self );

	from_account = ofa_iaccount_filter_get_account(
			OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_FROM );
	to_account = ofa_iaccount_filter_get_account(
			OFA_IACCOUNT_FILTER( priv->account_filter ), IACCOUNT_FILTER_TO );
	all_accounts = ofa_iaccount_filter_get_all_accounts(
			OFA_IACCOUNT_FILTER( priv->account_filter ));

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;",
			from_account ? from_account : "",
			to_account ? to_account : "",
			all_accounts ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			priv->new_page ? "True":"False" );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	my_isettings_set_string( priv->settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
