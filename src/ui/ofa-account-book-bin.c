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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"

#include "ui/ofa-account-filter-vv-bin.h"
#include "ui/ofa-account-book-bin.h"

/* private instance data
 */
typedef struct {
	gboolean               dispose_has_run;

	/* initialization
	 */
	ofaIGetter            *getter;

	/* UI
	 */
	ofaAccountFilterVVBin *account_filter;
	ofaDateFilterHVBin    *date_filter;
	GtkWidget             *new_page_btn;

	/* internals
	 */
	gboolean               new_page;
}
	ofaAccountBookBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-account-book-bin.ui";
static const gchar *st_settings         = "RenderAccountsBook";

static void setup_bin( ofaAccountBookBin *bin );
static void setup_account_selection( ofaAccountBookBin *bin );
static void setup_date_selection( ofaAccountBookBin *bin );
static void setup_others( ofaAccountBookBin *bin );
static void on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBookBin *self );
static void on_new_page_toggled( GtkToggleButton *button, ofaAccountBookBin *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBookBin *self );
static void load_settings( ofaAccountBookBin *bin );
static void set_settings( ofaAccountBookBin *bin );

G_DEFINE_TYPE_EXTENDED( ofaAccountBookBin, ofa_account_book_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaAccountBookBin ))

static void
accounts_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_book_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_bin_parent_class )->finalize( instance );
}

static void
accounts_book_bin_dispose( GObject *instance )
{
	ofaAccountBookBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_BOOK_BIN( instance ));

	priv = ofa_account_book_bin_get_instance_private( OFA_ACCOUNT_BOOK_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_book_bin_parent_class )->dispose( instance );
}

static void
ofa_account_book_bin_init( ofaAccountBookBin *self )
{
	static const gchar *thisfn = "ofa_account_book_bin_init";
	ofaAccountBookBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_BOOK_BIN( self ));

	priv = ofa_account_book_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_account_book_bin_class_init( ofaAccountBookBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_book_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_book_bin_finalize;

	/**
	 * ofaAccountBookBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountBookBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ACCOUNT_BOOK_BIN,
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
 * ofa_account_book_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a newly allocated #ofaAccountBookBin object.
 */
ofaAccountBookBin *
ofa_account_book_bin_new( ofaIGetter *getter )
{
	ofaAccountBookBin *self;
	ofaAccountBookBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	self = g_object_new( OFA_TYPE_ACCOUNT_BOOK_BIN, NULL );

	priv = ofa_account_book_bin_get_instance_private( self );

	priv->getter = getter;

	setup_bin( self );
	setup_account_selection( self );
	setup_date_selection( self );
	setup_others( self );

	load_settings( self );

	return( self );
}

static void
setup_bin( ofaAccountBookBin *bin )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "abb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_account_selection( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	GtkWidget *parent;
	ofaAccountFilterVVBin *filter;

	priv = ofa_account_book_bin_get_instance_private( bin );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "account-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_account_filter_vv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));
	priv->account_filter = filter;

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_account_filter_changed ), bin );

}

static void
setup_date_selection( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_account_book_bin_get_instance_private( bin );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));
	priv->date_filter = filter;

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_date_filter_changed ), bin );

}

static void
setup_others( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	GtkWidget *toggle;

	priv = ofa_account_book_bin_get_instance_private( bin );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p3-one-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	priv->new_page_btn = toggle;

	g_signal_connect( toggle, "toggled", G_CALLBACK( on_new_page_toggled ), bin );
}

static void
on_account_filter_changed( ofaIAccountFilter *filter, ofaAccountBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaAccountBookBin *self )
{
	ofaAccountBookBinPrivate *priv;

	priv = ofa_account_book_bin_get_instance_private( self );

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaAccountBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_account_book_bin_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_account_book_bin_is_valid( ofaAccountBookBin *bin, gchar **message )
{
	ofaAccountBookBinPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_BIN( bin ), FALSE );

	priv = ofa_account_book_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( message ){
		*message = NULL;
	}

	valid = ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, message ) &&
			ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, message );

	if( valid ){
		set_settings( bin );
	}

	return( valid );
}

/**
 * ofa_account_book_bin_get_account_filter:
 */
ofaIAccountFilter *
ofa_account_book_bin_get_account_filter( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	ofaIAccountFilter *filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_BIN( bin ), NULL );

	priv = ofa_account_book_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	filter = OFA_IACCOUNT_FILTER( priv->account_filter );

	return( filter );
}

/**
 * ofa_account_book_bin_get_new_page_per_account:
 */
gboolean
ofa_account_book_bin_get_new_page_per_account( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_BIN( bin ), FALSE );

	priv = ofa_account_book_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	new_page = priv->new_page;
return( new_page );
}

/**
 * ofa_account_book_bin_get_date_filter:
 */
ofaIDateFilter *
ofa_account_book_bin_get_date_filter( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_BOOK_BIN( bin ), NULL );

	priv = ofa_account_book_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/*
 * setttings:
 * account_from;account_to;all_accounts;effect_from;effect_to;new_page_per_account;
 */
static void
load_settings( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = ofa_account_book_bin_get_instance_private( bin );

	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
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
		on_new_page_toggled( GTK_TOGGLE_BUTTON( priv->new_page_btn ), bin );
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaAccountBookBin *bin )
{
	ofaAccountBookBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;
	const gchar *from_account, *to_account;
	gboolean all_accounts;

	priv = ofa_account_book_bin_get_instance_private( bin );

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

	ofa_settings_user_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
